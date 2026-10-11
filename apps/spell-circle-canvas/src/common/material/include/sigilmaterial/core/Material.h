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
#include <sigilmaterial/core/Parameters.h>
#include <sigilmaterial/core/Target.h>
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
class Leaf;
class Program;
class Recipe;
class UniformBlock;
struct FrameData;
struct Layer;
struct LayerOptions;
struct MaterialParts;
struct SurfaceOptions;

/** A RELIEF CUT FROM THE UNIT A MATERIAL IS PAINTED INTO: the unit's
 *  coverage as a height, softened over the shoulder. The painter supplies
 *  the unit when it paints — a glyph, a word, a line, a box, a stroke —
 *  so one material cuts each letter under a glyph ink and the whole
 *  passage as one mark under an ink spread across the canvas. */
struct ReliefOptions {
  /** Shoulder width in logical pixels. Zero leaves the unit flat. */
  float shoulder = 2.0f;
  /** Height relative to the shoulder width: positive raises the unit,
   *  negative impresses it, and zero leaves it flat. */
  float depth = 1.0f;
  bool operator==(const ReliefOptions&) const = default;
};

/** A BRUSHING THAT RUNS THE WAY THE UNIT RUNS: a highlight stretched along
 *  a stroke's tangent, a box's long axis, a glyph's box's long axis. */
struct BrushedOptions {
  /** The anisotropy, 0 none to 1 a highlight drawn into a line. */
  float strength = 1.0f;
  /** Degrees counter-clockwise from the unit's direction. */
  float rotation = 0.0f;
  bool operator==(const BrushedOptions&) const = default;
};

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
  /** THE PART WITH ITS INPUT @p name WRITTEN TO @p values, for a part a
   *  renderer computes from named inputs of its own (a procedural graph
   *  that cooks its pictures from them) — what `Material::set()` reaches
   *  on a material whose base is such a part. Null when the part takes
   *  no input by that name, which is every part that takes none. */
  virtual std::shared_ptr<const Part> withInput(
      std::string_view name, std::span<const float> values) const {
    (void)name;
    (void)values;
    return nullptr;
  }
  /** The same with @p name following @p value — what `Material::bind()`
   *  reaches. Null when the part takes no input by that name. */
  virtual std::shared_ptr<const Part> withBinding(
      std::string_view name, motion::Animatable<float> value) const {
    (void)name;
    (void)value;
    return nullptr;
  }
};
}  // namespace detail

/** WHAT A REGION OR A SURFACE LOOKS LIKE, as one value built up by
 *  composition. The base is a colour, a program (an instance of a
 *  recipe), or a source only a renderer can supply (a gradient, an
 *  image); `layer()` stacks further materials over it, `surface()` adds
 *  the surface a renderer shades, and `effects()` a filter chain
 *  over the painted layer's coverage. A `Color` converts implicitly, so
 *  every place that takes a material takes a colour.
 *
 *  A PROGRAM base is a recipe instance: the VALUES as the bytes the shader
 * receives, the BINDINGS that replace a field's bytes at every resolve, and the
 *  CHILDREN that fill the recipe's declared slots. A live binding or a
 *  live child makes the whole instance live. EQUALITY is by value —
 *  recipe identity, bytes, bindings, children and settings — so two
 *  materials describing the same thing compare equal and a node prunes.
 *  @trap A live binding compares by the output's IDENTITY, never by the
 *  number behind it. */
class Material {
 public:
  /** A flat colour. */
  // NOLINTNEXTLINE(google-explicit-constructor)
  Material(Color color);
  /** The designated-initialiser form, most often written through
   *  `from({.base = …, .layers = {…}, .surface = {…}, .effects = …})`.
   *  The parts' layers stack over whatever layers the base already
   *  carries. */
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
  /** The unit this material is painted into, raised or impressed into
   *  its surface: `SurfaceOptions::relief`, on the surface stated so far
   *  or on the stock one. */
  Material& relief(ReliefOptions options);
  /** A brushing along the unit this material is painted into:
   *  `SurfaceOptions::anisotropy`, `anisotropyRotation` and
   *  `anisotropyAlongUnit`, on the surface stated so far or on the stock
   *  one. */
  Material& brushed(BrushedOptions options);
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

