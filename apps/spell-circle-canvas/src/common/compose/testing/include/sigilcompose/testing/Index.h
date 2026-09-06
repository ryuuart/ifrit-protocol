#pragma once

/** @file
 * The two spatial indexes the read-back checks read their answers out of.
 *
 * Both exist for the same reason: a check asks ONE question of a figure
 * hundreds of thousands of times, and asking it of the whole figure every
 * time is quadratic in the term a band grows in. A band is one
 * quadrilateral per sampled step, so a long one is thousands of edges,
 * and a lattice over it is a quarter of a million points — the product is
 * the whole cost of a verification pass, and neither factor is what the
 * check is about.
 *
 * `RowIndex` answers point-in-path for a lattice: each row of the lattice
 * is resolved once into the crossings the path makes with it, so a point
 * is a binary search of its own row instead of a walk of every verb. It
 * answers WHAT `SkPath::contains` ANSWERS, by counting crossings under
 * the rule Skia counts them by — and where a point lands ON an edge,
 * where that rule hands the decision to a tie-break this cannot see, it
 * asks the path itself.
 *
 * `CellIndex` answers which flattened edges a ray can meet: the edges are
 * bucketed into square cells and the ray walks the cells its line passes
 * through, so a cast tests the edges along its line instead of every edge
 * of the figure. The cull is exact — an edge the line crosses lies in a
 * cell the line passes through — so the crossings a cast reads are the
 * crossings it would have found the long way.
 *
 * Both are built per call by the check that uses them and thrown away
 * with it; neither is a paint-loop structure, and neither is thread-safe.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace sigil::compose::test {

/** A path's straight segments resolved into the crossings of each ROW of
 *  a sampling lattice, so that `contains()` for a point on a row is a
 *  search of that row's crossings rather than a walk of the whole path.
 *
 *  The row's crossings are where the path's segments cut the row's line,
 *  in order, each carrying the winding of everything up to and including
 *  itself. A point's fill is then the winding at the last crossing before
 *  it, which is one binary search — where asking the path is a walk of
 *  every verb, and a band drawn as one quadrilateral per sampled step has
 *  thousands.
 *
 *  The answer is `SkPath::contains`'s, always. A path carrying curves, an
 *  inverse fill or a coordinate that is not finite is not indexed at all
 *  and every point is asked of the path; and an indexed path is asked
 *  about any point lying within a whisker of one of its own crossings,
 *  because a point ON an edge is decided by counting on-curve hits and,
 *  at the last, by comparing tangents — none of which a crossing table
 *  can see — as is any row a vertex of the path lies on. */
class RowIndex {
 public:
  RowIndex() = default;

  /** Indexes @p path for a lattice of @p rows rows across @p region: row
   *  r is the line y = region.top() + (r + ½)·h, which is the y a caller
   *  sampling that lattice takes its points from. */
  RowIndex(const SkPath& path, const SkRect& region, int rows)
      : m_path(path), m_bounds(path.getBounds()) {
    m_evenOdd = path.getFillType() == SkPathFillType::kEvenOdd;
    if (rows < 1 || region.height() <= 0 || path.isInverseFillType()) return;
    m_top = region.top();
    m_height = region.height() / (float)rows;
    m_rows = rows;
    std::vector<Segment> segments;
    if (!gather(path, segments)) return;
    file(segments);
    m_indexed = true;
  }

  /** True when a point may be asked of the index rather than the path.
   *  False is not an error: it means every point is asked of the path,
   *  which is what the check did before there was an index. */
  bool indexed() const { return m_indexed; }

  /** `SkPath::contains(x, y)` for a point on row @p row, where @p y is the
   *  row's own line — the y the caller sampled the lattice at. */
  bool contains(float x, float y, int row) const {
    if (!m_indexed || row < 0 || row >= m_rows) return m_path.contains(x, y);
    // The gate SkPath::contains opens with, so a point outside the bounds
    // is the same cheap `false` here that it is there.
    if (!m_bounds.contains(x, y)) return false;
    if (m_rowAsks[(size_t)row]) return m_path.contains(x, y);
    const uint32_t lo = m_rowStart[(size_t)row],
                   hi = m_rowStart[(size_t)row + 1];
    const float* first = m_crossAt.data() + lo;
    const float* last = m_crossAt.data() + hi;
    const float* at = std::lower_bound(first, last, x);
    // ON a crossing is the path's question, not this table's: the width
    // of "on" is the room the two spellings of the same crossing —
    // Skia's cross product and this table's intercept — have to differ
    // in, which is a few of the last bits of the coordinate.
    const float whisker = 16.0f * std::numeric_limits<float>::epsilon() *
                          std::max(1.0f, std::abs(x));
    if ((at != last && *at - x <= whisker) ||
        (at != first && x - *(at - 1) <= whisker))
      return m_path.contains(x, y);
    int winding =
        at == first ? 0 : m_crossWinding[lo + (size_t)(at - first) - 1];
    if (m_evenOdd) winding &= 1;
    return winding != 0;
  }

