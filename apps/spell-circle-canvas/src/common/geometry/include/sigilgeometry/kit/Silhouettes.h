#pragma once

/** @file
 * The 2D shelf of the geometry kit — the free-form answer to "everything
 * is a box".
 *
 * Every generator here is a COMPARABLE VALUE: parameters, a
 * `path(SkSize)` member and an `operator==`. That is what lets a consumer
 * that caches drawings prove two frames asked for the same silhouette and
 * keep the recording it already has. A raw `std::function` cannot be
 * compared, so a consumer handed one has to redo the work every frame;
 * these values are the way out of that.
 *
 * Give your own generator `path(SkSize)` plus `operator==` and it has the
 * same standing as these — the kit is stock, never privileged.
 *
 * Every generator is also CALLABLE over a size, so it converts to a plain
 * path-over-size function anywhere one is wanted — a band spine, a text
 * baseline, your own wrapper.
 *
 * Generators compose through wrappers: `rounded(star(5), 8)` is a
 * five-point star with consistently rounded points, which is what a
 * box-corner radius cannot do for a silhouette that has no box corners. A
 * wrapper is comparable whenever what it wraps is.
 */

#include "sigilgeometry/kit/Corners.h"
#include "sigilgeometry/kit/Curves.h"
#include "sigilgeometry/kit/Generators.h"
