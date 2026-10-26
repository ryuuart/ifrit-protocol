#pragma once

/** @file
 * @ingroup compose-kit
 *
 * SigilCompose KIT — the marquee: text in motion that costs a repaint and
 * never a reflow.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Layout.h>
#include <sigilmotion/values/Animatable.h>

#include <utility>

namespace sigil::compose::kit {

/** How a marquee runs: the phase that slides it, the gap between the two
 *  copies, and whether each copy's width is PINNED.
 *
 *  `contentWidth` above 0 rides each copy in a box of exactly that width,
 *  so text content can never wrap against the clip viewport. Left at 0
 *  the strip resolves its width against the clip box instead, which wraps
 *  a long line to two — measure once (`intrinsicSize(content,
 *  fonts).width()`, or `ctx.measure(strip).width()`) and pin it whenever
 *  the content is text. */
struct MarqueeOptions {
  /** The phase, in px: how far the strip has slid, negative to the left.
   *  The marquee folds it over the content and the gap as they resolve
   *  where it lands, so a phase that keeps running loops without a seam
   *  and its driver need not know how long the loop is; a phase the
   *  driver keeps within [-(width + gap), 0] draws as it stands. */
  motion::Animatable<float> phase;
  /** Between the two copies: any length, so a custom property may state
   *  it, which the loop's period reads where the marquee lands. */
  Dimension gap = 0.0f;
  float contentWidth = 0.0f;
};

/** The seamless ticker (news crawl, y2k status bar): `content` twice in a
 *  row inside a clipped box, slid by the phase. Binding translateX is
 *  paint-only volatility: the strip's recording replays every frame,
 *  nothing re-records. Keep `content` keyless (it mounts twice). */
inline Element marquee(const Element& content, MarqueeOptions how) {
  const bool pin = how.contentWidth > 0;
  // ONE LOOP IS ONE COPY AND THE GAP: the pinned width and the gap, or —
  // where the copies size themselves — half the strip, which holds two
  // copies and one gap, and half the gap more.
  const Dimension period = pin ? Dimension(how.contentWidth) + how.gap
                               : percent(50) + how.gap * 0.5f;
  auto copy = [&] {
    return pin ? box()
                     .width(Dimension(how.contentWidth))
                     .flexShrink(0)
                     .children({content})
               : content;
  };
  return box()
      .overflow(Overflow::Clip)
      .children({box()
                     .row()
                     .gap(how.gap)
                     .flexShrink(0)
                     .alignSelf(Align::Start)
                     .translateX(std::move(how.phase), period)
                     .children({copy()})
                     .children({copy()})});
}

}  // namespace sigil::compose::kit