  const SkRect& bounds() const { return m_bounds; }

 private:
  struct Segment {
    SkPoint a, b;
  };

  bool gather(const SkPath& path, std::vector<Segment>& out) const {
    // Iterated the way SkPath::contains iterates it — force-closed, so the
    // segment closing an open contour is the one Skia also counts.
    SkPath::Iter iter(path, true);
    SkPoint pts[4];
    for (SkPath::Verb verb = iter.next(pts); verb != SkPath::kDone_Verb;
         verb = iter.next(pts)) {
      switch (verb) {
        case SkPath::kLine_Verb:
          if (!std::isfinite(pts[0].fX) || !std::isfinite(pts[0].fY) ||
              !std::isfinite(pts[1].fX) || !std::isfinite(pts[1].fY))
            return false;
          out.push_back({pts[0], pts[1]});
          break;
        case SkPath::kMove_Verb:
        case SkPath::kClose_Verb:
          break;
        default:
          return false;  // a curve — the winding of one is Skia's to count
      }
    }
    return true;
  }

  float rowY(int row) const { return m_top + ((float)row + 0.5f) * m_height; }

  /** The rows a segment can cross, with a row of slack at each end so
   *  that the exact test below, and not this arithmetic, decides. */
  void slack(const Segment& s, int& lo, int& hi) const {
    const float ylo = std::min(s.a.fY, s.b.fY);
    const float yhi = std::max(s.a.fY, s.b.fY);
    lo = std::max((int)std::floor((ylo - m_top) / m_height - 0.5f) - 1, 0);
    hi = std::min((int)std::ceil((yhi - m_top) / m_height - 0.5f) + 1,
                  m_rows - 1);
  }

  /** Where segment @p s cuts row @p row and which way, or false where it
   *  does not cut it at all.
   *
   *  Half-open at the segment's lower end, and horizontal segments left
   *  out: exactly the rule `SkPath::contains` counts by, so the winding
   *  built out of these crossings is the winding it would have summed. */
  bool cut(const Segment& s, float y, float& at, int& dir) const {
    const float y0 = s.a.fY, y1 = s.b.fY;
    if (y0 == y1) return false;
    float ylo = y0, yhi = y1;
    dir = 1;
    if (y0 > y1) {
      std::swap(ylo, yhi);
      dir = -1;
    }
    if (y < ylo || y >= yhi) return false;
    at = s.a.fX + (s.b.fX - s.a.fX) * ((y - y0) / (y1 - y0));
    return true;
  }

  void file(const std::vector<Segment>& segments) {
    m_rowAsks.assign((size_t)m_rows, 0);
    std::vector<uint32_t> counts((size_t)m_rows, 0);
    for (const Segment& s : segments) {
      int lo = 0, hi = 0;
      slack(s, lo, hi);
      for (int r = lo; r <= hi; ++r) {
        float at = 0;
        int dir = 0;
        if (cut(s, rowY(r), at, dir)) ++counts[(size_t)r];
        // A VERTEX on this row's line — which a horizontal segment lying
        // along it is two of. Skia counts an on-curve hit there and
        // decides by a rule no crossing table can see, so the row is the
        // path's to answer, all of it.
        if (rowY(r) == s.a.fY || rowY(r) == s.b.fY) m_rowAsks[(size_t)r] = 1;
      }
    }
    m_rowStart.assign((size_t)m_rows + 1, 0);
    for (int r = 0; r < m_rows; ++r)
      m_rowStart[(size_t)r + 1] = m_rowStart[(size_t)r] + counts[(size_t)r];
    std::vector<std::pair<float, int>> crossings(m_rowStart.back());
    std::vector<uint32_t> fill(m_rowStart.begin(), m_rowStart.end() - 1);
    for (const Segment& s : segments) {
      int lo = 0, hi = 0;
      slack(s, lo, hi);
      for (int r = lo; r <= hi; ++r) {
        float at = 0;
        int dir = 0;
        if (cut(s, rowY(r), at, dir)) crossings[fill[(size_t)r]++] = {at, dir};
      }
    }
    m_crossAt.assign(crossings.size(), 0.0f);
    m_crossWinding.assign(crossings.size(), 0);
    for (int r = 0; r < m_rows; ++r) {
      const uint32_t lo = m_rowStart[(size_t)r], hi = m_rowStart[(size_t)r + 1];
      std::sort(crossings.begin() + lo, crossings.begin() + hi,
                [](const auto& a, const auto& b) { return a.first < b.first; });
      int winding = 0;
      for (uint32_t i = lo; i < hi; ++i) {
        winding += crossings[i].second;
        m_crossAt[i] = crossings[i].first;
        m_crossWinding[i] = winding;  // the winding to the RIGHT of it
      }
    }
  }

