#pragma once

/** @file
 * @ingroup media-core
 * THIS LIBRARY ON A RESOURCE HUB: `registerDecoders(hub)`, the one call a
 * host makes wherever it builds its hub, after which `hub.load<media::Image>`,
 * `hub.load<media::Channels>` and `hub.load<media::Video>` answer — with
 * this library's own options, cached and reloaded like anything else the
 * hub holds — and the probe a hub asks `probe<media::Metadata>(hub, uri)`
 * by. `registerDecoders` is a template over the hub, so this library
 * knows no resource library.
 */

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

#include "sigilmedia/core/Metadata.h"
#include "sigilmedia/image/Decode.h"
#include "sigilmedia/video/Video.h"

namespace sigil::media {

/** WHAT @p bytes SAY ABOUT THEMSELVES, image or video, without a
 *  decode: an image's size, frames and layers along the image route
 *  first, then a video's container and stream. Nothing when neither
 *  reads them. @p nameHint only sharpens sniffing. */
std::optional<Metadata> probe(std::span<const std::byte> bytes,
                              const std::filesystem::path& nameHint = {});

/** THE SAME PROBE, UNDER THE NAME A BYTE SOURCE ASKS BY: found by
 *  argument-dependent lookup on the tag, so a hub reads a document's
 *  metadata without knowing a format. */
inline std::optional<Metadata> probeResource(
    std::type_identity<Metadata>, std::span<const std::byte> bytes,
    const std::filesystem::path& nameHint = {}) {
  return probe(bytes, nameHint);
}

/** Puts the `Image`, `Channels` and `Video` decoders on @p hub, each
 *  with the options its load names. A host calls this once, wherever it
 *  builds its hub.
 *  @trap Registering a type again replaces the decoder later loads run,
 *  so a host wanting its own image decode registers it AFTERWARDS. */
template <typename Hub>
void registerDecoders(Hub& hub) {
  registerDecoder<Image>(hub, [](const auto& bytes, std::string_view hint,
                                 const ImageOptions& options) {
    return decodeDocument(std::type_identity<Image>{}, bytes.span(), options,
                          std::filesystem::path(hint));
  });
  registerDecoder<Channels>(hub, [](const auto& bytes, std::string_view hint) {
    return decodeDocument(std::type_identity<Channels>{}, bytes.span(),
                          std::filesystem::path(hint));
  });
  registerDecoder<Video>(hub, [](const auto& bytes, std::string_view hint,
                                 const VideoOptions& options) {
    return decodeDocument(std::type_identity<Video>{}, bytes.span(), options,
                          std::filesystem::path(hint));
  });
}

}  // namespace sigil::media
