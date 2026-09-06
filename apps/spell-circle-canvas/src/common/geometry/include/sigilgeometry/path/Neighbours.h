#pragma once
/** @file
 * ONE UNIFORM GRID OVER A SET OF POINTS, built once and asked many
 * times.
 *
 * Every question of the form "what is near here" — which points fall
 * within a radius, which is the nearest, which k are nearest — is the
 * same question, and answering it by walking every point is the shape a
 * scatter that must not clump, a packing that must not overlap, a
 * relaxation, a cluster and an attribute gather all degrade into. One
 * grid answers all of them, and the cost of an answer stops depending on
 * how many points there are and starts depending on how many are NEAR.
 *
 * It is a snapshot, not a view: the positions are copied in, so a caller
 * may move its own points while the index still answers about where they
 * were, and a moved point set is a new index rather than an update. That
 * is what makes a relaxation a loop of rebuilds and each pass' answers
 * consistent with one another.
 *
 * The currency is three dimensions, and two-dimensional points enter with
 * `z` at zero. A flat set therefore occupies one layer of cells and costs
 * exactly what a two-dimensional grid would; nothing is spent on the
 * axis that is not used.
 */
#include <cstddef>
#include <cstdint>
#include <functional>
#include <glm/ext/vector_int3.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <optional>
#include <span>
#include <vector>

namespace sigil::geometry::path {

/** A uniform grid over points, holding their positions and answering
 *  proximity about them.
 *
 *  THE CELL SIZE IS THE ONE DIAL. Left at zero it is chosen so that the
 *  average cell holds a couple of points, which is what makes a radius
 *  query touch a bounded number of cells whatever the count. Given
 *  explicitly it should be about the radius that will be asked for:
 *  cells much smaller than the radius are many to walk, and cells much
 *  larger sweep in points that were never close. The grid is also
 *  BOUNDED IN CELLS — a set spread across a huge extent with a tiny cell
 *  would otherwise want more cells than points — so a cell size given
 *  here is a request the index may coarsen, and `cell()` answers what it
 *  actually used.
 *
 *  An empty index answers nothing to every query, and is what a
 *  default-constructed one is. */
class Neighbours {
 public:
  Neighbours() = default;

  /** The index over `points`, at `cell` (zero chooses one). */
  explicit Neighbours(std::span<const glm::vec3> points, float cell = 0);
  /** The same over flat points, each lifted to `z` of zero. */
  explicit Neighbours(std::span<const glm::vec2> points, float cell = 0);

  [[nodiscard]] size_t size() const { return m_points.size(); }
  [[nodiscard]] bool empty() const { return m_points.empty(); }
  /** The cell edge the grid was actually built at. */
  [[nodiscard]] float cell() const { return m_cell; }
  /** The point at `index`, as it was when the index was built. */
  [[nodiscard]] glm::vec3 point(size_t index) const { return m_points[index]; }
  /** How many cells the grid spans on each axis. */
  [[nodiscard]] glm::ivec3 dimensions() const { return m_dimensions; }

  /** THE INDICES WITHIN `radius` OF `p`, in no particular order,
   *  appended to `out` after clearing it. The vector is the caller's so
   *  a loop of queries allocates once. A radius of zero or less answers
   *  nothing. */
  void within(glm::vec3 p, float radius, std::vector<uint32_t>& out) const;
  /** The same, allocating its own answer. */
  [[nodiscard]] std::vector<uint32_t> within(glm::vec3 p, float radius) const;
  /** The same for a flat query point, at `z` of zero. */
  [[nodiscard]] std::vector<uint32_t> within(glm::vec2 p, float radius) const;

  /** THE NEAREST INDEX TO `p`, or nothing when the index is empty.
   *  Rings of cells are walked outward from the one `p` falls in and the
   *  walk stops when no unvisited ring can hold anything nearer, so the
   *  answer is exact rather than "nearest within the first cell". */
  [[nodiscard]] std::optional<uint32_t> nearest(glm::vec3 p) const;
  /** THE `k` NEAREST INDICES TO `p`, NEAREST FIRST. Fewer than `k` come
   *  back when the index holds fewer points than that. */
  [[nodiscard]] std::vector<uint32_t> nearest(glm::vec3 p, int k) const;
  /** The nearest to `p` that is not `p` itself — the query every
   *  spacing measurement wants, since a point indexed with the set it is
   *  measured against is always its own nearest. */
  [[nodiscard]] std::optional<uint32_t> nearestOther(glm::vec3 p,
                                                     uint32_t skip) const;

