/** @file
 * How a cell size is chosen, how the buckets are laid down, and how a
 * radius query walks them into an answer in index order.
 */

#include "sigilmotion/physics/Neighbourhood.h"

#include <algorithm>
#include <cmath>

namespace sigil::motion::physics {
namespace {

/** How many points a cell should hold on average. Small enough that a
 *  query does not sweep in a crowd, large enough that the offset table
 *  is not mostly empty. */
constexpr float kPointsPerCell = 2.0f;

/** The most cells a grid takes, as a multiple of the point count. A set
 *  spread thinly across a huge extent would otherwise want a table far
 *  larger than the points in it, and a table that does not fit is a
 *  worse answer than cells slightly too coarse. */
constexpr size_t kCellsPerPoint = 4;

}  // namespace

Neighbourhood::Neighbourhood(std::span<const Vec2> positions, float cell) {
  build(positions, cell);
}

void Neighbourhood::build(std::span<const Vec2> positions, float cell) {
  const size_t count = positions.size();
  if (count == 0) {
    m_columns = m_rows = 0;
    m_starts.assign(1, 0);
    m_ordered.clear();
    m_placed.clear();
    return;
  }

  // A NON-FINITE COORDINATE TAKES NO PART IN THE BOUNDS. One of them
  // would spread through the extent, and the cell count is a floor of a
  // division by it — undefined rather than merely wrong. Such a point
  // still gets a bucket: `cellOf` puts anything it cannot place in the
  // first cell, so every index the caller handed in is still answerable.
  Vec2 lo{}, hi{};
  bool anyFinite = false;
  for (const Vec2 at : positions) {
    if (!std::isfinite(at.x) || !std::isfinite(at.y)) continue;
    lo = anyFinite ? Vec2{std::min(lo.x, at.x), std::min(lo.y, at.y)} : at;
    hi = anyFinite ? Vec2{std::max(hi.x, at.x), std::max(hi.y, at.y)} : at;
    anyFinite = true;
  }
  const Vec2 extent = anyFinite ? hi - lo : Vec2{};

  // A cell size nobody asked for: the edge of the square that would hold
  // `kPointsPerCell` points if the set filled its own bounding box
  // evenly. An axis of no extent does not count toward the area — a set
  // strung out along a line is a one-dimensional problem and must not be
  // given a cell size derived from a box of no thickness.
  if (!(cell > 0.0f)) {
    float area = 1.0f;
    int axes = 0;
    if (extent.x > 0.0f) {
      area *= extent.x;
      ++axes;
    }
    if (extent.y > 0.0f) {
      area *= extent.y;
      ++axes;
    }
    const float perCell = area * kPointsPerCell / (float)count;
    cell = axes > 0 ? std::pow(perCell, 1.0f / (float)axes) : 1.0f;
  }
  if (!(cell > 0.0f) || !std::isfinite(cell)) cell = 1.0f;

  // The grid is bounded in cells, so a requested size may be coarsened
  // until the table fits. Each round doubles, which converges in a few
  // steps from any request.
  const size_t cap = count * kCellsPerPoint + 64;
  int columns = 1, rows = 1;
  for (int attempt = 0; attempt < 64; ++attempt) {
    const auto span = [cell](float reach) {
      const float spread = reach / cell;
      return std::isfinite(spread) ? std::max((int)std::floor(spread) + 1, 1)
                                   : 1;
    };
    columns = span(extent.x);
    rows = span(extent.y);
    if ((double)columns * (double)rows <= (double)cap) break;
    cell *= 2.0f;
  }

  m_cell = cell;
  m_inverseCell = 1.0f / cell;
  m_origin = anyFinite ? lo : Vec2{};
  m_columns = columns;
  m_rows = rows;

  const size_t cells = (size_t)columns * (size_t)rows;
  m_starts.assign(cells + 1, 0);
  for (const Vec2 at : positions) ++m_starts[bucketOf(at) + 1];
  for (size_t i = 1; i < m_starts.size(); ++i) m_starts[i] += m_starts[i - 1];

  // THE POSITIONS ARE STORED IN BUCKET ORDER BESIDE THE INDICES, so a
  // query reads one run of memory per cell instead of reaching back into
  // the caller's order for every candidate it tests. A cell's worth of
  // coordinates is then one cache line or two, which is what makes the
  // grid cheaper than the walk it replaces rather than merely shorter:
  // the walk reads its positions in order, and an index that gathered
  // would pay for every point it looked at what the walk pays for a
  // whole run of them.
  m_ordered.resize(count);
  m_placed.resize(count);
  m_cursor.assign(m_starts.begin(), m_starts.end() - 1);
  for (size_t i = 0; i < count; ++i) {
    const size_t slot = m_cursor[bucketOf(positions[i])]++;
    m_ordered[slot] = (uint32_t)i;
    m_placed[slot] = positions[i];
  }
}

size_t Neighbourhood::bucketOf(Vec2 at) const {
  int column = 0, row = 0;
  cellOf(at, column, row);
  return linear(column, row);
}

void Neighbourhood::cellOf(Vec2 at, int& column, int& row) const {
  const float across = (at.x - m_origin.x) * m_inverseCell;
  const float down = (at.y - m_origin.y) * m_inverseCell;
  column = std::clamp(std::isfinite(across) ? (int)std::floor(across) : 0, 0,
                      m_columns - 1);
  row = std::clamp(std::isfinite(down) ? (int)std::floor(down) : 0, 0,
                   m_rows - 1);
}

void Neighbourhood::within(Vec2 at, float radius,
                           std::vector<uint32_t>& out) const {
  out.clear();
  if (m_ordered.empty() || !(radius > 0.0f)) return;
  const float reachSquared = radius * radius;
  int leftColumn = 0, topRow = 0, rightColumn = 0, bottomRow = 0;
  cellOf({at.x - radius, at.y - radius}, leftColumn, topRow);
  cellOf({at.x + radius, at.y + radius}, rightColumn, bottomRow);
  // THE ANSWER IS AT MOST WHAT THE SWEPT CELLS HOLD, and each row of
  // them is one run of slots — the buckets of a row are laid down next
  // to each other — so the room the answer may need is known before a
  // single distance is measured. Taking it up front turns the test into
  // a store and an add with no branch on it, which is what lets a run of
  // candidates be measured at the rate the walk over every pair measures
  // its pairs.
  size_t room = 0;
  for (int row = topRow; row <= bottomRow; ++row) {
    const size_t band = linear(leftColumn, row);
    room += m_starts[band + (size_t)(rightColumn - leftColumn) + 1] -
            m_starts[band];
  }
  out.resize(room);
  size_t kept = 0;
  for (int row = topRow; row <= bottomRow; ++row) {
    const size_t band = linear(leftColumn, row);
    const size_t end = m_starts[band + (size_t)(rightColumn - leftColumn) + 1];
    for (size_t slot = m_starts[band]; slot < end; ++slot) {
      // The same subtraction and the same squared length a caller
      // comparing every pair writes, so the set that comes back is the
      // set that walk keeps rather than one rounded differently.
      const Vec2 offset = m_placed[slot] - at;
      out[kept] = m_ordered[slot];
      kept += offset.lengthSquared() <= reachSquared ? 1u : 0u;
    }
  }
  out.resize(kept);
  // A CELL'S RUN IS IN INDEX ORDER, because the scatter above walked the
  // caller's points in theirs, but a sweep collects several such runs and
  // their concatenation is not. Putting the answer in order is therefore
  // a sort of what came back — a handful, bounded by the radius — and
  // never a sort of the set.
  std::sort(out.begin(), out.end());
}

std::vector<uint32_t> Neighbourhood::within(Vec2 at, float radius) const {
  std::vector<uint32_t> out;
  within(at, radius, out);
  return out;
}

}  // namespace sigil::motion::physics
