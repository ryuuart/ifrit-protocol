#pragma once

/** @file
 * Material — an instance of a recipe: the recipe, its parameter values
 * mirrored as upload bytes, the live bindings that overwrite fields at
 * resolve, the materials filling its child slots, and the instance-side
 * settings a renderer reads. Comparable by value so a scene can prune,
 * and resolvable against a frame into the program plus the bytes to
 * upload, memoised on the last inputs.
 */

#include <sigilmaterial/core/FrameData.h>
#include <sigilmaterial/core/Leaf.h>
#include <sigilmaterial/core/Params.h>
#include <sigilmaterial/core/Program.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/core/UniformBlock.h>
#include <sigilmotion/values/Animatable.h>

#include <concepts>
#include <cstddef>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::material {

/** A recipe instance.
 *
 *  VALUES are held as the bytes the shader receives, written from the
 *  params struct at construction and by `set()` per field afterwards.
 *  BINDINGS replace a field's bytes at every resolve: a
 *  `motion::Animatable<float>` fills a float field with what it reads as
 *  this frame, a `UniformBlock` fills an array field with its current
 *  values. A field bound to a LIVE animatable — a `choreograph::Output`,
 *  bare or shaped through a `bind()` chain — is live, which is what
 *  `isAnimated()` answers; one bound to a plain number is a value like
 *  any other. The resolve memo keys on the sampled values, so a frame
 *  that changed nothing reuses the last resolve. CHILDREN fill the recipe's
 * declared slots with other materials or with leaves (a `Leaf`: an image and
 * its sampling, bound by the backend rather than compiled) and ride every
 * query: a live child makes the parent live, a different child makes the parent
 * unequal.
 *
 *  EQUALITY is by value: recipe identity, bytes, bindings under
 *  SigilMotion's rule for an animatable (a live binding by the Output's
 *  IDENTITY, never the number behind it; a plain value by its number) and
 *  a block by pointer, children by value, and the instance settings. Two
 * materials describing the same thing compare equal, which is what lets a node
 * prune. */
class Material {
 public:
  /** An instance of @p recipe with the field values of @p params, whose
   *  type must be the struct the recipe was defined over. */
  template <class P>
  Material(std::shared_ptr<const Recipe> recipe, const P& params)
      : Material(std::move(recipe), &params, sizeof(P), &schema<P>()) {}
  /** An instance whose fields all start at zero. */
  explicit Material(std::shared_ptr<const Recipe> recipe);

  const Recipe& recipe() const { return *m_recipe; }
  const std::shared_ptr<const Recipe>& recipePtr() const { return m_recipe; }
  /** THE SAME INSTANCE OVER @p recipe: the values, bindings, children and
   *  settings unchanged, resolving and caching against a second
   *  definition. @p recipe must have this one's params layout — it is a
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
    write(name, Kind::FloatArray, floats.data(), floats.size());
    return *this;
  }
  /** Rewrites every field from @p params. */
  template <class P>
  Material& set(const P& params) {
    write(&params, sizeof(P), &schema<P>());
    return *this;
  }
  /** The field's current bytes, reinterpreted. The caller names the type
   *  the field was declared with. */
  template <Uniform T>
  T get(std::string_view name) const {
    T out{};
    const Field* f = m_recipe->params().find(name);
    if (f && f->floats == UniformTraits<T>::floats)
      std::memcpy(&out, m_bytes.data() + f->offset, sizeof(T));
    return out;
  }

  /** Binds a float field to @p value: each resolve uploads what the
   *  animatable reads as now. `&someOutput` is the live case, a shaped
   *  `bind(&phase).cosine().target(…)` chain is the same case with the
   *  arithmetic moved next to the uniform it feeds, and a plain number is
   *  a value written once per resolve. `unbind()` clears it.
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
  /** Fills the child slot @p name. A slot the recipe does not declare is
   *  reported once and ignored. */
  Material& child(std::string_view name, Material material);
  /** Fills the child slot @p name with a leaf the backend binds directly.
   *  A slot the recipe does not declare is reported once and ignored. */
  Material& child(std::string_view name, std::shared_ptr<const Leaf> leaf);
  /** `child(name, shared_ptr<const Leaf>)` over a leaf value. */
  template <class L>
    requires std::derived_from<L, Leaf>
  Material& child(std::string_view name, L leaf) {
    return child(name, std::shared_ptr<const Leaf>(
                           std::make_shared<const L>(std::move(leaf))));
  }
  /** The material in slot @p name, or null — including when the slot
   *  holds a leaf. */
  const Material* child(std::string_view name) const;
  /** The leaf in slot @p name, or null — including when the slot holds a
   *  material. */
  const Leaf* leaf(std::string_view name) const;

  /** What fills a slot: exactly one of the two. */
  struct Slot {
    std::shared_ptr<const Material> material;
    std::shared_ptr<const Leaf> leaf;
  };
  /** The filled slots in recipe order. */
  std::span<const std::pair<std::string, Slot>> children() const {
    return m_children;
  }

  /** The strength a renderer blends this material in at, in [0, 1]. */
  Material& amount(float a01);
  float amount() const { return m_amount; }
  /** Snaps the time this material sees to @p hz steps per second, so a
   *  material that need not move every frame resolves only when the
   *  snapped clock advances. Zero (the default) leaves time continuous. */
  Material& quantizeTime(float hz);
  float quantizeTime() const { return m_quantizeHz; }
  /** Anchors the material to the root frame rather than the node's. */
  Material& worldSpace(bool on = true);
  bool worldSpace() const { return m_worldSpace; }

  /** Whether the upload can change between frames with no edit to the
   *  material: a bound output or block, a recipe reading time or content
   *  scale, or an animated child. */
  bool isAnimated() const;
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

  /** The author-set bytes, in the recipe's `params()` layout. */
  std::span<const std::byte> bytes() const { return m_bytes; }

 private:
  struct Binding {
    std::string name;
    /** One or the other: an array field carries the block, a float field
     *  carries the animatable. The block is what tells the two apart. */
    motion::Animatable<float> value{0.0f};
    std::shared_ptr<const UniformBlock> block;
  };
  Material(std::shared_ptr<const Recipe> recipe, const void* params,
           size_t size, const Schema* schema);
  void write(const void* params, size_t size, const Schema* schema);
  void write(std::string_view name, Kind kind, const void* floats,
             size_t count);
  Binding* binding(std::string_view name);
  void place(std::string_view name, Slot slot);

  std::shared_ptr<const Recipe> m_recipe;
  std::vector<std::byte> m_bytes;
  std::vector<Binding> m_bindings;
  std::vector<std::pair<std::string, Slot>> m_children;
  float m_amount = 1.0f;
  float m_quantizeHz = 0.0f;
  bool m_worldSpace = false;

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

}  // namespace sigil::material
