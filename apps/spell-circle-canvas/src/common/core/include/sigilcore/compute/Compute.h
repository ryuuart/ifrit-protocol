#pragma once

/** @file
 * @ingroup core-compute
 *
 * Every header of the compute leaf, for a consumer that wants the whole
 * of it.
 */

/** @defgroup core-compute Compute values
 *  The arithmetic several libraries have to agree on to the bit: the
 *  seeded mixers a jitter draws from, the stream a caller holds one of
 *  them as and the distributions drawn out of it, the noise field read
 *  at a point, the folds a cache key is accumulated with, the normal
 *  form a set of runs over one axis is put in, and the shaped curve a
 *  unit position is reshaped by. The standard library is the whole of
 *  its dependencies, so a shader's CPU twin, a point cook and a text
 *  cache all reach the same bodies. */

#include <sigilcore/compute/Chance.h>
#include <sigilcore/compute/Curve.h>
#include <sigilcore/compute/Field.h>
#include <sigilcore/compute/Hash.h>
#include <sigilcore/compute/Intervals.h>
#include <sigilcore/compute/Noise.h>
