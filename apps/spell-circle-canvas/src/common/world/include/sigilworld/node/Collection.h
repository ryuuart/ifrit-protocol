#pragma once

/** @file
 * @ingroup world-node
 * A SET OF NODES, described rather than enumerated: the collections a
 * node joins by name, and a value naming which of them a pass draws or a
 * light reaches.
 */

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::world {

/** WHAT A COLLECTION IS ASKED ABOUT: one node, as the terms read it — its
 *  key, the collections it joined, and the keys from the root down to its
 *  parent. */
struct Member {
  std::string_view key;
  std::span<const std::string> collections;
  std::span<const std::string> ancestors;
};

/** A SET OF NODES, as a comparable value, the way a USD collection names
 *  one: what it includes, and what it excludes from that.
 *
 *  Each entry is a TERM. A plain name is the members of that collection —
 *  every node that called `collection(name)`. A term beginning with `/` is
 *  a node and everything under it: `/rig` is the node keyed `rig` and its
 *  whole subtree, which is a USD path-based include read against keys.
 *
 *  An empty `include` includes every node, so a default-constructed
 *  collection is the whole scene; `exclude` then takes away whatever any
 *  of its terms names. Membership does not flow down the tree: a group
 *  that joins a collection is a member of it, and its children are not
 *  unless they join too or a `/key` term names their subtree. */
struct Collection {
  /** Terms a node must answer to one of; empty includes every node. */
  std::vector<std::string> include;
  /** Terms that take a node out, whatever included it. */
  std::vector<std::string> exclude;

  /** Is @p member in this set? */
  [[nodiscard]] bool contains(const Member& member) const;
  /** Whether this names every node: nothing included, nothing excluded. */
  [[nodiscard]] bool everything() const {
    return include.empty() && exclude.empty();
  }

  bool operator==(const Collection&) const = default;
};

/** Does @p member answer to @p term — the members of a collection by
 *  name, or the subtree a `/key` term names? */
[[nodiscard]] bool answers(const Member& member, std::string_view term);

}  // namespace sigil::world
