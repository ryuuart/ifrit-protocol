/** @file
 * The two ramp crossings.
 */

#include "sigilmaterial/skia/Ramp.h"

#include <include/core/SkPoint.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>
#include <sigilmaterial/skia/Color.h>

#include <utility>

namespace sigil::material::skia {

sk_sp<SkShader> verticalRamp(float y0, float y1,
                             const std::vector<RampStop>& ramp) {
  std::vector<SkColor4f> colors;
  std::vector<float> stops;
  colors.reserve(ramp.size());
  stops.reserve(ramp.size());
  for (const RampStop& stop : ramp) {
    colors.push_back(toSkColor(stop.color));
    stops.push_back(stop.pos);
  }
  const SkPoint ends[2] = {{0, y0}, {0, y1}};
  return SkShaders::LinearGradient(ends,
                                   SkGradient({{colors.data(), colors.size()},
                                               {stops.data(), stops.size()},
                                               SkTileMode::kClamp},
                                              {}));
}

Paint unitRamp(const std::vector<RampStop>& ramp) {
  std::vector<Stop> stops;
  stops.reserve(ramp.size());
  for (const RampStop& stop : ramp)
    stops.push_back({stop.pos, toSkColor(stop.color)});
  return Paint::linear({0, 0}, {0, 1}, std::move(stops));
}

}  // namespace sigil::material::skia
