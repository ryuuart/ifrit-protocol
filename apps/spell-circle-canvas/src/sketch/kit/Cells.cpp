#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/PixelStyles.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Grid.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilsketch/kit/Cells.h>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "WellPlate.h"

namespace sigil::sketch::kit {

namespace {

/** The air a comparison and a panel grid stand in where the caller states
 *  none. A grid's tracks are measured in pixels by its own layout, so
 *  these are numbers rather than the sheet's properties. */
constexpr float kComparisonGap = 20.0f;
constexpr float kComparisonTrackGap = 8.0f;
constexpr float kPanelGridGap = 20.0f;

}  // namespace

namespace detail {

compose::Element plate(const Well& specification, compose::Element surface,
                       compose::Dimension padding,
                       std::optional<compose::Dimension> corners) {
  // The sheet in force supplies the ground and the padding a well leaves
  // unset, as properties; the plate, its corners and its keyline are the
  // primitive's, and the two looks below are this library's own.
  const compose::Fill bed =
      specification.ground.value_or(compose::var("palette-cell-ground"));
  std::optional<compose::kit::Well::Content> held;
  if (specification.content)
    held = compose::kit::Well::Content{.across = specification.content->across,
                                       .down = specification.content->down};
  // A padding the caller stated is written by the primitive as before; an
  // unset one is the property, written onto the plate the primitive
  // answers, which is the plate whichever reading the well takes.
  const bool statedPadding = specification.padding.has_value();
  compose::Element plate = compose::kit::well(
      {.width = specification.width,
       .height = specification.height,
       .ground = bed,
       .padding = specification.padding.value_or(0.0f),
       .paddingVertical =
           statedPadding ? specification.paddingVertical : std::nullopt,
       .clip = specification.clip,
       .corners = specification.corners.value_or(0.0f),
       .keyline = specification.keyline,
       .keylineWidth = specification.keylineWidth,
       .content = held},
      std::move(surface));
  if (!statedPadding) {
    if (specification.paddingVertical)
      plate.padding(compose::Dimension(*specification.paddingVertical),
                    padding);
    else
      plate.padding(padding);
  }
  if (!specification.corners && corners)
    plate.borderRadius(compose::Corners{*corners});
  // THE RELIEF AND THE RECESS ARE THE GROUND'S EFFECTS: the plate is
  // filled again with its ground carrying a bevel, an inner shadow or both,
  // in that order. A ground written as a reference to the ink in force has
  // no material to carry them, and is taken as the colour it holds.
  material::Filter effects;
  const auto then = [&](const material::Filter& step) {
    effects = effects.isNone() ? step : effects.then(step);
  };
  if (specification.relief) {
    const Well::Relief& lift = *specification.relief;
    then(material::Filter::bevel({.depth = lift.depth,
                                  .size = lift.blur,
                                  .angleDegrees = lift.angleDegrees,
                                  .highlight = lift.light,
                                  .shadow = lift.shade}));
  }
  if (specification.recess) {
    const Well::Recess& hole = *specification.recess;
    then(material::Filter::shadow(
        hole.shade.colorValue,
        {.blur = hole.blur,
         .offset = sigil::geometry::path::fromSk(hole.offset),
         .inset = true}));
  }
  if (!effects.isNone()) {
    const material::Material ground =
        bed.material() ? *bed.material() : material::Material(bed.colorValue);
    plate.fill(material::from(ground).effects(effects));
  }
  if (specification.recess) {
    const Well::Recess& hole = *specification.recess;
    if (hole.lipLight && hole.lipDark)
      plate.overlay(compose::styles::bevelPair(*hole.lipLight, *hole.lipDark,
                                               hole.lipWidth,
                                               /*sunken=*/true));
  }
  return plate;
}

}  // namespace detail

compose::Element well(const Well& specification, compose::Element surface) {
  return detail::plate(specification, std::move(surface),
                       compose::var("spacing-well-padding"), std::nullopt);
}

compose::Element well(const Well& specification) {
  return well(specification,
              specification.placed ? compose::stack() : compose::box());
}

compose::Element caption(float measure, compose::Utf8 label, compose::Utf8 note,
                         compose::Element body,
                         compose::kit::Caption::Where where) {
  // The two lines the cell writes have document roles the sheet in force
  // styles, and the air around them is the sheet's caption gaps, read
  // where the cell lands; `body` keeps the sheets in force there too.
  return compose::kit::cell(
      {.where = where,
       .gap = compose::var("spacing-caption-gap"),
       .noteGap = compose::var("spacing-caption-note-gap"),
       .noteMeasure = measure},
      std::move(label), std::move(note), std::move(body));
}

compose::Element cell(const Cell& sheet, compose::Utf8 label,
                      compose::Utf8 note, compose::Element picture) {
  const float measure = sheet.measure.value_or(
      sheet.plate.width.unit == compose::Dimension::Unit::Px
          ? sheet.plate.width.value
          : 0.0f);
  // THE PLATE HOLDS THE PICTURE. Unset, it holds it as a box holds a
  // child; `Well::content` ranges it inside the plate instead, centred
  // where it says nothing, which is what a specimen smaller than its
  // plate asks for.
  compose::Element plate =
      sheet.plate.content ? well(sheet.plate, std::move(picture))
                          : well(sheet.plate).children({std::move(picture)});
  return caption(measure, std::move(label), std::move(note), std::move(plate),
                 sheet.where);
}

compose::Element comparison(Comparison specification) {
  using namespace compose;
  if (specification.cases.empty()) return box();
  const float gap = specification.gap.value_or(kComparisonGap);
  const float columnWidth = std::max(
      0.0f,
      (specification.measure - gap * float(specification.cases.size() - 1)) /
          float(specification.cases.size()));
  const auto any = [&](auto member) {
    return std::any_of(
        specification.cases.begin(), specification.cases.end(),
        [&](const ComparisonCase& one) { return !(one.*member).empty(); });
  };
  const bool titles = any(&ComparisonCase::title);
  const bool controls = any(&ComparisonCase::control);
  const bool notes = any(&ComparisonCase::note);
  const int tracks = 1 + int(titles) + int(controls) + int(notes);
  std::vector<Element> children;
  for (size_t column = 0; column < specification.cases.size(); ++column) {
    ComparisonCase& one = specification.cases[column];
    int row = 0;
    if (titles) {
      if (!one.title.empty())
        children.push_back(document::label(std::move(one.title))
                               .width(columnWidth)
                               .flexShrink(0)
                               .gridCells(int(column), row));
      ++row;
    }
    if (controls) {
      if (!one.control.empty())
        children.push_back(document::code(std::move(one.control))
                               .role("code")
                               .styleClass("control")
                               .width(columnWidth)
                               .flexShrink(0)
                               .gridCells(int(column), row));
      ++row;
    }
    children.push_back(box()
                           .column()
                           .width(columnWidth)
                           .flexShrink(0)
                           .alignItems(Align::Center)
                           .children({std::move(one.figure.flexShrink(0))})
                           .gridCells(int(column), row++));
    if (notes && !one.note.empty())
      children.push_back(document::caption(std::move(one.note))
                             .width(columnWidth)
                             .flexShrink(0)
                             .gridCells(int(column), row));
  }
  // Grid places already-measured children. Measure its tracks side by side
  // so Yoga cannot shrink the figures as a temporary vertical stack, and
  // give text its final column width before it reports a height.
  return layout(layouts::Grid{
                    .columns = layouts::repeatTrack(
                        int(specification.cases.size()), layouts::fr()),
                    .rows = layouts::repeatTrack(tracks, layouts::content()),
                    .gap = {gap, specification.trackGap.value_or(
                                     kComparisonTrackGap)},
                    .down = Align::Start})
      .row()
      .width(specification.measure)
      .flexShrink(0)
      .alignItems(Align::Start)
      .children(std::move(children));
}

compose::Element cells(Run run) {
  return compose::kit::cells(
      {.cells = std::move(run.cells),
       .column = run.column,
       .gap = run.gap ? compose::Dimension(*run.gap)
                      : compose::var("spacing-cell-gap"),
       .divider = run.ruled ? compose::var("palette-rule") : compose::Fill{},
       .align = run.align});
}

compose::Element panelGrid(PanelGrid grid) {
  return compose::kit::panelGrid(
      {.cells = std::move(grid.cells),
       .columns = grid.columns,
       .gap = grid.gap.value_or(kPanelGridGap),
       .rowGap = grid.rowGap,
       .divider = grid.ruled ? compose::var("palette-rule") : compose::Fill{},
       .align = grid.align,
       .measure = grid.measure});
}

}  // namespace sigil::sketch::kit
