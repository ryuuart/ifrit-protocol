/** @file
 * The scanline lattice: the rings turned so the lines lie flat, scanned,
 * and the marks turned back.
 */

#include "sigilgeometry/path/Lattice.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sigil::geometry::path {

namespace {

/** One edge of a turned ring, kept only where it spans scanlines: a
 *  horizontal edge crosses none of them. */
struct Edge {
  glm::vec2 from{0, 0};
  glm::vec2 to{0, 0};
};

}  // namespace

std::vector<LatticeMark> lattice(std::span<const Polyline> rings,
                                 const LatticeOptions& options) {
  if (!(options.spacing > 0)) return {};

  // Turn the rings so the lines run along +x, scan by y, and turn each
  // mark back. One rotation of the geometry rather than a projection per
  // edge per line.
  const float cosine = std::cos(options.angle);
  const float sine = std::sin(options.angle);
  float lowest = std::numeric_limits<float>::infinity();
  float highest = -std::numeric_limits<float>::infinity();
  std::vector<Edge> edges;
  std::vector<glm::vec2> turned;
  for (const Polyline& ring : rings) {
    if (ring.points.size() < 3) continue;
    turned.clear();
    turned.reserve(ring.points.size());
    for (const glm::vec2 point : ring.points) {
      const glm::vec2 at{point.x * cosine - point.y * sine,
                         point.x * sine + point.y * cosine};
      turned.push_back(at);
      lowest = std::min(lowest, at.y);
      highest = std::max(highest, at.y);
    }
    for (size_t i = 0; i < turned.size(); ++i) {
      const glm::vec2 from = turned[i];
      const glm::vec2 to = turned[(i + 1) % turned.size()];
      if (from.y != to.y) edges.push_back({from, to});
    }
  }
  if (edges.empty() || !std::isfinite(lowest) || !std::isfinite(highest))
    return {};

  std::vector<LatticeMark> marks;
  std::vector<float> crossings;
  // The first line sits half a gap in, so a lattice through a shape one
  // gap tall still marks it.
  float scan = lowest + options.spacing * 0.5f;
  float gap = options.spacing;
  int lines = 0;
  while (scan < highest && lines++ < options.maxLines) {
    crossings.clear();
    for (const Edge& edge : edges) {
      // A half-open test on each edge: a vertex shared by two edges is
      // crossed once, so a line through a corner does not pair with
      // itself.
      if ((edge.from.y <= scan) == (edge.to.y <= scan)) continue;
      crossings.push_back(edge.from.x + (scan - edge.from.y) /
                                            (edge.to.y - edge.from.y) *
                                            (edge.to.x - edge.from.x));
    }
    std::ranges::sort(crossings);
    for (size_t i = 0; i + 1 < crossings.size(); i += 2) {
      const float from = crossings[i];
      const float to = crossings[i + 1];
      marks.push_back({{from * cosine + scan * sine, -from * sine + scan * cosine},
                       {to * cosine + scan * sine, -to * sine + scan * cosine}});
    }
    scan += gap;
    gap = std::max(0.125f, gap * options.taper);
  }
  return marks;
}

}  // namespace sigil::geometry::path
