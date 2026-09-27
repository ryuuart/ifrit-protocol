#pragma once

/** @file
 * @ingroup material-advanced
 *
 * Stacking one material over another through a mask — the combinator
 * that makes local variation (rust over steel, dirt in the crevices) a
 * composition of materials rather than a bespoke recipe per pair. A
 * MASK is an ordinary material read as a scalar: its red channel,
 * clamped to 0..1, is how much of the top shows. A stack is a material
 * like any other, so every query answers over the whole of it.
 */

#include <sigilmaterial/core/BlendMode.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/advanced/Recipe.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace sigil::material {

/** The uniforms `over()`'s recipes read: how strongly the top material
 *  shows where the mask is fully on. */
struct OverParameters {
  float amount = 1.0f;
};

/** The combinator recipe for @p blend, defined once — the one a stack
 *  takes when its operands are not composed. Each declares the child
 *  slots `base`, `top` and `mask`. */
const std::shared_ptr<const Recipe>& overRecipe(BlendMode blend);

/** The name every recipe of a stack by @p blend carries, composed or
 *  not: what says a material is a stack. */
std::string stackName(BlendMode blend);

/** @p top stacked over @p base where @p mask says, by @p blend, at
 *  @p amount in 0..1 — how strongly the top shows where the mask is
 *  fully on. Three modes stack: `Normal` moves the base toward the top
 *  by the mask, `PlusLighter` adds the top scaled by the mask, and
 *  `Multiply` moves the base toward base × top by the mask. The three
 *  operands become the result's children, so the result compares,
 *  animates and resolves as one material.
 *  @silent any other mode stacks as `Normal` (reported once).
 *  @trap Where a target composes the stack, the operands' values and
 *  sampled slots are copied AT THE CALL, so a later edit to one of them
 *  is not seen and a live binding on one does not reach the body. */
Material over(Material base, Material top, Material mask,
              BlendMode blend = BlendMode::Normal, float amount = 1.0f);

/** The material @p material stacks on: the `base` child when @p material is an
 *  `over()` result, else @p material itself. Applied until the answer is not a
 *  stack, this is the bottom of the stack. */
const Material* under(const Material& material);

/** How many materials are stacked over the bottom of @p material: zero for a
 *  material `over()` never combined. */
int stackDepth(const Material& material);

}  // namespace sigil::material
