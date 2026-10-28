#pragma once

/** @file
 * @ingroup tree-reconcile
 *
 * The tree skeleton — the five fields every retained node carries for the
 * reconciler: its parent, the description it was resolved from, the memo
 * shell that produced that description when there was one, what that
 * memo's describe read that can change after it, and its children in tree
 * order. A host's node type derives from this and adds
 * whatever its phases retain.
 */

#include <sigilcore/follow/Follow.h>

#include <memory>
#include <vector>

namespace sigil::tree {

/** The reconciler's view of a retained node. `Derived` is the host's node
 *  type; `Description` is its description handle — a pointer-like value that is
 *  null before the first patch, dereferences to the description, and
 *  compares by identity, which is how a memo hit is recognised. */
template <class Derived, class Description>
struct Node {
  Derived* parent = nullptr;
  Description description;  ///< the resolved (post-memo) description
  Description memoShell;  ///< the memo element this node was described through
  /** What the memo's last describe read that can change after it, at the
   *  revisions it read; empty for a node no memo produced, and for a memo
   *  that read nothing followable. */
  core::FollowedReads followed;
  std::vector<std::unique_ptr<Derived>> children;

 private:
  // Constructible only through the type named as `Derived`. A node that
  // derives from this while naming another host's node type would hold
  // that host's children and hand them to this host's phases, and the
  // mismatch is invisible at the derivation.
  Node() = default;
  friend Derived;
};

}  // namespace sigil::tree
