#pragma once

/** @file
 * The keyed reconciler — descriptions reconciled onto a retained tree:
 * memo resolution, the identity prune, matching children by key and then
 * by position, mounting what is new, retiring what is gone, and the key
 * index read back off the result.
 */

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "sigilcore/reconcile/Env.h"
#include "sigilcore/reconcile/Host.h"
#include "sigilcore/reconcile/Stats.h"

namespace sigil::core {

/** Drives a host's retained tree from descriptions.
 *
 *  `Host` implements the ReconcileHost operations on itself; `Node` is
 *  its retained node type, derived from core::Node; `Desc` is its
 *  description handle. The three are named separately so a host can hold
 *  a Reconciler as a member before it is complete — the host is only
 *  reached inside the member functions, which are instantiated where they
 *  are called.
 *
 *  One pass is one `render()` (the whole tree from the root) or one
 *  `replaceContent()` (one node's single child); each advances `frame()`,
 *  which is what a retired node is stamped with. */
template <class Host, class NodeT, class DescT>
class Reconciler {
 public:
  using Node = NodeT;
  using Desc = DescT;
  using Value = DescValue<Desc>;
  /** Addressable key → node, rebuilt by indexKeys(). */
  using KeyIndex = std::unordered_map<std::string, Node*>;

  explicit Reconciler(Host& host) : m_host(host) {}

  /** The whole tree: mounts `root` from `desc` when there is none, else
   *  patches it. Zeroes the per-pass counts first. */
  void render(std::unique_ptr<Node>& root, const Desc& desc) {
    static_assert(ReconcileHost<Host, Node, Desc>);
    ++m_frame;
    m_stats.reset();
    if (!root) {
      root = m_host.create(desc, nullptr, 0, 1);
      m_stats.mounted++;
    } else {
      patch(*root, desc);
    }
  }

  /** One node's single content child: patched when it stands, mounted
   *  when it does not (retiring whatever else was there). The parent is
   *  invalidated either way. */
  void replaceContent(Node& parent, const Desc& desc) {
    static_assert(ReconcileHost<Host, Node, Desc>);
    ++m_frame;
    if (parent.children.size() == 1) {
      patch(*parent.children.front(), desc);
    } else {
      for (auto& child : parent.children)
        m_host.destroy(std::move(child), m_frame);
      m_stats.retired += (int64_t)parent.children.size();
      parent.children.clear();
      parent.children.push_back(m_host.create(desc, &parent, 0, 1));
      m_stats.mounted++;
      m_host.reorder(parent, /*structureChanged=*/true);
    }
    m_host.invalidate(parent);
  }

  /** Reconciles `node` onto `inst`: resolves a memo, swaps the description
   *  in, prunes on structural equality, and calls the host's onPatched()
   *  when the description changed, before reconciling the children. */
  void patch(Node& inst, const Desc& node) {
    static_assert(ReconcileHost<Host, Node, Desc>);
    m_stats.describedNodes++;
    bool described = true;
    Desc resolved = resolveMemo(inst.desc ? &inst : nullptr, node, described);
    if (m_host.memoOf(node))
      inst.memoShell = node;
    else
      inst.memoShell = Desc{};

    if (resolved == inst.desc)
      return;  // payload identity: untouched subtree (memo hit)

    Desc prev = std::move(inst.desc);
    inst.desc = resolved;

    // Structural prune (the no-memo path): a fresh description that is
    // provably identical to the retained one patches nothing and — key
    // property — dirties nothing; only its children keep reconciling.
    const bool own = !prev || !m_host.equal(prev, resolved);
    if (own) {
      m_stats.patchedNodes++;
      m_host.onPatched(inst, prev ? &*prev : nullptr, *resolved);
    }

    if (m_host.reconcilesChildren(resolved))
      patchChildren(inst, m_host.children(resolved));
  }

  /** Matches `newChildren` against the node's children by key, then by
   *  position among the unkeyed, patching survivors and creating the
   *  rest; then the host reorder()s the result and every unmatched child
   *  is retired. */
  template <class Range>
  void patchChildren(Node& inst, const Range& newChildren) {
    static_assert(ReconcileHost<Host, Node, Desc>);
    // Match by key when present, else by position among unkeyed children.
    std::vector<const Node*> oldOrder;
    oldOrder.reserve(inst.children.size());
    std::unordered_map<std::string, std::unique_ptr<Node>> keyed;
    std::vector<std::unique_ptr<Node>> unkeyed;
    for (auto& child : inst.children) {
      if (child) {
        oldOrder.push_back(child.get());
        const std::string& key = matchKeyOf(*child);
        if (!key.empty())
          keyed.emplace(key, std::move(child));
        else
          unkeyed.push_back(std::move(child));
      }
    }
    inst.children.clear();

    size_t unkeyedCursor = 0;
    size_t mountOrdinal = 0;  // order among children mounted THIS patch
    for (const auto& childElement : newChildren) {
      const Desc& node = m_host.descOf(childElement);
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
      }
    }

    // Mounts, unmounts, and reorders change what this node paints even
    // when every surviving child is identical — the structural prune must
    // not swallow them.
    bool structureChanged = oldOrder.size() != inst.children.size();
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
   *  (env equal, then props equal — both compared because both are read
   *  by the deferred describe), else the memo's produce under the
   *  environment its author had. A non-memo resolves to itself. */
  Desc resolveMemo(Node* existing, const Desc& node, bool& described) {
    const auto* memo = m_host.memoOf(node);
    if (!memo) {
      described = true;
      return node;
    }
    // A memo is a pure function of (props, ENVIRONMENT). The environment
    // is compared first and for the same reason props are: an `env::`
    // binding is read live by the deferred describe, so a memo that hit
    // on props alone would keep serving whatever environment it first
    // described under.
    if (existing && existing->memoShell) {
      const auto* previous = m_host.memoOf(existing->memoShell);
      if (previous && previous->env == memo->env &&
          previous->equal(previous->props, memo->props)) {
        m_stats.memoHits++;
        described = false;
        return existing->desc;  // reuse the previously described payload
      }
    }
    described = true;
    // …and the deferred call runs under the bindings its AUTHOR had, not
    // whatever scope this reconcile happens to sit inside (usually none).
    env::Restore restore(memo->env);
    return m_host.produce(*memo);
  }

  /** The key a node is MATCHED by: its memo shell's when it has one, else
   *  its description's. */
  const std::string& matchKeyOf(const Node& node) const {
    return m_host.keyOf(node.memoShell ? node.memoShell : node.desc);
  }
  /** The key a node is ADDRESSED by — the shell's when the shell carries
   *  one, else the description's. A memo shell with no key over a keyed
   *  payload answers to the payload's key. */
  const std::string& keyOf(const Node& node) const {
    const std::string& shell = matchKeyOf(node);
    return !shell.empty() ? shell : m_host.keyOf(node.desc);
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
    for (auto& child : inst.children) indexKeys(*child, byKey, visit);
  }

  const ReconcileStats& stats() const { return m_stats; }
  /** The pass counter: advanced by every render() and replaceContent(),
   *  and what destroy() is stamped with. */
  uint64_t frame() const { return m_frame; }

 private:
  Host& m_host;
  ReconcileStats m_stats;
  uint64_t m_frame = 0;
};

}  // namespace sigil::core
