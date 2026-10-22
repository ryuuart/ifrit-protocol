#pragma once

/** @file
 * @ingroup material-color
 *
 * TWO COLOURS MIXED IN A NAMED SPACE — CSS's `color-mix()` as a verb:
 * the spaces CSS names, read from their CSS spelling, and the mix that
 * walks between two colours in any one of them.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/color/Ramp.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace sigil::material {

/** THE SPACES CSS NAMES, each spelt as CSS spells it: `srgb`,
 *  `srgb-linear`, `display-p3`, `rec2020`, `a98-rgb`, `prophoto-rgb`,
 *  `xyz-d50`, `xyz-d65` (and `xyz`, the same), `lab`, `lch`, `oklab`,
 *  `oklch`, `hsl` and `hwb`. `Lab` and `Lch` are CSS's, under the D50
 *  white. */
enum class ColorSpace : uint8_t {
  Srgb,
  SrgbLinear,
  DisplayP3,
  Rec2020,
  A98Rgb,
  ProphotoRgb,
  XyzD50,
  XyzD65,
  Lab,
  Lch,
  Oklab,
  Oklch,
  Hsl,
  Hwb,
};

/** The space @p name spells, as CSS spells it and case-insensitively, or
 *  nothing for a name CSS does not give a space. */
std::optional<ColorSpace> readColorSpace(std::string_view name);

/** @p first and @p second mixed in @p space, @p amount of the way from the
 *  first to the second — CSS's `color-mix(in <space>, first, second p%)`
 *  with p as @p amount times 100. The channels mix premultiplied by alpha
 *  and the alpha mixes beside them, as CSS mixes; a polar space walks its
 *  hue along @p arc, and a colour with no hue to speak of (a grey) takes
 *  the other's. A result outside sRGB is brought inside as `readColor`
 *  brings one. */
Color colorMix(const Color& first, const Color& second, float amount,
               ColorSpace space = ColorSpace::Oklab,
               HueArc arc = HueArc::Shorter);

}  // namespace sigil::material
