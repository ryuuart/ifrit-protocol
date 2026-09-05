#pragma once

/** @file
 * The automatic table: a layout ALGORITHM over the LayoutScheme seam,
 * not a placement function.
 *
 * Every other scheme answers where child i falls from the container size
 * and a formula — a ring, a modular grid, a baseline rhythm — and could
 * have been written out by the author who used it. This one measures its
 * children twice, solves column widths against the content in them,
 * shares a surplus, and hands the resolved grid back so a caller can
 * print what it arrived at. That is the peer of the flex pass, so it
 * stands beside the seam rather than on the shelf of stock placements.
 */

#include <sigilcompose/core/Layout.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include <include/core/SkRect.h>
#include <include/core/SkSize.h>

namespace sigil::compose {

/** THE AUTO TABLE: unequal columns sized by what is in them, spans, and a
 *  surplus shared out in proportion — the layout every HTML table has run
 *  since tables were how a page was set.
 *
 *  Not a modular grid with a different name. `ModularGrid` divides the
 *  container into equal modules and puts children in them; a table does
 *  the opposite — the CONTENT decides the columns, and only what is left
 *  over is divided. That is why nothing here goes through
 *  `geometry::arrange`: a module is one size repeated, and no column of a
 *  table is the same width as the next.
 *
 *  Each child says which cells it takes with `Element::cells` and where it
 *  sits in them with `Element::cellAlign`. A child that says nothing flows
 *  into the next free cell, left to right and then down.
 *
 *      layout(layouts::Table{.width = 500, .spacing = 2, .padding = 1})
 *          .child(masthead().cells(0, 0, 5, 1).cellAlign(Align::End,
 *                                                        Align::Start))
 *          .child(panel().cells(1, 0, 1, 2))
 *
 *  THE ONE ASYMMETRY, and it is the browsers' and not a slip. A COLUMN's
 *  surplus is shared out in proportion to the widths already found, which
 *  is what puts every column of a real page on a fractional pixel. A
 *  ROWSPAN's height deficit is NOT: the whole of it lands on the LAST row
 *  the child spans, leaving the rows above at their own content height.
 *  Sharing it in proportion instead inflates the first row of every span
 *  and drags everything under it down the page. */
struct Table {
  /** 0 derives the count from the cells the children claimed. */
  int columns = 0;
  int rows = 0;
  /** The width the columns are solved to; 0 takes the container's. */
  float width = 0.0f;
  float spacing = 0.0f;  ///< between cells — a table's cellspacing
  float padding = 0.0f;  ///< inside one — its cellpadding

  /** THE RESOLVED GRID: the column widths and row heights the algorithm
   *  arrived at, and the content-box origin of each.
   *
   *  Exposed because a study that reproduces a printed or published table
   *  has to be able to print what it resolved and diff it against what the
   *  original measured. Reading the numbers back off the placed rects
   *  cannot do it: a cell's rect is the CHILD, aligned inside its box, so
   *  a column nothing fills leaves no trace at all. */
  struct Grid {
    std::vector<float> columnWidths, rowHeights;
    std::vector<float> columnX, rowY;
  };

  /** Where every child lands, in child order. */
  std::vector<SkRect> place(const LayoutInput& in) const {
    const std::vector<CellSpan> spans = flowed(in);
    const Grid grid = solve(in);
    std::vector<SkRect> rects(in.childSizes.size());
    for (size_t i = 0; i < spans.size(); ++i) {
      const CellSpan& s = spans[i];
      if ((size_t)s.column >= grid.columnX.size() ||
          (size_t)s.row >= grid.rowY.size())
        continue;  // outside the grid it was given; placed nowhere
      const SkSize box{extent(grid.columnWidths, s.column, s.columns),
                       extent(grid.rowHeights, s.row, s.rows)};
      const SkPoint at{grid.columnX[(size_t)s.column],
                       grid.rowY[(size_t)s.row]};
      const SkSize size{
          s.across == Align::Stretch ? box.width() : in.childSizes[i].width(),
          s.down == Align::Stretch ? box.height() : in.childSizes[i].height()};
      rects[i] = SkRect::MakeXYWH(at.fX + slack(s.across, box.width(),
                                                size.width()),
                                  at.fY + slack(s.down, box.height(),
                                                size.height()),
                                  size.width(), size.height());
    }
    return rects;
  }

