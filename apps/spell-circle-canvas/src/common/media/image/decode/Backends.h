#pragma once

/** @file
 * The backends the image route chooses between, one translation unit
 * each: the Skia codecs, the KTX reader, the SVG rasterizer when
 * SIGILMEDIA_HAS_SVG is defined, and the OpenImageIO reader when
 * SIGILMEDIA_HAS_OIIO is. Private to the decode feature.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string_view>

#include "sigilmedia/core/Image.h"
#include "sigilmedia/core/Metadata.h"
#include "sigilmedia/image/Channels.h"

namespace sigil::media::backend {

/** PNG, JPEG, WebP, GIF and AVIF through Skia's codecs, stills and
 *  animations, every frame composited into premultiplied N32; nothing
 *  when Skia does not recognise the bytes. */
std::optional<Image> decodeWithSkia(const std::byte* bytes, size_t size);

/** The same route as a probe: dimensions, frame count, format name. */
std::optional<Metadata> probeWithSkia(const std::byte* bytes, size_t size);

/** LDR web formats through the Skia codecs: the premultiplied N32
 *  pixels of the first frame normalized to 0..1 floats named R/G/B/A;
 *  nothing when Skia does not recognise the bytes. */
std::optional<Channels> decodeChannelsWithSkia(const std::byte* bytes,
                                               size_t size);

/** A channel group of @p channels composited into one picture, as
 *  `Channels::image` describes; null when the layer names nothing. */
sk_sp<SkImage> composite(const Channels& channels, std::string_view layer);

/** The twelve-byte identifier of a KTX 1 or KTX 2 container. */
bool looksLikeKtx(const std::byte* bytes, size_t size);

/** The base level of a KTX 1 or KTX 2 file as Channels — R, G, B, A in
 *  the texel's own number type, a cube map's six faces stacked into one
 *  column in the +x -x +y -y +z -z order the container names them, the
 *  same column OpenImageIO's DDS reader produces. Uncompressed texels
 *  only: 8-bit, half and float in one to four channels. A
 *  block-compressed, supercompressed, array, 3D or big-endian file is
 *  nothing. */
std::optional<Channels> decodeChannelsWithKtx(const std::byte* bytes,
                                              size_t size);

/** The same header as a probe: "ktx" or "ktx2", the base level's size
 *  (a cube map's as the column), channels and float-ness. */
std::optional<Metadata> probeWithKtx(const std::byte* bytes, size_t size);

#ifdef SIGILMEDIA_HAS_SVG

/** SVG has no magic number; sniff leading whitespace/BOM then "<?xml"
 *  or "<svg" (a .svg name counts as a hint too). */
bool looksLikeSvg(const std::byte* bytes, size_t size,
                  const std::filesystem::path& nameHint);

/** Rasterizes the SVG at the size the options ask for. */
std::optional<Image> decodeWithSvg(const std::byte* bytes, size_t size,
                                   const ImageOptions& options);

/** The root element's intrinsic size as a probe; nothing when the bytes
 *  do not parse. */
std::optional<Metadata> probeWithSvg(const std::byte* bytes, size_t size);

#endif  // SIGILMEDIA_HAS_SVG

#ifdef SIGILMEDIA_HAS_OIIO

/** Reads EVERY channel of the source as Channels: subimage 0's channels
 *  under their own names, plus any named same-size part's channels
 *  prefixed "part." (multi-part EXR layers become uniform with
 *  channel-prefix layers). */
std::optional<Channels> decodeChannelsWithOiio(
    const std::byte* bytes, size_t size, const std::filesystem::path& nameHint);

/** Metadata through OIIO: dimensions, channels, float-ness, and the
 *  layers read from channel prefixes and named parts. */
std::optional<Metadata> probeWithOiio(const std::byte* bytes, size_t size,
                                      const std::filesystem::path& nameHint);

#endif  // SIGILMEDIA_HAS_OIIO

}  // namespace sigil::media::backend