  SkPath m_path;
  SkRect m_bounds{SkRect::MakeEmpty()};
  std::vector<uint32_t> m_rowStart;     ///< rows + 1 offsets into the crossings
  std::vector<float> m_crossAt;         ///< where, sorted within each row
  std::vector<int32_t> m_crossWinding;  ///< the winding just past each
  /** Rows a horizontal segment lies on, which the path answers whole. */
  std::vector<uint8_t> m_rowAsks;
  float m_top = 0;
  float m_height = 1;
  int m_rows = 0;
  bool m_evenOdd = false;
  bool m_indexed = false;
};

/** Flattened edges — a run of point PAIRS — bucketed into square cells, so
 *  a ray consults the edges along its own line instead of every edge of
 *  the figure.
 *
 *  The cull is exact: an edge the line crosses lies in a cell the line
 *  passes through, and the walk visits every one of them. Edges the line
 *  misses may be returned too — a cell is not a line — which costs a
 *  crossing test that finds nothing and changes no answer. */
class CellIndex {
 public:
  CellIndex() = default;

  /** @p edges is one point pair per edge, as a contour walk lays them
   *  down. Cells are sized so that a few edges land in each. */
  explicit CellIndex(std::span<const SkPoint> edges) {
    m_count = (uint32_t)(edges.size() / 2);
    if (m_count == 0) return;
    SkRect box =
        SkRect::MakeLTRB(edges[0].fX, edges[0].fY, edges[0].fX, edges[0].fY);
    for (const SkPoint& p : edges) {
      if (!std::isfinite(p.fX) || !std::isfinite(p.fY)) return;
      box.fLeft = std::min(box.fLeft, p.fX);
      box.fTop = std::min(box.fTop, p.fY);
      box.fRight = std::max(box.fRight, p.fX);
      box.fBottom = std::max(box.fBottom, p.fY);
    }
    box.outset(1.0f, 1.0f);
    m_box = box;
    // Four edges to a cell on average: fewer and the walk pays for cells
    // it finds empty, more and it pays for edges nowhere near the ray.
    const float area = std::max(box.width() * box.height(), 1.0f);
    m_cell = std::max(std::sqrt(area * 4.0f / (float)m_count), 1e-3f);
    m_nx = std::max(1, (int)std::ceil(box.width() / m_cell));
    m_ny = std::max(1, (int)std::ceil(box.height() / m_cell));
    file(edges);
    m_stamp.assign(m_count, 0);
    m_indexed = true;
  }

  bool indexed() const { return m_indexed; }
  uint32_t edgeCount() const { return m_count; }

  /** Every edge the LINE through @p p along @p u can meet, as edge indices
   *  into @p out.
   *
   *  The whole line and not a span of it: a cast reads its crossings the
   *  way a rasterizer does, accumulating the fill rule from beyond the
   *  figure inward, so it needs every crossing the line makes and not
   *  only the near ones. What the walk leaves out is the edges the line
   *  passes nowhere near, which is nearly all of them. */
  void across(SkPoint p, SkVector u, std::vector<uint32_t>& out) const {
    out.clear();
    if (!m_indexed) {
      out.resize(m_count);
      for (uint32_t i = 0; i < m_count; ++i) out[i] = i;
      return;
    }
    // Long enough to leave the box from anywhere in or near it; the clip
    // brings it back to the part that can hold an edge.
    const float span = 2.0f * (m_box.width() + m_box.height()) +
                       std::abs(p.fX - m_box.centerX()) +
                       std::abs(p.fY - m_box.centerY());
    float x0 = p.fX - u.fX * span, y0 = p.fY - u.fY * span;
    float x1 = p.fX + u.fX * span, y1 = p.fY + u.fY * span;
    if (!clip(x0, y0, x1, y1)) return;
    ++m_visit;
    if (m_visit == 0) {  // the stamp wrapped: no edge may look visited
      std::fill(m_stamp.begin(), m_stamp.end(), 0);
      m_visit = 1;
    }
    const auto take = [&](int cx, int cy) {
      const size_t c = (size_t)cy * (size_t)m_nx + (size_t)cx;
      for (uint32_t i = m_cellStart[c]; i < m_cellStart[c + 1]; ++i) {
        const uint32_t edge = m_cellFile[i];
        if (m_stamp[edge] == m_visit) continue;
        m_stamp[edge] = m_visit;
        out.push_back(edge);
      }
    };
    // The cells the clipped span passes through, in order.
    int cx = cellX(x0), cy = cellY(y0);
    const int cxEnd = cellX(x1), cyEnd = cellY(y1);
    const float dx = x1 - x0, dy = y1 - y0;
    const int stepX = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
    const int stepY = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
    const float big = std::numeric_limits<float>::infinity();
    float tx = big, ty = big, dtx = big, dty = big;
    if (stepX != 0) {
      const float edge =
          m_box.fLeft + (float)(cx + (stepX > 0 ? 1 : 0)) * m_cell;
      tx = (edge - x0) / dx;
      dtx = m_cell / std::abs(dx);
    }
    if (stepY != 0) {
      const float edge =
          m_box.fTop + (float)(cy + (stepY > 0 ? 1 : 0)) * m_cell;
      ty = (edge - y0) / dy;
      dty = m_cell / std::abs(dy);
    }
    const int most = m_nx + m_ny + 2;
    for (int guard = 0; guard <= most; ++guard) {
      take(cx, cy);
      if (cx == cxEnd && cy == cyEnd) break;
      if (tx < ty) {
        cx += stepX;
        tx += dtx;
        if (cx < 0 || cx >= m_nx) break;
      } else {
        cy += stepY;
        ty += dty;
        if (cy < 0 || cy >= m_ny) break;
      }
    }
  }

