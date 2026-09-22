#pragma once

/** @file
 * @ingroup core-reconcile
 *
 * The host contract — what a retained runtime supplies so the reconciler
 * can drive it: how a description is read, and what the host does when the
 * reconciler mounts a node, finds its description changed, reorders a
 * parent's children, or retires a node.
 */

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

namespace sigil::core {

/** The pointee of a description handle. */
template <class Description>
using DescriptionValue =
    std::remove_reference_t<decltype(*std::declval<const Description&>())>;

/** WHAT THE RECONCILER ASKS OF ITS HOST, as a set of named operations
 *  the host implements on itself. The reconciler owns the tree's SHAPE
 *  — which node answers to which description, matched by key and then
 *  by position, memo resolution, the identity prune and the pass counts
 *  — and the host owns everything a node retains beyond that: layout
 *  state, paint caches, running motions.
 *  @trap `equal` decides the PRUNE, so anything the host cannot compare
 *  must answer false; and a node SURVIVES an identity change, so a kind
 *  that cannot carry its old state over rebuilds it in `onPatched`. */
template <class H, class Node, class Description>
concept ReconcileHost =
    requires(H& host, Node& node, const Node& cnode, Node* parent,
             const Description& description, std::unique_ptr<Node> owned,
             size_t n, uint64_t frame) {
      { host.keyOf(description) } -> std::convertible_to<std::string_view>;
      { host.equal(description, description) } -> std::convertible_to<bool>;
      { host.reconcilesChildren(description) } -> std::convertible_to<bool>;
      // The children a node reconciles: the description's, and whatever
      // the host keeps under the node beside them.
      { host.children(cnode, description).size() } -> std::convertible_to<size_t>;
      { host.memoOf(description) == nullptr } -> std::convertible_to<bool>;
      {
        host.create(description, parent, n, n)
      } -> std::same_as<std::unique_ptr<Node>>;
      host.onPatched(node,
                     static_cast<const DescriptionValue<Description>*>(nullptr),
                     *description);
      host.reorder(node, true);
      { host.remountRequired(cnode, cnode) } -> std::convertible_to<bool>;
      host.invalidate(node);
      host.destroy(std::move(owned), frame);
    };

}  // namespace sigil::core
