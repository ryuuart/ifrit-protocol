#pragma once
/** @file
 * A PATH READ AS SEGMENTS: the pieces between its nodes, each with the
 * control points that bend it, and the way back to a path.
 *
 * `Polyline` throws the curves away and `Contour` sees only arc length,
 * so neither can answer a question about a NODE — where the extremes of
 * a cubic are, which handles lie on their chord, whether two outlines
 * have the same nodes in the same order. This is the reading that can:
 * one `Segment` per drawn piece, its points in drawing order, and the
 * conic's weight beside them.
 *
 * A closed contour's closing line is the closure, not a segment: it is
 * implied by `closed` and drawn by `close()`, so a rectangle is three
 * segments and rebuilding it gives back the four points it came from.
 */
#include <include/core/SkPath.h>
#include <include/core/SkPathTypes.h>

#include <array>
#include <cstdint>
#include <glm/vec2.hpp>
#include <span>
#include <vector>

namespace sigil::geometry::path {

/** What bends one piece of a contour. */
enum class SegmentKind : uint8_t { Line, Quad, Conic, Cubic };

/** How many points a segment of that kind carries, its start included:
 *  two for a line, three for a quad or a conic, four for a cubic. */
constexpr int pointCount(SegmentKind kind) {
  switch (kind) {
    case SegmentKind::Line:
      return 2;
    case SegmentKind::Quad:
    case SegmentKind::Conic:
      return 3;
    case SegmentKind::Cubic:
      return 4;
  }
  return 2;
}

/** ONE PIECE OF A CONTOUR: its points in drawing order, the first the
 *  node it leaves and the last the node it arrives at, with the control
 *  points between them. `weight` is the conic's and is one for every
 *  other kind, so two segments compare by value without a special
 *  case. */
struct Segment {
  SegmentKind kind = SegmentKind::Line;
  std::array<glm::vec2, 4> points{};
  float weight = 1.0f;

  bool operator==(const Segment&) const = default;

  [[nodiscard]] int size() const { return pointCount(kind); }
  [[nodiscard]] glm::vec2 start() const { return points[0]; }
  [[nodiscard]] glm::vec2 end() const {
    return points[(size_t)pointCount(kind) - 1];
  }
};

/** ONE CONTOUR AS ITS SEGMENTS. A closed contour's last segment arrives
 *  at whatever node it arrives at and the closure carries the path back
 *  to `start()`, exactly as `SkPath::close()` does. */
struct SegmentContour {
  std::vector<Segment> segments;
  bool closed = false;

  bool operator==(const SegmentContour&) const = default;

  /** The node the contour is drawn from. A contour with no segment at
   *  all — a lone `moveTo` — starts at the origin. */
  [[nodiscard]] glm::vec2 start() const {
    return segments.empty() ? glm::vec2{0, 0} : segments.front().start();
  }
};

/** Every contour of `path`, in path order, as its segments. A contour
 *  holding no drawn piece is skipped, so what comes back is what the
 *  path draws. */
std::vector<SegmentContour> segments(const SkPath& path);

/** The contours back as a path, one `moveTo` per contour and a `close()`
 *  where the contour says it is closed. A path that went through
 *  `segments()` and came back through here is the path it started as,
 *  verb for verb and point for point, when it is rebuilt under the fill
 *  type it was read with. */
SkPath toPath(std::span<const SegmentContour> contours,
              SkPathFillType fill = SkPathFillType::kWinding);

/** WHETHER TWO OUTLINES HAVE THE SAME NODES IN THE SAME ORDER, and when
 *  they do not, the first reason they do not — which is what a caller
 *  needs in order to fix it rather than fall back to a resampling.
 *
 *  The order is the order the checks run in: a pair that differs in more
 *  than one way is named by the coarsest difference. */
enum class Compatible : uint8_t {
  /** Every contour pairs, segment for segment, kind for kind. */
  Yes,
  /** The paths draw a different number of contours. */
  ContourCount,
  /** A pair of contours holds a different number of segments, or one is
   *  closed and the other is not. */
  SegmentCount,
  /** A pair of contours holds the same segments in a different ORDER —
   *  the kinds agree only once one of them is started at another node,
   *  which is the one incompatibility a caller can repair without
   *  redrawing anything. */
  StartPoint,
  /** A pair of contours draws different kinds of piece at the same
   *  place, and no restart makes them agree. */
  SegmentKind,
};

/** The compatibility of `a` and `b`, contour by contour in path order. */
Compatible compatible(const SkPath& a, const SkPath& b);

/** ONE CONTOUR DRAWN THE OTHER WAY: its pieces in reverse order, each
 *  with its points reversed. A closed contour keeps the node it starts
 *  at — only its direction changes — and an open one starts where it
 *  used to end. A closing line the contour drew explicitly becomes the
 *  closure, since `close()` draws exactly that line. */
SegmentContour reversed(const SegmentContour& contour);

/** ONE CLOSED CONTOUR RESTARTED at its piece `at`, wrapping past the
 *  end. An open contour has ends and comes back untouched. */
SegmentContour startedAt(const SegmentContour& contour, size_t at);

/** THE SAME OUTLINE DRAWN THE OTHER WAY: every contour's segments in
 *  reverse order, each with its points reversed, so the curves survive
 *  where a polyline reversal would have flattened them. A closed contour
 *  keeps the node it starts at and only its direction changes; an open
 *  one starts where it used to end. */
SkPath reverse(const SkPath& path);

/** THE CONTOUR RESTARTED AT ANOTHER NODE: the segments of `contour` of
 *  `path` rolled so that segment `at` is drawn first. Only a closed
 *  contour can be restarted — an open one has ends, and moving its start
 *  would cut it — and an index past the end wraps. The drawn shape does
 *  not move; which node is first does. */
SkPath startAt(const SkPath& path, size_t contour, size_t at);

}  // namespace sigil::geometry::path
