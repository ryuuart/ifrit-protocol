/** @file
 * The segment reader and the way back, the compatibility question over
 * them, and the two rewrites that only change which way round a contour
 * is drawn and which node it starts at.
 */

#include "sigilgeometry/path/Segments.h"

#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathIter.h>
#include <include/core/SkPoint.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::path {

namespace {

Segment piece(SegmentKind kind, const SkPoint* points, float weight) {
  Segment out;
  out.kind = kind;
  out.weight = weight;
  for (int i = 0; i < pointCount(kind); ++i)
    out.points[(size_t)i] = fromSk(points[i]);
  return out;
}

void appendSegment(SkPathBuilder& out, const Segment& segment) {
  switch (segment.kind) {
    case SegmentKind::Line:
      out.lineTo(toSk(segment.points[1]));
      break;
    case SegmentKind::Quad:
      out.quadTo(toSk(segment.points[1]), toSk(segment.points[2]));
      break;
    case SegmentKind::Conic:
      out.conicTo(toSk(segment.points[1]), toSk(segment.points[2]),
                  segment.weight);
      break;
    case SegmentKind::Cubic:
      out.cubicTo(toSk(segment.points[1]), toSk(segment.points[2]),
                  toSk(segment.points[3]));
      break;
  }
}

Segment line(glm::vec2 from, glm::vec2 to) {
  Segment out;
  out.kind = SegmentKind::Line;
  out.points[0] = from;
  out.points[1] = to;
  return out;
}

/** THE WHOLE CYCLE a closed contour draws, its closing line spelled out
 *  as a segment — which is what lets a rewrite roll or reverse the
 *  pieces without the closure landing in the wrong place. The flag says
 *  whether a piece had to be spelled out, and it is what the way back
 *  needs: only a contour that carried its closing line implicitly may
 *  hand one back to the closure, or a rewrite would lose a node. */
struct Cycle {
  std::vector<Segment> pieces;
  bool closureSpelled = false;
};

Cycle cycleOf(const SegmentContour& contour) {
  Cycle cycle{contour.segments, false};
  if (contour.closed && !cycle.pieces.empty() &&
      cycle.pieces.back().end() != contour.start()) {
    cycle.pieces.push_back(line(cycle.pieces.back().end(), contour.start()));
    cycle.closureSpelled = true;
  }
  return cycle;
}

/** The cycle back as a contour: the closing LINE that was spelled out
 *  becomes the closure again, since `close()` draws exactly that line.
 *  Nothing else is dropped — a node the source drew is a node the
 *  answer draws. */
SegmentContour fromCycle(Cycle cycle, bool closed) {
  if (closed && cycle.closureSpelled && cycle.pieces.size() > 1 &&
      cycle.pieces.back().kind == SegmentKind::Line &&
      cycle.pieces.back().end() == cycle.pieces.front().start())
    cycle.pieces.pop_back();
  return SegmentContour{std::move(cycle.pieces), closed};
}

/** The pieces in the other order, each drawn from the node it used to
 *  arrive at. */
std::vector<Segment> reversedPieces(const std::vector<Segment>& segments) {
  std::vector<Segment> out;
  out.reserve(segments.size());
  for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
    Segment flipped = *it;
    const int n = it->size();
    for (int i = 0; i < n; ++i)
      flipped.points[(size_t)i] = it->points[(size_t)(n - 1 - i)];
    out.push_back(flipped);
  }
  return out;
}

/** Whether the two runs of segments draw the same kinds in the same
 *  order once `b` is rolled by `offset`. */
bool kindsAgree(const std::vector<Segment>& a, const std::vector<Segment>& b,
                size_t offset) {
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i].kind != b[(i + offset) % b.size()].kind) return false;
  return true;
}

}  // namespace

std::vector<SegmentContour> segments(const SkPath& path) {
  std::vector<SegmentContour> contours;
  SegmentContour current;
  bool open = false;
  const auto flush = [&] {
    if (open && !current.segments.empty())
      contours.push_back(std::move(current));
    current = {};
    open = false;
  };

  SkPathIter iter = path.iter();
  while (const std::optional<SkPathIter::Rec> rec = iter.next()) {
    const SkPoint* points = rec->fPoints.data();
    switch (rec->fVerb) {
      case SkPathVerb::kMove:
        flush();
        open = true;
        break;
      case SkPathVerb::kLine:
        current.segments.push_back(piece(SegmentKind::Line, points, 1.0f));
        break;
      case SkPathVerb::kQuad:
        current.segments.push_back(piece(SegmentKind::Quad, points, 1.0f));
        break;
      case SkPathVerb::kConic:
        current.segments.push_back(
            piece(SegmentKind::Conic, points, rec->fConicWeight));
        break;
      case SkPathVerb::kCubic:
        current.segments.push_back(piece(SegmentKind::Cubic, points, 1.0f));
        break;
      case SkPathVerb::kClose:
        // The closing line is the closure: `close()` draws it on the way
        // back, so keeping it as a segment as well would double it.
        current.closed = true;
        flush();
        break;
    }
  }
  flush();
  return contours;
}

SkPath toPath(std::span<const SegmentContour> contours, SkPathFillType fill) {
  SkPathBuilder out(fill);
  for (const SegmentContour& contour : contours) {
    if (contour.segments.empty()) continue;
    out.moveTo(toSk(contour.start()));
    for (const Segment& segment : contour.segments) appendSegment(out, segment);
    if (contour.closed) out.close();
  }
  return out.detach();
}

Compatible compatible(const SkPath& a, const SkPath& b) {
  const std::vector<SegmentContour> left = segments(a);
  const std::vector<SegmentContour> right = segments(b);
  if (left.size() != right.size()) return Compatible::ContourCount;
  for (size_t c = 0; c < left.size(); ++c) {
    const std::vector<Segment>& one = left[c].segments;
    const std::vector<Segment>& other = right[c].segments;
    if (one.size() != other.size() || left[c].closed != right[c].closed)
      return Compatible::SegmentCount;
    if (kindsAgree(one, other, 0)) continue;
    if (left[c].closed)
      for (size_t roll = 1; roll < other.size(); ++roll)
        if (kindsAgree(one, other, roll)) return Compatible::StartPoint;
    return Compatible::SegmentKind;
  }
  return Compatible::Yes;
}

SegmentContour reversed(const SegmentContour& contour) {
  Cycle cycle = cycleOf(contour);
  cycle.pieces = reversedPieces(cycle.pieces);
  return fromCycle(std::move(cycle), contour.closed);
}

SegmentContour startedAt(const SegmentContour& contour, size_t at) {
  if (!contour.closed || contour.segments.size() < 2) return contour;
  Cycle cycle = cycleOf(contour);
  const size_t roll = at % cycle.pieces.size();
  std::rotate(cycle.pieces.begin(), cycle.pieces.begin() + (std::ptrdiff_t)roll,
              cycle.pieces.end());
  return fromCycle(std::move(cycle), true);
}

SkPath reverse(const SkPath& path) {
  std::vector<SegmentContour> contours = segments(path);
  for (SegmentContour& contour : contours) contour = reversed(contour);
  return toPath(contours, path.getFillType());
}

SkPath startAt(const SkPath& path, size_t contour, size_t at) {
  std::vector<SegmentContour> contours = segments(path);
  if (contour >= contours.size()) return path;
  contours[contour] = startedAt(contours[contour], at);
  return toPath(contours, path.getFillType());
}

}  // namespace sigil::geometry::path
