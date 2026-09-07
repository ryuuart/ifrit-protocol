/** @file
 * The Skia encoders behind the routing surface: PNG, JPEG and WebP.
 */

#include <include/core/SkData.h>
#include <include/core/SkPixmap.h>
#include <include/encode/SkJpegEncoder.h>
#include <include/encode/SkPngEncoder.h>
#include <include/encode/SkWebpEncoder.h>

#include <algorithm>

#include "Backends.h"

namespace sigil::image::backend {

sk_sp<SkData> encodeWithSkia(const SkPixmap& pixels, Format format,
                             const EncodeOptions& options) {
  const int quality = std::clamp(options.quality, 0, 100);
  switch (format) {
    case Format::Png:
      return SkPngEncoder::Encode(pixels, SkPngEncoder::Options{});
    case Format::Jpeg: {
      SkJpegEncoder::Options jpeg;
      jpeg.fQuality = quality;
      return SkJpegEncoder::Encode(pixels, jpeg);
    }
    case Format::Webp: {
      SkWebpEncoder::Options webp;
      // At 100 the two WebP codecs part company: lossless spends the
      // quality number on effort rather than on fidelity, and a caller
      // asking for everything wants the pixels back unchanged.
      webp.fCompression = quality >= 100 ? SkWebpEncoder::Compression::kLossless
                                         : SkWebpEncoder::Compression::kLossy;
      webp.fQuality = (float)quality;
      return SkWebpEncoder::Encode(pixels, webp);
    }
    case Format::Exr:
      return nullptr;  // not Skia's; the OIIO backend writes it
  }
  return nullptr;
}

}  // namespace sigil::image::backend
