#pragma once

/** @file
 * @ingroup material-core
 *
 * Material — what a region or a surface looks like, built up by
 * composition: a BASE (a colour, a gradient, an image, a noise, a
 * program, or another material), a stack of LAYERS each blended over
 * the ones beneath it through an opacity and an optional mask, an
 * optional lit SURFACE response, and an optional EFFECTS stage that
 * reads the painted layer's coverage. A program base is an instance of
 * a recipe: its parameter values mirrored as upload bytes, the live
 * bindings that overwrite fields at resolve, the materials filling its
 * slots. Comparable by value so a scene can prune, and a program base
 * resolves against a frame into the program plus the bytes to upload,
 * memoised on the last inputs.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/BlendMode.h>
#include <sigilmaterial/core/FrameData.h>
#include <sigilmaterial/core/Leaf.h>
#include <sigilmaterial/core/Parameters.h>
#include <sigilmaterial/core/Program.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/core/UniformBlock.h>
#include <sigilmotion/values/Animatable.h>

#include <concepts>
#include <cstddef>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/** Materials as recipe instances, and everything a surface is described
 *  with: a recipe is the definition, a material one instance of it, and
 *  resolving an instance answers the compiled program plus the bytes to
 *  upload. Reach for this namespace to describe WHAT a surface is; the
 *  nested namespaces are where it is described from — the backends, the
 *  textures, the generators, and the values already built. */
namespace sigil::material {

class Filter;
struct Layer;
struct LayerOptions;
struct MaterialParts;
struct SurfaceOptions;

namespace detail {
/** A part of a material only a renderer can supply or read — a
 *  gradient or an image base, an effects stage — held behind this
 *  interface so the value model links no renderer. The renderer defines
 *  the subclass beside the executor that draws it. */
class Part {
 public:
  virtual ~Part() = default;
  /** Value equality against @p other, which may be of any part type. */
  virtual bool equals(const Part& other) const = 0;
  /** Whether the part can change between frames with no edit. */
  virtual bool isRunning() const = 0;
  /** Whether the part depends on the box it is painted into. */
  virtual bool geometryDependent() const = 0;
};
}  // namespace detail

/** WHAT A REGION OR A SURFACE LOOKS LIKE, as one value built up by
 *  composition. The base is a colour, a program (an instance of a
 *  recipe), or a source only a renderer can supply (a gradient, an
 *  image); `layer()` stacks further materials over it, `surface()` adds
 *  the lit response a 3D renderer reads, and `effects()` a filter chain
 *  over the painted layer's coverage. A `Color` converts implicitly, so
 *  every place that takes a material takes a colour.
 *
 *  A PROGRAM base is a recipe instance: the VALUES as the bytes the shader receives, the
 *  BINDINGS that replace a field's bytes at every resolve, and the
 *  CHILDREN that fill the recipe's declared slots. A live binding or a
 *  live child makes the whole instance live. EQUALITY is by value —
 *  recipe identity, bytes, bindings, children and settings — so two
 *  materials describing the same thing compare equal and a node prunes.
 *  @trap A live binding compares by the output's IDENTITY, never by the
 *  number behind it. */
class Material {
 public:
  /** No paint: a fully transparent colour base. */
  Material();
  /** A flat colour. */
  // NOLINTNEXTLINE(google-explicit-constructor)
  Material(Color color);
  /** The designated-initialiser form: `Material{{.base = …, .layers =
   *  {…}, .surface = SurfaceOptions{…}}}`. The parts' layers stack over
   *  whatever layers the base already carries. */
  // NOLINTNEXTLINE(google-explicit-constructor)
  Material(const MaterialParts& parts);
  /** A base only a renderer can supply — a gradient, an image — built by
   *  that renderer's own factories. */
  explicit Material(std::shared_ptr<const detail::Part> source);

  /** An instance of @p recipe with the field values of @p parameters, whose
   *  type must be the struct the recipe was defined over. */
  template <class P>
  Material(std::shared_ptr<const Recipe> recipe, const P& parameters)
      : Material(std::move(recipe), &parameters, sizeof(P), &schema<P>()) {}
  /** An instance whose fields all start at zero. */
  explicit Material(std::shared_ptr<const Recipe> recipe);

