#pragma once

/** @file
 * Stacking one material over another through a mask — the combinator
 * that makes local variation (rust over steel, dirt in the crevices) a
 * composition of materials rather than a bespoke recipe per pair.
 *
 * A MASK is an ordinary material whose output is read as a scalar: its
 * red channel, clamped to 0..1, says how much of the top material shows
 * at that point. `over()` returns a material like any other, so a stack
 * is built by applying it again, and every query — animated,
 * geometry-dependent, equality — answers over the whole stack because
 * the operands are its children.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>

#include <cstdint>
#include <memory>
#include <string_view>

namespace sigil::material {

/** How the top material's output combines with the one beneath it where
 *  the mask says. */
enum class Blend : uint8_t {
  Mix,       ///< the base moves toward the top by the mask
  Add,       ///< the top adds, scaled by the mask
  Multiply,  ///< the base moves toward base * top by the mask
};

/** The blend's name as messages and a recipe name spell it. */
std::string_view name(Blend blend);

/** The uniforms `over()`'s recipes read: how strongly the top material
 *  shows where the mask is fully on. */
struct OverParams {
  float amount = 1.0f;
};

/** The combinator recipe for @p blend, defined once. Each declares the
 *  child slots `base`, `top` and `mask`. */
const std::shared_ptr<const Recipe>& overRecipe(Blend blend);

/** @p top stacked over @p base where @p mask says, by @p blend. The
 *  three operands become the result's children, so the result compares,
 *  animates and resolves as one material. */
Material over(Material base, Material top, Material mask,
              Blend blend = Blend::Mix);

/** The material @p m stacks on: the `base` child when @p m is an
 *  `over()` result, else @p m itself. Applied until the answer is not a
 *  stack, this is the bottom of the stack. */
const Material* under(const Material& m);

/** How many materials are stacked over the bottom of @p m: zero for a
 *  material `over()` never combined. */
int stackDepth(const Material& m);

}  // namespace sigil::material
