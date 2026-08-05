#pragma once

/** @file
 * SigilShape geometry core — the resampling seam every higher tool
 * stands on. Blends need two outlines expressed as the SAME number of
 * arc-length samples before they can be interpolated; meshes need flat
 * polygons before they can be triangulated or extruded. Both reductions
 * live here: SkPath -> Polyline (adaptive curve flattening that keeps
 * corners exact) and SkPath -> Sampled (uniform arc-length resampling
 * with cyclic alignment for closed contours).
 *
 * Everything is plain Skia types; no other SigilShape header is
 * required. Multi-contour paths reduce to std::vector of per-contour
 * results in path order.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>

#include <vector>

namespace sigil::shape {

/** A flattened contour: straight segments only, corner anchors exact.
 *  `closed` means the last point joins back to the first (the closing
 *  point is NOT duplicated). */
struct Polyline {
  std::vector<SkPoint> points;
  bool closed = false;

  float length() const;
  SkPoint centroid() const;
  /** Signed area (positive = clockwise in Skia's y-down space). Open
   *  polylines are treated as if closed. */
  float signedArea() const;
  void reverse();
};

/** Flatten every contour of @p path to line segments. @p tolerance is
 *  the maximum distance the segments may deviate from the true curve. */
std::vector<Polyline> flatten(const SkPath& path, float tolerance = 0.25f);

/** A contour re-expressed as exactly `points.size()` samples spaced
 *  uniformly by arc length — the common currency of blending. */
struct Sampled {
  std::vector<SkPoint> points;
  bool closed = false;
  float sourceLength = 0;

  SkPoint centroid() const;
};

/** Resample one flattened contour to exactly @p count arc-length-uniform
 *  points. Closed contours produce `count` samples with implicit wrap;
 *  open contours include both endpoints. */
Sampled resample(const Polyline& contour, int count);

/** Resample every contour of @p path to @p count samples each. */
std::vector<Sampled> resample(const SkPath& path, int count,
                              float tolerance = 0.25f);

/** The cyclic offset of @p b (and whether to traverse it reversed) that
 *  minimizes total squared distance to @p a — how a blend decides which
 *  anchor of the target corresponds to the first anchor of the source.
 *  Both must hold the same number of points; only meaningful for closed
 *  contours. */
struct Alignment {
  int offset = 0;
  bool reversed = false;
};
Alignment bestAlignment(const Sampled& a, const Sampled& b);

/** Apply an Alignment: returns @p b rotated/reversed so index i of the
 *  result corresponds to index i of the alignment's reference. */
Sampled applyAlignment(const Sampled& b, const Alignment& alignment);

/** Rebuild a path from samples: `smooth` fits a Catmull-Rom cubic
 *  through the points (organic sources), otherwise a straight polygon
 *  (faithful when the samples are dense). */
SkPath toPath(const Sampled& samples, bool smooth = false);

/** Linear blend of two equally-sized sample sets: result = a*(1-t)+b*t.
 *  The zip every blend step reduces to. */
Sampled lerp(const Sampled& a, const Sampled& b, float t);

}  // namespace sigil::shape
