#include <sigilcompose/core/Grid.h>
#include <sigilcompose/kit/Specimen.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace sigil::compose::kit {

namespace {

Element arrangement(std::vector<Element> children,
                    std::vector<layouts::Track> columns,
                    std::vector<layouts::Track> rows, SkSize gap, Align down,
                    Align measuredAlign) {
  Element grid = layout(layouts::Grid{.columns = std::move(columns),
                                      .rows = std::move(rows),
                                      .gap = gap,
                                      .down = down})
                     .row()
                     .alignItems(measuredAlign);
  for (Element& cell : children) grid.child(std::move(cell));
  return grid;
}

std::vector<layouts::Track> interleave(std::vector<Element>& cells,
                                       Fill divider, float width,
                                       size_t perRow) {
  const bool ruled = divider.kind != Fill::Kind::None;
  std::vector<Element> out;
  for (size_t i = 0; i < cells.size(); ++i) {
    if (ruled && i % perRow != 0)
      out.push_back(box().width(width).fill(divider).cellAlign(Align::Start,
                                                               Align::Stretch));
    out.push_back(std::move(cells[i]));
  }
  std::vector<layouts::Track> tracks;
  for (size_t i = 0; i < perRow; ++i) {
    if (ruled && i > 0) tracks.push_back(layouts::px(width));
    tracks.push_back(layouts::fr());
  }
  cells = std::move(out);
  return tracks;
}

layouts::Track crossTrack(Align align) {
  return align == Align::Stretch
             ? layouts::minmax(layouts::content(), layouts::fr())
             : layouts::content();
}

}  // namespace

Element cells(Cells run) {
  Element shelf = box().gap(run.gap).alignItems(run.align);
  if (run.column)
    shelf.column();
  else
    shelf.row();
  const bool ruled = run.divider.kind != Fill::Kind::None;
  bool first = true;
  for (Element& cell : run.cells) {
    if (!first && ruled) {
      Element rule = run.column ? box().height(Dimension(run.dividerWidth))
                                : box().width(Dimension(run.dividerWidth));
      // The rule spans the run's whole cross extent whatever the cells'
      // own alignment is: a rule that stopped at the tallest cell's top
      // would read as a tick.
      shelf.child(rule.fill(run.divider).alignSelf(Align::Stretch));
    }
    first = false;
    shelf.child(std::move(cell));
  }
  return shelf;
}

Element panelGrid(PanelGrid grid) {
  const float gap = grid.gap;
  // Every cell takes one share, and a share has no floor of its own: that
  // is what makes three panels of very different content three equal
  // columns, where a run of content tracks would give each its own width.
  // A grid told nothing about how many stand across puts them all in one
  // row, which is that band and not a wrap of one column.
  const bool oneRow = grid.columns <= 0;
  if (oneRow) {
    std::vector<layouts::Track> tracks =
        interleave(grid.cells, grid.divider, grid.dividerWidth,
                   std::max<size_t>(grid.cells.size(), 1));
    return arrangement(std::move(grid.cells), std::move(tracks),
                       {crossTrack(grid.align)}, {gap, 0}, grid.align,
                       grid.align);
  }
  const size_t across = (size_t)grid.columns;
  // The wrap is the grid's own flow: the cells fill a row of equal shares
  // and drop to the next. A short last row keeps its cells at one share
  // each, because the shares are the TRACKS and not the children — there
  // is nothing to pad out.
  std::vector<layouts::Track> tracks =
      interleave(grid.cells, grid.divider, grid.dividerWidth, across);
  // The panels are ranged at their own height while they are measured, so
  // a row is as deep as the deepest panel in it and not as deep as the
  // grid; the grid then stretches them to the row it resolved.
  return arrangement(std::move(grid.cells), std::move(tracks), {},
                     {gap, grid.rowGap.value_or(gap)}, Align::Stretch,
                     Align::Start);
}

}  // namespace sigil::compose::kit
