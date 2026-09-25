/** @file
 * The palette's two crossings: an N x 1 image and the nearest-sampled
 * material over it.
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
  return image(std::move(table), Repeat::Pad, Repeat::Pad,
                      SkMatrix::I(), SkSamplingOptions(SkFilterMode::kNearest));
}

}  // namespace sigil::material::skia
