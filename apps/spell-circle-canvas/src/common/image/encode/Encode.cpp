/** @file
 * The routing encode: pixels and a format in, encoded bytes out, with
 * the CPU readback an SkImage needs before any encoder can see it.
 */

#include "sigilimage/encode/Encode.h"

#include <include/core/SkColorSpace.h>
#include <include/core/SkColorType.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "Backends.h"

namespace sigil::image {

namespace {

/** The colour type a readback lands in for @p format: the depth the
 *  format can hold, so nothing is thrown away on the way in and nothing
 *  is carried that the encoder would drop. */
SkColorType readbackType(Format format) {
  return format == Format::Exr ? kRGBA_F32_SkColorType : kN32_SkColorType;
}

}  // namespace

sk_sp<SkData> encodeImage(const SkPixmap& pixels, Format format,
                          const EncodeOptions& options) {
  if (!pixels.addr() || pixels.width() <= 0 || pixels.height() <= 0)
    return nullptr;
  if (format == Format::Exr) {
#ifdef SIGILIMAGE_HAS_OIIO_ENCODE
    return backend::encodeExrWithOiio(pixels);
#else
    return nullptr;
#endif
  }
  return backend::encodeWithSkia(pixels, format, options);
}

sk_sp<SkData> encodeImage(const SkImage& image, Format format,
                          const EncodeOptions& options) {
  const SkImageInfo info =
      SkImageInfo::Make(image.width(), image.height(), readbackType(format),
                        kPremul_SkAlphaType, image.refColorSpace());
  const size_t rowBytes = info.minRowBytes();
  if (rowBytes == 0) return nullptr;
  std::vector<uint8_t> storage(rowBytes * (size_t)info.height());
  const SkPixmap pixels(info, storage.data(), rowBytes);
  // A texture-backed image reads back through whatever context owns it;
  // passing none is the raster path, which is the only one this library
  // can speak. A device-resident image is read back by its owner and
  // handed here as a pixmap.
  if (!image.readPixels(nullptr, pixels, 0, 0)) return nullptr;
  return encodeImage(pixels, format, options);
}

std::optional<Format> formatForPath(const std::filesystem::path& path) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  if (ext == ".png") return Format::Png;
  if (ext == ".jpg" || ext == ".jpeg") return Format::Jpeg;
  if (ext == ".webp") return Format::Webp;
  if (ext == ".exr") return Format::Exr;
  return std::nullopt;
}

const char* extensionFor(Format format) {
  switch (format) {
    case Format::Png:
      return ".png";
    case Format::Jpeg:
      return ".jpg";
    case Format::Webp:
      return ".webp";
    case Format::Exr:
      return ".exr";
  }
  return ".png";
}

}  // namespace sigil::image
