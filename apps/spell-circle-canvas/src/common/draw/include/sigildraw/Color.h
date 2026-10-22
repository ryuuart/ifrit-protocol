#pragma once

/** @file
 * @ingroup draw-pen
 *
 * How p5 reads numbers and strings as a colour: the colour model with the
 * range of each channel, and the parse of a CSS colour string.
 */

#include <include/core/SkColor.h>
#include <sigildraw/Constants.h>
#include <sigilmaterial/color/Color.h>

#include <string_view>

namespace sigil::draw {

/** THE COLOUR MODEL NUMBERS ARE READ IN — p5's `colorMode`: RGB, HSB or
 *  HSL, and the value each channel's maximum stands for. p5's defaults
 *  are RGB over 255 on every channel; `colorMode(HSB)` and
 *  `colorMode(HSL)` with no maxima read hue over 360, the two others over
 *  100 and alpha over 1, as p5 does. */
struct ColorMode {
  Constant mode = RGB;
  float max1 = 255.0f;
  float max2 = 255.0f;
  float max3 = 255.0f;
  float maxA = 255.0f;

  /** p5's maxima for @p mode: 255 across for RGB, 360/100/100/1 for the
   *  two hue models. */
  static ColorMode standard(Constant mode);
  bool operator==(const ColorMode&) const = default;
};

/** Three channels and an alpha, read in @p mode. Values are clamped to
 *  their channel's range. */
material::Color colorFrom(const ColorMode& mode, float v1, float v2, float v3,
                          float alpha);

/** A grey and an alpha, read in @p mode: the grey is taken over the
 *  third channel's maximum, which is the brightness or lightness of a
 *  hue model and the blue of RGB, exactly as p5 reads it. */
material::Color colorFrom(const ColorMode& mode, float gray, float alpha);

/** Alpha at @p mode's own maximum. */
inline material::Color colorFrom(const ColorMode& mode, float v1, float v2,
                                 float v3) {
  return colorFrom(mode, v1, v2, v3, mode.maxA);
}
/** A grey at @p mode's own alpha maximum. */
inline material::Color colorFrom(const ColorMode& mode, float gray) {
  return colorFrom(mode, gray, mode.maxA);
}

/** A CSS colour string as p5 accepts one, read by
 *  `sigil::material::parseColor`, the library that owns colour, so a
 *  string means the same colour to a pen as to a material: the CSS p5's
 *  `color()` reads — hex, the CSS names, `rgb()`, `rgba()`, `hsl()`,
 *  `hsla()` — and the rest of CSS Color 4; p5's own `hsb()` is not CSS
 *  and is not read. Anything else is opaque black
 *  and reported once; p5's canvas keeps its previous style instead. */
material::Color parseColor(std::string_view css);

}  // namespace sigil::draw
