#pragma once
/** @file
 * @ingroup geometry-path
 *
 * THE ESCAPE HATCH TO SKIA: the one header of this library that names
 * the renderer standing behind an outline. Nothing else under
 * `sigilgeometry/` includes it; a caller that draws a geometry result
 * with Skia, hands a Skia path to the geometry library, or needs an
 * operator's answer as the Skia path it was computed as, includes this
 * header by name.
 *
 * Two things stand here. The CONVERSIONS between the library's own
 * values — glm vectors, `Rect`, `Transform`, `Outline`, the stroke words
 * — and Skia's point, size, rect, matrix, path and paint enumerations:
 * the same numbers under the other type, no reinterpretation. And the
 * SKIA FORMS of the path operators: each takes and answers the Skia path
 * its outline form converts to and from, so a caller already holding a
 * Skia path skips the crossing. An operator whose outline form differs
 * only in what it answers has no Skia form here; convert its answer with
 * `toSk`.
 */
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathTypes.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>

#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "sigilgeometry/path/Band.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Crossings.h"
#include "sigilgeometry/path/Direction.h"
#include "sigilgeometry/path/Edges.h"
#include "sigilgeometry/path/Extremes.h"
#include "sigilgeometry/path/Interpolate.h"
#include "sigilgeometry/path/Operations.h"
#include "sigilgeometry/path/Outline.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Segments.h"
#include "sigilgeometry/path/Stroke.h"
#include "sigilgeometry/path/Symmetry.h"
#include "sigilgeometry/path/Tidy.h"
#include "sigilgeometry/path/Transform.h"

class SkPathBuilder;

namespace sigil::geometry::path {

/** @name Conversions
 *  @{ */

/** The vector as a Skia point: the same two numbers under the other
 *  type, no reinterpretation. */
inline SkPoint toSk(glm::vec2 v) { return {v.x, v.y}; }
/** The point back as a vector, on the same terms. */
inline glm::vec2 fromSk(SkPoint p) { return {p.fX, p.fY}; }
/** A size as a vector: width into x, height into y. */
inline glm::vec2 fromSk(SkSize s) { return {s.width(), s.height()}; }
/** A size vector as a Skia size. */
inline SkSize toSkSize(glm::vec2 size) { return {size.x, size.y}; }

/** The rect's centre as a vector. */
inline glm::vec2 centre(const SkRect& r) { return {r.centerX(), r.centerY()}; }

/** The rectangle as a Skia rect. */
inline SkRect toSk(const Rect& r) {
  return SkRect::MakeLTRB(r.min.x, r.min.y, r.max.x, r.max.y);
}
/** A Skia rect as a rectangle. */
inline Rect fromSk(const SkRect& r) {
  return {{r.fLeft, r.fTop}, {r.fRight, r.fBottom}};
}

/** The outline as the Skia path that stands behind it: the same verbs
 *  and points under the same fill rule. Sharing, not copying — a Skia
 *  path is itself a shared value. */
SkPath toSk(const Outline& outline);
/** A Skia path as an outline, with its fill rule: even-odd and
 *  inverse-even-odd read as even-odd, the others as non-zero. */
Outline fromSk(SkPath path);
/** Each outline as its Skia path, in order. */
std::vector<SkPath> toSk(std::span<const Outline> outlines);
/** Each Skia path as an outline, in order. */
std::vector<Outline> fromSk(std::span<const SkPath> paths);

/** The fill rule as the Skia fill type a path is drawn under. */
inline SkPathFillType toSk(FillRule rule) {
  return rule == FillRule::EvenOdd ? SkPathFillType::kEvenOdd
                                   : SkPathFillType::kWinding;
}

/** The affine map as a Skia matrix. */
inline SkMatrix toSk(const Transform& transform) {
  const glm::mat3& m = transform.matrix;
  return SkMatrix::MakeAll(m[0][0], m[1][0], m[2][0], m[0][1], m[1][1],
                           m[2][1], 0, 0, 1);
}

/** A Skia matrix as the affine map, its perspective row dropped. */
inline Transform fromSk(const SkMatrix& matrix) {
  Transform transform;
  transform.matrix = glm::mat3(matrix.getScaleX(), matrix.getSkewY(), 0.0f,
                               matrix.getSkewX(), matrix.getScaleY(), 0.0f,
                               matrix.getTranslateX(), matrix.getTranslateY(),
                               1.0f);
  return transform;
}

/** The winding as the direction Skia draws a closed figure in. */
inline SkPathDirection toSk(Winding winding) {
  return winding == Winding::OutersClockwise ? SkPathDirection::kCW
                                             : SkPathDirection::kCCW;
}

/** The corner decision as Skia's, for a paint about to stroke. */
inline SkPaint::Join toSk(Join join) {
  switch (join) {
    case Join::Round:
      return SkPaint::kRound_Join;
    case Join::Miter:
      return SkPaint::kMiter_Join;
    case Join::Bevel:
      return SkPaint::kBevel_Join;
  }
  return SkPaint::kRound_Join;
}

/** The end decision as Skia's, on the same terms. */
inline SkPaint::Cap toSk(Cap cap) {
  switch (cap) {
    case Cap::Butt:
      return SkPaint::kButt_Cap;
    case Cap::Round:
      return SkPaint::kRound_Cap;
    case Cap::Square:
      return SkPaint::kSquare_Cap;
  }
  return SkPaint::kButt_Cap;
}
/** @} */

/** @name Contours over a Skia path
 *  `Contour::of`, `Contour::lengthOf`, `Contour::segment` and
 *  `Contour::split` on a Skia path, and the append a builder-driven walk
 *  joins two pieces with.
 *  @{ */
/** Every contour of @p path, in path order; degenerate ones skipped. */
std::vector<Contour> contoursOf(const SkPath& path, bool forceClosed = false);
/** The total length of every contour of @p path, seams not counted. */
float lengthOf(const SkPath& path);
/** The piece of @p contour between two distances, as a Skia path. */
SkPath segmentOf(const Contour& contour, float from, float to);
/** @p contour cut in two at @p distance, as two open Skia paths. */
std::pair<SkPath, SkPath> splitOf(const Contour& contour, float distance);
/** The piece of @p contour between two distances appended to @p out. It
 *  opens with a moveTo unless @p startWithMoveTo is false, in which case
 *  it continues the builder's current contour from where that contour
 *  stands — which is how the two pieces of a window across a closed
 *  contour's seam join into one run. */
void appendSegment(SkPathBuilder& out, const Contour& contour, float from,
                   float to, bool startWithMoveTo = true);
SkPath parallel(const SkPath& path, float across, float step = 4.0f);
SkPath displace(const SkPath& path, float amplitude, float wavelength,
                bool zigzag);
SkPath cornerWindows(const SkPath& path, float radius, bool keepNearCorners,
                     float angleDeg);
/** @} */

/** @name Bands over a Skia path
 *  @{ */
SkPath profileOffset(const SkPath& spine, const Profile& profile);
SkPath bandRegion(const SkPath& spine, const Profile& width,
                  Formation formation = Formation::Center);
SkPath sweptRegion(const SkPath& spine, const SweepWidth& width,
                   const Sweep& sweep = {});
/** @} */

/** @name Crossings over Skia paths
 *  @{ */
std::vector<Crossing> discoverCrossings(std::span<const SkPath> strands);
inline std::vector<Crossing> discoverCrossings(
    std::initializer_list<SkPath> strands) {
  return discoverCrossings(
      std::span<const SkPath>(strands.begin(), strands.size()));
}
SkPath crossingPatch(const SkPath& a, float reachA, const SkPath& b,
                     float reachB, glm::vec2 at, float maxRadius);
/** @} */

/** @name Node arithmetic over a Skia path
 *  @{ */
SkPath direction(const SkPath& path, const DirectionOptions& options = {});
SkPath edges(const SkPath& outline, Edge mask, float step = 3.0f);
SkPath insetOutline(const SkPath& outline, float px);
SkPath extremes(const SkPath& path, const ExtremeOptions& options = {});
std::vector<glm::vec2> extremeNodes(const SkPath& path,
                                    const ExtremeOptions& options = {});
std::optional<SkPath> interpolate(const SkPath& a, const SkPath& b, float t);
SkPath tidy(const SkPath& path, float tolerance = 0.1f,
            const TidyOptions& options = {});
std::vector<Polyline> flatten(const SkPath& path, float tolerance = 0.25f);
std::vector<Sampled> resample(const SkPath& path, int count,
                              float tolerance = 0.25f);
std::vector<SegmentContour> segments(const SkPath& path);
/** The contours back as a Skia path under @p fill. */
SkPath toPath(std::span<const SegmentContour> contours, SkPathFillType fill);
Compatible compatible(const SkPath& a, const SkPath& b);
SkPath reverse(const SkPath& path);
SkPath startAt(const SkPath& path, size_t contour, size_t at);
SkPath copies(const Symmetry& symmetry, const SkPath& path);
/** @} */

}  // namespace sigil::geometry::path

