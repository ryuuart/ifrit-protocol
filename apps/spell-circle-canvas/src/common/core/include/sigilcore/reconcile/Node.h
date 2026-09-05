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
 *  type; `Description` is its description handle — a pointer-like value that is
 *  null before the first patch, dereferences to the description, and
 *  compares by identity, which is how a memo hit is recognised. */
template <class Derived, class Description>
struct Node {
  Derived* parent = nullptr;
  Description description;       ///< the resolved (post-memo) description
  Description memoShell;  ///< the memo element this node was described through
  std::vector<std::unique_ptr<Derived>> children;

 private:
  // Constructible only through the type named as `Derived`. A node that
  // derives from this while naming another host's node type would hold
  // that host's children and hand them to this host's phases, and the
  // mismatch is invisible at the derivation.
  Node() = default;
  friend Derived;
};

}  // namespace sigil::core
