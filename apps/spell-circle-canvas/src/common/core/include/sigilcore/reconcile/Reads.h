#pragma once

/** @file
 * DECLARED READS — what one node of a reconciled tree reads off another,
 * and the order that puts every reader after what it read.
 *
 * A reconciled tree is settled by passes, and most of what a pass does
 * depends only on the node it is looking at. Some of it does not: a label
 * placed at a word, a rule cut to a block, a connector between two boxes,
 * a light aimed at a mesh — each is a node whose answer is a function of
 * ANOTHER node's finished answer. Until a reader says which node it reads,
 * the only order a host can run them in is the order they were written in,
 * and a reader written before what it reads is then one pass behind.
 *
 * A `Read` is that declaration: a key, and which facet of that key's node
 * is being read. `orderByReads` turns a set of them into the order the
 * readers must run in. Nothing here knows what a facet MEANS — a bounds is
 * a rect to one host and a world-space box to another — and nothing here
 * resolves a key. Both belong to the host; the ordering does not.
 */

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sigil::core {

/** WHICH FACET of a node a reader reads.
 *
 *  The facets are ordered by how much of the node's work each needs: a
 *  BOUNDS is settled by layout alone, an OUTLINE by the node's shape, a
 *  COVERAGE by what it actually drew, and UNITS by the finer structure a
 *  content node produces (a text's words and lines). A host that has no
 *  meaning for one simply never declares it. */
enum class Facet : uint8_t { Bounds, Outline, Coverage, Units };

/** ONE DECLARED READ: the node read, and the facet of it. */
struct Read {
  std::string key;
  Facet facet = Facet::Bounds;
  bool operator==(const Read&) const = default;
};

/** ORDERS READERS AFTER WHAT THEY READ.
 *
 *  @p keys gives each reader's OWN key — empty for one nothing can read —
 *  and @p reads gives, in the same order, what each one declares it reads.
 *  The answer is a permutation of the indices in which every reader comes
 *  after every reader whose key it reads.
 *
 *  IT IS STABLE, and that is the property that makes it safe to adopt:
 *  readers that read none of each other keep the order they were given, so
 *  a host whose readers are independent — which is nearly every host, on
 *  nearly every tree — runs them in exactly the order it ran them in
 *  before. Only a real edge moves anything.
 *
 *  A CYCLE IS BROKEN WHERE IT CLOSES. The readers caught in one keep their
 *  declaration order among themselves and are emitted after everything
 *  they could be put after; a cyclic declaration is therefore a
 *  slightly-off pass rather than a hang, which is the same bargain a
 *  bounded convergence loop makes. A read of a key no reader answers to is
 *  not an edge and orders nothing — the key may name an ordinary node,
 *  which is settled before any reader runs.
 */
[[nodiscard]] std::vector<uint32_t> orderByReads(
    std::span<const std::string> keys,
    std::span<const std::vector<Read>> reads);

}  // namespace sigil::core
