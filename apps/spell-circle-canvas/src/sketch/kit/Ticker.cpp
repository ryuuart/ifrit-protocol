#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Marquee.h>
#include <sigilcompose/typography/Presets.h>
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
using compose::Text;

compose::Element ticker(Ticker strip) {
  // A number rather than the sheet's property: the caller wraps the phase
  // over the content's width and this gap, so it has to know the gap.
  const float gap = strip.gap.value_or(10.0f);
  Element window = compose::kit::marquee(strip.content,
                                         {.phase = std::move(strip.phase),
                                          .gap = gap,
                                          .contentWidth = strip.contentWidth});
  if (strip.width.unit != Dimension::Unit::Auto) window.width(strip.width);
  if (strip.height.unit != Dimension::Unit::Auto) window.height(strip.height);
  return window;
}

compose::Element timeline(const Timeline& scale) {
  using compose::var;
  const Theme& house = houseTheme();
  const Dimension thickness =
      scale.height ? Dimension(*scale.height) : var("spacing-bar-height");
  const compose::Fill railPaint =
      scale.rail.value_or(compose::var("palette-rule"));
  const Fill inkFill = scale.ink.value_or(compose::var("palette-ash"));
  const Dimension reachOf =
      scale.tick ? Dimension(*scale.tick) : var("spacing-tick-reach");

  // The rail carries the marks, so a mark's position is a percentage of
  // the rail's own resolved width and the scale needs no measurement.
  Element rail = box().height(thickness);
  rail.fill(railPaint);
  if (scale.width.unit != Dimension::Unit::Auto) rail.width(scale.width);
  for (const Timeline::Mark& mark : scale.marks) {
    const Dimension reach = mark.major ? reachOf : reachOf * 0.5f;
    Element tick =
        box()
            .absolute()
            .left(compose::percent(std::clamp(mark.at, 0.0f, 1.0f) * 100))
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
  // The line the words stand on is one and six tenths of the eyebrow's
  // size: the box takes the eyebrow role, so its em is the size the sheet
  // sets an eyebrow in, and the words inside it inherit nothing else.
  Element words = box().role("eyebrow").height(Dimension(weave::em(1.6f)));
  bool any = false;
  for (const Timeline::Mark& mark : scale.marks) {
    if (!mark.major || mark.label.empty()) continue;
    any = true;
    Text label = compose::document::eyebrow(mark.label)
                     .role("eyebrow", house.font(house.type.eyebrow),
                           var("palette-ash"));
    if (mark.ink)
      detail::documentInk(label, *mark.ink);
    else if (scale.ink)
      detail::documentInk(label, *scale.ink);
    words.children(
        {box()
             .absolute()
             .left(compose::percent(std::clamp(mark.at, 0.0f, 1.0f) * 100))
             .width(0)
             .row()
             .justifyContent(compose::Justify::Center)
             .children({std::move(label.flexShrink(0))})});
  }
  if (!scale.below && any) {
    column.children({std::move(words.margin(0, 0, reachOf, 0))});
    column.children({std::move(rail)});
    return column;
  }
  column.children({std::move(rail)});
  if (any) column.children({std::move(words.margin(reachOf, 0, 0, 0))});
  return column;
}

}  // namespace sigil::sketch::kit
