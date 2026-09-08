/** @file
 * THE BOOLEANS AND THE OFFSET: two paths combined through Skia's path
 * ops, and one path grown or shrunk — either as a band the source's own
 * area is united with or cut by, or, where the answer has to stay
 * interpolable against the source, by moving the source's own nodes
 * along their bisectors. The composition that runs a list of operators
 * in order stands here too, because it is the vocabulary's own verb.
 */

#include "sigilgeometry/path/Ops.h"

#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/pathops/SkPathOps.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

#include "OpsInternal.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Segments.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::path::ops {

namespace {

SkPath binary(const SkPath& a, const SkPath& b, SkPathOp op) {
  SkPath out;
  if (!Op(a, b, op, &out)) return SkPath();
  return out;
}

SkPaint::Join skJoin(Join join) {
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

SkPaint::Cap skCap(Cap cap) {
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

/** The unit vector 90 degrees to the LEFT of travel, in Skia's y-down
 *  space — the library-wide across-the-path direction. */
glm::vec2 leftOf(glm::vec2 direction) { return {direction.y, -direction.x}; }

glm::vec2 unitOr(glm::vec2 v, glm::vec2 fallback) {
  const float len = glm::length(v);
  return len > 1e-9f ? v / len : fallback;
}

/** THE OFFSET THAT MOVES THE SOURCE'S OWN NODES. Each node travels along
 *  the bisector of the two edges meeting there, far enough that both
 *  offset edges pass through it — `distance / sin(theta/2)` — capped at
 *  `miterLimit` distances so a needle-sharp corner blunts rather than
 *  shoots off. Each handle travels along its own chord's normal. The
 *  node count, their order and their kinds are untouched, which is the
 *  whole point: the answer still interpolates against the source. */
SkPath movedNodes(const SkPath& path, float distance,
                  const OffsetOptions& options) {
  std::vector<SegmentContour> contours = segments(path);
  const float cap = std::max(options.miterLimit, 1.0f);
  for (SegmentContour& contour : contours) {
    const size_t pieces = contour.segments.size();
    if (pieces == 0) continue;
    const bool spelled =
        contour.closed && contour.segments.back().end() != contour.start();
    const size_t nodes = (contour.closed && !spelled) ? pieces : pieces + 1;

    std::vector<glm::vec2> shift(nodes, glm::vec2{0, 0});
    for (size_t j = 0; j < nodes; ++j) {
      const glm::vec2 out =
          j < pieces
              ? leavingAlong(contour.segments[j])
              : (contour.closed
                     ? unitOr(contour.start() - contour.segments.back().end(),
                              arrivingAlong(contour.segments.back()))
                     : arrivingAlong(contour.segments.back()));
      glm::vec2 in;
      if (j > 0) {
        in = arrivingAlong(contour.segments[j - 1]);
      } else if (contour.closed) {
        in = spelled ? unitOr(contour.start() - contour.segments.back().end(),
                              arrivingAlong(contour.segments.back()))
                     : arrivingAlong(contour.segments.back());
      } else {
        in = out;
      }
      const glm::vec2 bisector = unitOr(leftOf(in) + leftOf(out), leftOf(out));
      const float reach = std::max(glm::dot(bisector, leftOf(in)), 1.0f / cap);
      shift[j] = bisector * (distance / reach);
    }

    for (size_t j = 0; j < pieces; ++j) {
      Segment& piece = contour.segments[j];
      const int last = piece.size() - 1;
      const glm::vec2 start = piece.points[0];
      const glm::vec2 end = piece.points[(size_t)last];
      for (int i = 1; i < last; ++i) {
        const glm::vec2 chord =
            i * 2 <= last
                ? unitOr(piece.points[(size_t)i] - start, leavingAlong(piece))
                : unitOr(end - piece.points[(size_t)i], arrivingAlong(piece));
        piece.points[(size_t)i] += leftOf(chord) * distance;
      }
      piece.points[0] += shift[j];
      piece.points[(size_t)last] += shift[(j + 1) % nodes];
    }
  }
  return toPath(contours, path.getFillType());
}

}  // namespace

SkPath unite(const SkPath& a, const SkPath& b) {
  return binary(a, b, kUnion_SkPathOp);
}
SkPath subtract(const SkPath& a, const SkPath& b) {
  return binary(a, b, kDifference_SkPathOp);
}
SkPath intersect(const SkPath& a, const SkPath& b) {
  return binary(a, b, kIntersect_SkPathOp);
}
SkPath exclude(const SkPath& a, const SkPath& b) {
  return binary(a, b, kXOR_SkPathOp);
}

SkPath unite(const std::vector<SkPath>& paths) {
  SkOpBuilder builder;
  for (const SkPath& p : paths) builder.add(p, kUnion_SkPathOp);
  SkPath out;
  if (!builder.resolve(&out)) return SkPath();
  return out;
}

SkPath simplify(const SkPath& path) {
  SkPath out;
  if (!Simplify(path, &out)) return path;
  return out;
}

SkPath offset(const SkPath& path, float distance,
              const OffsetOptions& options) {
  if (options.keepCompatible) return movedNodes(path, distance, options);
  if (std::abs(distance) < 1e-3f) return path;

  const float position = std::clamp(options.position, 0.0f, 1.0f);
  // The band's centreline slides from one side of the source to the
  // other as `position` runs 0 to 1, and its half-width opens from
  // nothing at either end to the whole distance in the middle.
  const float centre = distance * (1.0f - 2.0f * position);
  const float halfWidth =
      std::abs(distance) * (1.0f - std::abs(1.0f - 2.0f * position));

  const SkPath spine =
      centre == 0 ? path : parallel(path, centre, options.step);
  if (halfWidth <= 0) return spine;

  SkPaint stroke;
  stroke.setStyle(SkPaint::kStroke_Style);
  stroke.setStrokeWidth(halfWidth * 2.0f);
  stroke.setStrokeJoin(skJoin(options.join));
  stroke.setStrokeCap(skCap(options.cap));
  stroke.setStrokeMiter(options.miterLimit);
  const SkPath band = skpathutils::FillPathWithPaint(spine, stroke);

  // A band that reaches across the source encloses the source's own
  // edge, so the source and the band together are the grown area and
  // the source without it the shrunk one. A band to one side encloses
  // nothing of the source and is the answer itself.
  if (centre - halfWidth < 0 && centre + halfWidth > 0)
    return distance > 0 ? unite(path, band) : simplify(subtract(path, band));
  return band;
}

PathOp chain(std::vector<PathOp> steps) {
  return [steps = std::move(steps)](const SkPath& path) {
    SkPath current = path;
    for (const PathOp& step : steps)
      if (step) current = step(current);
    return current;
  };
}

}  // namespace sigil::geometry::path::ops
