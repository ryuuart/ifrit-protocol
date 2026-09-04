#pragma once

/** @file
 * Recipe — a material's definition: the params struct that is its ABI,
 * one body per shading language, the child slots it samples and the
 * frame values it reads. A recipe is defined once and shared; every
 * Material is an instance of one.
 */

#include <sigilmaterial/core/Params.h>
#include <sigilmaterial/core/Target.h>

#include <boost/container/map.hpp>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::material {

/** The per-frame values a body may read. Declaring one adds its uniform
 *  after the params and tells the tier queries what the material depends
 *  on. Bit-valued so a recipe's set is one integer. */
enum class FrameInput : uint8_t {
  Time = 1,            ///< `uniform float uTime`, seconds
  Resolution = 2,      ///< `uniform float2 uResolution`, the node's pixels
  ContentScale = 4,    ///< `uniform float uContentScale`
  WorldTransform = 8,  ///< `uniform float3x3 uWorld`, local to root
};

/** A material definition. IDENTITY is the object: two recipes compiled
 *  from the same text are two recipes, each with its own programs, so a
 *  recipe is defined once — held in a `shared_ptr<const Recipe>` beside
 *  the code that owns it — and instantiated many times. Value equality
 *  (`operator==`) compares the definition text and is what a test uses;
 *  the program cache and a Material's prune comparison use identity. */
class Recipe {
 public:
  /** A recipe whose ABI is the params struct @p P, named @p name for
   *  messages and for the identity a cache key spells. */
  template <class P>
  static Recipe of(std::string name) {
    return Recipe(std::move(name), schema<P>());
  }
  /** A recipe whose ABI is @p params directly rather than a C++ struct's
   *  — the door for a definition COMPOSED while the library runs, whose
   *  fields are other recipes' fields and so belong to no one type.
   *  `of<P>()` is the ordinary way in; a caller here owes the same rule
   *  a params struct is checked against, that the layout is packed
   *  floats. */
  static Recipe of(std::string name, const Schema& params);

  /** The body for @p target: everything after the generated
   *  declarations, so for SkSL the `half4 main(float2 p) { ... }` and its
   *  helpers. Replaces an earlier body for the same target. */
  Recipe& body(Target target, std::string source);
  /** Declares a child slot: a second material sampled by name, which the
   *  generated declarations expose as `uniform shader NAME` in SkSL. */
  Recipe& child(std::string slot);
  /** Declares that the body reads @p input; its uniform is generated and
   *  its value uploaded each resolve. */
  Recipe& frame(FrameInput input);

  const std::string& name() const { return m_name; }
  /** The params struct's layout — the author-set uniforms alone. */
  const Schema& params() const { return m_params; }
  /** The full upload layout: the params, then the declared frame inputs
   *  in enum order. A program's uniforms are set from bytes in this
   *  layout. */
  const Schema& layout() const { return m_layout; }
  /** The body for @p target, or null when none was given. */
  const std::string* body(Target target) const;
  /** WHETHER ANY BODY OF THIS RECIPE READS THE FIELD @p name.
   *
   *  A field no body spells is a dial that does nothing: the bytes are
   *  uploaded and the picture does not change, which at a call site is
   *  indistinguishable from a wrong value. Asking the bodies is the only
   *  way to know — a shading compiler's reflection reports what the
   *  source DECLARED, and the declarations are generated from the params
   *  whether the body reads them or not.
   *
   *  Spelled means as a WHOLE IDENTIFIER, so a `low` inside `lowEdge` is
   *  a different name; a recipe with no body at all answers yes, having
   *  nothing to say. */
  bool readsField(std::string_view name) const;
  bool has(Target target) const { return body(target) != nullptr; }
  /** The targets that have a body, in Target order. */
  std::vector<Target> targets() const;
  std::span<const std::string> children() const { return m_children; }
  bool reads(FrameInput input) const { return (m_frame & (uint8_t)input) != 0; }
  /** The declared frame inputs as one bit set. */
  uint8_t frameInputs() const { return m_frame; }

  /** The generated head of the program: the params' uniforms, the frame
   *  uniforms, then the child slots, in @p target's syntax. */
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

  /** Definition equality: name, layout, bodies, children, frame inputs. */
  bool operator==(const Recipe&) const = default;

 private:
  Recipe(std::string name, const Schema& params);
  void relayout();

  std::string m_name;
  Schema m_params;
  Schema m_layout;
  boost::container::map<Target, std::string> m_bodies;
  std::vector<std::string> m_children;
  uint8_t m_frame = 0;
};

/** The uniform name of a frame input. */
std::string_view uniformName(FrameInput input);

}  // namespace sigil::material
