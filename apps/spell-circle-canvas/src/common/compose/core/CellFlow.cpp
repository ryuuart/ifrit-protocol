/** @file
 * The cell flow: where the children that claimed nothing land. One body,
 * because every cell-shaped scheme asks the same question — the auto
 * table and the grid differ in how they SIZE their tracks, never in how
 * they fill them.
 */

#include <sigilcompose/core/Layout.h>

#include <algorithm>
#include <vector>

namespace sigil::compose {

void flowCells(std::vector<CellSpan>& spans, int columns, bool dense) {
  const int cols = std::max(columns, 1);
  // One flag per cell of the grid, grown as the rows are reached: a run
  // that never leaves the first row never allocates past it.
  std::vector<char> taken;
  const auto held = [&](int column, int row) {
    if (column < 0 || column >= cols || row < 0) return true;
    const size_t at = (size_t)row * (size_t)cols + (size_t)column;
    return at < taken.size() && taken[at] != 0;
  };
  const auto claim = [&](int column, int row, int wide, int tall) {
    for (int r = row; r < row + tall; ++r)
      for (int c = column; c < column + wide; ++c) {
        if (c < 0 || c >= cols || r < 0) continue;
        const size_t at = (size_t)r * (size_t)cols + (size_t)c;
        if (taken.size() <= at) taken.resize(at + 1, 0);
        taken[at] = 1;
      }
  };
  // THE DECLARED SPANS FIRST, all of them, so a flowing child cannot land
  // on a cell a later declaration claims — the failure of a scheme that
  // counts its flow from zero as it goes.
  for (const CellSpan& s : spans)
    if (s.declared) claim(s.column, s.row, s.columns, s.rows);

  size_t cursor = 0;
  for (CellSpan& s : spans) {
    if (s.declared) continue;
    // A span wider than the grid is as wide as the grid: there is no cell
    // a wider one could ever be free at, and the search below has no way
    // out but to find one.
    const int wide = std::clamp(s.columns, 1, cols);
    const int tall = std::max(s.rows, 1);
    size_t at = dense ? 0 : cursor;
    for (;; ++at) {
      const int c = (int)(at % (size_t)cols);
      const int r = (int)(at / (size_t)cols);
      if (c + wide > cols) continue;  // a span may not straddle the edge
      bool free = true;
      for (int dr = 0; dr < tall && free; ++dr)
        for (int dc = 0; dc < wide && free; ++dc) free = !held(c + dc, r + dr);
      if (!free) continue;
      s.column = c;
      s.row = r;
      // The span it was placed at is the span it gets: a child that asked
      // for more columns than the grid has is laid out over the cells it
      // actually holds.
      s.columns = wide;
      s.rows = tall;
      claim(c, r, wide, tall);
      if (!dense) cursor = at + 1;
      break;
    }
  }
}

}  // namespace sigil::compose
