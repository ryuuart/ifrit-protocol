#pragma once

/** @file
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
  /** The caller-owned WRAPPING phase, in px. Step it over
   *  [-(width + gap), 0] and the loop is invisible. */
  motion::Animatable<float> phase;
  float gap = 0.0f;
  float contentWidth = 0.0f;
};

/** The seamless ticker (news crawl, y2k status bar): `content` twice in a
 *  row inside a clipped box, slid by the phase. Binding translateX is
 *  paint-only volatility: the strip's recording replays every frame,
 *  nothing re-records. Keep `content` keyless (it mounts twice). */
inline Element marquee(const Element& content, MarqueeOptions how) {
  const bool pin = how.contentWidth > 0;
  auto copy = [&] {
    return pin ? box()
                     .width(Dimension(how.contentWidth))
                     .shrink(0)
                     .child(content)
               : content;
  };
  return box().clip(true).child(box()
                                    .row()
                                    .gap(how.gap)
                                    .shrink(0)
                                    .alignSelf(Align::Start)
                                    .translateX(std::move(how.phase))
                                    .child(copy())
                                    .child(copy()));
}

}  // namespace sigil::compose::kit
