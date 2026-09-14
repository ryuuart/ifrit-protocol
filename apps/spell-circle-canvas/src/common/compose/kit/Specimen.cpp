#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Grid.h>
#include <sigilcompose/kit/Specimen.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace sigil::compose::kit {

// ---------------------------------------------------------------------------
// The specimen well

Element well(const Well& spec, Element surface) {
  // THE WELL THAT HOLDS: the plate is a surface of its own and what it
  // was handed stands inside it at its own measure. Nothing of the spec
  // reaches that element, which is the whole difference from the reading
  // below — a picture smaller than its plate keeps the size it was drawn
  // at.
  if (spec.content) return well(spec).children({std::move(surface)});
  if (spec.width.unit != Dimension::Unit::Auto) surface.width(spec.width);
  if (spec.height.unit != Dimension::Unit::Auto) surface.height(spec.height);
  if (!spec.ground.none()) surface.fill(spec.ground);
  if (spec.paddingY)
    surface.padding(Dimension(spec.padding), Dimension(*spec.paddingY));
  else if (spec.padding != 0.0f)
    surface.padding(Dimension(spec.padding));
  if (spec.clip) surface.clip();
  if (spec.corners > 0.0f) surface.corners(Corners{spec.corners});
  // Inside its own box: a keyline centred on the boundary would put half
  // its width outside, and a plate that is not the width it was given is
  // the one thing a fixed surface may not be.
  // A keyline stated as Fill::none() draws none, which is the spelling a
  // ground takes: a plate whose only rule runs elsewhere asks for it.
  if (spec.keyline && spec.keyline->kind != Fill::Kind::None)
    surface.stroke(compose::stroke(spec.keylineWidth, *spec.keyline,
                                   PathFormat::Align::Inner));
  return surface;
}

Element well(const Well& spec) {
  Well plate = spec;
  plate.content.reset();
  Element surface = well(plate, spec.placed ? stack() : box());
  if (spec.content)
    surface.alignItems(spec.content->across).justify(spec.content->down);
  return surface;
}

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
  for (Element& cell : children) grid.children({std::move(cell)});
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
      shelf.children({rule.fill(run.divider).alignSelf(Align::Stretch)});
    }
    first = false;
    shelf.children({std::move(cell)});
  }
  return shelf;
}

Element panelGrid(PanelGrid grid) {
  const float gap = grid.gap;
  const Dimension measure = grid.measure;
  const auto measured = [measure](Element built) {
    if (measure.unit != Dimension::Unit::Auto) built.width(measure);
    return built;
  };
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
    return measured(arrangement(std::move(grid.cells), std::move(tracks),
                                {crossTrack(grid.align)}, {gap, 0}, grid.align,
                                grid.align));
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
  return measured(arrangement(std::move(grid.cells), std::move(tracks), {},
                              {gap, grid.rowGap.value_or(gap)}, Align::Stretch,
                              Align::Start));
}

}  // namespace sigil::compose::kit