  /** The recipe of a program base. A material whose base is not a
   *  program answers an empty recipe — no parameters, no slots and no
   *  body in any language — so a reader that asks is told nothing rather
   *  than crashing; `hasProgram()` is the question to ask first. */
  const Recipe& recipe() const;
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
   *  once and ignored. On a material whose base is a part that takes
   *  inputs of its own, the part's input @p name is written instead. */
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
  /** Writes every input a generated input struct states — a struct
   *  whose `inputs()` lists `{identifier, values}` pairs, as the header
   *  generated beside a procedural archive does — each as `set(name,
   *  values)` would. */
  template <class P>
    requires requires(const P& parameters) {
      { parameters.inputs().begin()->identifier };
      { std::span<const float>(parameters.inputs().begin()->values) };
    }
  Material& set(const P& parameters) {
    for (const auto& input : parameters.inputs())
      set(input.identifier, std::span<const float>(input.values));
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
    if (const std::byte* bytes = fieldBytes(name, UniformTraits<T>::floats))
      std::memcpy(&out, bytes, sizeof(T));
    return out;
  }

  /** Binds a float field to @p value: each resolve uploads what the
   *  animatable reads as now. A `motion::animatable(…)` value is the live
   *  case, a shaped `motion::bind(phase, {.envelope = …, .to = {…}})` is
   *  the same case with the arithmetic moved next to the uniform it feeds,
   *  and a plain number is a value written once per resolve. `unbind()` clears
   * it.
   *
   *  A material holds no clock, so an animatable carrying its OWN
   *  transition has nothing to run it and reads as its target. Motion
   *  into a shader arrives through an Output the host steps.
   *
   *  On a material whose base is a part that takes inputs of its own,
   *  the part's input @p name follows @p value instead. */
  Material& bind(std::string_view name, motion::Animatable<float> value);
  /** Binds a color field to @p value, uploading its four straight sRGB
   *  components without clamping. Constant and live values follow the
   *  same resolve and equality rules as scalar bindings. */
  Material& bind(std::string_view name, motion::Animatable<Color> value);
  /** Drops the binding on @p name, leaving whatever `set()` last wrote in
   *  the field. Unknown names are ignored. */
  Material& unbind(std::string_view name);
  /** Binds an array field to @p block, whose size must equal the field's
   *  float count. Each resolve uploads its last committed values; draft
   *  edits remain hidden even when another input changes. Null clears the
   *  binding. */
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
   *  material: a bound output or block, a recipe reading time, or an
   *  animated child. */
  bool isRunning() const;
  /** Whether the upload depends on where and how large the node is, or
   *  on the scale it is drawn at: a recipe reading the resolution, the
   *  world transform or the content scale, or a geometry-dependent child. */
  bool geometryDependent() const;

  bool operator==(const Material& other) const;

  /** What a renderer uploads: the program for the target and variant,
   *  and the bytes in the recipe's `layout()`. The program is null when
   *  the recipe has no compiled form for the target, which the cache has
   *  already reported. The bytes stay unchanged while this result or a
   *  copy of it is held, including across later resolves. */
  struct Resolved {
    Resolved() = default;
    std::shared_ptr<Program> program;
    std::span<const std::byte> bytes;

   private:
    friend class Material;
    Resolved(std::shared_ptr<Program> compiled,
             std::shared_ptr<const std::vector<std::byte>> storage)
        : program(std::move(compiled)),
          bytes(*storage),
          m_storage(std::move(storage)) {}
    std::shared_ptr<const std::vector<std::byte>> m_storage;
  };
  /** Samples the bindings, injects the declared frame inputs, and looks
   *  up the program. Memoised: when the sampled bytes, target and variant
   *  equal the previous call's, the previous result is returned without a
   *  cache lookup. Resolving an unchanged material and its copies is
   *  synchronized; authoring edits and writes to live inputs require
   *  external synchronization. */
  Resolved resolve(Target target, const FrameData& frame,
                   Variant variant = {}) const;

  /** The author-set bytes, in the recipe's `parameters()` layout. */
  std::span<const std::byte> bytes() const { return m_bytes; }

