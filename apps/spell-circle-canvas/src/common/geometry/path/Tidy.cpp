/** @file
 * The three redundancies a tidy-up takes out, each measured as how far
 * the outline moves when the node goes.
 */

#include "sigilgeometry/path/Tidy.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <glm/geometric.hpp>
#include <vector>

#include "sigilgeometry/path/Segments.h"

namespace sigil::geometry::path {

namespace {

/** How far `point` stands from the segment `from`-`to`. A segment of no
 *  length is a point, and the answer is the distance to it. */
float offLine(glm::vec2 point, glm::vec2 from, glm::vec2 to) {
  const glm::vec2 run = to - from;
  const float length = glm::length(run);
  if (!(length > 1e-9f)) return glm::length(point - from);
  const float along =
      std::clamp(glm::dot(point - from, run) / (length * length), 0.0f, 1.0f);
  return glm::length(point - (from + run * along));
}

/** Whether a curve's handles lie close enough to its own chord that the
 *  curve is the chord. */
bool isChord(const Segment& piece, float tolerance) {
  const glm::vec2 from = piece.start();
  const glm::vec2 to = piece.end();
  for (int i = 1; i < piece.size() - 1; ++i)
    if (offLine(piece.points[(size_t)i], from, to) > tolerance) return false;
  return true;
}

Segment asLine(glm::vec2 from, glm::vec2 to) {
  Segment out;
  out.kind = SegmentKind::Line;
  out.points[0] = from;
  out.points[1] = to;
  return out;
}

/** One pass over a contour. Answers whether anything moved, so the
 *  caller can run again: taking one node out can leave its neighbour
 *  redundant, and a single pass would stop halfway down a staircase. */
bool tidyOnce(std::vector<Segment>& pieces, bool closed, float tolerance,
              const TidyOptions& options) {
  bool moved = false;

  if (options.duplicates && pieces.size() > 2) {
    std::vector<Segment> kept;
    kept.reserve(pieces.size());
    for (const Segment& piece : pieces) {
      const bool nothing = glm::length(piece.end() - piece.start()) <= tolerance &&
                           isChord(piece, tolerance);
      if (nothing && kept.size() + 1 < pieces.size() && pieces.size() > 2)
        continue;
      kept.push_back(piece);
    }
    if (kept.size() != pieces.size()) {
      moved = true;
      // A dropped piece leaves a gap: the piece before it now has to
      // arrive where the piece after it starts.
      for (size_t i = 0; i + 1 < kept.size(); ++i)
        kept[i].points[(size_t)kept[i].size() - 1] = kept[i + 1].start();
      pieces = std::move(kept);
    }
  }

  if (options.reduceOrder)
    for (Segment& piece : pieces)
      if (piece.kind != SegmentKind::Line && isChord(piece, tolerance)) {
        piece = asLine(piece.start(), piece.end());
        moved = true;
      }

  if (options.collinear && pieces.size() >= 2) {
    // One sweep rather than one merge: a straight run cut into a hundred
    // nodes collapses in a single pass, where restarting after every
    // merge would walk the run once per node it drops.
    std::vector<Segment> kept;
    kept.reserve(pieces.size());
    const auto joins = [&](const Segment& before, const Segment& after) {
      return before.kind == SegmentKind::Line &&
             after.kind == SegmentKind::Line &&
             glm::length(after.end() - before.start()) > 1e-9f &&
             offLine(before.end(), before.start(), after.end()) <= tolerance;
    };
    for (const Segment& piece : pieces) {
      if (!kept.empty() && joins(kept.back(), piece))
        kept.back() = asLine(kept.back().start(), piece.end());
      else
        kept.push_back(piece);
    }
    // The wrap-around node exists only where the last piece arrives at
    // the first one's start; where a closure carries a line between
    // them, those two pieces are not neighbours. A ring of two pieces
    // has no node to spare.
    const bool ring = closed && kept.size() > 2 &&
                      kept.back().end() == kept.front().start();
    if (ring && joins(kept.back(), kept.front())) {
      kept.front() = asLine(kept.back().start(), kept.front().end());
      kept.pop_back();
    }
    if (kept.size() != pieces.size()) {
      pieces = std::move(kept);
      moved = true;
    }
  }
  return moved;
}

}  // namespace

SkPath tidy(const SkPath& path, float tolerance, const TidyOptions& options) {
  std::vector<SegmentContour> contours = segments(path);
  for (SegmentContour& contour : contours) {
    // Passes are bounded by the node count: every pass that changes
    // anything takes at least one node out, so the walk cannot run on.
    for (size_t pass = contour.segments.size() + 1; pass > 0; --pass)
      if (!tidyOnce(contour.segments, contour.closed, tolerance, options))
        break;
  }
  return toPath(contours, path.getFillType());
}

}  // namespace sigil::geometry::path
