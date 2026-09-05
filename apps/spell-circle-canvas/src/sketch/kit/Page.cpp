#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/kit/Page.h>

#include <utility>

namespace sigil::sketch::kit {

void stage(SketchContext& ctx, const Stage& surface) {
  CanvasSpec declared;
  declared.size = surface.size;
  declared.background = surface.background.value_or(theme().palette.ground);
  declared.captureSeconds = surface.captureAt;
  declared.oversample = surface.oversample;
  declared.plateOnly = surface.plateOnly;
  ctx.canvas(declared);
}

compose::Element page(const Page& sheet, compose::Element content) {
  const Theme& look = theme();
  const compose::kit::Sheet spec{
      .title = sheet.title,
      .subtitle = sheet.subtitle,
      .footer = sheet.footer,
      .titleStyle = look.style(look.type.title, look.palette.ink),
      .subtitleStyle = look.style(look.type.subtitle, look.palette.ash),
      .footerStyle = look.style(look.type.footer, look.palette.ash),
      .marginX = look.spacing.marginX,
      .marginTop = look.spacing.marginTop,
      .marginBottom = look.spacing.marginBottom,
      .subtitleGap = look.spacing.subtitleGap,
      .contentGap = look.spacing.contentGap,
      .ground = compose::Fill::color(look.palette.ground),
      .rule = sheet.ruled ? compose::Fill::color(look.palette.rule)
                          : compose::Fill{},
      .key = sheet.key};
  // A page is the whole surface: the sheet does not size itself, so this
  // is where the canvas is handed to it.
  return compose::kit::sheet(spec, std::move(content)).absolute().inset(0);
}

}  // namespace sigil::sketch::kit
