#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/PixelStyles.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Grid.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/kit/Cells.h>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace sigil::sketch::kit {

compose::Element well(const Well& spec, compose::Element surface) {
  const Theme& look = theme();
  const float padX = spec.padding.value_or(look.spacing.wellPadding);
  const float padY = spec.paddingY.value_or(padX);
  // The padding is chained here rather than handed down, because the
  // primitive takes one distance and a plate may be set tighter down than
  // across. Everything else is the primitive's.
  const Ground bed =
      spec.ground.value_or(compose::Fill::color(look.palette.cellGround));
  compose::Element plate = compose::kit::well({.width = spec.width,
                                               .height = spec.height,
                                               .padding = 0,
                                               .clip = spec.clip},
                                              std::move(surface));
  // The ground goes on after the primitive rather than through it,
  // because the primitive takes a Fill and a ground may be a material.
  bed.paint(plate);
  if (padX != 0 || padY != 0) plate.padding(padX, padY);
  if (spec.corners > 0) plate.corners(compose::Corners{spec.corners});
  if (spec.keyline)
    plate.stroke(compose::stroke(spec.keylineWidth, *spec.keyline,
                                 compose::PathFormat::Align::Inner));
  if (spec.relief) {
    const Well::Relief& lift = *spec.relief;
    plate.foreground(compose::styles::BevelEmboss{
        lift.depth, lift.blur, lift.angleDeg, lift.light, lift.shade});
  }
  if (spec.recess) {
    const Well::Recess& hole = *spec.recess;
    plate.foreground(compose::styles::InnerShadow{hole.shade.colorValue,
                                                  hole.offset, hole.blur});
    if (hole.lipLight && hole.lipDark)
      plate.overlay(compose::styles::bevelPair(*hole.lipLight, *hole.lipDark,
                                               hole.lipWidth,
                                               /*sunken=*/true));
  }
  return plate;
}

compose::Element well(const Well& spec) { return well(spec, compose::box()); }

compose::Element caption(float measure, std::u8string label, std::u8string note,
                         compose::Element body) {
  return compose::kit::cell(theme().voice(measure), std::move(label),
                            std::move(note), std::move(body));
}

namespace {

/** The hairline a ruled run stands between neighbours, spanning the run's
 *  whole cross extent however its cells are aligned. */
compose::Fill ruleOf(bool ruled) {
  return ruled ? compose::Fill::color(theme().palette.rule) : compose::Fill{};
}

constexpr float kRuleWidth = 1.0f;

/** THE ONE ARRANGEMENT UNDER ALL THREE VERBS BELOW: a grid whose tracks
 *  the caller states.
 *
 *  IT KEEPS THE FLOW DIRECTION AND CROSS ALIGNMENT of the run it stands
 *  for, and that is load-bearing rather than decorative. A child is
 *  MEASURED before it is placed, and what a container stretches its
 *  children along is the axis their measured size stops being their own
 *  on — a caption told to take the room it is given reports the room, a
 *  caption left alone reports itself. Stating the same direction and the
 *  same alignment the run always had makes every child measure exactly
 *  what it measured before; the tracks then place them.
 */
compose::Element arrangement(std::vector<compose::Element> children,
                             std::vector<compose::layouts::Track> columns,
                             std::vector<compose::layouts::Track> rows,
                             SkSize gap, compose::Align across,
                             compose::Align down, bool column,
                             compose::Align ranged) {
  compose::Element grid =
      compose::layout(compose::layouts::Grid{.columns = std::move(columns),
                                             .rows = std::move(rows),
                                             .gap = gap,
                                             .across = across,
                                             .down = down});
  if (column)
    grid.column();
  else
    grid.row();
  grid.alignItems(ranged);
  for (compose::Element& cell : children) grid.child(std::move(cell));
  return grid;
}

/** ONE HAIRLINE, standing across the run whatever its cells' own
 *  alignment is: a rule that stopped at the tallest cell's top would read
 *  as a tick. */
compose::Element hairline(compose::Fill rule, bool column) {
  compose::Element hair = column
                              ? compose::box().height(compose::Dim(kRuleWidth))
                              : compose::box().width(compose::Dim(kRuleWidth));
  return hair.fill(rule).cellAlign(
      column ? compose::Align::Stretch : compose::Align::Start,
      column ? compose::Align::Start : compose::Align::Stretch);
}

/** The cells with a hairline between each neighbouring pair of a row of
 *  @p perRow, and the track list that run needs: a cell track per cell, a
 *  rule-wide one per rule. A rule is a child on a track of its own, which
 *  is why standing one between two cells never changes what either gets. */
std::vector<compose::layouts::Track> interleave(
    std::vector<compose::Element>& cells, compose::layouts::Track cellTrack,
    compose::Fill rule, bool column, size_t perRow) {
  std::vector<compose::layouts::Track> tracks;
  const bool ruled = rule.kind != compose::Fill::Kind::None;
  std::vector<compose::Element> out;
  for (size_t i = 0; i < cells.size(); ++i) {
    if (ruled && i % perRow != 0) out.push_back(hairline(rule, column));
    out.push_back(std::move(cells[i]));
  }
  for (size_t i = 0; i < perRow; ++i) {
    if (ruled && i > 0) tracks.push_back(compose::layouts::px(kRuleWidth));
    tracks.push_back(cellTrack);
  }
  cells = std::move(out);
  return tracks;
}

/** The cross track of a run: a stretched run is as long across as the
 *  room it is given, an unstretched one as long as its widest cell. */
compose::layouts::Track crossTrack(compose::Align align) {
  return align == compose::Align::Stretch
             ? compose::layouts::minmax(compose::layouts::content(),
                                        compose::layouts::fr())
             : compose::layouts::content();
}

}  // namespace

