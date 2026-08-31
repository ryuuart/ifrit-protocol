/** @file
 * The routing entry points: decodeImage(), decodeChannels() and
 * probeImage() try the Skia codecs first, then the SVG and OpenImageIO
 * backends that are built in, by sniffing content.
 */

#include "sigilimage/decode/Decode.h"

#include <include/core/SkData.h>
#include <include/core/SkImage.h>

#include "Backends.h"

namespace sigil::image {

std::optional<ImageAsset> decodeImage(const std::byte* bytes, size_t size,
                                      const DecodeOptions& options,
                                      const std::filesystem::path& pathHint) {
  // Layer selection is an OIIO concept; the Skia-codec path handles
  // the web formats (and their animation) best, so it goes first
  // otherwise. SkCodec sniffs bytes and fails fast on foreign formats.
  if (options.layer.empty()) {
    if (auto asset = ImageAsset::decode(SkData::MakeWithoutCopy(bytes, size)))
      return asset;
  }
#ifdef SIGILIMAGE_HAS_SVG
  if (backend::looksLikeSvg(bytes, size, pathHint))
    if (auto asset = backend::decodeWithSvg(bytes, size, options)) return asset;
#endif
#ifdef SIGILIMAGE_HAS_OIIO
  if (auto channels = backend::decodeChannelsWithOiio(bytes, size, pathHint))
    if (sk_sp<SkImage> image = channels->makeImage(options.layer))
      return ImageAsset::wrap(std::move(image));
  return std::nullopt;
#else
  (void)pathHint;
  return std::nullopt;
#endif
}

std::optional<ChannelData> decodeChannels(
    const std::byte* bytes, size_t size,
    const std::filesystem::path& pathHint) {
  if (auto channels = backend::decodeChannelsWithSkia(bytes, size))
    return channels;
#ifdef SIGILIMAGE_HAS_OIIO
  return backend::decodeChannelsWithOiio(bytes, size, pathHint);
#else
  (void)pathHint;
  return std::nullopt;
#endif
}

std::optional<ImageProbe> probeImage(const std::byte* bytes, size_t size,
                                     const std::filesystem::path& pathHint) {
  if (auto sniff = ImageAsset::probe(SkData::MakeWithoutCopy(bytes, size)))
    return sniff;  // Skia path: web formats, channels stay the N32 four
#ifdef SIGILIMAGE_HAS_SVG
  if (backend::looksLikeSvg(bytes, size, pathHint))
    if (auto info = backend::probeWithSvg(bytes, size)) return info;
#endif
#ifdef SIGILIMAGE_HAS_OIIO
  return backend::probeWithOiio(bytes, size, pathHint);
#else
  (void)pathHint;
  return std::nullopt;
#endif
}

}  // namespace sigil::image
