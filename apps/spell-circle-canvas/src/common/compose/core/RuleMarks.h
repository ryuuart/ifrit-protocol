#pragma once

/** @file
 * Internal to the kernel — THE MARKS THE MATCHED RULES STATE, laid onto
 * the description a node is painted from: a rule's strokes, backgrounds,
 * overlays and foregrounds, and the effects of the fill and the ink it
 * states, under the node's own in every slot.
 */

#include <memory>
#include <span>

#include "ComposeInternal.h"

namespace sigil::compose::detail {

/** @p node DRESSED WITH THE MARKS the rules of @p matched state, weakest
 *  first: in each slot the rules' marks in the order the rules match, then
 *  the node's own, so a node's own `stroke()` paints over a class's
 *  keyline as a second call on the node itself would. Null where no rule
 *  states a mark, which leaves the node painted from its description. */
[[nodiscard]] std::shared_ptr<const ElementNode> dressedWithRules(
    const ElementNode& node, std::span<const Rule* const> matched);

}  // namespace sigil::compose::detail
