#include <sigilmaterial/field/Crt.h>
#include <sigilmaterial/kit/Crt.h>

namespace sigil::material::kit {

Material crt(const SkRect& bounds, float seconds) {
  return field::crt({
      .uBounds = {bounds.left(), bounds.top(), bounds.width(), bounds.height()},
      .uCurvature = 0.055f,
      .uRgbShift = 0.8f,
      .uScanPitch = 3.0f,
      .uRaster = 0.45f,
      .uBloomRadius = 2.4f,
      .uBloom = 0.38f,
      .uNoise = 0.012f,
      .uVignette = 0.22f,
      .uTime = seconds,
  });
}

}  // namespace sigil::material::kit
