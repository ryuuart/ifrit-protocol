#pragma once

/** @file
 * The backends the routing entry points choose between, one
 * translation unit each: the Skia codecs' channel read, the SVG
 * rasterizer when SIGILIMAGE_HAS_SVG is defined, and the OpenImageIO
 * reader when SIGILIMAGE_HAS_OIIO is. Private to the decode feature.
 */

#include <cstddef>
#include <filesystem>
#include <optional>

#include "sigilimage/asset/ImageAsset.h"
#include "sigilimage/decode/ChannelData.h"
#include "sigilimage/decode/Decode.h"

namespace sigil::image::backend {

/** LDR web formats through the Skia codecs: the premultiplied N32
 *  pixels of the first frame normalized to 0..1 floats named R/G/B/A;
 *  nullopt when Skia does not recognise the bytes. */
std::optional<ChannelData> decodeChannelsWithSkia(const std::byte* bytes,
                                                  size_t size);

#ifdef SIGILIMAGE_HAS_SVG

/** SVG has no magic number; sniff leading whitespace/BOM then "<?xml"
 *  or "<svg" (a .svg pathHint extension counts as a hint too). */
bool looksLikeSvg(const std::byte* bytes, size_t size,
                  const std::filesystem::path& pathHint);

/** Rasterizes the SVG at the size DecodeOptions asks for. */
std::optional<ImageAsset> decodeWithSvg(const std::byte* bytes, size_t size,
                                        const DecodeOptions& options);

/** The root element's intrinsic size as a probe; nullopt when the
 *  bytes do not parse. */
std::optional<ImageProbe> probeWithSvg(const std::byte* bytes, size_t size);

#endif  // SIGILIMAGE_HAS_SVG

#ifdef SIGILIMAGE_HAS_OIIO

/** Reads EVERY channel of the source as ChannelData: subimage 0's
 *  channels under their own names, plus any named same-size part's
 *  channels prefixed "part." (multi-part EXR layers become uniform
 *  with channel-prefix layers). */
std::optional<ChannelData> decodeChannelsWithOiio(
    const std::byte* bytes, size_t size, const std::filesystem::path& pathHint);

/** Metadata through OIIO: dimensions, channels, float-ness, and the
 *  layers read from channel prefixes and named parts. */
std::optional<ImageProbe> probeWithOiio(const std::byte* bytes, size_t size,
                                        const std::filesystem::path& pathHint);

#endif  // SIGILIMAGE_HAS_OIIO

}  // namespace sigil::image::backend
