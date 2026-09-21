#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Marquee.h>
#include <sigilsketch/kit/Ticker.h>

#include <algorithm>
#include <utility>

#include "DocumentInk.h"

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Dimension;
using compose::Element;
using compose::Fill;

compose::Element ticker(Ticker strip) {
  const Theme& look = theme();
  const float gap = strip.gap.value_or(look.spacing.labelGap);
  Element window = compose::kit::marquee(strip.content,
                                         {.phase = std::move(strip.phase),
                                          .gap = gap,
                                          .contentWidth = strip.contentWidth});
  if (strip.width.unit != Dimension::Unit::Auto) window.width(strip.width);
  if (strip.height.unit != Dimension::Unit::Auto) window.height(strip.height);
  return window;
}

compose::Element timeline(const Timeline& scale) {
  const Theme& look = theme();
  const float thickness = scale.height.value_or(look.spacing.barHeight);
  const compose::SurfacePaint railPaint =
      scale.rail.value_or(Fill::color(look.palette.rule));
  const Fill inkFill = scale.ink.value_or(Fill::color(look.palette.ash));
  const float reachOf = scale.tick.value_or(look.spacing.tickReach);

  // The rail carries the marks, so a mark's position is a percentage of
  // the rail's own resolved width and the scale needs no measurement.
  Element rail = box().height(thickness);
  railPaint.apply(rail);
  if (scale.width.unit != Dimension::Unit::Auto) rail.width(scale.width);
  for (const Timeline::Mark& mark : scale.marks) {
    const float reach = mark.major ? reachOf : reachOf * 0.5f;
    Element tick =
        box()
            .absolute()
            .left(compose::pct(std::clamp(mark.at, 0.0f, 1.0f) * 100))
            .width(1)
            .height(reach)
            .fill(inkFill);
    if (scale.below)
      tick.top(thickness);
    else
      tick.top(-reach);
    rail.children({std::move(tick)});
  }
  Element column = box().column();
  if (scale.width.unit != Dimension::Unit::Auto) column.width(scale.width);

  // Each word rides in a ZERO-WIDTH box pinned at its tick and centred
  // inside it, so it overhangs equally on both sides whatever it says —
  // which needs no measurement, and is what puts a word at 0 half outside
  // the rail, as a scale's end labels are.
  Element words = box().height(look.type.eyebrow.size * 1.6f);
  bool any = false;
  for (const Timeline::Mark& mark : scale.marks) {
    if (!mark.major || mark.label.empty()) continue;
    any = true;
    Element label = compose::document::eyebrow(mark.label)
                        .role(weave::rule("eyebrow").font(
                            look.font(look.type.eyebrow, look.palette.ash)));
    if (mark.ink)
      detail::documentInk(label, *mark.ink);
    else if (scale.ink)
      detail::documentInk(label, *scale.ink);
    words.children(
        {box()
             .absolute()
             .left(compose::pct(std::clamp(mark.at, 0.0f, 1.0f) * 100))
             .width(0)
             .row()
             .justifyContent(compose::Justify::Center)
             .children({std::move(label.flexShrink(0))})});
  }
  if (!scale.below && any) {
    column.children({std::move(
        words.margin(0, 0, reachOf, 0))});
    column.children({std::move(rail)});
    return column;
  }
  column.children({std::move(rail)});
  if (any)
    column.children({std::move(
        words.margin(reachOf, 0, 0, 0))});
  return column;
}

}  // namespace sigil::sketch::kit
