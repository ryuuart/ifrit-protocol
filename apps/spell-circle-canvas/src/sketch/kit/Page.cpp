#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>

#include <utility>

namespace sigil::sketch::kit {

void stage(Scene2DContext& ctx, const Stage& surface) {
  const Theme& look = surface.theme ? *surface.theme : houseTheme();
  CanvasSpecification declared;
  declared.size = surface.size;
  declared.background = material::skia::toSkColor(
      surface.background.value_or(look.palette.ground));
  declared.captureSeconds = surface.captureAt;
  declared.oversample = surface.oversample;
  declared.plateOnly = surface.plateOnly;
  ctx.canvas(declared);
  // The look stands at the root of whatever the sketch renders, so a
  // component anywhere in it reads the theme where it lands. A stage that
  // names none states the house properties and no role rules, which a
  // study setting its own type would otherwise have to undo.
  ctx.composer.setStyleSheet(surface.theme ? look.styleSheet()
                                           : look.properties());
}

void stage(Scene3DContext& ctx, const Stage& surface) {
  if (ctx.specification == nullptr) return;
  const Theme& look = surface.theme ? *surface.theme : houseTheme();
  CanvasSpecification declared;
  declared.size = surface.size;
  declared.background = material::skia::toSkColor(
      surface.background.value_or(look.palette.ground));
  declared.captureSeconds = surface.captureAt;
  declared.oversample = surface.oversample;
  declared.plateOnly = surface.plateOnly;
  *ctx.specification = declared;
}

compose::Element page(const Page& sheet, compose::Element content) {
  using compose::var;
  // Every distance and colour is the sheet's in force where the page
  // lands, read as a property; its heading, lead, footer and specimen
  // captions resolve their roles through the same sheet, and a nearer
  // sheet can change the fields it names for one panel.
  const compose::kit::Sheet specification{
      .title = sheet.title,
      .subtitle = sheet.subtitle,
      .footer = sheet.footer,
      .marginHorizontal = var("spacing-margin-horizontal"),
      .marginTop = var("spacing-margin-top"),
      .marginBottom = var("spacing-margin-bottom"),
      .subtitleGap = var("spacing-subtitle-gap"),
      .contentGap = var("spacing-content-gap"),
      .ground = sheet.ground.value_or(var("palette-ground")),
      .rule =
          sheet.ruled ? compose::Fill(var("palette-rule")) : compose::Fill{},
      .key = sheet.key};
  // A page is the whole surface: the sheet does not size itself, so this
  // is where the canvas is handed to it.
  //
  // ITS RUNNING TEXT IS THE `page` ROLE'S: the remark register — the one
  // line on a specimen sheet set in sentences rather than as a label — in
  // the theme's ink, so a leaf under a page that states no style of its
  // own is set in that. A leaf handed a whole `weave::TextStyle` inherits
  // nothing and is untouched.
  return compose::kit::sheet(specification, std::move(content))
      .absolute()
      .inset(0)
      .role("page");
}

}  // namespace sigil::sketch::kit