namespace sigil::geometry::path::operations {

/** @name The operators over Skia paths
 *  A pathops failure comes back as an empty path.
 *  @{ */
SkPath unite(const SkPath& a, const SkPath& b);
SkPath subtract(const SkPath& a, const SkPath& b);
SkPath intersect(const SkPath& a, const SkPath& b);
SkPath exclude(const SkPath& a, const SkPath& b);
SkPath unite(std::span<const SkPath> paths);
inline SkPath unite(std::initializer_list<SkPath> paths) {
  return unite(std::span<const SkPath>(paths.begin(), paths.size()));
}
/** The union over any range of Skia paths, including a view that builds
 *  them as it is walked. */
template <std::ranges::input_range R>
  requires(!std::convertible_to<R &&, std::span<const SkPath>> &&
           std::convertible_to<std::ranges::range_reference_t<R>, SkPath>)
SkPath unite(R&& paths) {
  // A range that is not already contiguous is walked into one, because the
  // union builder wants every path before it resolves any of them.
  std::vector<SkPath> held;
  for (auto&& path : paths) held.push_back(path);
  return unite(std::span<const SkPath>(held));
}
SkPath simplify(const SkPath& path);
SkPath offset(const SkPath& path, float distance,
              const OffsetOptions& options = {});
SkPath roundCorners(const SkPath& path, float radius,
                    const CornerOptions& options = {});
SkPath chamferCorners(const SkPath& path, float cut);
SkPath displaceSquare(const SkPath& src, float amplitude, float wavelength);
/** Each distort applied to a Skia path, as its `apply` is to an outline. */
SkPath distort(const Roughen& roughen, const SkPath& path);
SkPath distort(const Zigzag& zigzag, const SkPath& path);
SkPath distort(const PuckerBloat& puckerBloat, const SkPath& path);
SkPath distort(const Twirl& twirl, const SkPath& path);
/** @} */

}  // namespace sigil::geometry::path::operations
