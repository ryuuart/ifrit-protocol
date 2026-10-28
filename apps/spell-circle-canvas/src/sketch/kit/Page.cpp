#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>

#include <utility>
#include <variant>

namespace sigil::sketch::kit {

namespace {

/** The ground the stage's stylesheet states: the last `palette-ground`
 *  colour a `:root` rule of it sets, else the house ground. */
material::Color stagedGround(const Stage& surface) {
  material::Color ground = houseTheme().palette.ground;
  if (!surface.styleSheet) return ground;
  const compose::ElementSelector root = compose::rule(":root").selector();
  const compose::VarRef name = compose::var("palette-ground");
  for (const compose::Rule& each : surface.styleSheet->rules()) {
    if (!(each.selector() == root)) continue;
    if (const compose::VarValue* value = each.vars().find(name))
      if (const auto* colour = std::get_if<material::Color>(value))
        ground = *colour;
  }
  return ground;
}

/** The house custom properties, built once and held: what a stage stands
 *  on under any stylesheet it names. */
const compose::StyleSheet& houseProperties() {
  static const compose::StyleSheet properties = houseTheme().properties();
  return properties;
}

}  // namespace

void stage(Scene2DContext& ctx, const Stage& surface) {
  CanvasSpecification declared;
  declared.size = surface.size;
  declared.background = material::skia::toSkColor(
      surface.background.value_or(stagedGround(surface)));
  declared.captureSeconds = surface.captureAt;
  declared.oversample = surface.oversample;
  declared.plateOnly = surface.plateOnly;
  ctx.canvas(declared);
  // The look stands at the root of whatever the sketch renders, so a
  // component anywhere in it reads the stylesheet where it lands. Under it
  // stand the house custom properties and no role rules, which a study
  // setting its own type would otherwise have to undo: a stylesheet that
  // names a few rules still finds every property a component reads, and a
  // stock stylesheet sets every one of them again.
  ctx.composer.setStyleSheet(surface.styleSheet
                                 ? houseProperties() + *surface.styleSheet
                                 : houseProperties());
}

void stage(Scene3DContext& ctx, const Stage& surface) {
  if (ctx.specification == nullptr) return;
  CanvasSpecification declared;
  declared.size = surface.size;
  declared.background = material::skia::toSkColor(
      surface.background.value_or(stagedGround(surface)));
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
  // the stylesheet's ink, so a leaf under a page that states no style of its
  // own is set in that. A leaf handed a whole `weave::TextStyle` inherits
  // nothing and is untouched.
  return compose::kit::sheet(specification, std::move(content))
      .absolute()
      .inset(0)
      .role("page");
}

}  // namespace sigil::sketch::kit
