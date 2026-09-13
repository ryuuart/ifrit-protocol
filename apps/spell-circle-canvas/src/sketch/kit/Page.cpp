#include <sigilcompose/kit/Specimen.h>
#include <sigilcore/reconcile/Environment.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/style/StyleSheet.h>

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
  // THE REGISTERS ARE IN SCOPE FOR THE THREE LINES THE SHEET WRITES. The
  // title, the subtitle and the footer are set in the classes of those
  // names, and they are written inside this call rather than by the
  // caller, so the theme's sheet is bound here — which is what makes a
  // page set in the theme's voice whether or not a sketch bound one.
  // `content` was built before this call and keeps the classes it
  // resolved where it was written.
  const core::environment::Provide<weave::StyleSheet> registers(
      look.styleSheet());
  const compose::kit::Sheet specification{
      .title = sheet.title,
      .subtitle = sheet.subtitle,
      .footer = sheet.footer,
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
          .font(look.font(running))
          .ink(look.palette.ink);
  return surface;
}

}  // namespace sigil::sketch::kit
