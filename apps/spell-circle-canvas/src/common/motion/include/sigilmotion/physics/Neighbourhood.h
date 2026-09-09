#pragma once

/** @file
 * WHAT IS NEAR WHAT: a uniform grid over the positions of a point set,
 * built once and asked once per point.
 *
 * Everything in a simulation that reads more than one point at a time
 * asks the same question — which points are within a radius of this one
 * — and answering it by walking the whole set is the shape a flock, a
 * collision pass, a packing and a density gather all degrade into. One
 * grid answers it in the time the ANSWER takes rather than the time the
 * set does, and the cost of a query stops depending on how many points
 * there are and starts depending on how many are NEAR.
 *
 * The same grid stands in three dimensions in the geometry library's
 * path leaf, and it is the origin of this question in this tree. It is
 * not what stands here, and the reason is a link edge rather than a
 * disagreement: that leaf publishes Skia, glm and a Boost map, while the
 * physics feature's boundary is that a consumer which also draws links
 * it without inheriting a drawing library. What crosses instead is the
 * shape — two counting passes into one bucket array, a cell size chosen
 * so the average cell holds a couple of points, and a cell count bounded
 * by the point count so a thin set spread across a huge extent cannot
 * ask for a table larger than itself.
 */

#include <sigilmotion/physics/Points.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sigil::motion::physics {

/** A UNIFORM GRID OVER POSITIONS, answering which of them are within a
 *  radius of a place.
 *
 *  IT IS A SNAPSHOT, NOT A VIEW: the positions are copied in, so a
 *  caller may move its own points while the index still answers about
 *  where they were, and a moved set is a rebuild rather than an update.
 *  That is what makes a pass over a set consistent with itself — every
 *  point of one step sees the same arrangement, whatever order the pass
 *  reaches them in.
 *
 *  THE ANSWER COMES BACK IN INDEX ORDER. A force sums over the
 *  neighbours it finds and a sum of floats is not associative, so an
 *  answer arriving in whatever order the buckets happen to hold it would
 *  give a different number from the walk over every pair that finds
 *  exactly the same neighbours — and a different number again on any day
 *  the cell size moved. Index order is the one order every way of
 *  finding neighbours agrees on, and it is what makes this index
 *  swappable for the walk it replaces rather than a second answer.
 *
 *  THE CELL SIZE IS THE ONE DIAL. Left at zero it is chosen so the
 *  average cell holds a couple of points, which is what makes a query
 *  touch a bounded number of cells whatever the count. Given explicitly
 *  it should be about the radius that will be asked for: cells much
 *  smaller than the radius are many to walk, and cells much larger sweep
 *  in points that were never close. The grid is bounded in cells, so a
 *  size given here is a request the index may coarsen; `cell()` answers
 *  what it used.
 *
 *  An empty index answers nothing to every query, and is what a
 *  default-constructed one is. Rebuilding through `build` keeps the
 *  buffers, so a stepper that indexes every step allocates once. */
class Neighbourhood {
 public:
  Neighbourhood() = default;

  /** The index over @p positions, at @p cell (zero chooses one). */
  explicit Neighbourhood(std::span<const Vec2> positions, float cell = 0.0f);

  /** The same index built again over @p positions, reusing whatever this
   *  one already holds. Every answer it gave before is void. */
  void build(std::span<const Vec2> positions, float cell = 0.0f);

  [[nodiscard]] size_t size() const { return m_ordered.size(); }
  [[nodiscard]] bool empty() const { return m_ordered.empty(); }
  /** The cell edge the grid was actually built at. */
  [[nodiscard]] float cell() const { return m_cell; }
  /** How many cells the grid spans, across and down. */
  [[nodiscard]] int columns() const { return m_columns; }
  [[nodiscard]] int rows() const { return m_rows; }

  /** THE INDICES WITHIN @p radius OF @p at, LOWEST FIRST, written over
   *  @p out. The vector is the caller's so a loop of queries allocates
   *  once. A radius of zero or less answers nothing.
   *
   *  A point sitting exactly on @p at is inside every radius, so a
   *  caller asking about a point OF the set gets that point back and
   *  drops it by index — which is what a pair walk's `j == i` does, and
   *  is left to the caller because a coincident pair means different
   *  things to a flock and to a collision. */
  void within(Vec2 at, float radius, std::vector<uint32_t>& out) const;
  /** The same, allocating its own answer. */
  [[nodiscard]] std::vector<uint32_t> within(Vec2 at, float radius) const;

 private:
  /** Which column and row a position falls in, clamped to the grid, and
   *  which bucket that is. */
  void cellOf(Vec2 at, int& column, int& row) const;
  [[nodiscard]] size_t bucketOf(Vec2 at) const;
  [[nodiscard]] size_t linear(int column, int row) const {
    return (size_t)row * (size_t)m_columns + (size_t)column;
  }

  /** The indices, grouped by cell: cell `c` owns
   *  `[m_starts[c], m_starts[c + 1])`. One array and one offset table
   *  rather than a vector per cell, so building is two counting passes
   *  and a query is a contiguous read. */
  std::vector<uint32_t> m_ordered;
  /** The positions in the same order, so a query tests a run of
   *  coordinates that sit together rather than reaching back into the
   *  caller's order for each one. This is the snapshot. */
  std::vector<Vec2> m_placed;
  std::vector<uint32_t> m_starts;
  /** Scratch for the second counting pass, kept so a rebuild allocates
   *  nothing. */
  std::vector<uint32_t> m_cursor;
  Vec2 m_origin{};
  int m_columns = 0, m_rows = 0;
  float m_cell = 1.0f;
  float m_inverseCell = 1.0f;
};

}  // namespace sigil::motion::physics
