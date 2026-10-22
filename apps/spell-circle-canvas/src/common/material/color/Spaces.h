#pragma once

/** @file
 * The crossings between `Color` and every space CSS names, shared by the
 * reader of a colour's text and the mix in a named space. Private to the
 * colour feature.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/color/Mix.h>

namespace sigil::material::spaces {

/** Three components in some space, in that space's own CSS reference
 *  ranges: unit channels for the RGB and XYZ spaces, a hue in degrees
 *  first and two unit fractions for `hsl` and `hwb`, lightness on 0..100
 *  for `lab` and `lch` and on 0..1 for `oklab` and `oklch`, with the hue
 *  third for the two polar ones. */
struct Components {
  double first = 0, second = 0, third = 0;
};

/** @p color in @p space. */
Components toSpace(ColorSpace space, const Color& color);

/** The colour @p components stand for in @p space, at @p alpha clamped to
 *  0..1. Anything outside sRGB is brought inside by reducing its OKLCH
 *  chroma; `Srgb` keeps its channels as given, a channel above 1 being a
 *  legal float colour. */
Color fromSpace(ColorSpace space, Components components, float alpha);

/** Which of the three components is a hue in @p space: 0 for `hsl` and
 *  `hwb`, 2 for `lch` and `oklch`, and -1 where there is none. */
int hueIndex(ColorSpace space);

/** Whether @p components stand for a colour whose hue says nothing — a
 *  grey — in a polar @p space. */
bool hueIsPowerless(ColorSpace space, const Components& components);

}  // namespace sigil::material::spaces
