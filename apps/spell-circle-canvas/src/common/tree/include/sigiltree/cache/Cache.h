#pragma once

/** @file
 * @ingroup tree-cache
 *
 * SigilTreeCache in one include: the cache policy, the settled-subtree
 * proof, the stability release and the bake seam.
 */

/** @defgroup tree-cache The caching proof
 *  What a host may keep between frames. Given what a host declares about
 *  one node and what its children answered, it decides whether a subtree
 *  is provably static for the frame, whether a value memo may hold it,
 *  and whether the artefact in hand should be baked, replayed or thrown
 *  away. It owns the decision; the host owns the artefact. */

#include <sigiltree/cache/Bake.h>
#include <sigiltree/cache/Policy.h>
#include <sigiltree/cache/Settle.h>
#include <sigiltree/cache/Volatility.h>
