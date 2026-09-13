#include <sigilcompose/kit/Specimen.h>
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
  // THE THREE LINES THE SHEET WRITES ARE SET IN THE THEME'S REGISTERS:
  // the title, the subtitle and the footer resolve through the theme's
  // sheet carried on those lines themselves, so a page is in the theme's
  // voice whatever sheet the tree above states, and `content` keeps the
  // sheets in force where the page lands.
  const compose::kit::Sheet specification{
      .title = sheet.title,
      .subtitle = sheet.subtitle,
      .footer = sheet.footer,
      .styles = look.styleSheet(),
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