 private:
  struct Binding {
    std::string name;
    /** An array field carries the block; scalar and color fields carry
     *  an animatable of their value type. */
    std::variant<motion::Animatable<float>, motion::Animatable<Color>> value{
        motion::Animatable<float>(0.0f)};
    std::shared_ptr<const UniformBlock> block;
  };
  Material(std::shared_ptr<const Recipe> recipe, const void* parameters,
           size_t size, const Schema* schema);
  void write(const void* parameters, size_t size, const Schema* schema);
  /** Writes the input @p name of a base part that takes inputs; false
   *  when the base is a program or its part takes no such input. */
  bool writePartInput(std::string_view name, std::span<const float> values);
  void write(std::string_view name, ParameterType kind, const void* floats,
             size_t count);
  /** The bytes of the field @p name when it spans @p floats floats; null
   *  otherwise. */
  const std::byte* fieldBytes(std::string_view name, size_t floats) const;
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

  struct ResolveState;
  std::shared_ptr<ResolveState> m_resolve;

  /** Writes a material's slot children into one body, which reads every
   *  part of an instance: its values, its bindings and its leaves. */
  friend Material inlined(const Material& material);
};

/** EVERY SLOT HOLDING A MATERIAL, WRITTEN INTO ONE SLANG BODY. A device
 *  compiler is handed one body per material and cannot reach a child
 *  material in a slot, so the children's bodies are inlined into the
 *  parent's, each slot read becoming a call of the child's `surface`, and
 *  the children's values, live bindings and leaves carried under the
 *  slot's name as a prefix — a flipbook in a surface's base-colour slot
 *  plays on the device as it does on a page. A child with no Slang body
 *  stays in its slot. The parent's own names are unchanged, so a reader
 *  asking a field or a leaf slot of the result asks it as before; the
 *  composed definition is made once per set of definitions. A material
 *  with no material in a slot comes back as it is. */
Material inlined(const Material& material);

/** ONE CHANNEL OF A MATERIAL READ AS A GREY VALUE: what a layer's mask
 *  reads from its source, and what `channel()` picks out of a texture that
 *  packs several values into one image — a mask in red, a noise in green,
 *  a gradient in blue — wherever a material is taken. */
enum class ColorChannel : uint8_t {
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
  ColorChannel channel = ColorChannel::Alpha;
  float low = 0, high = 1;
  bool invert = false;
  bool operator==(const Mask&) const = default;
};

/** A LAYER'S MASK, OR NONE: an optional `Mask` that also takes a mask's
 *  own fields in braces, so a layer says where it applies in place —
 *  `{.mask = {depth, ColorChannel::Luminance}}`, or with its range and
 *  inversion after the channel. Empty, the layer applies everywhere. */
struct LayerMask : std::optional<Mask> {
  using std::optional<Mask>::optional;
  LayerMask() = default;
  LayerMask(std::optional<Mask> mask)  // NOLINT(google-explicit-constructor)
      : std::optional<Mask>(std::move(mask)) {}
  LayerMask(Material source, ColorChannel channel,  // NOLINT
            float low = 0, float high = 1, bool invert = false)
      : std::optional<Mask>(
            Mask{std::move(source), channel, low, high, invert}) {}
  friend bool operator==(const LayerMask& left, const LayerMask& right) {
    return static_cast<const std::optional<Mask>&>(left) ==
           static_cast<const std::optional<Mask>&>(right);
  }
};

/** How a layer meets what is beneath it. */
struct LayerOptions {
  BlendMode blend = BlendMode::Normal;
  /** The layer composites in full with its blend mode, and the result
   *  mixes back toward what is beneath by this fraction. */
  float opacity = 1;
  LayerMask mask;
  bool operator==(const LayerOptions&) const = default;
};

/** A stretch of scalar values, from `low` to `high`; either may be the
 *  larger, which reverses the stretch. */
struct ValueRange {
  float low = 0, high = 1;
  bool operator==(const ValueRange&) const = default;
};

/** How `remap()` reads its source and what it answers. */
struct RemapOptions {
  /** The stretch of the source's channel that is read. */
  ValueRange from;
  /** The values the two ends of `from` become. */
  ValueRange to;
  ColorChannel channel = ColorChannel::Luminance;
};