  /** @name Building it up
   *  @{ */
  /** Stacks @p source over everything beneath it, composited with the
   *  normal blend at full opacity. */
  Material& layer(Material source);
  /** Stacks @p source over everything beneath it with @p options: its
   *  blend mode, its opacity, and the mask that says where it applies. */
  Material& layer(Material source, const LayerOptions& options);
  /** The lit response a 3D renderer reads: metallic, roughness, normal,
   *  emission and the rest. A material without one is flat. */
  Material& surface(const SurfaceOptions& options);
  /** The effects stage: a filter chain over the painted layer's
   *  coverage — shadows and glows under it, strokes and bevels over it.
   *  A renderer that has no coverage (a 3D surface) ignores it and says
   *  so once. Defined by the renderer that owns the filter. */
  Material& effects(const Filter& chain);
  /** @} */

  /** @name Reading the parts (for renderers)
   *  @{ */
  /** Whether the base is a program — a recipe instance. */
  bool hasProgram() const { return m_recipe != nullptr; }
  /** The base colour, when the base is a colour. */
  const Color* color() const;
  /** The renderer-supplied base, when the base is one. */
  const detail::Part* source() const;
  /** The layers, bottom first. */
  std::span<const Layer> layers() const;
  /** The lit response, when one was stated. */
  const SurfaceOptions* surface() const;
  /** The effects stage, when one was stated. Defined by the renderer that
   *  owns the filter. */
  const Filter* effects() const;
  /** Whether anything beyond the base was stated: a layer, a surface or
   *  effects — or the base is not a program. */
  bool isComposed() const { return m_composition != nullptr; }
  /** The base alone: this material without its layers, surface and
   *  effects. */
  [[nodiscard]] Material base() const;
  /** @} */

  /** The recipe of a program base. Only a material that `hasProgram()`
   *  has one. */
  const Recipe& recipe() const { return *m_recipe; }
  const std::shared_ptr<const Recipe>& recipePointer() const {
    return m_recipe;
  }
  /** THE SAME INSTANCE OVER @p recipe: the values, bindings, children and
   *  settings unchanged, resolving and caching against a second
   *  definition. @p recipe must have this one's parameters layout — it is a
   *  SPECIALIZATION of the same ABI, a body rewritten around a size or a
   *  constant a renderer knows only at draw — and a layout that differs is
   *  reported once and the material comes back on its own recipe. The
   *  specialization is a distinct identity, so it compiles and caches
   *  apart, which is the point: one program per specialization rather than
   *  one per draw. */
  [[nodiscard]] Material withRecipe(std::shared_ptr<const Recipe> recipe) const;

  /** Sets the field @p name to @p value. A name the recipe does not
   *  declare, or a value whose kind does not match the field, is reported
   *  once and ignored. */
  template <Uniform T>
  Material& set(std::string_view name, const T& value) {
    write(name, UniformTraits<T>::kind, &value, UniformTraits<T>::floats);
    return *this;
  }
  /** Sets the field @p name from @p floats, whose count must be the
   *  field's — the door for an array whose length is known only at run
   *  time. A count that is not the field's is reported once and ignored. */
  Material& set(std::string_view name, std::span<const float> floats) {
    write(name, ParameterType::FloatArray, floats.data(), floats.size());
    return *this;
  }
  /** Rewrites every field from @p parameters. */
  template <class P>
  Material& set(const P& parameters) {
    write(&parameters, sizeof(P), &schema<P>());
    return *this;
  }
  /** The field's current bytes, reinterpreted. The caller names the type
   *  the field was declared with. */
  template <Uniform T>
  T get(std::string_view name) const {
    T out{};
    const Field* f = m_recipe ? m_recipe->parameters().find(name) : nullptr;
    if (f && f->floats == UniformTraits<T>::floats)
      std::memcpy(&out, m_bytes.data() + f->offset, sizeof(T));
    return out;
  }

