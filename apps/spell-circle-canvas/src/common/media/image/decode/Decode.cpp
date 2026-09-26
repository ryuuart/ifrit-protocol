/** @file
 * The image route: the Skia codecs first, then KTX, then the SVG and
 * OpenImageIO backends that are built in, chosen by sniffing content.
 */

#include "sigilmedia/image/Decode.h"

#include <utility>
#include <vector>

#include "Backends.h"

namespace sigil::media {

namespace {

/** A picture composited from channels, as a one-frame document. */
std::optional<Image> stillOf(sk_sp<SkImage> picture) {
  if (!picture) return std::nullopt;
  std::vector<Frame> frames(1);
  frames.front().image = std::move(picture);
  return Image(std::move(frames));
}

}  // namespace

std::optional<Image> decodeDocument(std::type_identity<Image>,
                                    std::span<const std::byte> encoded,
                                    const ImageOptions& options,
                                    const std::filesystem::path& nameHint) {
  const std::byte* bytes = encoded.data();
  const size_t size = encoded.size();
  if (!bytes || size == 0) return std::nullopt;
  // Layer selection is OpenImageIO's; the Skia route reads the web
  // formats and their animation best, so it goes first otherwise, and
  // SkCodec fails fast on a foreign format.
  if (options.layer.empty())
    if (auto image = backend::decodeWithSkia(bytes, size)) return image;
  if (backend::looksLikeKtx(bytes, size))
    if (auto channels = backend::decodeChannelsWithKtx(bytes, size))
      if (auto image = stillOf(backend::composite(*channels, options.layer)))
        return image;
#ifdef SIGILMEDIA_HAS_SVG
  if (backend::looksLikeSvg(bytes, size, nameHint))
    if (auto image = backend::decodeWithSvg(bytes, size, options)) return image;
#endif
#ifdef SIGILMEDIA_HAS_OIIO
  if (auto channels = backend::decodeChannelsWithOiio(bytes, size, nameHint))
    return stillOf(backend::composite(*channels, options.layer));
  return std::nullopt;
#else
  (void)nameHint;
  return std::nullopt;
#endif
}

std::optional<Channels> decodeDocument(std::type_identity<Channels>,
                                       std::span<const std::byte> encoded,
                                       const std::filesystem::path& nameHint) {
  const std::byte* bytes = encoded.data();
  const size_t size = encoded.size();
  if (!bytes || size == 0) return std::nullopt;
  if (auto channels = backend::decodeChannelsWithSkia(bytes, size))
    return channels;
  if (backend::looksLikeKtx(bytes, size))
    return backend::decodeChannelsWithKtx(bytes, size);
#ifdef SIGILMEDIA_HAS_OIIO
  return backend::decodeChannelsWithOiio(bytes, size, nameHint);
#else
  (void)nameHint;
  return std::nullopt;
#endif
}

std::optional<Metadata> probeDocument(std::type_identity<Image>,
                                      std::span<const std::byte> encoded,
                                      const std::filesystem::path& nameHint) {
  const std::byte* bytes = encoded.data();
  const size_t size = encoded.size();
  if (!bytes || size == 0) return std::nullopt;
  if (auto metadata = backend::probeWithSkia(bytes, size))
    return metadata;  // the web formats: channels stay the N32 four
  if (backend::looksLikeKtx(bytes, size))
    return backend::probeWithKtx(bytes, size);
#ifdef SIGILMEDIA_HAS_SVG
  if (backend::looksLikeSvg(bytes, size, nameHint))
    if (auto metadata = backend::probeWithSvg(bytes, size)) return metadata;
#endif
#ifdef SIGILMEDIA_HAS_OIIO
  return backend::probeWithOiio(bytes, size, nameHint);
#else
  (void)nameHint;
  return std::nullopt;
#endif
}

}  // namespace sigil::media