 private:
  int cellX(float x) const {
    return std::clamp((int)std::floor((x - m_box.fLeft) / m_cell), 0, m_nx - 1);
  }
  int cellY(float y) const {
    return std::clamp((int)std::floor((y - m_box.fTop) / m_cell), 0, m_ny - 1);
  }

  /** The part of the span inside the indexed box, or false when none of
   *  it is. */
  bool clip(float& x0, float& y0, float& x1, float& y1) const {
    float t0 = 0.0f, t1 = 1.0f;
    const float dx = x1 - x0, dy = y1 - y0;
    const float p[4] = {-dx, dx, -dy, dy};
    const float q[4] = {x0 - m_box.fLeft, m_box.fRight - x0, y0 - m_box.fTop,
                        m_box.fBottom - y0};
    for (int i = 0; i < 4; ++i) {
      if (p[i] == 0.0f) {
        if (q[i] < 0.0f) return false;
        continue;
      }
      const float t = q[i] / p[i];
      if (p[i] < 0.0f)
        t0 = std::max(t0, t);
      else
        t1 = std::min(t1, t);
    }
    if (t0 > t1) return false;
    const float ax = x0 + t0 * dx, ay = y0 + t0 * dy;
    const float bx = x0 + t1 * dx, by = y0 + t1 * dy;
    x0 = ax;
    y0 = ay;
    x1 = bx;
    y1 = by;
    return true;
  }

  void file(std::span<const SkPoint> edges) {
    const size_t cells = (size_t)m_nx * (size_t)m_ny;
    std::vector<uint32_t> counts(cells, 0);
    const auto box = [&](uint32_t e, int& lox, int& loy, int& hix, int& hiy) {
      const SkPoint a = edges[(size_t)e * 2], b = edges[(size_t)e * 2 + 1];
      lox = cellX(std::min(a.fX, b.fX));
      hix = cellX(std::max(a.fX, b.fX));
      loy = cellY(std::min(a.fY, b.fY));
      hiy = cellY(std::max(a.fY, b.fY));
    };
    for (uint32_t e = 0; e < m_count; ++e) {
      int lox, loy, hix, hiy;
      box(e, lox, loy, hix, hiy);
      for (int y = loy; y <= hiy; ++y)
        for (int x = lox; x <= hix; ++x) ++counts[(size_t)y * m_nx + x];
    }
    m_cellStart.assign(cells + 1, 0);
    for (size_t c = 0; c < cells; ++c)
      m_cellStart[c + 1] = m_cellStart[c] + counts[c];
    m_cellFile.assign(m_cellStart.back(), 0);
    std::vector<uint32_t> at(m_cellStart.begin(), m_cellStart.end() - 1);
    for (uint32_t e = 0; e < m_count; ++e) {
      int lox, loy, hix, hiy;
      box(e, lox, loy, hix, hiy);
      for (int y = loy; y <= hiy; ++y)
        for (int x = lox; x <= hix; ++x)
          m_cellFile[at[(size_t)y * m_nx + x]++] = e;
    }
  }

  SkRect m_box{SkRect::MakeEmpty()};
  std::vector<uint32_t> m_cellStart;  ///< nx·ny + 1 offsets into m_cellFile
  std::vector<uint32_t> m_cellFile;   ///< edge indices, cell by cell
  /** Which walk last returned each edge, so a cast that meets a cell it
   *  has already taken edges from does not return them twice. */
  mutable std::vector<uint32_t> m_stamp;
  mutable uint32_t m_visit = 0;
  float m_cell = 1;
  int m_nx = 0;
  int m_ny = 0;
  uint32_t m_count = 0;
  bool m_indexed = false;
};

}  // namespace sigil::compose::test