  /** Binds a float field to @p value: each resolve uploads what the
   *  animatable reads as now. A `motion::animatable(…)` value is the live
   *  case, a shaped `motion::bind(phase, {.envelope = …, .to = {…}})` is
   *  the same case with the arithmetic moved next to the uniform it feeds,
   *  and a plain number is a value written once per resolve. `unbind()` clears it.
   *
   *  A material holds no clock, so an animatable carrying its OWN
   *  transition has nothing to run it and reads as its target. Motion
   *  into a shader arrives through an Output the host steps. */
  Material& bind(std::string_view name, motion::Animatable<float> value);
  /** Drops the binding on @p name, leaving whatever `set()` last wrote in
   *  the field. Unknown names are ignored. */
  Material& unbind(std::string_view name);
  /** Binds an array field to @p block, whose size must equal the field's
   *  float count: each resolve uploads the block's current values. Null
   *  clears the binding. */
  Material& bind(std::string_view name,
                 std::shared_ptr<const UniformBlock> block);
  /** Whether @p name carries a binding — an animatable or a block —
   *  rather than only the bytes `set()` last wrote. A field bound to a
   *  plain number is bound like any other; whether anything behind a
   *  binding MOVES is `isRunning()`. */
  bool isBound(std::string_view name) const;
  /** Fills the slot @p name. A slot the recipe does not declare is
   *  reported once and ignored. */
  Material& slot(std::string_view name, Material material);
  /** Fills the slot @p name with a leaf the backend binds directly.
   *  A slot the recipe does not declare is reported once and ignored. */
  Material& slot(std::string_view name, std::shared_ptr<const Leaf> leaf);
  /** `slot(name, shared_ptr<const Leaf>)` over a leaf value. */
  template <class L>
    requires std::derived_from<L, Leaf>
  Material& slot(std::string_view name, L leaf) {
    return slot(name, std::shared_ptr<const Leaf>(
                          std::make_shared<const L>(std::move(leaf))));
  }
  /** The material in slot @p name, or null — including when the slot
   *  holds a leaf. */
  const Material* slot(std::string_view name) const;
  /** The leaf in slot @p name, or null — including when the slot holds a
   *  material. */
  const Leaf* leaf(std::string_view name) const;

  /** What fills a slot: exactly one of the two. */
  struct Slot {
    std::shared_ptr<const Material> material;
    std::shared_ptr<const Leaf> leaf;
  };
  /** The filled slots in recipe order. */
  std::span<const std::pair<std::string, Slot>> slots() const {
    return m_slots;
  }

  /** The strength a renderer blends this material in at, in [0, 1]. */
  Material& amount(float fraction);
  float amount() const { return m_amount; }
  /** Snaps the time this material sees to @p rate steps per second, so a
   *  material that need not move every frame resolves only when the
   *  snapped clock advances. Zero (the default) leaves time continuous. */
  Material& quantizeTime(float rate);
  float quantizeTime() const { return m_quantizeHz; }
  /** Anchors the material to the root frame rather than the node's. */
  Material& worldSpace(bool on = true);
  bool worldSpace() const { return m_worldSpace; }

  /** Whether the upload can change between frames with no edit to the
   *  material: a bound output or block, a recipe reading time or content
   *  scale, or an animated child. */
  bool isRunning() const;
  /** Whether the upload depends on where and how large the node is: a
   *  recipe reading the resolution or the world transform, or a
   *  geometry-dependent child. */
  bool geometryDependent() const;

  bool operator==(const Material& other) const;

  /** What a renderer uploads: the program for the target and variant,
   *  and the bytes in the recipe's `layout()`. The program is null when
   *  the recipe has no compiled form for the target, which the cache has
   *  already reported. */
  struct Resolved {
    std::shared_ptr<Program> program;
    std::span<const std::byte> bytes;
  };
  /** Samples the bindings, injects the declared frame inputs, and looks
   *  up the program. Memoised: when the sampled bytes, target and variant
   *  equal the previous call's, the previous result is returned without a
   *  cache lookup. */
  Resolved resolve(Target target, const FrameData& frame,
                   Variant variant = {}) const;

  /** The author-set bytes, in the recipe's `parameters()` layout. */
  std::span<const std::byte> bytes() const { return m_bytes; }

