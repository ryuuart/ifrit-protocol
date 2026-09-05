#pragma once

/** @file
 * The host contract — what a retained runtime supplies so the reconciler
 * can drive it: how a description is read, and what the host does when the
 * reconciler mounts a node, finds its description changed, reorders a
 * parent's children, or retires a node.
 */

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace sigil::core {

/** The pointee of a description handle. */
template <class Description>
using DescriptionValue =
    std::remove_reference_t<decltype(*std::declval<const Description&>())>;

/** What the reconciler asks of its host.
 *
 *  The reconciler owns the tree's shape — which node answers to which
 *  description, matched by key and then by position, memo resolution, the
 *  identity prune and the pass counts — and the host owns everything a
 *  node retains beyond that: layout state, paint caches, running motions.
 *  The split is a set of named operations the host implements on itself:
 *
 *  Reading a description:
 *  - `keyOf(description)` — the description's key; empty means positional.
 *  - `equal(a, b)` — are two descriptions provably identical? Equal
 *    descriptions PRUNE: the node is not patched, nothing is dirtied, and
 *    only its children keep reconciling. Anything the host cannot compare
 *    must answer false.
 *  - `reconcilesChildren(description)` — does the reconciler walk this node's
 *    children, or does the host fill them by another path (a slot)?
 *  - `children(description)` — the child descriptions, as a sized range whose
 *    elements `descriptionOf()` reads a handle off.
 *  - `memoOf(description)` — the description's Memo, or null when it is not one.
 *  - `produce(memo)` — run the memo and read the description it made.
 *
 *  Acting on a node:
 *  - `create(description, parent, ordinal, count)` — a fresh node for `description`
 *    under `parent`, patched once through the reconciler. `ordinal` is
 *    the node's order among the children created in the same patch and
 *    `count` the parent's child count, for a host that staggers mounts.
 *  - `onPatched(node, prev, next)` — the description changed: `prev` is
 *    null on the first patch. The node and whatever it retains SURVIVE an
 *    identity change; a kind that cannot carry the old state over
 *    rebuilds it here and keeps the handle.
 *  - `reorder(parent, structureChanged)` — the children now stand in
 *    `parent.children` order; `structureChanged` says a child mounted,
 *    unmounted or moved, which the prune must not swallow.
 *  - `remountRequired(match, parent)` — must this surviving node be
 *    retired and created afresh rather than patched in place? For a
 *    property fixed at mount.
 *  - `invalidate(node)` — the one upward signal: the node's content
 *    changed and every cache above it is stale.
 *  - `destroy(node, frame)` — the node left the tree in reconcile pass
 *    `frame`. The host retires it now or queues it; nothing in the
 *    reconciler holds it after this call. */
template <class H, class Node, class Description>
concept ReconcileHost = requires(
    H& host, Node& node, const Node& cnode, Node* parent, const Description& description,
    std::unique_ptr<Node> owned, size_t n, uint64_t frame) {
  { host.keyOf(description) } -> std::convertible_to<std::string_view>;
  { host.equal(description, description) } -> std::convertible_to<bool>;
  { host.reconcilesChildren(description) } -> std::convertible_to<bool>;
  { host.children(description).size() } -> std::convertible_to<size_t>;
  { host.memoOf(description) == nullptr } -> std::convertible_to<bool>;
  { host.create(description, parent, n, n) } -> std::same_as<std::unique_ptr<Node>>;
  host.onPatched(node, static_cast<const DescriptionValue<Description>*>(nullptr), *description);
  host.reorder(node, true);
  { host.remountRequired(cnode, cnode) } -> std::convertible_to<bool>;
  host.invalidate(node);
  host.destroy(std::move(owned), frame);
};

}  // namespace sigil::core