/** ONE CHANNEL OF @p source MAPPED FROM ONE STRETCH OF VALUES ONTO
 *  ANOTHER, as a grey material: `from.low` reads as `to.low`, `from.high`
 *  as `to.high`, straight between them and held past either end. It is
 *  what drives a metalness, a roughness or a clear coat from one height
 *  or depth map: `remap(depth, {.to = {0.12f, 0.26f}})`. */
Material remap(const Material& source, const RemapOptions& options = {});

/** One layer of the stack: a material and how it meets the ones beneath. */
struct Layer {
  Material source;
  LayerOptions options;
  bool operator==(const Layer&) const = default;
};

/** A surface channel: a number, or a material read by the channel (a
 *  texture, a noise), as a node graph connects an input. */
using Channel = std::variant<float, Material>;

/** A surface colour channel: a colour as an author types it, or a material
 *  whose colour is read there, as a colour input of a node graph takes
 *  either a swatch or a map. */
using ColorInput = std::variant<Color, Material>;

/** THE LIT RESPONSE — metallic-roughness with transmission, clearcoat,
 *  sheen, anisotropy, iridescence and a dielectric's specular strength and
 *  colour; the names are the glTF physical material's. The base colour is the
 * material's own base and stack of layers; every channel here takes a number or
 * a material. It DESCRIBES a surface and shades nothing itself: a renderer that
 * shades a set shades it under the set's lights, and a 2D painter paints the
 * colours flat, or reads the channels out as surface maps
 *  (`skia::asMap`). Refraction in 2D is Filter::glass, which refracts the
 *  layer it is attached to. */
