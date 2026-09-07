#pragma once

/** @file
 * The five orderings a cascade deals its delays in, the seeded
 * permutation behind the scattered one, and the ranking that deals them
 * in an order the caller states.
 */

#include <sigilmotion/schedule/Spread.h>

#include <cstdint>
#include <vector>

namespace sigil::motion {

/** WHERE INDEX `i` OF `count` SITS IN A CASCADE, in multiples of the
 *  per-step delay — 0,1,2… from Start, reversed from End, the two
 *  symmetric V shapes for Center and Edges, and a seeded permutation for
 *  Random. @p seed is `Spread::seed` — read only by Random, where 0 keys
 *  the ranking hash on the count alone and a nonzero value deals an
 *  independent permutation.
 *
 *  ONE BODY for every host: a text track's units, a container's staggered
 *  children, a feed's rows, a set's mounting nodes. A second spelling
 *  would let `Spread::From` mean two different orders depending on what
 *  it was attached to.
 *
 *  Writes @p out in place — it is called once per cascade build per frame
 *  and keeps its caller's allocation. */
void cascadeOrder(Spread::From from, uint32_t count, uint32_t seed,
                  std::vector<float>& out);

/** WHERE INDEX `i` OF `count` SITS when the caller has stated the order
 *  themselves — `Spread::rankBy`, one number per unit, opened smallest
 *  first.
 *
 *  DENSE RANKING: the slot is how many DISTINCT smaller numbers there
 *  are, so units sharing a number share a slot and the slot above them is
 *  the next one up. A stagger dealt by radius should open a whole ring at
 *  once and the next ring one step later, not deal the ring out in
 *  whatever order its members happened to be described in.
 *
 *  A @p keys shorter than @p count leaves the units past its end at the
 *  LAST slot, and keys past the last unit are not read; either mismatch
 *  warns once, as a cue table does. A non-finite key sorts to the end.
 *
 *  Writes @p out in place, like `cascadeOrder`. */
void cascadeRanks(const std::vector<float>& keys, uint32_t count,
                  std::vector<float>& out);

}  // namespace sigil::motion
