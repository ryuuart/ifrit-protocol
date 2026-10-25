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
#include <optional>
#include <string_view>
#include <utility>

#include "WellPlate.h"

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Corners;
using compose::Dimension;
using compose::Element;
using compose::Fill;
using compose::var;

compose::Element backdrop(const Backdrop& ground) {
  Element surface = box().absolute().inset(0);
  surface.fill(ground.ground.value_or(compose::var("palette-ground")));
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
  // The plate a panel stands on is not a specimen well: it is padded,
  // rounded and ruled by the sheet's own panel distances unless the
  // caller states otherwise, and the well below puts the sheet's ground
  // and its recess or relief into the same value.
  Well plate = region.body;
  if (!plate.keyline) plate.keyline = compose::var("palette-rule");
  return detail::plate(
      plate,
      compose::kit::panel(
          {.eyebrow = region.eyebrow,
           .title = region.title,
           .note = region.note,
           .rule = region.ruled ? compose::var("palette-rule") : Fill{},
           .gap = var("spacing-caption-gap"),
           .titleGap = var("spacing-caption-note-gap")},
          std::move(content)),
      var("spacing-panel-padding"), var("spacing-panel-corners"));
}

namespace {

/** A radius the caller stated, rounding only where it is more than
 *  nothing, or the sheet's property where the caller stated none. */
void rounded(Element& surface, const std::optional<float>& stated,
             std::string_view property) {
  if (!stated)
    surface.borderRadius(Corners{var(property)});
  else if (*stated > 0)
    surface.borderRadius(Corners{*stated});
}

}  // namespace

compose::Element frame(const Frame& chrome, compose::Element screen) {
  Element opening = compose::box().column().flexGrow(1);
  opening.fill(chrome.screen.value_or(compose::var("palette-ground")));
  opening.overflow(compose::Overflow::Clip).children({std::move(screen)});
  rounded(opening, chrome.screenCorners, "spacing-screen-corners");
  const Fill rule = chrome.keyline.value_or(compose::var("palette-rule"));
  if (rule.kind != Fill::Kind::None)
    opening.stroke(compose::stroke(1, rule, compose::PathFormat::Align::Inner));

  // The bezel is the shell's padding, so the screen is the flex child it
  // insets and nothing is measured while the frame is built.
  const Dimension bezel =
      chrome.bezel ? Dimension(*chrome.bezel) : var("spacing-bezel");
  Element shell = compose::box().column().padding(bezel);
  shell.fill(chrome.shell.value_or(compose::var("palette-cell-ground")));
  if (chrome.width.unit != Dimension::Unit::Auto) shell.width(chrome.width);
  if (chrome.height.unit != Dimension::Unit::Auto) shell.height(chrome.height);
  rounded(shell, chrome.corners, "spacing-panel-corners");
  shell.children({std::move(opening)});
  if (!chrome.plate.empty()) {
    const Theme& house = houseTheme();
    shell.children({compose::document::eyebrow(chrome.plate)
                        .role("eyebrow", house.font(house.type.eyebrow),
                              var("palette-ash"))
                        .margin(bezel * 0.5f, 0, 0, 0)
                        .alignSelf(Align::Center)});
  }
  return shell;
}

compose::Element frame(const Frame& chrome) {
  return frame(chrome, compose::box());
}

}  // namespace sigil::sketch::kit
