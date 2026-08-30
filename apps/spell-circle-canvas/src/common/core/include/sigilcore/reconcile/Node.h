#pragma once

/** @file
 * The tree skeleton — the four fields every retained node carries for the
 * reconciler: its parent, the description it was resolved from, the memo
 * shell that produced that description when there was one, and its
 * children in tree order. A host's node type derives from this and adds
 * whatever its phases retain.
 */

#include <memory>
#include <vector>

namespace sigil::core {

/** The reconciler's view of a retained node. `Derived` is the host's node
 *  type; `Desc` is its description handle — a pointer-like value that is
 *  null before the first patch, dereferences to the description, and
 *  compares by identity, which is how a memo hit is recognised. */
template <class Derived, class Desc>
struct Node {
  Derived* parent = nullptr;
  Desc desc;       ///< the resolved (post-memo) description
  Desc memoShell;  ///< the memo element this node was described through
  std::vector<std::unique_ptr<Derived>> children;
};

}  // namespace sigil::core
