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
 *
 * TWO KINDS OF TARGET READ A STACK, and only one of them can reach the
 * operands. A target whose child slot is a SHADER — SkSL's is — samples
 * each operand's own program, so one body over three slots is the whole
 * story. A target handed exactly ONE body per material cannot reach a
 * child material at all; for it a stack is COMPOSED: `over()` builds a
 * recipe out of its operands' own definitions, whose parameters are
 * theirs under a prefix per operand, whose sampled slots are theirs, and
 * whose body inlines all three of their bodies and mixes what they
 * return. The composition costs one recipe and one program per distinct
 * triple of definitions, and buys nothing for a target that samples its
 * operands, so it is built only where a compiler that needs it is
 * installed. A stack composed and a stack not composed are the same
 * material otherwise: the same operands as children, the same walk down,
 * the same recipe NAME — which is what says a material is a stack, since
 * a composed one carries a recipe built for its own operands.
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
struct OverParams {
  float amount = 1.0f;
};

/** The combinator recipe for @p blend, defined once — the one a stack
 *  takes when its operands are not composed. Each declares the child
 *  slots `base`, `top` and `mask`. */
const std::shared_ptr<const Recipe>& overRecipe(Blend blend);

/** The name every recipe of a stack by @p blend carries, composed or
 *  not: what says a material is a stack. */
std::string stackName(Blend blend);

/** @p top stacked over @p base where @p mask says, by @p blend. The
 *  three operands become the result's children, so the result compares,
 *  animates and resolves as one material.
 *
 *  Where the stack is COMPOSED the operands' parameter values and their
 *  sampled slots are copied into the result at the moment of the call,
 *  so a later edit to one of them is not seen and a live binding on one
 *  of them does not reach the composed body — the operand still rides
 *  every query as a child, so the stack still reports itself animated. */
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
