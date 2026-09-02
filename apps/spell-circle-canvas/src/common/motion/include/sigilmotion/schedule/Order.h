#pragma once

/** @file
 * The five orderings a cascade deals its delays in, and the seeded
 * permutation behind the scattered one.
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

}  // namespace sigil::motion
