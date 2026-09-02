/** @file
 * Reading an image out at the width a device texture wants it.
 */

#include <sigilskia/graphite/Pixels.h>

#include <include/core/SkColorType.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>

namespace sigil::skia {

namespace {

/** The pixels read back into @p type, tightly packed. Empty when the
 *  image cannot answer. */
template <class T>
std::vector<T> read(const sk_sp<SkImage>& image, SkColorType type,
                    int valuesPerTexel) {
  if (!image) return {};
  const int w = image->width(), h = image->height();
  if (w <= 0 || h <= 0) return {};
  std::vector<T> out((size_t)w * h * valuesPerTexel);
  const SkImageInfo info =
      SkImageInfo::Make(w, h, type, kPremul_SkAlphaType);
  const SkPixmap pixmap(info, out.data(),
                        (size_t)w * valuesPerTexel * sizeof(T));
  if (!image->readPixels(nullptr, pixmap, 0, 0)) return {};
  return out;
}

}  // namespace

bool isFloatImage(const sk_sp<SkImage>& image) {
  if (!image) return false;
  const SkColorType type = image->colorType();
  return type == kRGBA_F32_SkColorType || type == kRGBA_F16_SkColorType ||
         type == kRGBA_F16Norm_SkColorType;
}

std::vector<uint16_t> halfFloatPixels(const sk_sp<SkImage>& image) {
  return read<uint16_t>(image, kRGBA_F16_SkColorType, 4);
}

std::vector<uint8_t> bytePixels(const sk_sp<SkImage>& image) {
  return read<uint8_t>(image, kRGBA_8888_SkColorType, 4);
}

}  // namespace sigil::skia
