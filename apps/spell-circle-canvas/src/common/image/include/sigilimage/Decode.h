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

/**
 * The raw decoded color data: every channel the source carries, as
 * named interleaved float planes — the format-neutral bridge between
 * decoders and consumers (Skia, and any library that wants numbers
 * rather than an SkImage).
 *
 * LDR sources (PNG/JPEG/… via Skia codecs) arrive as premultiplied
 * R,G,B,A normalized to 0..1; float sources (EXR, float TIFF) keep
 * their full HDR range and channel names ("glow.R", "depth.Z"…).
 * Multi-part EXR parts matching the base dimensions merge in with
 * their part name as the prefix.
 */
struct ChannelData {
  int width = 0;
  int height = 0;
  bool floatingPoint = false;      // source was float/HDR
  std::vector<std::string> names;  // source order
  std::vector<float> data;         // interleaved: w*h*names.size()

  /** Index of a channel by exact name; -1 when absent. */
  int index(std::string_view name) const;

  float at(int x, int y, int channel) const {
    return data[((size_t)y * width + x) * names.size() + channel];
  }

  /** Composites channels into an SkImage: `layer` selects a channel
   *  group exactly like DecodeOptions::layer (empty = plain R/G/B/A,
   *  luminance repeats, missing alpha = 1). Float data lands as
   *  RGBA_F32, LDR as N32. Null when the layer names nothing. */
  sk_sp<SkImage> makeImage(std::string_view layer = {}) const;

  /** Composites explicit channel indices (-1 = missing: alpha fills
   *  with 1, g/b repeat r). */
  sk_sp<SkImage> makeImage(int r, int g, int b, int a) const;
};

/** Decodes every channel the source carries. */
std::optional<ChannelData>
decodeChannels(const std::byte *bytes, size_t size,
               const std::filesystem::path &pathHint = {});

} // namespace sigil::image
