#pragma once
/** @file
 * Polylines — a path flattened to points, resampled to a count, aligned
 * to another for interpolation. Everything that treats an outline as a
 * list of vertices starts here: shape blending, roughening, corner
 * walks, scatter along an edge.
 *
 * Only `flatten` reads a Skia path, and only `toPath` and `smoothThrough`
 * write one; the rest is plain vectors so it composes with any source of
 * points.
 */
#include <include/core/SkPath.h>
#include <include/core/SkRect.h>

#include <functional>
#include <glm/vec2.hpp>
#include <span>
#include <vector>

namespace sigil::geometry::path {

/** One flattened contour: points in order, optionally closed. */
struct Polyline {
  std::vector<glm::vec2> points;
  bool closed = false;
  /** ONE SCALAR RIDING EACH VERTEX: pressure along a brush centreline, a
   *  width along a rail, a weight along a coastline — the same word a
   *  point cloud's attributes use, one value per point. Either empty
   *  (the polyline carries nothing) or exactly as long as `points`.
   *  Everything here that moves the points interpolates the lane with
   *  them, so a caller reading a resampled curve never re-derives what
   *  the value there was. */
  std::vector<float> lane;

  float length() const;
  /** Length-weighted centroid of the edges. */
  glm::vec2 centroid() const;
  /** Signed area (positive = clockwise in Skia's y-down space). Open
   *  polylines are treated as if closed. */
  float signedArea() const;
  /** The rect every point fits in; no points at all is an empty rect. */
  SkRect bounds() const;
  /** Whether `point` is inside, by the even-odd ray rule. The polyline
   *  is read as a RING — its last point joins its first whether or not
   *  `closed` is set — because containment is a question about an area,
   *  and an open chain bounds one exactly as a closed one does. Fewer
   *  than three points bound nothing and contain nothing. */
  bool contains(glm::vec2 point) const;
  void reverse();
};

/** The rect every point of every polyline fits in. */
SkRect bounds(std::span<const Polyline> lines);

/** Whether `point` is inside the EVEN-ODD union of the rings: inside an
 *  odd number of them. The first ring an outer boundary and the rest
 *  holes, or a set of islands — the rule is the same one, and it is the
 *  rule a filled path with `SkPathFillType::kEvenOdd` is drawn by, so a
 *  point tested here and a pixel painted there agree. */
bool containsEvenOdd(std::span<const Polyline> rings, glm::vec2 point);

/** Where the segment from `from` to `to` crosses the edges of `line`,
 *  NEAREST `from` FIRST. A closed polyline's seam edge counts. An edge
 *  the segment runs along contributes nothing: two parallel lines have
 *  no one crossing, and answering either end of the overlap would be a
 *  choice rather than a measurement. */
std::vector<glm::vec2> edgeCrossings(const Polyline& line, glm::vec2 from,
                                     glm::vec2 to);

/** Every contour of `path` as a polyline, curves subdivided until they
 *  deviate from the chord by at most `tolerance` pixels. */
std::vector<Polyline> flatten(const SkPath& path, float tolerance = 0.25f);

/** `count` points spaced evenly by arc length along a parametric curve
 *  `f: [t0, t1] → point`. The polyline the generators in every catalog
 *  are built from. */
Polyline sample(const std::function<glm::vec2(float)>& f, float t0, float t1,
                int count, bool closed);

/** A polyline resampled to a fixed count, remembering the length it was
 *  taken from. Two `Sampled` of one count interpolate point-for-point. */
struct Sampled {
  std::vector<glm::vec2> points;
  bool closed = false;
  float sourceLength = 0;