struct SurfaceOptions {
  Channel metallic = 0.0f;
  /** How rough, 0 a mirror to 1 fully diffuse: it sets how wide the
   *  highlight spreads and how blurred a reflection is. */
  Channel roughness = 0.5f;
  /** How much of the light arriving from everywhere — the environment
   *  and ambient light — reaches the surface past its own creases. A
   *  light from one direction is not occluded by it. */
  Channel occlusion = 1.0f;
  /** A tangent-space normal map. */
  std::optional<Material> normal;
  float normalScale = 1;
  /** Whether the normal map is authored green-down (DirectX), as some
   *  tools write it. */
  bool normalDirectX = false;
  /** A RELIEF FROM THE UNIT the surface is painted into, resolved by the
   *  painter: the unit's coverage softened over the shoulder, as a height
   *  whose normals are combined with `normal`. A renderer with no unit to
   *  read — a body wearing the material with no page behind it — refuses
   *  it. */
  std::optional<ReliefOptions> relief;
  Color emission = {0, 0, 0, 1};
  float emissionStrength = 0;
  std::optional<Material> emissionMap;
  /** Alpha below a positive threshold is cut out; zero blends. In Skia,
   *  surviving colour-stack samples retain their premultiplied alpha. */
  float alphaCutoff = 0;
  /** A CLEAR COAT over the surface: a second, colourless reflection of
   *  this weight, 0 none to 1 a full coat, with its own roughness and its
   *  own normal — lacquer over wood, varnish over paint. The surface
   *  beneath is seen through it, dimmed by what the coat reflects. */
  Channel clearcoat = 0.0f;
  Channel clearcoatRoughness = 0.0f;
  /** A tangent-space normal map for the coat alone; without one the coat
   *  lies smooth over the surface's geometry whatever the surface's own
   *  normal map does. */
  std::optional<Material> clearcoatNormal;
  /** SHEEN: a soft reflection standing over the surface at the rim and
   *  toward a light behind it, as velvet, felt and dust show; black is
   *  none. The surface beneath gives up what the sheen reflects.
   *  `sheenRoughness`, 0 to 1, is how wide the sheen spreads. */
  ColorInput sheenColor = Color{0, 0, 0, 1};
  Channel sheenRoughness = 0.0f;
  /** ANISOTROPY: the highlight stretched along one direction of the
   *  surface, as brushed metal and spun discs show — 0 none to 1 a
   *  highlight drawn into a line. `anisotropyRotation` turns that direction,
   *  in degrees counter-clockwise, from the way the surface's u coordinate
   *  runs. `anisotropyDirection` is a map of the direction across the
   *  surface: red and green the direction in the surface's own frame,
   *  encoded as a normal map is, blue a scale on the strength. */
  float anisotropy = 0;
  float anisotropyRotation = 0;
  std::optional<Material> anisotropyDirection;
  /** THE BRUSHING RUNS ALONG THE UNIT the surface is painted into, resolved
   *  by the painter in place of `anisotropyDirection`: a stroke's tangent,
   *  a box's long axis, a glyph's box's long axis, turned by
   *  `anisotropyRotation`. A renderer with no unit to read refuses it. */
  bool anisotropyAlongUnit = false;
  /** IRIDESCENCE: a thin film over the surface — soap, oil, a tempered
   *  metal's oxide — whose interference tints the reflection by the angle
   *  it is seen at; 0 none to 1 a full film. `iridescenceIor` is the film's
   *  index of refraction. `iridescenceThickness` is the film's thickness in
   *  nanometres; with `iridescenceThicknessMap` stated, the map runs the
   *  thickness from `iridescenceThicknessMinimum` at black to
   *  `iridescenceThickness` at white. */
  Channel iridescence = 0.0f;
  float iridescenceIor = 1.3f;
  float iridescenceThickness = 400;
  float iridescenceThicknessMinimum = 100;
  std::optional<Material> iridescenceThicknessMap;
  /** SPECULAR: how strongly a dielectric reflects, 0 none to 1 the four per
   *  cent every dielectric reflects head on, and the colour that reflection
   *  is tinted with. A metal's reflection is its base colour and ignores
   *  both. */
  Channel specular = 1.0f;
  ColorInput specularColor = Color{1, 1, 1, 1};
  /** TRANSMISSION: light passing THROUGH the surface — glass, liquid,
   *  tinted plastic — as the glTF physical material says it. `transmission`
   *  is how much of what stands behind comes through, 0 none to 1 clear;
   *  what does not reflect passes, so a fully transmissive surface keeps
   *  only its reflection and loses its diffuse side. It is bent by `ior`,
   *  the index of refraction, across a volume `thickness` scene units deep
   *  — zero is a thin wall, which bends nothing — and tinted by the base
   *  colour. A renderer that holds a picture of the scene shows that scene
   *  through it, displaced by the bend and blurred by the roughness; one
   *  that holds only the environment shows the environment. */
  Channel transmission = 0.0f;
  float ior = 1.5f;
  Channel thickness = 0.0f;
  /** …and for instanced geometry, the instance scalar named here
   *  multiplies `thickness` on each copy, so one body of drops of many
   *  sizes is as thick as each drop is. Empty, or a name the instances do
   *  not carry, multiplies by one. */
  std::string thicknessAttribute;
  /** VOLUME: what the medium takes out of the light it carries. Light that
   *  has travelled `attenuationDistance` scene units through it has become
   *  `attenuationColor`; farther, deeper still — so a thick slab of tinted
   *  glass is darker and more coloured than a thin one. Zero distance
   *  takes nothing out. */
  Color attenuationColor = {1, 1, 1, 1};
  float attenuationDistance = 0;
  /** DISPERSION: the index of refraction varying across the colours, so
   *  each of red, green and blue is bent by its own amount and an edge
   *  seen through the glass fringes; 0 none. A strength of one spreads the
   *  index by a fortieth of its excess over one on either side of green. */
  float dispersion = 0;
  /** DISTORTION, for an authored effect rather than a material: what
   *  stands behind the surface is displaced by a two-channel DATA map —
   *  red across and green up the surface, which on a body is the way its u
   *  runs and the way a normal map's green points, half-grey still — by
   *  `distortionStrength` scene units at the surface, as heat haze and a shockwave bend what is seen
   *  through them. No light is involved in the displacement: the
   *  surface's own lit colour is laid over the displaced picture by its
   *  alpha, so a fully transparent surface is the displacement alone. An
   *  unlit surface does not carry it. Without a map, or at zero strength,
   *  nothing moves. */
  std::optional<Material> distortion;
  float distortionStrength = 0;
  float reflectionWeight = 1;
  /** No lighting: the base colour as it is. */
  bool unlit = false;
  /** HOW THE SURFACE COMPOSITES over what stands behind it, once shaded:
   *  `Normal` lays it over by its alpha; `PlusLighter` adds it — a glow, a
   *  flame, a beam, which only ever brighten and need no order among
   *  themselves; `Multiply` darkens what is behind by it; `Screen`
   *  lightens it; `Modulate` multiplies by the premultiplied colour. A 3D
   *  renderer reads it as its blend state; the 2D painter composites a
   *  node by the node's own blend mode and ignores it. */
  BlendMode blend = BlendMode::Normal;
  /** Whether the surface writes depth, so that what is drawn after it and
   *  behind it is hidden. Unstated, an opaque surface composited
   *  normally writes it and every other surface does not. A surface
   *  composited normally that writes depth IS OPAQUE: a 3D renderer draws
   *  it replacing what it covers, its alpha included, so a translucent
   *  surface states `false` here or a blend other than `Normal`. */
  std::optional<bool> depthWrite;
  /** SOFT EDGES WHERE IT MEETS THE SCENE: the distance, in scene units,
   *  over which the surface fades out as it approaches the opaque surface
   *  behind it, so a sprite crossing a floor shows no hard line. Zero does
   *  not fade. */
  float depthFade = 0;
  bool operator==(const SurfaceOptions&) const = default;
};

