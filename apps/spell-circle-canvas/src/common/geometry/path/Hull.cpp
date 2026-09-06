/** @file
 * The convex hull by the monotone chain, and the alpha shape as the
 * boundary of whichever triangles were small enough to keep.
 */
#include "sigilgeometry/path/Hull.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>

#include "sigilgeometry/path/Triangulate.h"

namespace sigil::geometry::path {
namespace {

/** Which side of the line from `o` to `a` the point `b` falls on, as
 *  twice the signed area of the triangle. Zero means the three are on
 *  one line. */
double cross(glm::vec2 o, glm::vec2 a, glm::vec2 b) {
  return ((double)a.x - o.x) * ((double)b.y - o.y) -
         ((double)a.y - o.y) * ((double)b.x - o.x);
}

/** Andrew's monotone chain: the points sorted, then the lower and upper
 *  hulls built by keeping only the turns that go one way. Exact in the
 *  sense that matters — every decision is one sign of one determinant —
 *  and it needs no triangulation, which is what makes the convex answer
 *  independent of the alpha one. */
std::vector<glm::vec2> convexChain(std::vector<glm::vec2> sorted) {
  const size_t count = sorted.size();
  if (count < 3) return {};
  std::vector<glm::vec2> chain(count * 2);
  size_t at = 0;
  for (size_t i = 0; i < count; ++i) {
    while (at >= 2 && cross(chain[at - 2], chain[at - 1], sorted[i]) <= 0) --at;
    chain[at++] = sorted[i];
  }
  for (size_t i = count - 1, lower = at + 1; i-- > 0;) {
    while (at >= lower && cross(chain[at - 2], chain[at - 1], sorted[i]) <= 0)
      --at;
    chain[at++] = sorted[i];
  }
  // The walk comes back to where it started; the repeat is not a vertex.
  chain.resize(at > 0 ? at - 1 : 0);
  return chain;
}

/** The boundary edges of a set of kept triangles, stitched into rings.
 *  An edge belongs to exactly one kept triangle when it is on the
 *  boundary, and taking it in that triangle's own order is what makes the
 *  ring come out wound the way the triangle is — so a ring around a hole,
 *  whose triangles lie the other side of it, comes out wound against the
 *  one that contains it. */
std::vector<Polyline> stitch(const Triangulation& mesh,
                             const std::vector<bool>& kept) {
  std::map<std::pair<uint32_t, uint32_t>, int> counts;
  std::vector<std::pair<uint32_t, uint32_t>> directed;
  for (size_t t = 0; t < mesh.triangles.size(); ++t) {
    if (!kept[t]) continue;
    const glm::uvec3 triangle = mesh.triangles[t];
    for (int corner = 0; corner < 3; ++corner) {
      const uint32_t from = triangle[corner];
      const uint32_t to = triangle[(corner + 1) % 3];
      ++counts[{std::min(from, to), std::max(from, to)}];
      directed.emplace_back(from, to);
    }
  }

  std::multimap<uint32_t, uint32_t> outgoing;
  for (const auto& [from, to] : directed)
    if (counts[{std::min(from, to), std::max(from, to)}] == 1)
      outgoing.emplace(from, to);

  std::vector<Polyline> rings;
  while (!outgoing.empty()) {
    Polyline ring;
    ring.closed = true;
    const uint32_t start = outgoing.begin()->first;
    uint32_t at = start;
    while (true) {
      const auto edge = outgoing.find(at);
      if (edge == outgoing.end()) break;
      const uint32_t next = edge->second;
      outgoing.erase(edge);
      ring.points.push_back(mesh.points[at]);
      at = next;
      if (at == start) break;
    }
    if (ring.points.size() >= 3) rings.push_back(std::move(ring));
  }
  return rings;
}

}  // namespace

std::vector<Polyline> hull(std::span<const glm::vec2> points, float alpha) {
  if (points.size() < 3) return {};

  if (!(alpha < std::numeric_limits<float>::infinity())) {
    std::vector<glm::vec2> sorted(points.begin(), points.end());
    std::sort(sorted.begin(), sorted.end(), [](glm::vec2 a, glm::vec2 b) {
      return a.x != b.x ? a.x < b.x : a.y < b.y;
    });
    sorted.erase(std::unique(sorted.begin(), sorted.end(),
                             [](glm::vec2 a, glm::vec2 b) {
                               return a.x == b.x && a.y == b.y;
                             }),
                 sorted.end());
    std::vector<glm::vec2> chain = convexChain(std::move(sorted));
    if (chain.size() < 3) return {};
    Polyline ring;
    ring.closed = true;
    ring.points = std::move(chain);
    return {std::move(ring)};
  }

  const Triangulation mesh = delaunay(points);
  if (mesh.triangles.empty()) return {};
  std::vector<bool> kept(mesh.triangles.size(), false);
  for (size_t t = 0; t < mesh.triangles.size(); ++t)
    kept[t] = mesh.circumradius(t) <= alpha;
  return stitch(mesh, kept);
}

}  // namespace sigil::geometry::path
