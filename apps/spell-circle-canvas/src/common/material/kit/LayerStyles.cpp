/** @file
 * The gel and chrome colour tables, and the contour table a satin band
 * remaps blurred coverage through.
 */

#include "sigilmaterial/kit/LayerStyles.h"

#include <algorithm>
#include <cmath>

namespace sigil::material::kit {

Color scaled(Color c, float k, float a) {
  return {c.r * k, c.g * k, c.b * k, a};
}

Color toward(Color c, Color target, float t, float a) {
  return {c.r + (target.r - c.r) * t, c.g + (target.g - c.g) * t,
          c.b + (target.b - c.b) * t, a};
}

std::vector<RampStop> aquaBodyRamp(Color tint) {
  return {{0.0f, scaled(tint, 0.72f, 0.9f)},
          {0.55f, tint},
          {1.0f, toward(tint, {1, 1, 1, 1}, 0.35f, 0.95f)}};
}

std::vector<RampStop> aquaGlowRamp(Color tint, float strength) {
  return {{0.0f, {1, 1, 1, 0}},
          {1.0f, toward(tint, {1, 1, 1, 1}, 0.80f, strength)}};
}

Color aquaHalo(Color tint) { return toward(tint, {1, 1, 1, 1}, 0.30f, 0.5f); }
Color aquaTopBand(Color tint) { return scaled(tint, 0.36f, 0.45f); }
Color aquaHairline(Color tint) { return scaled(tint, 0.45f, 0.6f); }

std::vector<RampStop> chromeRamp(ChromePalette palette) {
  if (palette == ChromePalette::Silver)
    return {{0.0f, rgb(0xFDFDFD)},  {0.2f, rgb(0xD2D8DD)},
            {0.48f, rgb(0xA5ADB5)}, {0.5f, rgb(0x6F7880)},
            {0.52f, rgb(0xE9ECEF)}, {0.8f, rgb(0xC6CDD3)},
            {1.0f, rgb(0x9BA3AC)}};
  return {{0.0f, rgb(0xF4F7FA)},  {0.35f, rgb(0x97A1AC)},
          {0.49f, rgb(0x3A4654)}, {0.51f, rgb(0x1E2833)},
          {0.62f, rgb(0x5C6B7C)}, {1.0f, rgb(0xDCE4EA)}};
}

std::array<uint8_t, 256> contourRing(float center, float width) {
  std::array<uint8_t, 256> table{};
  for (int i = 0; i < 256; ++i) {
    const float a = (float)i / 255.0f;
    const float d = std::abs(a - center) / std::max(0.05f, width * 0.5f);
    const float peak = std::max(0.0f, 1.0f - d);
    // The cubic ease, spelled here rather than reached for: this table is
    // data with no clock behind it.
    const float eased = peak * peak * (3.0f - 2.0f * peak);
    table[(size_t)i] = (uint8_t)std::lround(255.0f * eased);
  }
  return table;
}

}  // namespace sigil::material::kit
