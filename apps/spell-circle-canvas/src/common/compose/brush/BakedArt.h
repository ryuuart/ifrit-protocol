#pragma once
/** @file
 * Whether a baked picture is still the one its art made — the question
 * every brush that bakes an element asks before it stamps the bake.
 */

#include <memory>

namespace sigil::compose::detail {
struct ElementNode;
}

namespace sigil::compose::brush {

/** Is @p held the node @p now, by IDENTITY rather than by address? A cache
 *  that remembers a bare pointer cannot tell a destroyed art node from a
 *  new one handed the same address by the allocator, and would stamp the
 *  old bake for the new art. A weak handle expires with the node it
 *  names, so the two can never be confused. Two empty handles are the
 *  same nothing, which is what "no art here" means. */
inline bool bakedFromNode(
    const std::weak_ptr<sigil::compose::detail::ElementNode>& held,
    const std::shared_ptr<sigil::compose::detail::ElementNode>& now) {
  return !held.owner_before(now) && !now.owner_before(held);
}

}  // namespace sigil::compose::brush
