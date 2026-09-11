#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/PixelStyles.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/kit/Cells.h>

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
  const compose::SurfacePaint bed =
      spec.ground.value_or(compose::Fill::color(look.palette.cellGround));
  compose::Element plate = compose::kit::well({.width = spec.width,
                                               .height = spec.height,
                                               .ground = bed,
                                               .padding = 0,
                                               .clip = spec.clip},
                                              std::move(surface));
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
       .align = grid.align});
}

}  // namespace sigil::sketch::kit
