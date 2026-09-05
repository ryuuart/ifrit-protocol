#pragma once

/** @file
 * Y2K CHROME: one era's look, as a bundle of this library's mechanisms
 * over SigilMaterial's chrome palettes. A drop shadow, the palette's
 * vertical ramp with its hard stop at the horizon, a white specular
 * sliver straddling that horizon, a chisel bevel, and a dark keyline
 * stroked outside the silhouette.
 *
 * `material::kit::kChromeHorizonFrac` is where the hard stop sits, as a
 * fraction of the node's height: position hand-added glints against it
 * times the height and they stay on the horizon at any size.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Shape.h>
#include <sigilmaterial/kit/LayerStyles.h>
#include <sigilmaterial/skia/Paint.h>

namespace sigil::compose::kit {

/** The bundle's knobs. */
struct ChromeOptions {
  using Palette = material::kit::ChromePalette;
  Palette palette = Palette::Steel;
  bool horizonSliver = true;  ///< white specular sliver straddling 50%
  float keylineWidth = 2.0f;
  material::Color keyline = material::rgb(0x10141A);
  float bevelDepth = 3.0f, bevelSize = 5.0f;
  bool operator==(const ChromeOptions&) const = default;
};

/** The chrome body: the palette's vertical ramp, with its hard stop at the
 *  horizon, drawn through the shape's outline. */
struct ChromeBody {
  ChromeOptions::Palette palette = ChromeOptions::Palette::Steel;
  bool operator==(const ChromeBody&) const = default;

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** The finishing pass: a 1 px white top edge plus the white specular sliver
 *  straddling the horizon, both clipped inside the shape.
 *
 *  The sliver FADES OUT at both ends, and must. A specular band drawn as a
 *  hard rectangle ends in two blunt vertical stubs, and on a chrome
 *  wordmark — where the glyphs already chop the band into segments — those
 *  stubs read as an unfinished strikethrough rather than as light. */
struct ChromeSliver {
  float horizonFrac = material::kit::kChromeHorizonFrac;
  /** Fraction of the width the highlight takes to reach full strength. */
  float falloff = 0.22f;
  bool operator==(const ChromeSliver&) const = default;

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** The drop-in chrome bundle: drop shadow, palette ramp, horizon sliver,
 *  chisel bevel, and a dark keyline stroked OUTSIDE the silhouette. The
 *  Silver palette skips the dark inner top band, which would fight its
 *  white top edge. */
LayerStyle y2kChrome(ChromeOptions opts = {});

}  // namespace sigil::compose::kit
