/** @file
 * The colour value's packed spelling, and the walk around the hue wheel.
 */

#include "sigilmaterial/color/Color.h"

namespace sigil::material {

Color rgb(uint32_t hex, float a) {
  return {(float)((hex >> 16u) & 0xffu) / 255.0f,
          (float)((hex >> 8u) & 0xffu) / 255.0f, (float)(hex & 0xffu) / 255.0f,
          a};
}

Color hsv(float hueDegrees, float saturation, float value, float a) {
  float h = std::fmod(hueDegrees, 360.0f);
  if (h < 0.0f) h += 360.0f;
  const float s = std::clamp(saturation, 0.0f, 1.0f);
  const float v = std::clamp(value, 0.0f, 1.0f);
  const float chroma = v * s;
  const float sector = h / 60.0f;
  // The ramp across one sixth of the wheel: full at the two primaries
  // that bound the sector, zero at the secondary between them.
  const float ramp =
      chroma * (1.0f - std::abs(std::fmod(sector, 2.0f) - 1.0f));
  const float base = v - chroma;
  float r = 0, g = 0, b = 0;
  switch ((int)sector) {
    case 0:
      r = chroma, g = ramp;
      break;
    case 1:
      r = ramp, g = chroma;
      break;
    case 2:
      g = chroma, b = ramp;
      break;
    case 3:
      g = ramp, b = chroma;
      break;
    case 4:
      r = ramp, b = chroma;
      break;
    default:
      r = chroma, b = ramp;
      break;
  }
  return {r + base, g + base, b + base, a};
}

}  // namespace sigil::material
