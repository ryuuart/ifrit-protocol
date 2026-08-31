#pragma once
/** @file
 * Polylines — a path flattened to points, resampled to a count, aligned
 * to another for interpolation. Everything that treats an outline as a
 * list of vertices starts here: shape blending, roughening, corner
 * walks, scatter along an edge.
 *
 * Only `flatten` reads a Skia path and only `toPath` writes one; the rest
 * is plain vectors so it composes with any source of points.
 */
#include <include/core/SkPath.h>

#include <functional>
#include <glm/vec2.hpp>
#include <vector>

namespace sigil::geometry::path {

/** One flattened contour: points in order, optionally closed. */
struct Polyline {
  std::vector<glm::vec2> points;
  bool closed = false;

  float length() const;
  /** Length-weighted centroid of the edges. */
  glm::vec2 centroid() const;
  /** Signed area (positive = clockwise in Skia's y-down space). Open
   *  polylines are treated as if closed. */
  float signedArea() const;
  void reverse();
};

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

/** Point-for-point interpolation, pairing by index over whichever of the
 *  two is shorter. Closure comes from `a`; the source length interpolates
 *  with the points, so a blend still knows how long the curve it stands
 *  for is. */
Sampled lerp(const Sampled& a, const Sampled& b, float t);

}  // namespace sigil::geometry::path
