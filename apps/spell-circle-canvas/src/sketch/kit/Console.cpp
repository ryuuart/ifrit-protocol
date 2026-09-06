#include <sigilcompose/kit/Plate.h>
#include <sigilsketch/kit/Console.h>

#include <utility>

namespace sigil::sketch::kit {

compose::Element console(const Console& panel) {
  const Theme& look = theme();
  const float size = panel.size.value_or(look.type.captionLabel.size);
  const compose::Fill border =
      panel.border.value_or(compose::Fill::color(look.palette.rule));
  compose::Element plate = compose::kit::console(
      {.feeds = panel.feeds,
       .style = {.window = {.visible = panel.visible,
                            .gap = look.spacing.rowGap},
                 .styles = compose::kit::tinted(
                     look.type.mono, size, panel.ink.value_or(look.palette.ink),
                     panel.levels)},
       .stacked = panel.stacked,
       .stackGap = look.spacing.rowGap,
       .plate = {.paddingX = look.spacing.panelPadding,
                 .paddingY = look.spacing.panelPadding * 0.6f,
                 .gap = look.spacing.labelGap,
                 .border = border,
                 .divider = border}});
  // The ground goes on after the primitive rather than through it,
  // because the primitive takes a Fill and a ground may be a material.
  panel.ground.value_or(compose::Fill::color(look.palette.cellGround))
      .paint(plate);
  return plate;
}

}  // namespace sigil::sketch::kit
