#pragma once

/** @file
 * The decode surface of SigilImage: DecodeOptions, and the three entry
 * points that route bytes between the backends by sniffing content —
 * decodeImage(), probeImage() and decodeChannels(). Skia's codecs cover
 * the web formats (PNG/JPEG/WebP/GIF/AVIF, animation included); a KTX 1
 * or 2 with uncompressed texels is read from its header, its base level
 * only and a cube map's six faces as one 1:6 column; the OpenImageIO
 * backend, when built in (SIGILIMAGE_HAS_OIIO), extends decoding and
 * probing to EXR (with layer/channel selection), PSD (composited), TIFF,
 * HDR, DDS (a cube map as the same column), and the rest of OIIO's
 * roster, float sources landing as RGBA_F32 SkImages so HDR range
 * survives into compositing; the Skia SVG module, when built in
 * (SIGILIMAGE_HAS_SVG), rasterizes SVG sources at
 * DecodeOptions::width/height (intrinsic size by default).
 *
 * Resource ACCESS (URIs, mounts, caching, hot reload) is SigilIO's
 * concern; this header only ever sees bytes.
 */

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
  /** EXR layer / channel-group to composite (e.g. "diffuse" selects
   *  diffuse.R/G/B[/A]); empty = the default layer. Ignored by formats
   *  without layers. */
  std::string layer;

  /** Target raster size for vector sources (SVG). 0 on one axis derives
   *  it from the other by aspect; both 0 rasterizes at the intrinsic
   *  size (falling back to 512 for percent-sized SVGs that have none).
   *  Ignored by raster formats. */
  int width = 0;
  int height = 0;

  bool operator==(const DecodeOptions&) const = default;
};

/** Decodes an image from bytes, routing between the Skia-codec path
 *  and the OpenImageIO backend by sniffing content. `pathHint` (just
 *  the name matters) sharpens OIIO's format detection. */
std::optional<ImageAsset> decodeImage(
    const std::byte* bytes, size_t size, const DecodeOptions& options = {},
    const std::filesystem::path& pathHint = {});

/** Metadata without a full decode, same routing. */
std::optional<ImageProbe> probeImage(
    const std::byte* bytes, size_t size,
    const std::filesystem::path& pathHint = {});

/** THE SAME PROBE, UNDER THE NAME A BYTE SOURCE ASKS BY. A source knows
 *  where bytes live and how many there are; what they mean is this
 *  library's answer, so the question reaches it here — found by
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