 private:
  struct Binding {
    std::string name;
    /** One or the other: an array field carries the block, a float field
     *  carries the animatable. The block is what tells the two apart. */
    motion::Animatable<float> value{0.0f};
    std::shared_ptr<const UniformBlock> block;
  };
  Material(std::shared_ptr<const Recipe> recipe, const void* parameters,
           size_t size, const Schema* schema);
  void write(const void* parameters, size_t size, const Schema* schema);
  void write(std::string_view name, ParameterType kind, const void* floats,
             size_t count);
  Binding* binding(std::string_view name);
  void place(std::string_view name, Slot slot);

  struct Composition;
  Composition& compose();
  /** The effects stage behind the renderer's filter (see effects()). */
  void placeEffects(std::shared_ptr<const detail::Part> effects);
  const detail::Part* effectsPart() const;

  std::shared_ptr<const Recipe> m_recipe;
  std::vector<std::byte> m_bytes;
  std::vector<Binding> m_bindings;
  std::vector<std::pair<std::string, Slot>> m_slots;
  float m_amount = 1.0f;
  float m_quantizeHz = 0.0f;
  bool m_worldSpace = false;
  /** The base when it is not a program, the layers, the surface and the
   *  effects; null for a bare program. Shared and copied on write, so a
   *  material costs a program no more than a pointer. */
  std::shared_ptr<const Composition> m_composition;

  struct Memo {
    bool valid = false;
    Target target{};
    Variant variant{};
    std::vector<std::byte> bytes;
    std::shared_ptr<Program> program;
  };
  mutable Memo m_memo;
  mutable std::vector<std::byte> m_scratch;
};

/** What a layer's mask reads from its source material. */
enum class MaskChannel : uint8_t {
  Alpha,      ///< coverage (CSS mask-mode: alpha)
  Luminance,  ///< brightness (CSS mask-mode: luminance)
  Red,
  Green,
  Blue,
};

/** WHERE A LAYER APPLIES: one channel of any material — an image, a
 *  noise, a signed-distance shape — remapped from [low, high] onto
 *  [0, 1] and optionally inverted. */
struct Mask {
  Material source;
  MaskChannel channel = MaskChannel::Alpha;
  float low = 0, high = 1;
  bool invert = false;
  bool operator==(const Mask&) const = default;
};

/** How a layer meets what is beneath it. */
struct LayerOptions {
  BlendMode blend = BlendMode::Normal;
  /** The layer composites in full with its blend mode, and the result
   *  mixes back toward what is beneath by this fraction. */
  float opacity = 1;
  std::optional<Mask> mask;
  bool operator==(const LayerOptions&) const = default;
};

/** One layer of the stack: a material and how it meets the ones beneath. */
struct Layer {
  Material source;
  LayerOptions options;
  bool operator==(const Layer&) const = default;
};

/** A surface channel: a number, or a material read by the channel (a
 *  texture, a noise), as a node graph connects an input. */
using Channel = std::variant<float, Material>;

/** THE LIT RESPONSE a 3D renderer reads — metallic-roughness with
 *  transmission and clearcoat. The base colour is the material's own
 *  base; every channel here takes a number or a material. */
struct SurfaceOptions {
  Channel metallic = 0.0f;
  Channel roughness = 0.5f;
  Channel occlusion = 1.0f;
  /** A tangent-space normal map. */
  std::optional<Material> normal;
  float normalScale = 1;
  /** Whether the normal map is authored green-down (DirectX), as some
   *  tools write it. */
  bool normalDirectX = false;
  Color emission = {0, 0, 0, 1};
  float emissionStrength = 0;
  std::optional<Material> emissionMap;
  /** Alpha below this is cut out; zero blends. */
  float alphaCutoff = 0;
  float clearcoat = 0;
  float transmission = 0;
  float ior = 1.5f;
  float thickness = 40;
  Color absorption = {0, 0, 0, 1};
  float reflectionWeight = 1;
  /** No lighting: the base colour as it is. */
  bool unlit = false;
  bool operator==(const SurfaceOptions&) const = default;
};

/** The designated-initialiser form of a material. The effects stage is
 *  added with `effects()`, since the filter belongs to its renderer. */
struct MaterialParts {
  Material base;
  std::vector<Layer> layers;
  std::optional<SurfaceOptions> surface;
};

/** Starts a chain from any base: `from(hexColor(0x223344)).layer(…)`. */
inline Material from(Material base) { return base; }

}  // namespace sigil::material
