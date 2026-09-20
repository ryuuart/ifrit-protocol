#pragma once

/** @file
 * @ingroup material-core
 *
 * Recipe — a material's definition: the parameter struct that is its ABI,
 * one body per shading language, the slots it samples and the
 * frame values it reads. A recipe is defined once and shared; every
 * Material is an instance of one.
 */

#include <sigilmaterial/core/Parameters.h>
#include <sigilmaterial/core/Target.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::material {

/** The per-frame values a body may read. Declaring one adds its uniform
 *  after the parameters and tells the tier queries what the material depends
 *  on. Bit-valued so a recipe's set is one integer. */
enum class FrameInput : uint8_t {
  Time = 1,            ///< `uniform float uTime`, seconds
  Resolution = 2,      ///< `uniform float2 uResolution`, the node's pixels
  ContentScale = 4,    ///< `uniform float uContentScale`
  WorldTransform = 8,  ///< `uniform float3x3 uWorld`, local to root
};

/** WHAT AN EXECUTOR MAKES A SLOT OUT OF THE LAYER. A recipe run over a
 *  rendered layer reads that layer in a slot the executor fills; a slot
 *  declared with one of these is filled from the SAME layer put through
 *  a filter first, so a body that needs a blurred copy of its input
 *  reads one tap of it instead of gathering the blur itself, per pixel,
 *  for as long as the picture is on screen. */
enum class LayerFilter : uint8_t {
  Blurred = 1,  ///< a Gaussian at the sigma the named field carries
};

/** A slot an executor fills, and what it fills it from. */
struct LayerSlot {
  std::string name;
  LayerFilter filter = LayerFilter::Blurred;
  /** The parameter whose value is the filter's amount — local pixels of
   *  Gaussian sigma for `Blurred`. */
  std::string amountField;
  bool operator==(const LayerSlot&) const = default;
};

/** A material definition. IDENTITY is the object: two recipes compiled
 *  from the same text are two recipes, each with its own programs, so a
 *  recipe is defined once — held in a `shared_ptr<const Recipe>` beside
 *  the code that owns it — and instantiated many times. Value equality
 *  (`operator==`) compares the definition text and is what a test uses;
 *  the program cache and a Material's prune comparison use identity. */
class Recipe {
 public:
  /** A recipe whose ABI is the parameter struct @p P, named @p name for
   *  messages and for the identity a cache key spells. */
  template <class P>
  static Recipe of(std::string name) {
    return Recipe(std::move(name), schema<P>());
  }
  /** A recipe whose ABI is @p parameters directly rather than a C++ struct's
   *  — the door for a definition COMPOSED while the library runs, whose
   *  fields are other recipes' fields and so belong to no one type.
   *  `of<P>()` is the ordinary way in; a caller here owes the same rule
   *  a parameter struct is checked against, that the layout is packed
   *  floats. */
  static Recipe of(std::string name, const Schema& parameters);

  /** The body for @p target: everything after the generated
   *  declarations, so for SkSL the `half4 main(float2 p) { ... }` and its
   *  helpers. Replaces an earlier body for the same target. */
  Recipe& body(Target target, std::string source);
  /** Declares a slot: a second material sampled by name, which the
   *  generated declarations expose as `uniform shader NAME` in SkSL. */
  Recipe& slot(std::string slot);
  /** DECLARES A SLOT AN EXECUTOR FILLS from the layer through @p filter,
   *  whose amount is the value of the parameter @p amountField. It is
   *  declared and sampled like any other slot; only who fills it
   *  differs, and `Material::slot` on the name still wins.
   *  @trap The amount is read ONCE, when the executor builds, so a bound
   *  one does not re-filter; a field the parameters do not declare as
   *  one float runs the filter at zero, which looks filled and is not. */
  Recipe& slot(std::string slot, LayerFilter filter, std::string amountField);
  /** Declares that the body reads @p input; its uniform is generated and
   *  its value uploaded each resolve. */
  Recipe& frame(FrameInput input);
  /** DECLARES THE BODY CHANNELWISE over the slot @p slot: each output
   *  channel depends on the same input channel and on nothing else, and
   *  @p slot holds ONE ROW of samples that is the response of red, green
   *  and blue. A renderer over an eight-bit surface may then run a
   *  256-entry per-channel table instead of a program.
   *  @trap The claim is the author's and is not checked: a body that
   *  mixes channels and declares this paints two different pictures. */
  Recipe& channelwise(std::string slot);

