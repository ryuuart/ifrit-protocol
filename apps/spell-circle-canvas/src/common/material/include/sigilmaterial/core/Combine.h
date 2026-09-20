#pragma once

/** @file
 * @ingroup material-core
 *
 * Stacking one material over another through a mask — the combinator
 * that makes local variation (rust over steel, dirt in the crevices) a
 * composition of materials rather than a bespoke recipe per pair. A
 * MASK is an ordinary material read as a scalar: its red channel,
 * clamped to 0..1, is how much of the top shows. A stack is a material
 * like any other, so every query answers over the whole of it.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>

#include <cstdint>
#include <memory>
#include <string>
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
struct OverParameters {
  float amount = 1.0f;
};

/** The combinator recipe for @p blend, defined once — the one a stack
 *  takes when its operands are not composed. Each declares the child
 *  slots `base`, `top` and `mask`. */
const std::shared_ptr<const Recipe>& overRecipe(Blend blend);

/** The name every recipe of a stack by @p blend carries, composed or
 *  not: what says a material is a stack. */
std::string stackName(Blend blend);

/** @p top stacked over @p base where @p mask says, by @p blend, at
 *  @p amount in 0..1 — how strongly the top shows where the mask is
 *  fully on. The three operands become the result's children, so the
 *  result compares, animates and resolves as one material.
 *  @trap Where a target composes the stack, the operands' values and
 *  sampled slots are copied AT THE CALL, so a later edit to one of them
 *  is not seen and a live binding on one does not reach the body. */
Material over(Material base, Material top, Material mask,
              Blend blend = Blend::Mix, float amount = 1.0f);

/** The material @p m stacks on: the `base` child when @p m is an
 *  `over()` result, else @p m itself. Applied until the answer is not a
 *  stack, this is the bottom of the stack. */
const Material* under(const Material& m);

/** How many materials are stacked over the bottom of @p m: zero for a
 *  material `over()` never combined. */
int stackDepth(const Material& m);

}  // namespace sigil::material
