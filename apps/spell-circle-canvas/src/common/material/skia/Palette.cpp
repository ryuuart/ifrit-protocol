/** @file
 * A picture read down to a run of straight sRGB colours, and handed to
 * the colour leaf's extraction.
 */

#include "sigilmaterial/skia/Palette.h"

#include <include/core/SkColorSpace.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSamplingOptions.h>

#include <algorithm>
#include <vector>

namespace sigil::material::skia {

Palette palette(const sk_sp<SkImage>& image, const PaletteOptions& options,
                int longestSide) {
  if (!image) return {};
  const int width = image->width(), height = image->height();
  if (width <= 0 || height <= 0) return {};

  int readWidth = width, readHeight = height;
  if (longestSide > 0 && std::max(width, height) > longestSide) {
    const float factor = (float)longestSide / (float)std::max(width, height);
    readWidth = std::max(1, (int)std::lround((double)width * factor));
    readHeight = std::max(1, (int)std::lround((double)height * factor));
  }

  // STRAIGHT alpha and the sRGB space the colour leaf's value is defined
  // in: a premultiplied read would fold every transparent pixel's colour
  // towards black, and those are exactly the pixels the extraction is
  // about to weigh or drop.
  const SkImageInfo info =
      SkImageInfo::Make(readWidth, readHeight, kRGBA_F32_SkColorType,
                        kUnpremul_SkAlphaType, SkColorSpace::MakeSRGB());
  std::vector<float> channels((size_t)readWidth * (size_t)readHeight * 4);
  const SkPixmap pixmap(info, channels.data(), (size_t)readWidth * 4 * sizeof(float));
  const bool read =
      readWidth == width && readHeight == height
          ? image->readPixels(nullptr, pixmap, 0, 0)
          : image->scalePixels(pixmap, SkSamplingOptions(SkFilterMode::kLinear,
                                                         SkMipmapMode::kLinear));
  if (!read) return {};

  std::vector<Color> colors((size_t)readWidth * (size_t)readHeight);
  for (size_t i = 0; i < colors.size(); ++i)
    colors[i] = Color{channels[i * 4], channels[i * 4 + 1], channels[i * 4 + 2],
                      channels[i * 4 + 3]};
  return material::palette(colors, options);
}

}  // namespace sigil::material::skia
