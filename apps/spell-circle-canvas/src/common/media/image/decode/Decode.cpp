/** @file
 * The image route: the Skia codecs first, then KTX, then the SVG and
 * OpenImageIO backends that are built in, chosen by sniffing content.
 */

#include "sigilmedia/image/Decode.h"

#include <string>
#include <utility>
#include <vector>

#include "Backends.h"
#include "sigilmedia/advanced/Skia.h"

namespace sigil::media {

namespace {

/** A picture composited from channels, as a one-frame document; the
 *  error says which layer was missing when none could be composited. */
std::expected<Image, core::Error> stillOf(sk_sp<SkImage> picture,
                                          const std::string& layer) {
  if (!picture) {
    if (!layer.empty())
      return std::unexpected(
          core::Error{"the image carries no layer named '" + layer + "'"});
    return std::unexpected(
        core::Error{"the image's channels do not composite to a picture"});
  }
  std::vector<Frame> frames(1);
  frames.front().image = std::move(picture);
  return Image(std::move(frames));
}

/** What every decoder of the image route answers when none of them read
 *  the bytes. */
core::Error unreadImage() {
  return {"the bytes are no image format this build reads", {}, {}};
}

/** What a decoder answers for no bytes at all. */
core::Error noBytes() { return {"an image needs bytes and has none", {}, {}}; }

}  // namespace

std::expected<Image, core::Error> decodeDocument(
    std::type_identity<Image>, std::span<const std::byte> encoded,
    const ImageOptions& options, const std::filesystem::path& nameHint) {
  const std::byte* bytes = encoded.data();
  const size_t size = encoded.size();
  if (!bytes || size == 0) return std::unexpected(noBytes());
  // Layer selection is OpenImageIO's; the Skia route reads the web
  // formats and their animation best, so it goes first otherwise, and
  // SkCodec fails fast on a foreign format.
  if (options.layer.empty())
    if (auto image = backend::decodeWithSkia(bytes, size, options.colorSpace))
      return std::move(*image);
  // A KTX texture that does not composite leaves the route to the
  // decoders after it, and its own word stands if none of them reads it.
  core::Error refused = unreadImage();
  if (backend::looksLikeKtx(bytes, size)) {
    if (auto channels = backend::decodeChannelsWithKtx(bytes, size)) {
      auto image = stillOf(
          backend::composite(*channels, options.layer, options.colorSpace),
          options.layer);
      if (image) return image;
      refused = std::move(image.error());
    } else {
      refused = {
          "the bytes begin as a KTX texture and do not read as one", {}, {}};
    }
  }
#ifdef SIGILMEDIA_HAS_SVG
  if (backend::looksLikeSvg(bytes, size, nameHint))
    if (auto image = backend::decodeWithSvg(bytes, size, options))
      return std::move(*image);
#endif
#ifdef SIGILMEDIA_HAS_OIIO
  if (auto channels = backend::decodeChannelsWithOiio(bytes, size, nameHint))
    return stillOf(
        backend::composite(*channels, options.layer, options.colorSpace),
        options.layer);
#else
  (void)nameHint;
#endif
  return std::unexpected(std::move(refused));
}

std::expected<Channels, core::Error> decodeDocument(
    std::type_identity<Channels>, std::span<const std::byte> encoded,
    const std::filesystem::path& nameHint) {
  const std::byte* bytes = encoded.data();
  const size_t size = encoded.size();
  if (!bytes || size == 0) return std::unexpected(noBytes());
  if (auto channels = backend::decodeChannelsWithSkia(bytes, size))
    return std::move(*channels);
  if (backend::looksLikeKtx(bytes, size)) {
    if (auto channels = backend::decodeChannelsWithKtx(bytes, size))
      return std::move(*channels);
    return std::unexpected(
        core::Error{"the bytes begin as a KTX texture and do not read as one"});
  }
#ifdef SIGILMEDIA_HAS_OIIO
  if (auto channels = backend::decodeChannelsWithOiio(bytes, size, nameHint))
    return std::move(*channels);
  return std::unexpected(unreadImage());
#else
  (void)nameHint;
  return std::unexpected(unreadImage());
#endif
}

std::expected<Metadata, core::Error> probeDocument(
    std::type_identity<Image>, std::span<const std::byte> encoded,
    const std::filesystem::path& nameHint) {
  const std::byte* bytes = encoded.data();
  const size_t size = encoded.size();
  if (!bytes || size == 0) return std::unexpected(noBytes());
  if (auto metadata = backend::probeWithSkia(bytes, size))
    return std::move(*metadata);  // the web formats: channels stay the N32 four
  if (backend::looksLikeKtx(bytes, size)) {
    if (auto metadata = backend::probeWithKtx(bytes, size))
      return std::move(*metadata);
    return std::unexpected(
        core::Error{"the bytes begin as a KTX texture and do not read as one"});
  }
#ifdef SIGILMEDIA_HAS_SVG
  if (backend::looksLikeSvg(bytes, size, nameHint))
    if (auto metadata = backend::probeWithSvg(bytes, size))
      return std::move(*metadata);
#endif
#ifdef SIGILMEDIA_HAS_OIIO
  if (auto metadata = backend::probeWithOiio(bytes, size, nameHint))
    return std::move(*metadata);
  return std::unexpected(unreadImage());
#else
  (void)nameHint;
  return std::unexpected(unreadImage());
#endif
}

}  // namespace sigil::media
