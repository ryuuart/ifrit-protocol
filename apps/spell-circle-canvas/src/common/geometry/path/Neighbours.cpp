/** @file
 * The uniform grid: how a cell size is chosen, how the buckets are laid
 * down, and how a radius and a k-nearest query walk them.
 */
#include "sigilgeometry/path/Neighbours.h"

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>
#include <limits>

namespace sigil::geometry::path {
namespace {

/** How many points a cell should hold on average. Small enough that a
 *  radius query does not sweep in a crowd, large enough that the offset
 *  table is not mostly empty. */
constexpr float kPointsPerCell = 2.0f;

/** The most cells a grid takes, as a multiple of the point count. A set
 *  spread thinly across a huge extent would otherwise want a table far
 *  larger than the points in it, and a table that does not fit is a
 *  worse answer than cells slightly too coarse. */
constexpr size_t kCellsPerPoint = 4;

}  // namespace

Neighbours::Neighbours(std::span<const glm::vec3> points, float cell)
    : m_points(points.begin(), points.end()) {
  build(cell);
}

Neighbours::Neighbours(std::span<const glm::vec2> points, float cell) {
  m_points.reserve(points.size());
  for (const glm::vec2 point : points) m_points.emplace_back(point, 0.0f);
  build(cell);
}

void Neighbours::build(float cell) {
  if (m_points.empty()) {
    m_dimensions = {0, 0, 0};
    m_starts.assign(1, 0);
    return;
  }

  glm::vec3 lo = m_points.front(), hi = m_points.front();
  for (const glm::vec3 point : m_points) {
    lo = glm::min(lo, point);
    hi = glm::max(hi, point);
  }
  const glm::vec3 extent = hi - lo;

  // A cell size nobody asked for: the edge of the cube that would hold
  // `kPointsPerCell` points if the set filled its own bounding box
  // evenly. Axes of no extent do not count toward the volume — a flat
  // sheet is a two-dimensional problem and must not be given a cell size
  // derived from a zero-thickness box.
  if (!(cell > 0)) {
    float volume = 1.0f;
    int axes = 0;
    for (int axis = 0; axis < 3; ++axis)
      if (extent[axis] > 0) {
        volume *= extent[axis];
        ++axes;
      }
    const float perCell = volume * kPointsPerCell / (float)m_points.size();
    cell = axes > 0 ? std::pow(perCell, 1.0f / (float)axes) : 1.0f;
  }
  if (!(cell > 0) || !std::isfinite(cell)) cell = 1.0f;

  // The grid is bounded in cells, so a requested size may be coarsened
  // until the table fits. Each round doubles, which converges in a few
  // steps from any request.
  const size_t cap = m_points.size() * kCellsPerPoint + 64;
  glm::ivec3 dimensions{1, 1, 1};
  for (int attempt = 0; attempt < 64; ++attempt) {
    double cells = 1.0;
    for (int axis = 0; axis < 3; ++axis) {
      const auto span = (int)std::floor(extent[axis] / cell) + 1;
      dimensions[axis] = std::max(span, 1);
      cells *= (double)dimensions[axis];
    }
    if (cells <= (double)cap) break;
    cell *= 2.0f;
  }

  m_cell = cell;
  m_inverseCell = 1.0f / cell;
  m_origin = lo;
  m_dimensions = dimensions;

  const size_t cells =
      (size_t)dimensions.x * (size_t)dimensions.y * (size_t)dimensions.z;
  m_starts.assign(cells + 1, 0);
  std::vector<uint32_t> cellOfPoint(m_points.size());
  for (size_t i = 0; i < m_points.size(); ++i) {
    const glm::ivec3 at = cellOf(m_points[i]);
    const auto index = (uint32_t)linear(at.x, at.y, at.z);
    cellOfPoint[i] = index;
    ++m_starts[index + 1];
  }
  for (size_t i = 1; i < m_starts.size(); ++i) m_starts[i] += m_starts[i - 1];

  m_ordered.resize(m_points.size());
  std::vector<uint32_t> cursor(m_starts.begin(), m_starts.end() - 1);
  for (size_t i = 0; i < m_points.size(); ++i)
    m_ordered[cursor[cellOfPoint[i]]++] = (uint32_t)i;
}

glm::ivec3 Neighbours::cellOf(glm::vec3 p) const {
  glm::ivec3 at{0, 0, 0};
  for (int axis = 0; axis < 3; ++axis) {
    const float offset = (p[axis] - m_origin[axis]) * m_inverseCell;
    const int index = std::isfinite(offset) ? (int)std::floor(offset) : 0;
    at[axis] = std::clamp(index, 0, m_dimensions[axis] - 1);
  }
  return at;
}

std::span<const uint32_t> Neighbours::cellPoints(int x, int y, int z) const {
  if (x < 0 || y < 0 || z < 0 || x >= m_dimensions.x || y >= m_dimensions.y ||
      z >= m_dimensions.z)
    return {};
  const size_t index = linear(x, y, z);
  return std::span<const uint32_t>(m_ordered.data() + m_starts[index],
                                   m_starts[index + 1] - m_starts[index]);
}

void Neighbours::within(glm::vec3 p, float radius,
                        std::vector<uint32_t>& out) const {
  out.clear();
  forEachWithin(p, radius, [&](uint32_t index) { out.push_back(index); });
}

std::vector<uint32_t> Neighbours::within(glm::vec3 p, float radius) const {
  std::vector<uint32_t> out;
  within(p, radius, out);
  return out;
}

std::vector<uint32_t> Neighbours::within(glm::vec2 p, float radius) const {
  return within(glm::vec3(p, 0.0f), radius);
}

std::optional<uint32_t> Neighbours::nearest(glm::vec3 p) const {
  return nearestOther(p, std::numeric_limits<uint32_t>::max());
}

std::optional<uint32_t> Neighbours::nearestOther(glm::vec3 p,
                                                 uint32_t skip) const {
  if (m_points.empty()) return std::nullopt;

  const glm::ivec3 centre = cellOf(p);
  const int reach = std::max({m_dimensions.x, m_dimensions.y, m_dimensions.z});
  float best = std::numeric_limits<float>::infinity();
  std::optional<uint32_t> found;

  // Rings of cells outward from the one the query falls in. A ring at
  // `r` cells out cannot hold anything nearer than (r - 1) cells, so the
  // walk stops one ring after the first hit rather than at it.
  for (int ring = 0; ring <= reach; ++ring) {
    if (found) {
      const float floorDistance = (float)(ring - 1) * m_cell;
      if (floorDistance > 0 && floorDistance * floorDistance > best) break;
    }
    for (int z = centre.z - ring; z <= centre.z + ring; ++z)
      for (int y = centre.y - ring; y <= centre.y + ring; ++y)
        for (int x = centre.x - ring; x <= centre.x + ring; ++x) {
          // Only the shell: the interior was walked by an earlier ring.
          const int reachX = std::abs(x - centre.x);
          const int reachY = std::abs(y - centre.y);
          const int reachZ = std::abs(z - centre.z);
          if (std::max({reachX, reachY, reachZ}) != ring) continue;
          for (const uint32_t index : cellPoints(x, y, z)) {
            if (index == skip) continue;
            const glm::vec3 delta = m_points[index] - p;
            const float squared = glm::dot(delta, delta);
            if (squared < best) {
              best = squared;
              found = index;
            }
          }
        }
  }
  return found;
}

std::vector<uint32_t> Neighbours::nearest(glm::vec3 p, int k) const {
  std::vector<uint32_t> answer;
  if (k <= 0 || m_points.empty()) return answer;

  // The k nearest are found by growing the search radius until the ring
  // walked holds enough of them, then sorting what was gathered. Starting
  // from a radius that would hold k points if the set were even keeps the
  // growth to a round or two.
  const auto want = (size_t)std::min<size_t>((size_t)k, m_points.size());
  float radius = m_cell * std::cbrt((float)want / kPointsPerCell + 1.0f);
  std::vector<uint32_t> gathered;
  const glm::vec3 extent =
      glm::vec3(m_dimensions) * m_cell + glm::vec3(m_cell);
  const float limit = glm::length(extent) + m_cell;
  while (true) {
    within(p, radius, gathered);
    if (gathered.size() >= want || radius >= limit) break;
    radius *= 2.0f;
  }

  const size_t keep = std::min(want, gathered.size());
  std::partial_sort(gathered.begin(), gathered.begin() + (long)keep,
                    gathered.end(), [&](uint32_t a, uint32_t b) {
                      const glm::vec3 da = m_points[a] - p;
                      const glm::vec3 db = m_points[b] - p;
                      const float sa = glm::dot(da, da);
                      const float sb = glm::dot(db, db);
                      return sa != sb ? sa < sb : a < b;
                    });
  answer.assign(gathered.begin(), gathered.begin() + (long)keep);
  return answer;
}

void relax(std::span<glm::vec3> points, const Relaxation& relaxation,
           const std::function<glm::vec3(glm::vec3)>& hold) {
  if (points.size() < 2 || !(relaxation.radius > 0)) return;
  const float radius = relaxation.radius;
  const float strength = std::clamp(relaxation.strength, 0.0f, 1.0f);
  if (strength <= 0) return;

  std::vector<glm::vec3> push(points.size());
  for (int pass = 0; pass < relaxation.iterations; ++pass) {
    // A fresh index per pass: the index is a snapshot by contract, and a
    // pass must push against where its neighbours are NOW rather than
    // where they started. Every point moves off the same snapshot, so the
    // result does not depend on the order they are walked in.
    const Neighbours index(points, radius);
    std::fill(push.begin(), push.end(), glm::vec3(0));
    for (size_t i = 0; i < points.size(); ++i) {
      const glm::vec3 here = points[i];
      index.forEachWithin(here, radius, [&](uint32_t other) {
        if ((size_t)other == i) return;
        const glm::vec3 away = here - points[other];
        const float distance = glm::length(away);
        // Coincident points have no bearing to separate along, and
        // inventing one would make the answer depend on the order.
        if (!(distance > 0)) return;
        push[i] += away * ((radius - distance) / (distance * radius));
      });
    }
    for (size_t i = 0; i < points.size(); ++i) {
      glm::vec3 moved = points[i] + push[i] * (strength * radius * 0.5f);
      if (hold) moved = hold(moved);
      points[i] = moved;
    }
  }
}

}  // namespace sigil::geometry::path
