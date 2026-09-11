#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Ground.h>
#include <sigilsketch/kit/Panel.h>

#include <algorithm>
#include <utility>

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Corners;
using compose::Dim;
using compose::Element;
using compose::Fill;
using compose::text;

compose::Element backdrop(const Backdrop& ground) {
  const Theme& look = theme();
  Element surface = box().absolute().inset(0);
  ground.ground.value_or(Fill::color(look.palette.ground)).apply(surface);
  if (!ground.key.empty()) surface.key(ground.key);
  if (ground.grain > 0) {
    // The grain is a fill of its own laid over the ground rather than a
    // grained ground, so a ground that is a gradient or an image takes the
    // same grain the flat one does.
    surface.child(box().absolute().inset(0).fill(compose::kit::grained(
        {0.5f, 0.5f, 0.5f, 1}, ground.grain, ground.grainScale)));
  }
  if (ground.vignette > 0) {
    SkColor4f edge = ground.edge.value_or(SkColor4f{0, 0, 0, 1});
    edge.fA = std::clamp(ground.vignette, 0.0f, 1.0f);
    surface.child(box().absolute().inset(0).fill(
        compose::kit::vignette(ground.over, edge)));
  }
  return surface;
}

compose::Element frame(const Frame& chrome, compose::Element screen) {
  const Theme& look = theme();
  Element opening = compose::box().column().grow(1);
  chrome.screen.value_or(Fill::color(look.palette.ground)).apply(opening);
  opening.clip().child(std::move(screen));
  if (const float round =
          chrome.screenCorners.value_or(look.spacing.screenCorners);
      round > 0)
    opening.corners(Corners{round});
  const Fill rule = chrome.keyline.value_or(Fill::color(look.palette.rule));
  if (rule.kind != Fill::Kind::None)
    opening.stroke(compose::stroke(1, rule, compose::PathFormat::Align::Inner));

  const float bezel = chrome.bezel.value_or(look.spacing.bezel);
  Element shell = compose::box().column().padding(bezel);
  chrome.shell.value_or(Fill::color(look.palette.cellGround)).apply(shell);
  if (chrome.width.unit != Dim::Unit::Auto) shell.width(chrome.width);
  if (chrome.height.unit != Dim::Unit::Auto) shell.height(chrome.height);
  if (const float round = chrome.corners.value_or(look.spacing.panelCorners);
      round > 0)
    shell.corners(Corners{round});
  shell.child(std::move(opening));
  if (!chrome.plate.empty())
    shell.child(
        text(chrome.plate, look.style(look.type.eyebrow, look.palette.ash))
            .margin(0, bezel * 0.5f, 0, 0)
            .alignSelf(Align::Center));
  return shell;
}

compose::Element frame(const Frame& chrome) {
  return frame(chrome, compose::box());
}

}  // namespace sigil::sketch::kit
