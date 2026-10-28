#pragma once

/** @file
 * @ingroup tree-reconcile
 *
 * The keyed reconciler — descriptions reconciled onto a retained tree:
 * memo resolution and the describe a memo follows, the identity prune, matching
 * children by key and then by position, mounting what is new, retiring what is
 * gone, and the key index read back off the result.
 */

#include <sigilcore/follow/Follow.h>

#include <boost/container_hash/hash.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigiltree/reconcile/Host.h"
#include "sigiltree/reconcile/Stats.h"

/** THE KERNELS A RETAINED RUNTIME IS HOSTED ON, and the leaves under
 *  them. The reconciler takes descriptions built fresh every frame and
 *  reconciles them onto a tree the host retains, touching only what
 *  changed; the caching proof decides, from what a host declares and
 *  what its children answered, whether a subtree may be kept between
 *  frames. Both own the DECISION and never the thing — layout state,
 *  paint caches and running motions are the host's, reached through
 *  named operations the host implements on itself.
 *
 *  The nested namespaces are the leaves a library far from any
 *  reconciler links on its own: `noise`, `chance`, `hash` and `curve`
 *  for the arithmetic several libraries must agree on to the bit,
 *  `schedule` for where independent work runs, and `hardware` for the GPU
 *  device seam. */
namespace sigil::tree {

/** The hash of an addressable key, taking the key as the view a caller
 *  holds it in: a table hashed with this and compared with
 *  `std::equal_to<>` answers a `std::string_view` without first copying it
 *  into a `std::string`. */
struct KeyHash {
  using is_transparent = void;
  using is_avalanching = void;
  size_t operator()(std::string_view key) const noexcept {
    return boost::hash<std::string_view>{}(key);
  }
};

/** Drives a host's retained tree from descriptions.
 *
 *  `Host` implements the ReconcileHost operations on itself; `Node` is
 *  its retained node type, derived from tree::Node; `Description` is its
 *  description handle. The three are named separately so a host can hold
 *  a Reconciler as a member before it is complete — the host is only
 *  reached inside the member functions, which are instantiated where they
 *  are called.
 *
 *  One pass is one `render()` (the whole tree from the root), one
 *  `replaceContent()` (one node's single child) or one `redescribe()` (one
 *  memo's subtree); each advances `frame()`, which is what a retired node
 *  is stamped with. */
template <class Host, class NodeT, class DescriptionT>
class Reconciler {
 public:
  using Node = NodeT;
  using Description = DescriptionT;
  using Value = DescriptionValue<Description>;
  /** Addressable key → node, rebuilt by indexKeys(). */
  using KeyIndex =
      boost::unordered_flat_map<std::string, Node*, KeyHash, std::equal_to<>>;

  explicit Reconciler(Host& host) : m_host(host) {}

  /** The whole tree: mounts `root` from `description` when there is none, else
   *  patches it. Zeroes the per-pass counts first. */
  void render(std::unique_ptr<Node>& root, const Description& description) {
    static_assert(ReconcileHost<Host, Node, Description>);
    ++m_frame;
    m_stats.reset();
    if (!root) {
      root = m_host.create(description, nullptr, 0, 1);
      m_stats.mounted++;
    } else {
      patch(*root, description);
    }
  }

  /** One node's single content child: patched when it stands, mounted
   *  when it does not (retiring whatever else was there). The parent is
   *  invalidated either way. */
  void replaceContent(Node& parent, const Description& description) {
    static_assert(ReconcileHost<Host, Node, Description>);
    ++m_frame;
    if (parent.children.size() == 1) {
      patch(*parent.children.front(), description);
    } else {
      for (auto& child : parent.children)
        m_host.destroy(std::move(child), m_frame);
      m_stats.retired += (int64_t)parent.children.size();
      parent.children.clear();
      parent.children.push_back(m_host.create(description, &parent, 0, 1));
      m_stats.mounted++;
      m_host.reorder(parent, /*structureChanged=*/true);
    }
    m_host.invalidate(parent);
  }

  /** Reconciles `node` onto `inst`: resolves a memo, swaps the description
   *  in, prunes on structural equality, and calls the host's onPatched()
   *  when the description changed, before reconciling the children. */
  void patch(Node& inst, const Description& node) {
    static_assert(ReconcileHost<Host, Node, Description>);
    bool described = true;
    Description resolved = resolveMemo(inst, node, described);
    // What a memo hit skips is the DESCRIBE, so a hit is not counted as
    // one; the count and `memoHits` beside it then add up to the nodes
    // the pass visited.
    if (described) m_stats.describedNodes++;
    if (m_host.memoOf(node))
      inst.memoShell = node;
    else
      inst.memoShell = Description{};
    apply(inst, std::move(resolved));
  }

