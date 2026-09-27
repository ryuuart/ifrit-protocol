/** @file
 * The still route: pixels and a format in, encoded bytes out, with the
 * CPU readback a picture needs before any encoder can see it.
 */

#include "sigilmedia/image/Encode.h"
#include "sigilmedia/advanced/Skia.h"

#include <include/core/SkColorSpace.h>
#include <include/core/SkColorType.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>
#include <sigilmedia/advanced/Formats.h>
#include <sigilmedia/image/Channels.h>

#include <cstring>
#include <vector>

#include "Backends.h"

namespace sigil::media {

namespace {

/** The colour type a readback lands in for @p format: the depth the
 *  format can hold, so nothing is thrown away on the way in and nothing
 *  is carried that the encoder would drop. */
SkColorType readbackType(Format format) {
  return format == Format::Exr ? kRGBA_F32_SkColorType : kN32_SkColorType;
}

std::vector<std::byte> bytesOf(const sk_sp<SkData>& data) {
  if (!data || data->size() == 0) return {};
  std::vector<std::byte> bytes(data->size());
  std::memcpy(bytes.data(), data->data(), data->size());
  return bytes;
}

}  // namespace

bool canEncode(Format format) {
  // Skia's three encoders are linked with Skia itself; only EXR stands
  // behind a backend that a build can be without, and a movie is the
  // Encoder's.
  if (format == Format::Mp4) return false;
  if (format != Format::Exr) return true;
#ifdef SIGILMEDIA_HAS_OIIO_ENCODE
  return backend::canEncodeExrWithOiio();
#else
  return false;
#endif
}

std::vector<std::byte> encode(const SkPixmap& pixels, Format format,
                              const EncodeOptions& options) {
  if (!pixels.addr() || pixels.width() <= 0 || pixels.height() <= 0)
    return {};
  if (format == Format::Mp4) return {};
  if (format == Format::Exr) {
#ifdef SIGILMEDIA_HAS_OIIO_ENCODE
    return bytesOf(backend::encodeExrWithOiio(pixels));
#else
    return {};
#endif
  }
  return bytesOf(backend::encodeWithSkia(pixels, format, options));
}

std::vector<std::byte> encode(const Channels& channels, Format format,
                              const EncodeOptions& options) {
  (void)options;  // EXR is lossless at every setting
  // EVERY OTHER FORMAT IS THREE OR FOUR CHANNELS WITH FIXED MEANINGS, so
  // there is nothing for it to do with a name — a caller with a layer
  // and a PNG composites the group into an image first.
  if (format != Format::Exr) return {};
#ifdef SIGILMEDIA_HAS_OIIO_ENCODE
  return bytesOf(backend::encodeExrChannelsWithOiio(channels));
#else
  (void)channels;
  return {};
#endif
}

std::vector<std::byte> encode(const Picture& held, Format format,
                              const EncodeOptions& options) {
  const sk_sp<SkImage> image = toSk(held);
  if (!image) return {};
  const SkImage& picture = *image;
  const SkImageInfo info =
      SkImageInfo::Make(picture.width(), picture.height(),
                        readbackType(format), kPremul_SkAlphaType,
                        picture.refColorSpace());
  const size_t rowBytes = info.minRowBytes();
  if (rowBytes == 0) return {};
  std::vector<uint8_t> storage(rowBytes * (size_t)info.height());
  const SkPixmap pixels(info, storage.data(), rowBytes);
  // A texture-backed picture reads back through whatever context owns
  // it; passing none is the raster path, which is the only one this
  // library can speak. A device-resident picture is read back by its
  // owner and handed here as a pixmap.
  if (!picture.readPixels(nullptr, pixels, 0, 0)) return {};
  return encode(pixels, format, options);
}

std::vector<std::byte> encode(const Image& image, Format format,
                              const EncodeOptions& options) {
  if (image.frames().empty()) return {};
  const sk_sp<SkImage> picture = deviceImage(image.frames().front(), nullptr);
  if (!picture) return {};
  return encode(fromSk(picture), format, options);
}

std::vector<std::byte> encodeResource(const Image& image,
                                      const std::filesystem::path& path) {
  const std::optional<Format> format = formatForPath(path);
  if (!format || *format == Format::Mp4) return {};
  return encode(image, *format);
}

}  // namespace sigil::media
