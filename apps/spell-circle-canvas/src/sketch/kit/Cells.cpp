#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/PixelStyles.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Grid.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/kit/Cells.h>
#include <sigilweave/layout/StyleSheet.h>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace sigil::sketch::kit {

compose::Element well(const Well& specification, compose::Element surface) {
  const Theme& look = theme();
  // The theme supplies the ground and the padding; the plate, its corners
  // and its keyline are the primitive's, and the two looks below are this
  // library's own.
  const compose::SurfacePaint bed = specification.ground.value_or(
      compose::Fill::color(look.palette.cellGround));
  std::optional<compose::kit::Well::Content> held;
  if (specification.content)
    held = compose::kit::Well::Content{.across = specification.content->across,
                                       .down = specification.content->down};
  compose::Element plate = compose::kit::well(
      {.width = specification.width,
       .height = specification.height,
       .ground = bed,
       .padding = specification.padding.value_or(look.spacing.wellPadding),
       .paddingY = specification.paddingY,
       .clip = specification.clip,
       .corners = specification.corners.value_or(0.0f),
       .keyline = specification.keyline,
       .keylineWidth = specification.keylineWidth,
       .content = held},
      std::move(surface));
  if (specification.relief) {
    const Well::Relief& lift = *specification.relief;
    plate.foreground(compose::styles::BevelEmboss{
        lift.depth, lift.blur, lift.angleDeg, lift.light, lift.shade});
  }
  if (specification.recess) {
    const Well::Recess& hole = *specification.recess;
    plate.foreground(compose::styles::InnerShadow{hole.shade.colorValue,
                                                  hole.offset, hole.blur});
    if (hole.lipLight && hole.lipDark)
      plate.overlay(compose::styles::bevelPair(*hole.lipLight, *hole.lipDark,
                                               hole.lipWidth,
                                               /*sunken=*/true));
  }
  return plate;
}

compose::Element well(const Well& specification) {
  return well(specification,
              specification.placed ? compose::stack() : compose::box());
}

compose::Element caption(float measure, compose::Utf8 label, compose::Utf8 note,
                         compose::Element body) {
  // The two lines the cell writes are set in the theme's registers,
  // carried on those lines by the voice; `body` keeps the sheets in force
  // where the cell lands.
  return compose::kit::cell(theme().voice(measure), std::move(label),
                            std::move(note), std::move(body));
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
  return caption(measure, std::move(label), std::move(note), std::move(plate));
}

compose::Element comparison(Comparison specification) {
  using namespace compose;
  const Theme& look = theme();
  if (specification.cases.empty()) return box();
  const float gap = specification.gap.value_or(look.spacing.cellGap);
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
                               .cells(int(column), row));
      ++row;
    }
    if (controls) {
      if (!one.control.empty())
        children.push_back(
            document::code(std::move(one.control))
                .role(weave::rule("code").font(
                    look.font({.size = 10.5f, .mono = true}, look.palette.ash)))
                .width(columnWidth)
                .flexShrink(0)
                .cells(int(column), row));
      ++row;
    }
    children.push_back(box()
                           .column()
                           .width(columnWidth)
                           .flexShrink(0)
                           .alignItems(Align::Center)
                           .children({std::move(one.figure.flexShrink(0))})
                           .cells(int(column), row++));
    if (notes && !one.note.empty())
      children.push_back(document::caption(std::move(one.note))
                             .width(columnWidth)
                             .flexShrink(0)
                             .cells(int(column), row));
  }
  // Grid places already-measured children. Measure its tracks side by side
  // so Yoga cannot shrink the figures as a temporary vertical stack, and
  // give text its final column width before it reports a height.
  return layout(layouts::Grid{
                    .columns = layouts::repeatTrack(
                        int(specification.cases.size()), layouts::fr()),
                    .rows = layouts::repeatTrack(tracks, layouts::content()),
                    .gap = {gap, specification.trackGap.value_or(8)},
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
       .gap = run.gap.value_or(theme().spacing.cellGap),
       .divider = run.ruled ? compose::Fill::color(theme().palette.rule)
                            : compose::Fill{},
       .align = run.align});
}

compose::Element panelGrid(PanelGrid grid) {
  return compose::kit::panelGrid(
      {.cells = std::move(grid.cells),
       .columns = grid.columns,
       .gap = grid.gap.value_or(theme().spacing.cellGap),
       .rowGap = grid.rowGap,
       .divider = grid.ruled ? compose::Fill::color(theme().palette.rule)
                             : compose::Fill{},
       .align = grid.align,
       .measure = grid.measure});
}

}  // namespace sigil::sketch::kit