  /** DESCRIBES A MEMO NODE AGAIN from the shell it was last described
   *  through — the same properties, a fresh describe — and patches the
   *  result in as patch() would, reconciling the subtree under it. What a
   *  host calls when a value the memo's describe read has changed since
   *  (`Node::followed` answers `moved()`); a node no memo produced is left
   *  alone. */
  void redescribe(Node& inst) {
    static_assert(ReconcileHost<Host, Node, Description>);
    if (!inst.memoShell) return;
    const auto* memo = m_host.memoOf(inst.memoShell);
    if (!memo) return;
    ++m_frame;
    m_stats.describedNodes++;
    apply(inst, produce(inst, *memo));
  }

  /** DESCRIBES AGAIN EVERY MEMO IN @p following WHOSE READS MOVED, once
   *  each however many of its reads moved, and answers how many were
   *  described. A memo under another that moved is described by that one's
   *  patch when its own reads moved too, and is not described a second
   *  time; the rest are disjoint subtrees, so describing one retires no
   *  other. @p following is the host's list of nodes whose `followed` is
   *  not empty, and is stale once this answers more than zero: a describe
   *  can mount and retire nodes, so the host rebuilds the list before it
   *  asks again. */
  template <class Range>
  size_t redescribeMoved(const Range& following) {
    std::vector<Node*> moved;
    for (Node* inst : following)
      if (inst && inst->followed.moved()) moved.push_back(inst);
    if (moved.empty()) return 0;
    // A set, so the walk up from each moved node asks in constant time
    // however many moved: a frame that moves every memo of a long list is
    // the case this has to stay linear in.
    const boost::unordered_flat_set<const Node*> movedSet(moved.begin(),
                                                          moved.end());
    std::vector<Node*> topmost;
    for (Node* inst : moved) {
      bool under = false;
      for (const Node* above = inst->parent; above && !under;
           above = above->parent)
        under = movedSet.contains(above);
      if (!under) topmost.push_back(inst);
    }
    for (Node* inst : topmost) redescribe(*inst);
    return topmost.size();
  }

  /** Matches `newChildren` against the node's children by key, then by
   *  position among the unkeyed, patching survivors and creating the
   *  rest; then the host reorder()s the result and every unmatched child
   *  is retired. */
  template <class Range>
  void patchChildren(Node& inst, const Range& newChildren) {
    static_assert(ReconcileHost<Host, Node, Description>);
    // Match by key when present, else by position among unkeyed children.
    std::vector<const Node*> oldOrder;
    oldOrder.reserve(inst.children.size());
    boost::unordered_flat_map<std::string, std::unique_ptr<Node>> keyed;
    std::vector<std::unique_ptr<Node>> unkeyed;
    for (auto& child : inst.children) {
      if (child) {
        oldOrder.push_back(child.get());
        const std::string& key = matchKeyOf(*child);
        // A key names ONE child. A second sibling under the same key
        // matches nothing by key, but it is still a retained child, so
        // it joins the unkeyed ones — where it is either matched by
        // position or retired through the host below. Dropping it here
        // would destroy it without the host ever hearing of it.
        auto slot = key.empty() ? keyed.end() : keyed.try_emplace(key).first;
        if (slot != keyed.end() && !slot->second)
          slot->second = std::move(child);
        else
          unkeyed.push_back(std::move(child));
      }
    }
    inst.children.clear();

    size_t unkeyedCursor = 0;
    size_t mountOrdinal = 0;  // order among children mounted THIS patch
    bool structureChanged = false;
    for (const auto& childElement : newChildren) {
      const Description& node = m_host.descriptionOf(childElement);
      std::unique_ptr<Node> match;
      const std::string& key = m_host.keyOf(node);
      if (!key.empty()) {
        if (auto it = keyed.find(key); it != keyed.end()) {
          match = std::move(it->second);
          keyed.erase(it);
        }
      } else if (unkeyedCursor < unkeyed.size()) {
        match = std::move(unkeyed[unkeyedCursor++]);
      }
      if (match && m_host.remountRequired(*match, inst)) {
        // Unmounts; the fresh mount below picks the right mode.
        m_host.destroy(std::move(match), m_frame);
        // The destroyed child is gone, and the mount below is what replaces
        // it: say so rather than leaning on the moved-from pointer's value.
        match = nullptr;
        m_stats.retired++;
        structureChanged = true;
      }

      if (match) {
        match->parent = &inst;
        patch(*match, node);
        inst.children.push_back(std::move(match));
      } else {
        inst.children.push_back(
            m_host.create(node, &inst, mountOrdinal, newChildren.size()));
        m_stats.mounted++;
        ++mountOrdinal;
        structureChanged = true;
      }
    }

    // Mounts, unmounts, and reorders change what this node paints even
    // when every surviving child is identical — the structural prune must
    // not swallow them.
    structureChanged |= oldOrder.size() != inst.children.size();
    if (!structureChanged)
      for (size_t i = 0; i < oldOrder.size(); ++i)
        if (oldOrder[i] != inst.children[i].get()) {
          structureChanged = true;
          break;
        }
    m_host.reorder(inst, structureChanged);
    // Unmatched old children retire here, after the reorder has detached
    // them from whatever the host keeps its children attached to.
    for (size_t i = unkeyedCursor; i < unkeyed.size(); ++i) {
      m_host.destroy(std::move(unkeyed[i]), m_frame);
      m_stats.retired++;
    }
    for (auto& [key, child] : keyed) {
      m_host.destroy(std::move(child), m_frame);
      m_stats.retired++;
    }
  }

