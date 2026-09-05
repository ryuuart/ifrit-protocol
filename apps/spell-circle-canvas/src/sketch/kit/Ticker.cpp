#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilsketch/kit/Ticker.h>

#include <algorithm>
#include <utility>

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Dim;
using compose::Element;
using compose::Fill;
using compose::text;

compose::Element ticker(Ticker strip) {
  const Theme& look = theme();
  const float gap = strip.gap.value_or(look.spacing.labelGap);
  Element window =
      strip.contentWidth > 0
          ? compose::kit::marquee(std::move(strip.content), strip.contentWidth,
                                  strip.phase, gap)
          : compose::kit::marquee(strip.content, strip.phase, gap);
  if (strip.width.unit != Dim::Unit::Auto) window.width(strip.width);
  if (strip.height.unit != Dim::Unit::Auto) window.height(strip.height);
  return window;
}

compose::Element timeline(const Timeline& scale) {
  const Theme& look = theme();
  const float thickness = scale.height.value_or(look.spacing.barHeight);
  const Fill railFill =
      scale.rail.value_or(Fill::color(look.palette.rule));
  const Fill inkFill = scale.ink.value_or(Fill::color(look.palette.ash));

  // The rail carries the marks, so a mark's position is a percentage of
  // the rail's own resolved width and the scale needs no measurement.
  Element rail = box().height(Dim(thickness)).fill(railFill);
  if (scale.width.unit != Dim::Unit::Auto) rail.width(scale.width);
  for (const Timeline::Mark& mark : scale.marks) {
    const float reach = mark.major ? scale.tick : scale.tick * 0.5f;
    Element tick = box()
                       .absolute()
                       .left(compose::pct(std::clamp(mark.at, 0.0f, 1.0f) * 100))
                       .width(Dim(1))
                       .height(Dim(reach))
                       .fill(inkFill);
    if (scale.below)
      tick.top(Dim(thickness));
    else
      tick.top(Dim(-reach));
    rail.child(std::move(tick));
  }
  Element column = box().column();
  if (scale.width.unit != Dim::Unit::Auto) column.width(scale.width);

  // Each word rides in a ZERO-WIDTH box pinned at its tick and centred
  // inside it, so it overhangs equally on both sides whatever it says —
  // which needs no measurement, and is what puts a word at 0 half outside
  // the rail, as a scale's end labels are.
  Element words = box().height(Dim(look.type.eyebrow.size * 1.6f));
  bool any = false;
  for (const Timeline::Mark& mark : scale.marks) {
    if (!mark.major || mark.label.empty()) continue;
    any = true;
    words.child(
        box()
            .absolute()
            .left(compose::pct(std::clamp(mark.at, 0.0f, 1.0f) * 100))
            .width(Dim(0))
            .row()
            .justify(compose::Justify::Center)
            .child(text(mark.label,
                        look.style(look.type.eyebrow, look.palette.ash))
                       .shrink(0)));
  }
  if (!scale.below && any) {
    column.child(std::move(words.margin(0, 0, 0, scale.tick)));
    column.child(std::move(rail));
    return column;
  }
  column.child(std::move(rail));
  if (any) column.child(std::move(words.margin(0, scale.tick, 0, 0)));
  return column;
}

}  // namespace sigil::sketch::kit
