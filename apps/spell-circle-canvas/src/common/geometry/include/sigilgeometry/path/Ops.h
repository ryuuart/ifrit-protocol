#pragma once

/** @file
 * SigilGeometry path operations — the Pathfinder panel and the Distort
 * menu, as values. Two families:
 *
 *  - BOOLEANS over Skia's pathops: unite/subtract/intersect/exclude
 *    plus simplify (self-intersection cleanup) and a stroke-expand
 *    offset() — Illustrator's Offset Path. All pure functions:
 *    SkPath in, SkPath out.
 *  - DISTORTS as parameter structs: Roughen, Zigzag, PuckerBloat,
 *    Twirl. Each is a small value carrying its dials and applying on
 *    demand (operator()), so a recipe stays editable — restack, retune,
 *    re-apply; the source path is never consumed. `chain()` composes
 *    any of them with ad-hoc lambdas.
 *
 * Distorts run over the Geometry.h resampling currency, so they respect
 * contours and closure and compose with blend keys, extrude sources,
 * and pathfinder results alike.
 */

#include <include/core/SkPath.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace sigil::geometry::ops {

/** The Pathfinder four (binary, `a` is the back object, `b` the front —
 *  subtract() is Minus Front). Empty result on pathops failure. */
SkPath unite(const SkPath& a, const SkPath& b);
SkPath subtract(const SkPath& a, const SkPath& b);
SkPath intersect(const SkPath& a, const SkPath& b);
SkPath exclude(const SkPath& a, const SkPath& b);
/** N-ary union — merge a whole stack at once. */
SkPath unite(const std::vector<SkPath>& paths);

/** Resolve self-intersections and redundant winding into a clean
 *  even-odd-equivalent outline (Pathfinder's Merge, roughly). */
SkPath simplify(const SkPath& path);

/** Offset Path: grow (delta > 0) or shrink (delta < 0) a CLOSED shape
 *  by delta px, round joins. Implemented as stroke-expansion + boolean,
 *  which is robust for UI-scale geometry; a polygon-clipper backend can
 *  slot in later for cartography-grade needs. */
SkPath offset(const SkPath& path, float delta);

// ---------------------------------------------------------------------------
// Distorts. All resample-based: segmentPx bounds fidelity (smaller =
// truer curves, more points).

/** Roughen — seeded jitter along the contour normal. `smooth` rebuilds
 *  with Catmull-Rom (Illustrator's Smooth points vs Corner). */
struct Roughen {
  float amplitude = 4;
  float segmentPx = 8;
  uint32_t seed = 1;
  bool smooth = true;

  SkPath apply(const SkPath& path) const;
  SkPath operator()(const SkPath& path) const { return apply(path); }
};

/** Zig Zag — a regular wave along the contour; `smooth` = sine ridges,
 *  otherwise hard saw teeth. `wavelengthPx` is crest to crest. */
struct Zigzag {
  float amplitude = 6;
  float wavelengthPx = 24;
  bool smooth = false;

  SkPath apply(const SkPath& path) const;
  SkPath operator()(const SkPath& path) const { return apply(path); }
};

/** Pucker (amount < 0) & Bloat (amount > 0) — the radial power warp
 *  about the shape's centroid, ±1 full strength. */
struct PuckerBloat {
  float amount = 0.5f;
  float segmentPx = 6;

  SkPath apply(const SkPath& path) const;
  SkPath operator()(const SkPath& path) const { return apply(path); }
};

/** Twirl — rotation about the centroid, strongest at the middle and
 *  easing to zero at the silhouette radius. */
struct Twirl {
  float angleDeg = 60;
  float segmentPx = 6;

  SkPath apply(const SkPath& path) const;
  SkPath operator()(const SkPath& path) const { return apply(path); }
};

/** A step in a non-destructive recipe; every distort above converts. */
using PathOp = std::function<SkPath(const SkPath&)>;

/** Left-to-right composition: chain({offsetBy(4), Roughen{...}}). */
PathOp chain(std::vector<PathOp> steps);

/** offset() as a recipe step. */
inline PathOp offsetBy(float delta) {
  return [delta](const SkPath& p) { return offset(p, delta); };
}

}  // namespace sigil::geometry::ops
