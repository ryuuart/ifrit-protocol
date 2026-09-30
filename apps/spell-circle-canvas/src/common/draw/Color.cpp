/** @file
 * p5's colour arguments: the models and their ranges, and the CSS string.
 */

#include <sigildraw/Color.h>
#include <sigilmaterial/color/Color.h>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace sigil::draw {

namespace {

float unit(float v, float max) {
  if (!(max > 0.0f)) return 0.0f;
  return std::clamp(v / max, 0.0f, 1.0f);
}

/** Hue in [0, 1), saturation and value in [0, 1] to RGB. */
material::Color fromHsb(float h, float s, float v, float a) {
  h = h - std::floor(h);
  const float c = v * s;
  const float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
  const float m = v - c;
  float r = 0, g = 0, b = 0;
  const int sector = (int)std::floor(h * 6.0f) % 6;
  switch (sector) {
    case 0:
      r = c;
      g = x;
      break;
    case 1:
      r = x;
      g = c;
      break;
    case 2:
      g = c;
      b = x;
      break;
    case 3:
      g = x;
      b = c;
      break;
    case 4:
      r = x;
      b = c;
      break;
    default:
      r = c;
      b = x;
      break;
  }
  return {r + m, g + m, b + m, a};
}

/** Hue in [0, 1), saturation and lightness in [0, 1] to RGB. */
material::Color fromHsl(float h, float s, float l, float a) {
  const float c = (1.0f - std::fabs(2.0f * l - 1.0f)) * s;
  const float v = l + c / 2.0f;
  const float sv = v > 0.0f ? c / v : 0.0f;
  return fromHsb(h, sv, v, a);
}

}  // namespace

ColorMode ColorMode::standard(Constant mode) {
  if (mode == HSB || mode == HSL) return {mode, 360.0f, 100.0f, 100.0f, 1.0f};
  return {RGB, 255.0f, 255.0f, 255.0f, 255.0f};
}

material::Color colorFrom(const ColorMode& mode, float v1, float v2, float v3,
                          float alpha) {
  const float a = unit(alpha, mode.maxA);
  const float c2 = unit(v2, mode.max2);
  const float c3 = unit(v3, mode.max3);
  switch (mode.mode) {
    case HSB:
      return fromHsb(unit(v1, mode.max1), c2, c3, a);
    case HSL:
      return fromHsl(unit(v1, mode.max1), c2, c3, a);
    default:
      return {unit(v1, mode.max1), c2, c3, a};
  }
}

material::Color colorFrom(const ColorMode& mode, float gray, float alpha) {
  const float g = unit(gray, mode.max3);
  return {g, g, g, unit(alpha, mode.maxA)};
}

material::Color parseColor(std::string_view css) {
  return material::parseColor(css);
}

}  // namespace sigil::draw
