#pragma once

/** @file
 * @ingroup core-reconcile
 *
 * DECLARED READS — what one node of a reconciled tree reads off another,
 * and the order that puts every reader after what it read.
 *
 * A `Read` is that declaration: a key, and which facet of that key's node
 * is being read. `orderByReads` turns a set of them into the order the
 * readers must run in. Nothing here knows what a facet MEANS, and nothing
 * here resolves a key: both belong to the host, and the ordering does not.
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

/** ORDERS READERS AFTER WHAT THEY READ. @p keys gives each reader's OWN
 *  key — empty for one nothing can read — and @p reads, in the same
 *  order, what each declares it reads; the answer is a permutation of
 *  the indices. It is STABLE, so readers that read none of each other
 *  keep the order they were given.
 *  @silent a read names a key no reader answers to: that is not an edge
 *  and orders nothing. A CYCLE is broken where it closes, leaving a
 *  slightly-off pass rather than a hang.
 */
[[nodiscard]] std::vector<uint32_t> orderByReads(
    std::span<const std::string> keys,
    std::span<const std::vector<Read>> reads);

}  // namespace sigil::core
