/** @file
 * Artwork in, image out: the one place a brush format meets a codec.
 */

#include "Images.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <sigilimage/decode/Decode.h>

#include <cstdint>

namespace sigil::draw::brush::format {

sk_sp<SkImage> decodeArtwork(std::span<const std::byte> bytes) {
  if (bytes.empty()) return nullptr;
  std::optional<sigil::image::ImageAsset> asset =
      sigil::image::decodeImage(bytes.data(), bytes.size());
  if (!asset || asset->frames().empty()) return nullptr;
  return asset->frames().front().image;
}

sk_sp<SkImage> coverageImage(std::span<const uint8_t> coverage, int width,
                             int height) {
  if (width <= 0 || height <= 0) return nullptr;
  if (coverage.size() < (size_t)width * (size_t)height) return nullptr;
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(
          SkImageInfo::Make(width, height, kAlpha_8_SkColorType,
                            kPremul_SkAlphaType)))
    return nullptr;
  for (int y = 0; y < height; ++y) {
    uint8_t* row = bitmap.getAddr8(0, y);
    for (int x = 0; x < width; ++x) row[x] = coverage[(size_t)y * width + x];
  }
  bitmap.setImmutable();
  return SkImages::RasterFromBitmap(bitmap);
}

}  // namespace sigil::draw::brush::format
