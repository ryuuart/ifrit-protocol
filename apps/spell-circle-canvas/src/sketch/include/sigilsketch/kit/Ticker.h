#pragma once

/** @file
 * THINGS ALONG AN AXIS: the strip that crawls past a window, and the rail
 * a scale is marked off on.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilmotion/values/Animated.h>
#include <sigilsketch/kit/Theme.h>

#include <optional>
#include <string>
#include <vector>

namespace sigil::sketch::kit {

/** THE CRAWL: one strip of content, run past a window twice so the loop
 *  has no seam. */
struct Ticker {
  /** What crawls. Keep it KEYLESS — it is mounted twice. */
  compose::Element content;
  /** The strip's own width, measured once with `ctx.measure(strip).width()`.
   *  0 lets the copies size themselves, which a text strip must not do:
   *  unpinned, it resolves against the window and wraps to two lines. */
  float contentWidth = 0;
  /** The offset in px, wrapped over [-(contentWidth + gap), 0] by
   *  whoever owns it. Binding it is paint-only: the strip's recording
   *  replays and nothing re-records. */
  motion::Animatable<float> phase;
  /** Between the two copies; unset is the theme's label gap. */
  std::optional<float> gap;
  compose::Dim width;
  compose::Dim height;
};

/** THE CRAWL, clipped to its window.
 *
 *      sketch::kit::ticker({.content = strip, .contentWidth = w,
 *                           .phase = bind(&crawl), .width = Dim(320)})
 */
[[nodiscard]] compose::Element ticker(Ticker strip);

/** A SCALE MARKED OFF ALONG A RAIL: a bar with ticks on it and the words
 *  that name them. */
struct Timeline {
  /** One mark: where it stands along the rail, and what it is called. */
  struct Mark {
    /** 0 at the rail's start, 1 at its end. */
    float at = 0;
    /** Empty draws the tick and no word. */
    std::u8string label;
    /** A minor mark's tick is half as long and its word is absent. */
    bool major = true;
  };
  std::vector<Mark> marks;
  compose::Dim width;
  /** Unset is the theme's bar height. */
  std::optional<float> height;
  /** How far a major tick reaches past the rail. */
  float tick = 5;
  /** The rail; unset is the theme's rule colour. */
  std::optional<compose::Fill> rail;
  /** The ticks and the words; unset is the theme's ash. */
  std::optional<compose::Fill> ink;
  /** true puts the ticks and words under the rail, false over it. */
  bool below = true;
};

/** THE SCALE.
 *
 *      sketch::kit::timeline({.marks = {{0, u8"0 ms"}, {0.5f, {}, false},
 *                                       {1, u8"640 ms"}},
 *                             .width = Dim(420)})
 *
 *  A mark's word is CENTRED on its tick and clipped by nothing, so a word
 *  at 0 or at 1 reaches past the rail's end by half its own width — which
 *  is what a scale's end labels do, and why the rail is usually inset
 *  from the column it stands in. */
[[nodiscard]] compose::Element timeline(const Timeline& scale);

}  // namespace sigil::sketch::kit
