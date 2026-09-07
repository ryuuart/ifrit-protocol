/** @file
 * The two named skies. Each is one radiance function handed to
 * `EnvironmentMap::baked()`; nothing here knows how a panorama is
 * blurred or convolved.
 */

#include "sigilmaterial/kit/Environments.h"

#include <algorithm>
#include <cmath>

namespace sigil::material::kit {

namespace {

float gauss(float x, float sigma) {
  return std::exp(-(x * x) / (2 * sigma * sigma));
}

/** Angular distance on the equirect u axis (wraps). */
float alongU(float a, float b) {
  const float d = std::abs(a - b);
  return std::min(d, 1.0f - d);
}

}  // namespace

EnvironmentMap studioEnvironment(int width) {
  return EnvironmentMap::baked(width, [](float u, float v) -> SkV3 {
    // Graded neutral shell: bright zenith, dim floor.
    const float sky = std::pow(std::clamp(1.0f - v, 0.0f, 1.0f), 1.4f);
    SkV3 c = {0.10f + 0.55f * sky, 0.11f + 0.56f * sky, 0.13f + 0.60f * sky};
    // Floor bounce card below the horizon.
    if (v > 0.62f) {
      const float f = gauss(v - 0.78f, 0.10f) * 0.5f;
      c += {f * 0.9f, f * 0.85f, f * 0.75f};
    }
    // Three softboxes: key, fill, rim strip.
    const float key =
        gauss(alongU(u, 0.30f), 0.055f) * gauss(v - 0.30f, 0.09f) * 3.2f;
    const float fill =
        gauss(alongU(u, 0.72f), 0.075f) * gauss(v - 0.38f, 0.12f) * 1.1f;
    const float rim =
        gauss(alongU(u, 0.02f), 0.02f) * gauss(v - 0.22f, 0.20f) * 2.2f;
    c += {key + fill + rim, key + fill + rim, key + fill + rim};
    return c;
  });
}

EnvironmentMap sunsetEnvironment(int width) {
  return EnvironmentMap::baked(width, [](float u, float v) -> SkV3 {
    const float horizon = 0.52f;
    if (v < horizon) {
      // Banded sky falling toward a hot horizon stripe.
      const float t = v / horizon;  // 0 zenith -> 1 horizon
      SkV3 top = {0.05f, 0.10f, 0.30f};
      SkV3 low = {1.05f, 0.45f, 0.15f};
      SkV3 c = top + (low - top) * std::pow(t, 1.6f);
      // The classic chrome bands.
      const float band =
          0.5f + 0.5f * std::sin(t * 40.0f + std::cos(t * 13.0f));
      c *= 0.82f + 0.18f * band * std::pow(t, 2.0f);
      // Sun blob.
      const float sun =
          gauss(alongU(u, 0.5f), 0.035f) * gauss(v - horizon + 0.06f, 0.045f);
      c += SkV3{2.6f, 1.6f, 0.7f} * sun;
      // Horizon flare line.
      c += SkV3{1.6f, 0.9f, 0.45f} * gauss(v - horizon, 0.008f);
      return c;
    }
    // Ground: near-black violet falling away.
    const float t = (v - horizon) / (1.0f - horizon);
    SkV3 c = {0.16f, 0.05f, 0.16f};
    c = c * (1.0f - t * 0.8f);
    c += SkV3{0.5f, 0.2f, 0.4f} * gauss(v - horizon, 0.02f);
    return c;
  });
}

}  // namespace sigil::material::kit
