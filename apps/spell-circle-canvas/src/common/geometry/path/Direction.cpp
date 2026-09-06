/** @file
 * The nesting of a set of rings, and the rewrite that puts an outline's
 * windings, its contour order and its start points right.
 */

#include "sigilgeometry/path/Direction.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <vector>

#include "sigilgeometry/path/Segments.h"

namespace sigil::geometry::path {

namespace {

/** The ring one contour flattens to, or an empty ring when the contour
 *  bounds no area. */
Polyline ringOf(const SegmentContour& contour) {
  const SegmentContour closedForArea{contour.segments, true};
  const std::vector<Polyline> flat = flatten(toPath({&closedForArea, 1}));
  return flat.empty() ? Polyline{} : flat.front();
}

/** Which node of a closed contour is the bottom-left one: lowest on the
 *  page in Skia's y-down space, and the leftmost of those that are
 *  level. */
size_t bottomLeft(const SegmentContour& contour) {
  // Every node the contour draws, in the order a restart indexes them:
  // each piece's start, and — where the closure carries a real line —
  // the node the last piece arrives at, which no piece starts from.
  std::vector<glm::vec2> nodes;
  nodes.reserve(contour.segments.size() + 1);
  for (const Segment& segment : contour.segments)
    nodes.push_back(segment.start());
  if (!contour.segments.empty() &&
      contour.segments.back().end() != contour.start())
    nodes.push_back(contour.segments.back().end());

  size_t best = 0;
  for (size_t i = 1; i < nodes.size(); ++i)
    if (nodes[i].y > nodes[best].y ||
        (nodes[i].y == nodes[best].y && nodes[i].x < nodes[best].x))
      best = i;
  return best;
}

}  // namespace

std::vector<Nesting> nesting(std::span<const Polyline> rings) {
  const size_t count = rings.size();
  std::vector<Nesting> out(count);
  for (size_t i = 0; i < count; ++i) {
    if (rings[i].points.size() < 3) continue;
    // The tightest ring that encloses this one is its parent, and how
    // many enclose it at all is its depth — the even-odd reading, which
    // is the rule a filled path with `kEvenOdd` is drawn by.
    float tightest = std::numeric_limits<float>::max();
    for (size_t j = 0; j < count; ++j) {
      if (i == j || rings[j].points.size() < 3) continue;
      if (!rings[j].contains(rings[i].points[0])) continue;
      ++out[i].depth;
      const float area = std::abs(rings[j].signedArea());
      if (area < tightest) {
        tightest = area;
        out[i].parent = (int)j;
      }
    }
  }
  return out;
}

SkPath direction(const SkPath& path, const DirectionOptions& options) {
  std::vector<SegmentContour> contours = segments(path);
  if (contours.empty()) return path;

  std::vector<Polyline> rings;
  rings.reserve(contours.size());
  for (const SegmentContour& contour : contours) rings.push_back(ringOf(contour));
  const std::vector<Nesting> where = nesting(rings);

  for (size_t i = 0; i < contours.size(); ++i) {
    const bool hole = where[i].depth % 2 == 1;
    // Positive signed area is a CLOCKWISE ring in Skia's y-down space.
    const bool wantClockwise =
        (options.winding == Winding::OutersClockwise) != hole;
    const float area = rings[i].signedArea();
    if (area != 0 && (area > 0) != wantClockwise)
      contours[i] = reversed(contours[i]);
    if (options.resetStart && contours[i].closed)
      contours[i] = startedAt(contours[i], bottomLeft(contours[i]));
  }

  if (options.orderContours) {
    std::vector<size_t> order(contours.size());
    std::iota(order.begin(), order.end(), (size_t)0);
    std::ranges::stable_sort(order, [&](size_t a, size_t b) {
      return where[a].depth < where[b].depth;
    });
    std::vector<SegmentContour> sorted;
    sorted.reserve(contours.size());
    for (const size_t index : order) sorted.push_back(std::move(contours[index]));
    contours = std::move(sorted);
  }
  return toPath(contours, path.getFillType());
}

}  // namespace sigil::geometry::path