  const std::string& name() const { return m_name; }
  /** The parameter struct's layout — the author-set uniforms alone. */
  const Schema& parameters() const { return m_parameters; }
  /** The full upload layout: the parameters, then the declared frame inputs
   *  in enum order. A program's uniforms are set from bytes in this
   *  layout. */
  const Schema& layout() const { return m_layout; }
  /** The body for @p target, or null when none was given. */
  const std::string* body(Target target) const;
  /** WHETHER ANY BODY OF THIS RECIPE READS THE FIELD @p name. A field
   *  no body spells is a dial that does nothing, and asking the bodies
   *  is the only way to know, since a compiler's reflection reports what
   *  the generated declarations DECLARED. Spelled means as a WHOLE
   *  IDENTIFIER, so a `low` inside `lowEdge` is a different name.
   *  @trap A recipe with no body at all answers yes. */
  bool readsField(std::string_view name) const;
  /** `readsField(name)` for a field of `parameters()`, answered without
   *  looking the name up again. */
  bool readsField(const Field& field) const;
  /** WHETHER THE BODY FOR @p target SAMPLES THE CHILD SLOT @p slot. A
   *  slot is declared on the recipe and sampled by whichever bodies name
   *  it, and the two need not agree, so a target's declarations carry
   *  the slots its own body spells and no others — an unread slot still
   *  costs a program an image sampler, of which a device has few.
   *  Spelled means as a WHOLE IDENTIFIER.
   *  @trap A target with no body answers yes. */
  bool samples(Target target, std::string_view slot) const;
  bool has(Target target) const { return body(target) != nullptr; }
  /** The targets that have a body, in Target order. */
  std::vector<Target> targets() const;
  std::span<const std::string> slots() const { return m_slots; }
  /** The slots an executor fills, in declaration order. Every one of
   *  them is also in `slots()`. */
  std::span<const LayerSlot> layerSlots() const { return m_layerSlots; }
  /** The slot holding the per-channel response, or EMPTY when the
   *  recipe made no channelwise claim. */
  const std::string& channelwiseSlot() const { return m_channelwise; }
  bool reads(FrameInput input) const { return (m_frame & (uint8_t)input) != 0; }
  /** The declared frame inputs as one bit set. */
  uint8_t frameInputs() const { return m_frame; }

  /** The generated head of the program: the parameters' uniforms, the frame
   *  uniforms, then the slots this target's body samples, in
   *  @p target's syntax. */
  std::string declarations(Target target) const;
  /** declarations() followed by the body — the complete text a compiler
   *  is handed. Empty when there is no body for @p target. */
  std::string source(Target target) const;

  /** Identity, as a cache key and a message spell it. */
  struct Id {
    std::string name;
    const Recipe* recipe = nullptr;
    bool operator==(const Id&) const = default;
  };
  Id id() const { return {m_name, this}; }

  /** Definition equality: name, layout, bodies, children, frame inputs,
   *  the channelwise declaration. */
  bool operator==(const Recipe&) const = default;

 private:
  Recipe(std::string name, const Schema& parameters);
  void relayout();
  void rescan();
  bool spelled(std::string_view name) const;

  std::string m_name;
  Schema m_parameters;
  Schema m_layout;
  /** One body per target, sorted by target: `targets()` promises that
   *  order, and definition equality compares the bodies in it. */
  std::vector<std::pair<Target, std::string>> m_bodies;
  std::vector<std::string> m_slots;
  /** The subset of m_slots an executor fills from the layer. */
  std::vector<LayerSlot> m_layerSlots;
  /** channelwise()'s slot; empty means the body is not channelwise. */
  std::string m_channelwise;
  /** Per parameter field, whether a body spells it — settled once when a
   *  body is set, because a material writes every field of every
   *  instance it builds and each write asks. */
  std::vector<uint8_t> m_read;
  uint8_t m_frame = 0;
};

/** The uniform name of a frame input. */
std::string_view uniformName(FrameInput input);

}  // namespace sigil::material
