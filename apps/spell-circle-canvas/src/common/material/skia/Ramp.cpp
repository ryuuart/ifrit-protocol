/** @file
 * The two ramp crossings.
 */

#include "sigilmaterial/skia/Ramp.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPoint.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>
#include <sigilmaterial/skia/Color.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
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

sk_sp<SkImage> paletteImage(const Palette& palette) {
  if (palette.empty()) return nullptr;
  const int n = (int)palette.size();
  SkBitmap bitmap;
  // Straight alpha, so an entry that is partly transparent crosses as the
  // colour it was authored as rather than as that colour already
  // multiplied down.
  bitmap.allocPixels(
      SkImageInfo::Make(n, 1, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType));
  auto byte = [](float v) {
    return (uint32_t)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
  };
  for (int i = 0; i < n; ++i) {
    const Color c = palette.entries[(size_t)i];
    *bitmap.getAddr32(i, 0) =
        byte(c.r) | (byte(c.g) << 8) | (byte(c.b) << 16) | (byte(c.a) << 24);
  }
  bitmap.setImmutable();
  return bitmap.asImage();
}

Paint paletteLookup(const Palette& palette) {
  sk_sp<SkImage> table = paletteImage(palette);
  if (!table) return {};
  return Paint::image(std::move(table), SkTileMode::kClamp, SkTileMode::kClamp,
                      SkMatrix::I(),
                      SkSamplingOptions(SkFilterMode::kNearest));
}

}  // namespace sigil::material::skia
