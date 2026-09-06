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
 * Distorts run over the resampled-polyline currency, so they respect
 * contours and closure and compose with blend keys, extrude sources,
 * and pathfinder results alike.
 */

#include <include/core/SkPath.h>
#include <sigilcore/compute/Chance.h>

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

/** How two pieces of an offset mark meet at a corner. */
enum class Join : uint8_t { Round, Miter, Bevel };
/** How an offset mark ends where the source has an end. */
enum class Cap : uint8_t { Butt, Round, Square };

/** THE DIALS OF ONE OFFSET.
 *
 *  `position` is the one that makes this a single operator rather than a
 *  family. It is CONTINUOUS: at 0 the offset is the single curve a
 *  distance to the LEFT of travel, at 1 the single curve the same
 *  distance to the right, and at 0.5 it is both at once — the band that
 *  straddles the source, which for a filled shape is that shape grown by
 *  the distance (or shrunk, at a negative one). Everything between is
 *  the band slid across the source, its two rails at
 *  `distance·(1 ± 2·position ∓ 1)`. A three-valued side enum would be
 *  three operators wearing one name. */
struct OffsetOptions {
  Join join = Join::Round;
  Cap cap = Cap::Round;
  /** How many distances a mitred corner's point may stand from the
   *  corner it came from before it is cut off. */
  float miterLimit = 4.0f;
  /** Where the offset sits across the source: 0 is wholly left of
   *  travel, 0.5 straddles it, 1 is wholly right. Clamped. */
  float position = 0.5f;
  /** MOVE THE SOURCE'S OWN NODES rather than form a new outline: every
   *  node travels along its corner bisector and every handle along its
   *  segment's normal, so the answer has the nodes the source had, in
   *  the same order and of the same kinds, and the two still
   *  interpolate. Which way "out" is comes from the contour's own
   *  winding, so this is the one spelling whose sign follows the
   *  drawing rather than the boolean. A needle-sharp corner's mitre is
   *  capped by `miterLimit`, blunting the corner rather than dropping
   *  the node. It is the WHOLE offset when it is set: `position` and
   *  `step`, which describe a band and a walk, say nothing about moving
   *  a node and are not read. */
  bool keepCompatible = false;
  /** The stride the sideways walk takes where a walk is used — away
   *  from `position` 0.5, where Skia's stroker answers instead. */
  float step = 4.0f;
  bool operator==(const OffsetOptions&) const = default;
};

/** OFFSET: the mark `path` becomes a distance to the side of itself.
 *
 *  ONE operator for what an outline offset, a concentric frame, a
 *  parallel rail and a bolder silhouette all are. A positive @p distance
 *  offsets to the LEFT of travel, which for a filled shape at the
 *  default `position` is outward — the library-wide sign convention,
 *  shared with `parallel` and `profile::offset`.
 *
 *  A band that STRADDLES the source encloses the source's own edge, so
 *  what is answered there is the source with the band added (a positive
 *  distance) or taken away (a negative one) — the grown or shrunk area,
 *  which is what an outline offset means. A band that lies to one side
 *  touches no interior and is answered as itself.
 *
 *  Implemented as stroke-expansion plus a boolean where the band
 *  straddles, which is robust for UI-scale geometry, and as the
 *  contour walk `parallel` elsewhere; a polygon-clipper backend can slot
 *  in later for cartography-grade needs. */
SkPath offset(const SkPath& path, float distance,
              const OffsetOptions& options = {});

/** WHICH CORNERS A ROUNDING TAKES, AND HOW HARD.
 *
 *  Every dial here is off by default, and with all of them off the
 *  rounding is Skia's own corner effect over every corner of the path —
 *  which is the common case and stays exactly as cheap as it was. */
struct CornerOptions {
  /** Only corners that turn by more than this many degrees. Zero rounds
   *  every corner there is. */
  float minTurnDeg = 0.0f;
  /** Round only the corners that turn OUTWARD, read off the contour's
   *  own winding — a reflex corner is left as it is. */
  bool outwardOnly = false;
  /** SCALE EACH RADIUS BY THE CORNER'S ANGLE so that every corner's arc
   *  stands the same distance out from the vertex it replaced: an acute
   *  corner takes a smaller radius and an obtuse one a larger. Without
   *  it a shallow corner reads as barely rounded beside a sharp one cut
   *  by the same number. */
  bool visual = false;
  bool operator==(const CornerOptions&) const = default;
};

/** Round Corners: every sharp corner of the path replaced by an arc of
 *  @p radius. Non-positive radius returns the path unchanged, and a path
 *  the effect refuses comes back unchanged rather than empty.
 *
 *  WITH ANY OPTION SET this is a POLYLINE treatment: the selection and
 *  the visual correction are read off the two straight legs meeting at a
 *  corner, so a joint where either side is a curve passes through
 *  untouched. Skia's corner effect, which the default options use, has
 *  no such limit and no such dials. */
SkPath roundCorners(const SkPath& path, float radius,
                    const CornerOptions& options = {});

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
 *  with Catmull-Rom (Illustrator's Smooth points vs Corner).
 *
 *  The displacement is drawn from ONE SEEDED STREAM, the same value
 *  every other seeded thing in this tree draws from, so a roughened
 *  outline re-rolls identically on every platform and a caller that
 *  wants an evenly spread jitter rather than an independent one says so
 *  with `source` — a low-discrepancy sequence roughens without the
 *  clumps independent draws leave. Each contour draws from its own
 *  stream, so adding one contour does not re-roll the others.
 *
 *  `seed` and `parameter` are the pair every seeded value in this tree
 *  is described by, spelled the same way here as in `Distribution`: the
 *  parameter is the sequence's own dial — a Halton base, a stratum
 *  count — and a source that has none ignores it. */
struct Roughen {
  float amplitude = 4;
  float segmentPx = 8;
  uint64_t seed = 1;
  uint32_t parameter = 0;
  bool smooth = true;
  core::chance::Source source = core::chance::Source::Pcg;

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

/** Pucker (amount < 0) & Bloat (amount > 0) — the radial power warp,
 *  ±1 full strength. EACH CONTOUR WARPS ABOUT ITS OWN centroid, so a
 *  donut's hole puckers about the hole and a word's letters each about
 *  themselves; one centroid over the whole figure would drag the outer
 *  contours across the inner ones. */
struct PuckerBloat {
  float amount = 0.5f;
  float segmentPx = 6;

  SkPath apply(const SkPath& path) const;
  SkPath operator()(const SkPath& path) const { return apply(path); }
};

/** Twirl — rotation about EACH CONTOUR'S OWN centroid, strongest at the
 *  middle and easing to zero at that contour's silhouette radius. */
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
inline PathOp offsetBy(float delta, const OffsetOptions& options = {}) {
  return
      [delta, options](const SkPath& p) { return offset(p, delta, options); };
}

}  // namespace sigil::geometry::path::ops