  /** The column widths, row heights and origins, without placing anything.
   *  `place()` calls it; a caller that wants to REPORT the grid calls it
   *  itself, and gets exactly the numbers the placement used. */
  Grid solve(const LayoutInput& in) const {
    const std::vector<CellSpan> spans = flowed(in);
    const int cols = std::max(columnCount(spans), 1);
    const int lines = std::max(rowCount(spans), 1);
    const float pitch = 2 * padding + spacing;

    Grid grid;
    grid.columnWidths.assign((size_t)cols, 0.0f);
    grid.rowHeights.assign((size_t)lines, 0.0f);

    // 1. Every column is at least as wide as the widest thing that sits
    //    in it alone. A spanning child says nothing here — its width is
    //    a claim about several columns together, not about any one.
    for (size_t i = 0; i < spans.size(); ++i)
      if (spans[i].columns == 1 && (size_t)spans[i].column < grid.columnWidths.size())
        grid.columnWidths[(size_t)spans[i].column] = std::max(
            grid.columnWidths[(size_t)spans[i].column], in.childSizes[i].width());

    // 2. Then the spanning children top their columns up, narrowest span
    //    first, so a wide span sees what the narrow ones already asked
    //    for instead of paying for them twice.
    for (int k = 2; k <= cols; ++k)
      for (size_t i = 0; i < spans.size(); ++i) {
        if (spans[i].columns != k) continue;
        const float have = extent(grid.columnWidths, spans[i].column, k);
        const float deficit = in.childSizes[i].width() - have;
        if (deficit <= 0) continue;
        const float share = have - (float)(k - 1) * pitch;
        for (int j = 0; j < k && (size_t)(spans[i].column + j) < grid.columnWidths.size();
             ++j) {
          float& w = grid.columnWidths[(size_t)(spans[i].column + j)];
          w += share > 0 ? deficit * w / share : deficit / (float)k;
        }
      }

    // 3. What the table is wider than its content, shared out in
    //    proportion — the step that leaves every column on a fraction.
    float content = 0;
    for (float w : grid.columnWidths) content += w;
    const float table = width > 0 ? width : in.container.width();
    const float surplus =
        table - (content + (float)cols * 2 * padding + (float)(cols + 1) * spacing);
    if (surplus > 0 && content > 0)
      for (float& w : grid.columnWidths) w += surplus * w / content;

    // 4. Rows, by the same first step…
    for (size_t i = 0; i < spans.size(); ++i)
      if (spans[i].rows == 1 && (size_t)spans[i].row < grid.rowHeights.size())
        grid.rowHeights[(size_t)spans[i].row] = std::max(
            grid.rowHeights[(size_t)spans[i].row], in.childSizes[i].height());
    // …and deliberately NOT the same second one: the whole of a rowspan's
    //    deficit lands on the last row it covers.
    for (int k = 2; k <= lines; ++k)
      for (size_t i = 0; i < spans.size(); ++i) {
        if (spans[i].rows != k) continue;
        const float deficit =
            in.childSizes[i].height() - extent(grid.rowHeights, spans[i].row, k);
        const size_t last = (size_t)(spans[i].row + k - 1);
        if (deficit > 0 && last < grid.rowHeights.size())
          grid.rowHeights[last] += deficit;
      }

    grid.columnX = origins(grid.columnWidths, pitch);
    grid.rowY = origins(grid.rowHeights, pitch);
    return grid;
  }

 private:
  /** Every child's cells, with the ones that said nothing flowed into the
   *  free cells left over — left to right, then down. */
  std::vector<CellSpan> flowed(const LayoutInput& in) const {
    std::vector<CellSpan> out(in.childSizes.size());
    for (size_t i = 0; i < out.size(); ++i)
      if (i < in.childCells.size()) out[i] = in.childCells[i];
    const int cols = std::max(columnCount(out), 1);
    std::vector<char> taken;
    const auto claim = [&](int column, int row, int wide, int tall) {
      for (int r = row; r < row + tall; ++r)
        for (int c = column; c < column + wide; ++c) {
          if (c < 0 || c >= cols || r < 0) continue;
          const size_t at = (size_t)r * (size_t)cols + (size_t)c;
          if (taken.size() <= at) taken.resize(at + 1, 0);
          taken[at] = 1;
        }
    };
    for (const CellSpan& s : out)
      if (s.declared) claim(s.column, s.row, s.columns, s.rows);
    size_t next = 0;
    for (CellSpan& s : out) {
      if (s.declared) continue;
      // The next cell NO declared child claimed, so an explicit span and a
      // flowing child cannot land on each other — the failure a scheme
      // that counts flow from zero has, where child four of eight lands
      // back on top of the one placed at (0,0).
      while (next < taken.size() && taken[next]) ++next;
      s.column = (int)(next % (size_t)cols);
      s.row = (int)(next / (size_t)cols);
      claim(s.column, s.row, 1, 1);
      ++next;
    }
    return out;
  }

  int columnCount(const std::vector<CellSpan>& spans) const {
    if (columns > 0) return columns;
    int most = 1;
    for (const CellSpan& s : spans)
      if (s.declared) most = std::max(most, s.column + s.columns);
    return most;
  }
  int rowCount(const std::vector<CellSpan>& spans) const {
    if (rows > 0) return rows;
    int most = 1;
    for (const CellSpan& s : spans) most = std::max(most, s.row + s.rows);
    return most;
  }

  /** How far `count` tracks from `first` reach, cell gaps included. */
  float extent(const std::vector<float>& tracks, int first, int count) const {
    float total = (float)(count - 1) * (2 * padding + spacing);
    for (int j = 0; j < count; ++j) {
      const size_t at = (size_t)(first + j);
      if (first + j >= 0 && at < tracks.size()) total += tracks[at];
    }
    return total;
  }
  /** The content-box origin of each track. */
  std::vector<float> origins(const std::vector<float>& tracks,
                             float pitch) const {
    std::vector<float> at(tracks.size(), 0.0f);
    if (!at.empty()) at[0] = spacing + padding;
    for (size_t i = 1; i < at.size(); ++i)
      at[i] = at[i - 1] + tracks[i - 1] + pitch;
    return at;
  }
  /** Where a `size` sits in a `box` under one alignment. */
  static float slack(Align how, float box, float size) {
    switch (how) {
      case Align::Center:
        return (box - size) * 0.5f;
      case Align::End:
        return box - size;
      default:
        return 0.0f;
    }
  }
};

}  // namespace sigil::compose
