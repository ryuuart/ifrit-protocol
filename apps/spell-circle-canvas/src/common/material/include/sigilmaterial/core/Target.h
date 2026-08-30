#pragma once

/** @file
 * The two keys a compiled program is asked for by: the shading language a
 * renderer speaks (`Target`) and the small ordered `Variant` a renderer
 * uses to tell apart the programs it builds from one recipe.
 */

#include <compare>
#include <cstdint>
#include <string_view>

namespace sigil::material {

/** The shading language a recipe body is written in. A recipe carries one
 *  body per target it supports; a renderer asks for the one it can run. */
enum class Target : uint8_t {
  SkSL,   ///< Skia's runtime-effect language, `half4 main(float2 p)`.
  Slang,  ///< Slang, for the GPU backends that compile it to their own IR.
};

/** The target's name as it appears in messages and generated source. */
constexpr std::string_view name(Target target) {
  switch (target) {
    case Target::SkSL:
      return "SkSL";
    case Target::Slang:
      return "Slang";
  }
  return "?";
}

/** A RENDERER-OWNED KEY that distinguishes programs built from one recipe
 *  for one target — a pass that wants premultiplied output, a
 *  colour-managed build, a debug view. The value's meaning belongs to the
 *  renderer that registers the compiler; the cache only needs it ordered
 *  and comparable, which is why it is one integer with named bits laid
 *  over it by the renderer. The default variant is zero: the plain build. */
struct Variant {
  uint32_t bits = 0;

  constexpr auto operator<=>(const Variant&) const = default;
  /** This variant with @p bit also set. */
  constexpr Variant with(uint32_t bit) const { return {bits | bit}; }
  constexpr bool has(uint32_t bit) const { return (bits & bit) != 0; }
};

}  // namespace sigil::material
