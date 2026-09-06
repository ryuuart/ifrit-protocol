#include <sigilcompose/kit/Plate.h>
#include <sigilsketch/kit/Console.h>

#include <utility>

namespace sigil::sketch::kit {

compose::Element console(const Console& panel) {
  const Theme& look = theme();
  const float size = panel.size.value_or(look.type.captionLabel.size);
  const compose::Fill border =
      panel.border.value_or(compose::Fill::color(look.palette.rule));
  // The voice is built here rather than through the compose kit's
  // colour-only helper, because a console's ink is a Fill: a shader over
  // the base rows shades them, and a level names a colour of its own.
  const Register voice{.size = size, .mono = true};
  weave::StyleSet styles(look.style(
      voice, panel.ink.value_or(compose::Fill::color(look.palette.ink))));
  for (const auto& [name, color] : panel.levels)
    styles.set(name, look.style(voice, color));
  compose::Element plate = compose::kit::console(
      {.feeds = panel.feeds,
       .style = {.window = {.visible = panel.visible,
                            .gap = look.spacing.rowGap},
                 .styles = std::move(styles)},
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