  /** The description a memo resolves to: the retained payload on a hit
   *  (properties of the same type and equal, and nothing the last describe
   *  read changed since), else the memo's produce. A non-memo resolves to
   *  itself. `inst` is the node the description lands on, before its first
   *  patch or after any number. */
  Description resolveMemo(Node& inst, const Description& node,
                          bool& described) {
    const auto* memo = m_host.memoOf(node);
    if (!memo) {
      described = true;
      inst.followed.clear();
      return node;
    }
    // A memo is a pure function of its properties and of the followable
    // values its describe read, so equal properties over unmoved reads
    // reuse the payload the previous describe produced.
    if (inst.description && inst.memoShell && !inst.followed.moved()) {
      const auto* previous = m_host.memoOf(inst.memoShell);
      if (previous && previous->properties.type() == memo->properties.type() &&
          previous->equal(previous->properties, memo->properties)) {
        m_stats.memoHits++;
        described = false;
        return inst.description;  // reuse the previously described payload
      }
    }
    described = true;
    return produce(inst, *memo);
  }

  /** The key a node is MATCHED by: its memo shell's when it has one, else
   *  its description's. */
  const std::string& matchKeyOf(const Node& node) const {
    return m_host.keyOf(node.memoShell ? node.memoShell : node.description);
  }
  /** The key a node is ADDRESSED by — the shell's when the shell carries
   *  one, else the description's. A memo shell with no key over a keyed
   *  payload answers to the payload's key. */
  const std::string& keyOf(const Node& node) const {
    const std::string& shell = matchKeyOf(node);
    return !shell.empty() ? shell : m_host.keyOf(node.description);
  }

  /** Rebuilds `byKey` over the subtree at `inst` and calls `visit` on
   *  every node in tree order — the host's own per-node indexes ride the
   *  same walk. Clears nothing: the caller decides what one rebuild
   *  covers. */
  template <class Visit>
  void indexKeys(Node& inst, KeyIndex& byKey, Visit&& visit) {
    const std::string& key = keyOf(inst);
    if (!key.empty()) byKey[key] = &inst;
    visit(inst);
    for (auto& child : inst.children)
      if (child) indexKeys(*child, byKey, visit);
  }

  const ReconcileStats& stats() const { return m_stats; }
  /** The pass counter: advanced by every render(), replaceContent() and
   *  redescribe(), and what destroy() is stamped with. */
  uint64_t frame() const { return m_frame; }

 private:
  /** The host's describe, followed: every followable value it reads lands
   *  in the node's `followed`, replacing what the previous describe read. */
  template <class MemoType>
  Description produce(Node& inst, const MemoType& memo) {
    core::FollowedReads reads;
    Description produced;
    {
      const core::FollowScope scope(reads);
      produced = m_host.produce(memo);
    }
    inst.followed = std::move(reads);
    return produced;
  }

  /** Swaps @p resolved in as the node's description and patches what
   *  changed: nothing when it is the payload already held (a memo hit), the
   *  host's onPatched() when it is not provably equal, and the children
   *  either way. */
  void apply(Node& inst, Description resolved) {
    if (resolved == inst.description)
      return;  // payload identity: untouched subtree (memo hit)

    Description prev = std::move(inst.description);
    inst.description = resolved;

    // Structural prune (the no-memo path): a fresh description that is
    // provably identical to the retained one patches nothing and — key
    // property — dirties nothing; only its children keep reconciling.
    const bool own = !prev || !m_host.equal(prev, resolved);
    if (own) {
      m_stats.patchedNodes++;
      m_host.onPatched(inst, prev ? &*prev : nullptr, *resolved);
    }

    // The children are the host's answer for THIS node rather than the
    // description's list alone: a host that keeps children of its own
    // beside the described ones — content an operator built under the
    // node — answers with both, so one pass reconciles them together.
    if (m_host.reconcilesChildren(resolved))
      patchChildren(inst, m_host.children(inst, resolved));
  }

  Host& m_host;
  ReconcileStats m_stats;
  uint64_t m_frame = 0;
};

}  // namespace sigil::tree
