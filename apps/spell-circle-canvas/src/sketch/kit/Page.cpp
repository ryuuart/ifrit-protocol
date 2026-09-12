#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>

#include <utility>

namespace sigil::sketch::kit {

void stage(SketchContext& ctx, const Stage& surface) {
  CanvasSpecification declared;
  declared.size = surface.size;
  declared.background = surface.background.value_or(theme().palette.ground);
  declared.captureSeconds = surface.captureAt;
  declared.oversample = surface.oversample;
  declared.plateOnly = surface.plateOnly;
  declared.nonlinearPicture = surface.nonlinearPicture;
  ctx.canvas(declared);
}

compose::Element page(const Page& sheet, compose::Element content) {
  const Theme& look = theme();
  const compose::kit::Sheet specification{
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
      .ground =
          sheet.ground.value_or(compose::Fill::color(look.palette.ground)),
      .rule = sheet.ruled ? compose::Fill::color(look.palette.rule)
                          : compose::Fill{},
      .key = sheet.key};
  // A page is the whole surface: the sheet does not size itself, so this
  // is where the canvas is handed to it.
  //
  // AND IT IS THE ROOT OF THE CASCADE. The sheet's running text is the
  // remark register — the one line on a specimen sheet set in sentences
  // rather than as a label — so a leaf under a page that states no style
  // of its own is set in that, in the theme's ink. A leaf handed a whole
  // `weave::TextStyle` inherits nothing and is untouched.
  const Register& running = look.type.captionNote;
  compose::Element surface =
      compose::kit::sheet(specification, std::move(content))
          .absolute()
          .inset(0)
          .font({.face = running.face ? running.face
                                      : (running.mono ? look.type.mono
                                                      : look.type.sans),
                 .size = running.size,
                 .track = running.track})
          .ink(look.palette.ink);
  return surface;
}

}  // namespace sigil::sketch::kit
