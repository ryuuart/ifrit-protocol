#pragma once

/** @file
 * ChannelData: every channel a source carries as named interleaved
 * float planes — the format-neutral bridge between decoders and
 * consumers — with the compositing that turns a channel group back
 * into an SkImage.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::image {

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

}  // namespace sigil::image
