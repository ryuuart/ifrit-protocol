#pragma once

/** @file
 * SigilGeometry path operations — the Pathfinder panel and the Distort
 * menu, as values. Two families:
 *
 *  - BOOLEANS over Skia's pathops: unite/subtract/intersect/exclude
 *    plus simplify (self-intersection cleanup) and a stroke-expand
 *    offset() — Illustrator's Offset Path. All pure functions:
 *    SkPath in, SkPath out. In the binary four `a` is the back object
 *    and `b` the front, and a pathops failure comes back as an empty
 *    path rather than an error. Beside them the two POLYLINE corner and
 *    displacement treatments, roundCorners/chamferCorners and
 *    displaceSquare.
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

namespace sigil::geometry::path::ops {

/** Everything either shape covers. */
SkPath unite(const SkPath& a, const SkPath& b);
/** Minus Front: `a` with everything the front shape covers taken out. */
SkPath subtract(const SkPath& a, const SkPath& b);
/** Only what both shapes cover. */
SkPath intersect(const SkPath& a, const SkPath& b);
/** What one shape covers and the other does not. */
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

/** Round Corners: every sharp corner of the path replaced by an arc of
 *  @p radius. Non-positive radius returns the path unchanged, and a path
 *  the effect refuses comes back unchanged rather than empty. */
SkPath roundCorners(const SkPath& path, float radius);

/** CUT EVERY LINE-LINE CORNER of @p path with a straight bevel @p cut px
 *  along each leg — on an orthogonal route's right angles that is the
 *  45-degree face of the game-UI and PCB corner convention, which
 *  `SkCornerPathEffect` cannot spell because it only rounds. The cut
 *  clamps to half of each adjacent leg, so short legs degenerate to a
 *  diagonal rather than crossing over. Straight-through vertices are left
 *  alone; closed polyline contours chamfer the closing vertex too, so a
 *  routed loop and a `shapes::chamfered` panel agree.
 *
 *  THIS IS A POLYLINE TREATMENT. A contour containing ANY curve segment —
 *  quad, conic or cubic — is copied through completely untouched, so a
 *  chamfer over an arc, a rounded route, or anything already run through a
 *  corner effect is a silent no-op on that contour. */
SkPath chamferCorners(const SkPath& path, float cut);

/** A SQUARE WAVE across the mark: the contour walked at a fixed
 *  wavelength and displaced by +/- @p amplitude on its normal with
 *  vertical jumps between — battlements, the Greek meander key, a stepped
 *  circuit trace. The wavelength is rounded so a whole number of periods
 *  fits the contour, which is what keeps a closed mark from meeting itself
 *  mid-step. */
SkPath displaceSquare(const SkPath& src, float amplitude, float wavelength);

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

}  // namespace sigil::geometry::path::ops
