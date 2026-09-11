#pragma once

/** @file
 * Automatic table layout with intrinsic column widths, cell spans and
 * proportional surplus distribution.
 */

#include <include/core/SkRect.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Layout.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sigil::compose {

/** THE AUTO TABLE: unequal columns sized by what is in them, spans, and a
 *  surplus shared out in proportion — the layout every HTML table has run
 *  since tables were how a page was set.
 *
 *  Each child says which cells it takes with `Element::cells` and where it
 *  sits in them with `Element::cellAlign`. A child that says nothing flows
 *  into the next free cell, left to right and then down.
 *
 *      layout(Table{.width = 500, .spacing = 2, .padding = 1})
 *          .child(masthead().cells(0, 0, 5, 1).cellAlign(Align::End,
 *                                                        Align::Start))
 *          .child(panel().cells(1, 0, 1, 2))
 *
 *  A COLUMN IS SOLVED BETWEEN TWO WIDTHS, not from one. What its content
 *  WANTS is the widest thing in it; what its content NEEDS is the
 *  narrowest that thing goes without spilling — a paragraph's longest
 *  unbreakable run, and the measured size of everything else. Given more
 *  room than the columns want, the surplus is shared out in proportion;
 *  given less, every column gives up the same fraction of the distance
 *  between what it wants and what it needs, so a column with nothing to
 *  give up gives nothing. Narrower still and the columns stand at what
 *  they need and the table overflows, which is what a browser does rather
 *  than dropping content.
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

  /** THE WIDTH THE MARKUP GAVE A COLUMN, where it gave one, as ONE length
   *  per column: `Dim` in pixels (`120`, `120_px`), in percent
   *  (`30_pct`), or `autoDim()` for a column sized by what is in it. A
   *  width of no pixels is that same auto column, and a list shorter than
   *  the grid leaves the columns past its end sized that way too.
   *
   *  A PERCENTAGE IS A SHARE OF THE ROOM THE COLUMNS DIVIDE — the table's
   *  own width less its padding and spacing — so it can only be resolved
   *  once the table's width is known, which is where the auto rule takes
   *  it. The columns the markup left alone then divide what is left by
   *  that rule.
   *
   *  A declared column is FIXED however it was stated: it takes no share
   *  of a surplus and gives none up under a deficit, so a rail stated in
   *  the markup is the width it was stated at whatever else the page
   *  does. What is in it can still widen it — no column is narrower than
   *  the narrowest thing in it, whatever the markup asked for, and a
   *  percentage the content will not fit into is widened by the content
   *  exactly as a stated pixel width is. */
  std::vector<Dim> declaredWidths;

  /** WHAT A TABLE DOES WITH ROOM IT DOES NOT NEED. `Fill` takes the width
   *  it was given and shares the surplus across the columns, which is a
   *  table whose markup states a width. `Shrink` stops at what the content
   *  wants, which is a table whose markup states none: as wide as what is
   *  in it and no wider. Neither changes what happens when the room is too
   *  small — the columns fall toward what they need either way. */
  enum class Fit : uint8_t { Fill, Shrink };
  Fit fit = Fit::Fill;

  /** This scheme reads `LayoutInput::childMinSizes`: the narrowest a
   *  column's content can be set is the floor it is solved from, and no
   *  measured size carries it. */
  static constexpr bool readsChildMinSizes = true;

  bool operator==(const Table&) const = default;

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
      rects[i] =
          SkRect::MakeXYWH(at.fX + slack(s.across, box.width(), size.width()),
                           at.fY + slack(s.down, box.height(), size.height()),
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
    // What each column NEEDS, held beside what it wants. The two differ
    // only where a child can be set narrower than it was measured.
    std::vector<float> least((size_t)cols, 0.0f);
    auto narrowest = [&](size_t i) {
      return i < in.childMinSizes.size() ? in.childMinSizes[i].width()
                                         : in.childSizes[i].width();
    };

    // 1. Every column is at least as wide as the widest thing that sits
    //    in it alone. A spanning child says nothing here — its width is
    //    a claim about several columns together, not about any one.
    for (size_t i = 0; i < spans.size(); ++i) {
      const size_t c = (size_t)spans[i].column;
      if (spans[i].columns != 1 || spans[i].column < 0 ||
          c >= grid.columnWidths.size())
        continue;
      grid.columnWidths[c] =
          std::max(grid.columnWidths[c], in.childSizes[i].width());
      least[c] = std::max(least[c], narrowest(i));
    }

    // 2. Then the spanning children top their columns up, narrowest span
    //    first, so a wide span sees what the narrow ones already asked
    //    for instead of paying for them twice. Both widths are topped up:
    //    a span that cannot be set narrower than its columns hold raises
    //    what they need as well as what they want.
    auto topUp = [&](std::vector<float>& tracks, const CellSpan& s,
                     float want) {
      const float have = extent(tracks, s.column, s.columns);
      const float deficit = want - have;
      if (deficit <= 0) return;
      const float share = have - (float)(s.columns - 1) * pitch;
      for (int j = 0; j < s.columns && (size_t)(s.column + j) < tracks.size();
           ++j) {
        float& w = tracks[(size_t)(s.column + j)];
        w += share > 0 ? deficit * w / share : deficit / (float)s.columns;
      }
    };
    for (int k = 2; k <= cols; ++k)
      for (size_t i = 0; i < spans.size(); ++i) {
        if (spans[i].columns != k || spans[i].column < 0) continue;
        topUp(grid.columnWidths, spans[i], in.childSizes[i].width());
        topUp(least, spans[i], narrowest(i));
      }

    // 3. The room the table has for columns, which is what a percentage
    //    is a share of and what the two divisions below spend.
    const float table = width > 0 ? width : in.container.width();
    float room =
        table - ((float)cols * 2 * padding + (float)(cols + 1) * spacing);

    // 4. A column the markup gave a width takes it, and stands out of
    //    both divisions below — its pixels as stated, its percentage as
    //    that share of the room — unless what is in it needs more room
    //    than the markup asked for, which no column ever gives up.
    std::vector<uint8_t> stated((size_t)cols, 0u);
    for (size_t c = 0; c < declaredWidths.size() && c < (size_t)cols; ++c) {
      const Dim& asked = declaredWidths[c];
      const float px = asked.unit == Dim::Unit::Px ? asked.value
                       : asked.unit == Dim::Unit::Pct
                           ? room * asked.value / 100.0f
                           : 0.0f;
      if (px <= 0) continue;  // auto, and a width of nothing with it
      stated[c] = 1u;
      grid.columnWidths[c] = std::max(px, least[c]);
      least[c] = grid.columnWidths[c];
    }

    // 5. What the columns the markup left alone asked for, against what
    //    is left of the room once the stated ones have taken theirs.
    float wanted = 0, needed = 0;
    for (int c = 0; c < cols; ++c) {
      if (stated[(size_t)c]) {
        room -= grid.columnWidths[(size_t)c];
        continue;
      }
      wanted += grid.columnWidths[(size_t)c];
      needed += least[(size_t)c];
    }
    if (room >= wanted) {
      // What the table is wider than its content, shared out in
      // proportion — the step that leaves every column on a fraction. A
      // shrink-to-fit table declines the share and stops at its content.
      if (fit == Fit::Fill && wanted > 0)
        for (int c = 0; c < cols; ++c)
          if (!stated[(size_t)c])
            grid.columnWidths[(size_t)c] +=
                (room - wanted) * grid.columnWidths[(size_t)c] / wanted;
    } else if (room > needed && wanted > needed) {
      // Too narrow for what they want: each column gives up the same
      // fraction of the distance between its two widths.
      const float part = (room - needed) / (wanted - needed);
      for (int c = 0; c < cols; ++c)
        if (!stated[(size_t)c])
          grid.columnWidths[(size_t)c] =
              least[(size_t)c] +
              (grid.columnWidths[(size_t)c] - least[(size_t)c]) * part;
    } else {
      // Narrower than the content can be set at all: the columns stand at
      // what they need and the table runs past its width.
      for (int c = 0; c < cols; ++c)
        if (!stated[(size_t)c]) grid.columnWidths[(size_t)c] = least[(size_t)c];
    }

    // 6. Rows, by the same first step…
    for (size_t i = 0; i < spans.size(); ++i)
      if (spans[i].rows == 1 && (size_t)spans[i].row < grid.rowHeights.size())
        grid.rowHeights[(size_t)spans[i].row] = std::max(
            grid.rowHeights[(size_t)spans[i].row], in.childSizes[i].height());
    // …and deliberately NOT the same second one: the whole of a rowspan's
    //    deficit lands on the last row it covers.
    for (int k = 2; k <= lines; ++k)
      for (size_t i = 0; i < spans.size(); ++i) {
        if (spans[i].rows != k) continue;
        const float deficit = in.childSizes[i].height() -
                              extent(grid.rowHeights, spans[i].row, k);
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
   *  free cells left over — the kernel's own flow, which every
   *  cell-shaped scheme fills its grid with. */
  std::vector<CellSpan> flowed(const LayoutInput& in) const {
    std::vector<CellSpan> out(in.childSizes.size());
    for (size_t i = 0; i < out.size(); ++i)
      if (i < in.childCells.size()) out[i] = in.childCells[i];
    flowCells(out, columnCount(out));
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
