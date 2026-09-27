#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Board.h>
#include <sigilcompose/kit/Document.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilsketch/kit/Panel.h>

#include <algorithm>
#include <utility>

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Corners;
using compose::Dimension;
using compose::Element;
using compose::Fill;

compose::Element backdrop(const Backdrop& ground) {
  const Theme& look = theme();
  Element surface = box().absolute().inset(0);
  surface.fill(ground.ground.value_or(Fill::color(look.palette.ground)));
  if (!ground.key.empty()) surface.key(ground.key);
  if (ground.grain > 0) {
    // The grain is a fill of its own laid over the ground rather than a
    // grained ground, so a ground that is a gradient or an image takes the
    // same grain the flat one does: luminance noise soft-lit over mid grey,
    // which is soft light's identity, so the strength is linear and 0 is
    // exact.
    surface.children({box().absolute().inset(0).fill(
        material::from(material::Color{0.5f, 0.5f, 0.5f, 1})
            .layer(material::noise(ground.grainScale,
                                   {.octaves = 2, .grain = true}),
                   {.blend = material::BlendMode::SoftLight,
                    .opacity = std::clamp(ground.grain, 0.0f, 1.0f)}))});
  }
  if (ground.vignette > 0) {
    material::Color edge = ground.edge.value_or(material::Color{0, 0, 0, 1});
    edge.a = std::clamp(ground.vignette, 0.0f, 1.0f);
    material::Color clear = edge;
    clear.a = 0;
    // Transparent out to a little under half the way to the far corner,
    // then ramped to the edge colour at the corner itself.
    surface.children({box().absolute().inset(0).fill(material::radialGradient(
        {0.5f, 0.5f}, 1.0f, {{0.45f, clear}, {1.0f, edge}}))});
  }
  return surface;
}

compose::Element panel(const Panel& region, compose::Element content) {
  const Theme& look = theme();
  // The plate a panel stands on is not a specimen well: it is padded,
  // rounded and ruled by the theme's own panel distances unless the
  // caller states otherwise, and the well below puts the theme's ground
  // and its recess or relief into the same value.
  Well plate = region.body;
  if (!plate.padding) plate.padding = look.spacing.panelPadding;
  if (!plate.corners) plate.corners = look.spacing.panelCorners;
  if (!plate.keyline) plate.keyline = Fill::color(look.palette.rule);
  return well(plate, compose::kit::panel(
                         {.eyebrow = region.eyebrow,
                          .title = region.title,
                          .note = region.note,
                          .rule = region.ruled ? Fill::color(look.palette.rule)
                                               : Fill{},
                          .gap = look.spacing.captionGap,
                          .titleGap = look.spacing.captionNoteGap},
                         std::move(content)));
}

compose::Element frame(const Frame& chrome, compose::Element screen) {
  const Theme& look = theme();
  Element opening = compose::box().column().flexGrow(1);
  opening.fill(chrome.screen.value_or(Fill::color(look.palette.ground)));
  opening.overflow(compose::Overflow::Clip).children({std::move(screen)});
  if (const float round =
          chrome.screenCorners.value_or(look.spacing.screenCorners);
      round > 0)
    opening.borderRadius(Corners{round});
  const Fill rule = chrome.keyline.value_or(Fill::color(look.palette.rule));
  if (rule.kind != Fill::Kind::None)
    opening.stroke(compose::stroke(1, rule, compose::PathFormat::Align::Inner));

  const float bezel = chrome.bezel.value_or(look.spacing.bezel);
  Element shell = compose::box().column().padding(bezel);
  shell.fill(chrome.shell.value_or(Fill::color(look.palette.cellGround)));
  if (chrome.width.unit != Dimension::Unit::Auto) shell.width(chrome.width);
  if (chrome.height.unit != Dimension::Unit::Auto) shell.height(chrome.height);
  if (const float round = chrome.corners.value_or(look.spacing.panelCorners);
      round > 0)
    shell.borderRadius(Corners{round});
  shell.children({std::move(opening)});
  if (!chrome.plate.empty())
    shell.children(
        {compose::document::eyebrow(chrome.plate)
             .role("eyebrow", look.font(look.type.eyebrow, look.palette.ash))
             .margin(bezel * 0.5f, 0, 0, 0)
             .alignSelf(Align::Center)});
  return shell;
}

compose::Element frame(const Frame& chrome) {
  return frame(chrome, compose::box());
}

}  // namespace sigil::sketch::kit