  /** The indices stored in one cell, empty when the cell is outside the
   *  grid. What a caller walking cells itself reads; `forEachWithin` is
   *  written in terms of it. */
  [[nodiscard]] std::span<const uint32_t> cellPoints(int x, int y,
                                                     int z) const;

  /** `visit(index)` FOR EVERY POINT WITHIN `radius` OF `p`, allocating
   *  nothing at all. The order is cell order, which is not distance
   *  order; a caller that needs the nearest first asks `nearest`. */
  template <typename Visit>
  void forEachWithin(glm::vec3 p, float radius, Visit&& visit) const {
    if (m_points.empty() || !(radius > 0)) return;
    const float squared = radius * radius;
    const glm::ivec3 lo = cellOf(p - glm::vec3(radius));
    const glm::ivec3 hi = cellOf(p + glm::vec3(radius));
    for (int z = lo.z; z <= hi.z; ++z)
      for (int y = lo.y; y <= hi.y; ++y)
        for (int x = lo.x; x <= hi.x; ++x)
          for (const uint32_t index : cellPoints(x, y, z)) {
            const glm::vec3 delta = m_points[index] - p;
            if (glm::dot(delta, delta) <= squared) visit(index);
          }
  }

 private:
  /** Which cell a position falls in, clamped to the grid. */
  [[nodiscard]] glm::ivec3 cellOf(glm::vec3 p) const;
  /** The bucket offset of a cell already known to be inside the grid. */
  [[nodiscard]] size_t linear(int x, int y, int z) const {
    return (size_t)((z * m_dimensions.y + y) * m_dimensions.x + x);
  }
  void build(float cell);

  std::vector<glm::vec3> m_points;
  /** The point indices, grouped by cell: cell `c` owns
   *  `[m_starts[c], m_starts[c + 1])`. One array and one offset table
   *  rather than a vector per cell, so building is two counting passes
   *  and a query is a contiguous read. */
  std::vector<uint32_t> m_ordered;
  std::vector<uint32_t> m_starts;
  glm::vec3 m_origin{0, 0, 0};
  glm::ivec3 m_dimensions{0, 0, 0};
  float m_cell = 1;
  float m_inverseCell = 1;
};

/** HOW HARD POINTS ARE PUSHED OUT OF EACH OTHER'S WAY.
 *
 *  A relaxation is what turns a scatter that clumped into one that is
 *  evenly spread without being a lattice: every point is nudged away from
 *  the points inside its radius, all of them at once, and the pass is
 *  repeated. It is the same operator whether the points came from filling
 *  an outline, from a mesh surface or from a hand-placed set, so there is
 *  one body and the tiers differ only in what they hold the points to. */
struct Relaxation {
  /** How far a point pushes. Points further apart than this do not see
   *  each other at all, which is what bounds the work. */
  float radius = 1;
  /** How many passes. Each one is a fresh index over the moved points,
   *  because a pass must see where its neighbours are now. */
  int iterations = 4;
  /** What fraction of the push is taken each pass, 0 to 1. Below one the
   *  points settle instead of ringing. */
  float strength = 0.5f;
  friend bool operator==(const Relaxation&, const Relaxation&) = default;
};

/** POINTS PUSHED APART, in place.
 *
 *  Each pass indexes the points as they are, sums the outward push from
 *  every neighbour inside the radius weighted by how far inside it they
 *  are, and moves every point by `strength` of its own sum. Two
 *  coincident points have no direction to separate along and are left
 *  alone: a scatter answers that with a seed, not with an arbitrary
 *  bearing.
 *
 *  `hold`, when given, is applied to every moved point and its answer is
 *  where the point lands — a clamp back inside a shape, a projection onto
 *  a surface, a fixed axis. Without it the points relax freely and the
 *  set spreads. */
void relax(std::span<glm::vec3> points, const Relaxation& relaxation,
           const std::function<glm::vec3(glm::vec3)>& hold = {});

}  // namespace sigil::geometry::path
