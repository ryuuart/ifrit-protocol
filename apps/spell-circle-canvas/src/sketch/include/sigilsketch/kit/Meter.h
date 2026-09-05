#pragma once

/** @file
 * A FRACTION DRAWN: along a bar, or around a dial.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilsketch/kit/Theme.h>

#include <optional>
#include <string>

namespace sigil::sketch::kit {

/** A FRACTION ALONG A BAR — a level, a load, a progress. */
struct Meter {
  /** 0 to 1; anything outside is clamped, because a bar past its own end
   *  is a drawing error rather than a reading. */
  float fraction = 0;
  /** Over the bar at the left; empty draws the bar alone. */
  std::u8string label;
  /** Over the bar at the right, in the theme's figure colour — what the
   *  fraction came to, written out. */
  std::u8string reading;
  compose::Dim width;
  /** Unset is the theme's bar height. */
  std::optional<compose::Dim> height;
  /** The empty part; unset is the theme's cell ground. */
  std::optional<compose::Fill> track;
  /** The filled part; unset is the theme's figure colour. */
  std::optional<compose::Fill> bar;
  float corners = 0;
};

/** THE METER.
 *
 *      sketch::kit::meter({.fraction = load, .label = toU8("cache"),
 *                          .reading = toU8("74%"), .width = Dim(220)})
 *
 *  A LIVE fraction is a re-describe, not a binding: the filled part is a
 *  width, and a width is layout. Bind a paint-only reading — an opacity,
 *  a translate — where a meter must move every frame without one. */
[[nodiscard]] compose::Element meter(const Meter& bar);

/** A FRACTION AROUND A DIAL — the same reading where the picture wants a
 *  face rather than a rail. */
struct Gauge {
  /** 0 to 1, clamped. */
  float fraction = 0;
  float diameter = 64;
  /** How thick the ring is. Half the diameter fills it to the middle,
   *  which is a pie rather than a dial. */
  float thickness = 6;
  /** Skia's canvas convention: 0° is +x and the sweep runs clockwise, so
   *  the default starts at the lower left and sweeps three quarters. */
  float startDeg = 135;
  float sweepDeg = 270;
  /** The unswept part; unset is the theme's cell ground. */
  std::optional<compose::Fill> track;
  /** The swept part; unset is the theme's figure colour. */
  std::optional<compose::Fill> bar;
  /** Inside the dial, in the theme's figure colour; empty draws none. */
  std::u8string reading;
};

/** THE DIAL.
 *
 *      sketch::kit::gauge({.fraction = 0.62f, .diameter = 84,
 *                          .reading = toU8("0.62")})
 */
[[nodiscard]] compose::Element gauge(const Gauge& dial);

}  // namespace sigil::sketch::kit
