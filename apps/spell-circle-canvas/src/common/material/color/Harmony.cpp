/** @file
 * The hue rotation in OKLCH, and the five sets of angles a harmony is.
 */

#include "sigilmaterial/color/Harmony.h"

namespace sigil::material {

Color rotateHue(const Color& base, float degrees) {
  Oklch lch = toOklch(base);
  lch.hueDegrees += degrees;
  // Fitted rather than clamped: a hue the display cannot show at this
  // chroma gives up chroma, so every member of a set built by turning
  // one colour is still at the hue and the lightness it was asked for.
  return fitToSrgb(lch);
}

Palette harmony(const Color& base, Scheme scheme, float spreadDegrees) {
  Palette set;
  set.entries.push_back(base);
  auto add = [&](float degrees) {
    set.entries.push_back(rotateHue(base, degrees));
  };
  switch (scheme) {
    case Scheme::Complement:
      add(180.0f);
      break;
    case Scheme::SplitComplement:
      add(180.0f - spreadDegrees);
      add(180.0f + spreadDegrees);
      break;
    case Scheme::Analogous:
      add(spreadDegrees);
      add(-spreadDegrees);
      break;
    case Scheme::Triad:
      add(120.0f);
      add(240.0f);
      break;
    case Scheme::Tetrad:
      add(spreadDegrees);
      add(180.0f);
      add(180.0f + spreadDegrees);
      break;
  }
  return set;
}

}  // namespace sigil::material