compose::Element cells(Run run) {
  const float gap = run.gap.value_or(theme().spacing.cellGap);
  // A run keeps every cell at the size it was given, so its cells sit on
  // content tracks and the run is exactly as long as they are.
  const size_t count = run.cells.size();
  std::vector<compose::layouts::Track> tracks =
      interleave(run.cells, compose::layouts::content(), ruleOf(run.ruled),
                 run.column, std::max<size_t>(count, 1));
  if (run.column)
    return arrangement(std::move(run.cells), {crossTrack(run.align)},
                       std::move(tracks), {0, gap}, run.align,
                       compose::Align::Start, /*column=*/true, run.align);
  return arrangement(std::move(run.cells), std::move(tracks),
                     {crossTrack(run.align)}, {gap, 0}, compose::Align::Start,
                     run.align, /*column=*/false, run.align);
}

compose::Element panelGrid(PanelGrid grid) {
  const float gap = grid.gap.value_or(theme().spacing.cellGap);
  // Every cell takes one share, and a share has no floor of its own: that
  // is what makes three panels of very different content three equal
  // columns, where a run of content tracks would give each its own width.
  // A grid told nothing about how many stand across puts them all in one
  // row, which is that band and not a wrap of one column.
  const bool oneRow = grid.columns <= 0;
  if (oneRow) {
    std::vector<compose::layouts::Track> tracks =
        interleave(grid.cells, compose::layouts::fr(), ruleOf(grid.ruled),
                   /*column=*/false, std::max<size_t>(grid.cells.size(), 1));
    return arrangement(std::move(grid.cells), std::move(tracks),
                       {crossTrack(grid.align)}, {gap, 0},
                       compose::Align::Stretch, grid.align, /*column=*/false,
                       grid.align);
  }
  const size_t across = (size_t)grid.columns;
  // The wrap is the grid's own flow: the cells fill a row of equal shares
  // and drop to the next. A short last row keeps its cells at one share
  // each, because the shares are the TRACKS and not the children — there
  // is nothing to pad out.
  std::vector<compose::layouts::Track> tracks =
      interleave(grid.cells, compose::layouts::fr(), ruleOf(grid.ruled),
                 /*column=*/false, across);
  // The panels are ranged at their own height while they are measured, so
  // a row is as deep as the deepest panel in it and not as deep as the
  // grid; the grid then stretches them to the row it resolved.
  return arrangement(std::move(grid.cells), std::move(tracks), {},
                     {gap, grid.rowGap.value_or(gap)}, compose::Align::Stretch,
                     compose::Align::Stretch, /*column=*/false,
                     compose::Align::Start);
}

}  // namespace sigil::sketch::kit
