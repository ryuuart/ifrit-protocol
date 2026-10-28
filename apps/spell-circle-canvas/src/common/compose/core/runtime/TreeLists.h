#pragma once

/** @file
 * Internal to the kernel — the flat lists rebuilt with the key index each
 * render: the nodes that borrow another's geometry, flow around it, apply
 * adding operators, anchor marks on a path-laid run, thread into another
 * frame or follow a live value their memo read, and what the rebuild found
 * about the tree as a whole.
 */

#include <vector>

namespace sigil::compose::detail {

struct Instance;

/** THE TREE'S FLAT LISTS: one part of the composer's retained state, read
 *  off it by each field's own name. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct TreeLists {
  // THE BORROW LISTS, rebuilt with the key index each render: the nodes
  // whose own answer is a function of another keyed node's resolved
  // geometry, flat and in tree order, so the derive pass iterates them
  // instead of recursing the whole tree and a tree that borrows nothing
  // pays nothing.
  std::vector<Instance*> borrowInstances;
  std::vector<Instance*> flowInstances;  // contentFlowAround() text nodes
  // Nodes applying ADDING operators, in tree order — the list the
  // additions pass walks once layout has settled — and the nodes holding
  // what a pass before it attached, so an owner nothing attaches to any
  // more is emptied rather than left with last frame's elements.
  std::vector<Instance*> addingInstances;
  std::vector<Instance*> additionOwners;
  // Text nodes carrying textAttach() on a path-laid run. Their curve resolves
  // against the node's FINAL box, which measurement never sees, so their
  // marks resolve in a post-layout pass over this flat list instead of
  // inside measure like a flow run's.
  std::vector<Instance*> pathMarkInstances;
  // Text nodes that thread INTO another frame. The chain is walked in the
  // derive pass, because frame b's fill begins where frame a's RESULT ended
  // and the phase order has no edge for that.
  std::vector<Instance*> threadedInstances;
  // …and the frames THEY thread into, kept from the last walk so a frame
  // that stops being a target is unbounded again the moment it does.
  std::vector<Instance*> threadTargets;
  // Memo nodes whose last describe read a live value, in tree order: the
  // list the settle asks whether anything they read has changed since, so
  // a tree whose memos follow nothing asks nothing.
  std::vector<Instance*> followingInstances;
  // Nodes a followed placement query asked about, whose placement the
  // settle compares after each layout.
  std::vector<Instance*> placementInstances;
  bool hasDerived() const {
    return !borrowInstances.empty() || !flowInstances.empty() ||
           !threadedInstances.empty();
  }
  bool hasAdding() const {
    return !addingInstances.empty() || !additionOwners.empty();
  }
  // Recomputed with the key index, so unmounting the last arranging or
  // pinned node clears them rather than latching them on forever.
  bool hasCustomLayout = false;
  bool hasCenterPins = false;  // any centerAt() in the tree
  /** Whether some node of the tree is leaving, as the last key index
   *  rebuild found it. */
  bool hasLeaving = false;
};

}  // namespace sigil::compose::detail
