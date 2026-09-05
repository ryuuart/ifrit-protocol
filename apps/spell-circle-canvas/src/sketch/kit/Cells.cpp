#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/kit/Cells.h>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace sigil::sketch::kit {

compose::Element well(const Well& spec, compose::Element surface) {
  const Theme& look = theme();
  return compose::kit::well(
      {.width = spec.width,
       .height = spec.height,
       .ground = spec.ground.value_or(
           compose::Fill::color(look.palette.cellGround)),
       .padding = spec.padding.value_or(look.spacing.wellPadding),
       .clip = spec.clip},
      std::move(surface));
}

compose::Element well(const Well& spec) {
  return well(spec, compose::box());
}

compose::Element caption(float measure, std::u8string label,
                         std::u8string note, compose::Element body) {
  return compose::kit::cell(theme().voice(measure), std::move(label),
                            std::move(note), std::move(body));
}

namespace {

/** The hairline a ruled run stands between neighbours, spanning the run's
 *  whole cross extent however its cells are aligned. */
compose::Fill ruleOf(bool ruled) {
  return ruled ? compose::Fill::color(theme().palette.rule) : compose::Fill{};
}

}  // namespace

compose::Element cells(Run run) {
  const Theme& look = theme();
  return compose::kit::cells({.cells = std::move(run.cells),
                              .column = run.column,
                              .gap = run.gap.value_or(look.spacing.cellGap),
                              .divider = ruleOf(run.ruled),
                              .align = run.align});
}

compose::Element columns(Columns run) {
  // Every cell takes one share. `basis` is 0 so a cell's own content does
  // not bias the share it gets, which is what makes three panels of very
  // different content three equal columns.
  for (compose::Element& cell : run.cells)
    cell.grow(1).basis(compose::Dim(0)).shrink(1);
  return cells({.cells = std::move(run.cells),
                .gap = run.gap,
                .ruled = run.ruled,
                .align = run.align});
}

compose::Element panelGrid(PanelGrid grid) {
  const Theme& look = theme();
  const int across = std::max(grid.columns, 1);
  const float gap = grid.gap.value_or(look.spacing.cellGap);
  std::vector<compose::Element> rows;
  for (size_t i = 0; i < grid.cells.size(); i += (size_t)across) {
    std::vector<compose::Element> band;
    for (size_t j = i; j < std::min(i + (size_t)across, grid.cells.size()); ++j)
      band.push_back(std::move(grid.cells[j]));
    // The last band is filled out with empty shares, so a short row keeps
    // its cells at their own width instead of stretching them.
    while (band.size() < (size_t)across) band.push_back(compose::box());
    rows.push_back(columns(
        {.cells = std::move(band), .gap = gap, .ruled = grid.ruled}));
  }
  return cells({.cells = std::move(rows),
                .column = true,
                .gap = grid.rowGap.value_or(gap),
                .align = compose::Align::Stretch});
}

}  // namespace sigil::sketch::kit
