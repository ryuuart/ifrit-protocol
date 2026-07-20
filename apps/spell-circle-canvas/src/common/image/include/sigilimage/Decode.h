#pragma once
// Format-routing image decode over pluggable backends — the official
// decode surface of SigilImage. Skia's codecs cover the web formats
// (PNG/JPEG/WebP/GIF/AVIF, animation included); the OpenImageIO
// backend, when built in (SIGILIMAGE_HAS_OIIO), extends decoding and
// probing to EXR (with layer/channel selection), PSD (composited),
// TIFF, HDR, and the rest of OIIO's roster. Float sources land as
// RGBA_F32 SkImages so HDR range survives into compositing.
//
// Resource ACCESS (URIs, mounts, caching, hot reload) is SigilLoader's
// concern; this header only ever sees bytes.

#include "sigilimage/ImageAsset.h"

#include <cstddef>
#include <filesystem>

namespace sigil::image {

/** Options for decodes that support them. */
struct DecodeOptions {
  /** EXR layer / channel-group to composite (e.g. "diffuse" selects
   *  diffuse.R/G/B[/A]); empty = the default layer. Ignored by formats
   *  without layers. */
  std::string layer;

  bool operator==(const DecodeOptions &) const = default;
};

/** Decodes an image from bytes, routing between the Skia-codec path
 *  and the OpenImageIO backend by sniffing content. `pathHint` (just
 *  the name matters) sharpens OIIO's format detection. */
std::optional<ImageAsset>
decodeImage(const std::byte *bytes, size_t size,
            const DecodeOptions &options = {},
            const std::filesystem::path &pathHint = {});

/** Metadata without a full decode, same routing. */
std::optional<ImageProbe>
probeImage(const std::byte *bytes, size_t size,
           const std::filesystem::path &pathHint = {});

} // namespace sigil::image