  /** Plain average of the points. */
  glm::vec2 centroid() const;
};

/** `count` points spaced evenly by arc length along `contour`. A closed
 *  contour spreads them around the whole loop with no repeat of the seam;
 *  an open one puts the first and last points on its ends. */
Sampled resample(const Polyline& contour, int count);
/** Every contour of `path`, flattened at `tolerance` and resampled to the
 *  same `count` — which is what makes contours of two different paths
 *  pairable. */
std::vector<Sampled> resample(const SkPath& path, int count,
                              float tolerance = 0.25f);

/** How to rotate and possibly reverse one sampled contour so it pairs
 *  with another point-for-point with the least total travel. */
struct Alignment {
  int offset = 0;
  bool reversed = false;
};
/** The rotation and direction that minimise the summed squared distance
 *  between paired points, searched over every rotation and both
 *  directions. Contours of unequal point count get the identity
 *  alignment: there is no pairing to score. */
Alignment bestAlignment(const Sampled& a, const Sampled& b);
/** `b` with its points rolled and possibly reversed. The count and the
 *  source length are untouched — only which point is first changes. */
Sampled applyAlignment(const Sampled& b, const Alignment& alignment);

/** Back to a path: straight segments, or a Catmull-Rom cubic through
 *  the points when `smooth`. */
SkPath toPath(const Sampled& samples, bool smooth = false);
/** Straight segments through the points, closed when the polyline is. */
SkPath toPath(const Polyline& line);

/** A SMOOTH PATH STEERED BY THE POINTS: one quadratic per interior
 *  point, with that point as the control and the midpoint of the edge
 *  after it as the end. The curve leaves the first point, passes through
 *  the midpoint of every edge tangent to that edge, and arrives at the
 *  last — so the points steer it rather than lie on it, and it never
 *  leaves the hull they span. That is what makes a sparse chain of a few
 *  placed points read as one stroke instead of a chain of chords: a
 *  coastline given a dozen points, a brush centreline given four. Closed,
 *  it starts at the midpoint of the seam edge and comes round through
 *  every point; two points are a line and fewer are an empty path.
 *
 *  `toPath(sampled, true)` is the other smoothing, and the difference is
 *  the contract: its Catmull-Rom cubic PASSES THROUGH every point and may
 *  overshoot between two that turn sharply, while this one is bounded by
 *  the points and touches none of the interior ones. */
SkPath smoothThrough(std::span<const glm::vec2> points, bool closed = false);
/** The same, over a polyline: its points, closed when it is. */
SkPath smoothThrough(const Polyline& line);

/** `line` SUBDIVIDED SO NO STEP IS LONGER THAN `spacing`: every edge cut
 *  into equal steps, as few as will keep each one within the spacing.
 *  Resampling by SPACING rather than by count, and the difference from
 *  `resample` is which is held fixed — here every source vertex
 *  survives and the steps of two edges need not be the same length,
 *  there the count is exact and the vertices are not kept. A repeated
 *  vertex is not a step: an edge of no length contributes nothing past
 *  the point it starts at.
 *
 *  The lane, when the source carries one, is interpolated linearly
 *  between the two vertices of each edge. A closed polyline is
 *  subdivided round its seam edge as well. */
Polyline subdivide(const Polyline& line, float spacing);

/** THE CATMULL-ROM CURVE THROUGH `controls`, as points rather than as a
 *  path: the uniform basis, so the curve passes through every control,
 *  with each chord cut into equal steps no longer than `spacing`. The
 *  ends duplicate their neighbour, which is what gives an open chain a
 *  first and last tangent.
 *
 *  `curvature` blends each sample toward the chord it sits on: zero is
 *  the chords themselves, one is the full curve, and the values between
 *  are what keeps a hand-placed chain of controls from bowing further
 *  than the hand meant. Outside [0, 1] it is clamped.
 *
 *  The lane rides along, interpolated between the two controls the step
 *  lies between — the curve bends, the value does not. `toPath(sampled,
 *  smooth)` is the same basis written as an `SkPath` for drawing;
 *  this is the same basis as POINTS, for a caller that walks them. */
Polyline catmullRom(const Polyline& controls, float spacing,
                    float curvature = 1.0f);

/** Point-for-point interpolation, pairing by index over whichever of the
 *  two is shorter. Closure comes from `a`; the source length interpolates
 *  with the points, so a blend still knows how long the curve it stands
 *  for is. */
Sampled lerp(const Sampled& a, const Sampled& b, float t);

}  // namespace sigil::geometry::path