/** WHICH OF THE OPTIONAL LOBES a surface carries — sheen, anisotropy,
 *  iridescence, a dielectric's own specular, transmission, distortion, a
 *  clear coat. A
 * surface carries only the lobes it states: one that states no sheen samples
 * none of the sheen's maps and is shaded by a renderer that compiles no sheen
 * term for it, so a surface pays for exactly what it says. */
struct Lobes {
  bool sheen = false;
  bool anisotropy = false;
  bool iridescence = false;
  bool specular = false;
  /** Transmission with its volume and dispersion, which read the picture
   *  of the scene behind the surface. */
  bool transmission = false;
  /** A displacement of what stands behind, which reads that picture
   *  too. */
  bool distortion = false;
  /** A clear coat over the base, with its own roughness and its own
   *  normal. */
  bool clearcoat = false;
  bool operator==(const Lobes&) const = default;
};

/** Whether @p material reads the unit it is painted into: a surface
 *  stating a relief or a brushing along the unit. Only a painter that
 *  knows the unit — a page — can draw it. */
bool readsUnit(const Material& material);

/** The lobes @p options state: a sheen colour that is a map or other than
 *  black, an anisotropy other than zero, an iridescence that is a map or
 *  other than zero, a specular strength or colour that is a map or other
 *  than one and white, a transmission that is a map or other than zero, a
 *  distortion map at a strength other than zero, a clear coat that is a map
 *  or other than zero. An unlit surface states none. */
Lobes lobes(const SurfaceOptions& options);

/** A FILTER CHAIN AS THE EFFECTS STAGE of the designated form: any
 *  `Filter` converts. Empty for no effects. */
class EffectsStage {
 public:
  EffectsStage() = default;
  /** @p chain as the stage. Defined by the renderer that owns the
   *  filter. */
  // NOLINTNEXTLINE(google-explicit-constructor)
  EffectsStage(const Filter& chain);
  /** The stage as the part a material holds; null for none. */
  const std::shared_ptr<const detail::Part>& part() const { return m_part; }

 private:
  std::shared_ptr<const detail::Part> m_part;
};

/** THE DESIGNATED-INITIALISER FORM of a material: a base, layers over it,
 *  the lit response and the effects stage, each optional but the base.
 *  Written through `from()`, so one pair of braces holds it:
 *  `from({.base = hexColor(0xB8BDC4), .layers = {{noise(0.4f)}},
 *  .surface = {{.metallic = 1.f}}, .effects = Filter::shadow(black)})`. */
struct MaterialParts {
  Material base = Color{0, 0, 0, 0};
  std::vector<Layer> layers;
  std::optional<SurfaceOptions> surface;
  EffectsStage effects;
};

/** Starts a chain from any base: `from(hexColor(0x223344)).layer(…)`. */
inline Material from(Material base) { return base; }
/** A material written in the designated form, in one pair of braces. */
inline Material from(const MaterialParts& parts) { return Material(parts); }

}  // namespace sigil::material
