#pragma once

/** @file
 * @ingroup image-decode
 * The decode surface of SigilImage: DecodeOptions, and the three entry
 * points that route bytes between the backends by sniffing content —
 * decodeImage(), probeImage() and decodeChannels(). Skia's codecs cover
 * the web formats, a KTX with uncompressed texels is read from its
 * header, and the optional backends extend the roster where they are
 * built in. Resource ACCESS — URIs, mounts, caching, hot reload — is
 * another library's concern; this header only ever sees bytes.
 */

/** @defgroup image-decode Decoding
 *  Encoded bytes read into an asset, into metadata, or into named float
 *  planes, routed between the backends by what the bytes themselves say
 *  they are.
 *  @{ */
/** @} */

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <type_traits>

#include "sigilimage/asset/ImageAsset.h"
#include "sigilimage/decode/ChannelData.h"

namespace sigil::image {

/** Options for decodes that support them. */
struct DecodeOptions {
  /** EXR layer or channel group to composite — "diffuse" selects
   *  diffuse.R/G/B and its alpha. Empty is the default layer, and a
   *  format without layers ignores it. */
  std::string layer;

  /** Target raster size in px for a vector source. 0 on one axis
   *  derives it from the other by aspect; both 0 rasterizes at the
   *  intrinsic size, falling back to 512 for a percent-sized source
   *  that has none. A raster format ignores it. */
  int width = 0;
  int height = 0;  ///< The other axis of the same target raster size.

  bool operator==(const DecodeOptions&) const = default;
};

/** Decodes an image from bytes, routing between the backends by
 *  sniffing content. @p pathHint — only the name matters — sharpens
 *  format detection; nothing dispatches on it. */
std::optional<ImageAsset> decodeImage(
    const std::byte* bytes, size_t size, const DecodeOptions& options = {},
    const std::filesystem::path& pathHint = {});

/** Metadata without a full decode, same routing. */
std::optional<ImageProbe> probeImage(
    const std::byte* bytes, size_t size,
    const std::filesystem::path& pathHint = {});

/** THE SAME PROBE, UNDER THE NAME A BYTE SOURCE ASKS BY: found by
 *  argument-dependent lookup on the tag, against nothing but the
 *  standard library, so no resource library needs to know an image
 *  format and this one needs to know no resource library. */
inline std::optional<ImageProbe> probeResource(
    std::type_identity<ImageProbe>, std::span<const std::byte> bytes,
    const std::filesystem::path& pathHint = {}) {
  return probeImage(bytes.data(), bytes.size(), pathHint);
}

/** Decodes every channel the source carries. */
std::optional<ChannelData> decodeChannels(
    const std::byte* bytes, size_t size,
    const std::filesystem::path& pathHint = {});

}  // namespace sigil::image
