#pragma once

/** @file
 * @ingroup core-cache
 *
 * SigilCoreCache in one include: the cache policy, the settled-subtree
 * proof, the stability release, the bake seam and the rebuild guard.
 */

/** @defgroup core-cache The caching proof
 *  What a host may keep between frames. Given what a host declares about
 *  one node and what its children answered, it decides whether a subtree
 *  is provably static for the frame, whether a value memo may hold it,
 *  and whether the artefact in hand should be baked, replayed or thrown
 *  away. It owns the decision; the host owns the artefact. */

#include <sigilcore/cache/Bake.h>
#include <sigilcore/cache/Policy.h>
#include <sigilcore/cache/Rebuild.h>
#include <sigilcore/cache/Settle.h>
#include <sigilcore/cache/Volatility.h>
