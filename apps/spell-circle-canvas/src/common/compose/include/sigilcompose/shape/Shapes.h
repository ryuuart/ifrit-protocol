#pragma once

/** @file
 * SigilCompose shape kit — the free-form answer to "everything is a box".
 *
 * An extension over the kernel's shape() seam. Every generator here is a
 * COMPARABLE VALUE — parameters, a `path(SkSize)` member and an
 * `operator==` — so any element can *be* a star, blob, polygon or
 * squircle: fill, clip and every outline-following decoration (PathFormat,
 * ContourWalk) trace the shape, hitTest() honours it, and the node PRUNES
 * exactly like an unshapen one.
 *
 * That comparability is the whole point of the value form. A raw callable
 * handed to `shape()` is the escape hatch: it can never compare equal, so
 * its node is re-patched on every describe and its subtree re-records.
 * Give your own generators `path(SkSize)` plus `operator==` and they
 * become values with the same standing as these.
 *
 * Every generator is also CALLABLE over a size, so it converts to an
 * `OutlineFn` anywhere a raw path-over-size function is wanted — a band
 * spine, a TextPath baseline, your own wrapper.
 *
 * Generators compose through wrappers: `rounded(star(5), 8)` is a
 * five-point star with consistently rounded points, which is what
 * corners() cannot do for a silhouette that has no box corners. A wrapper
 * is comparable whenever what it wraps is.
 *
 * `edges()` runs the other way: it extracts the sub-contours of a resolved
 * outline that face a given box edge, so a per-edge treatment is
 * composition — `onEdges(Edge::Top, PathFormat…)` — rather than a new
 * primitive type.
 */

#include "sigilcompose/shape/Corners.h"
#include "sigilcompose/shape/Curves.h"
#include "sigilcompose/shape/Edges.h"
#include "sigilcompose/shape/Generators.h"
