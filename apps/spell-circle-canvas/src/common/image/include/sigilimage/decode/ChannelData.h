#pragma once

/** @file
 * @ingroup image-decode
 * ChannelData: every channel a source carries as named interleaved
 * float planes — the format-neutral bridge between decoders and
 * consumers — with the compositing that turns a channel group back
 * into an SkImage.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::image {

/**
 * The raw decoded colour data: every channel the source carries, as
 * named interleaved float planes — the format-neutral bridge between
 * decoders and consumers. LDR sources arrive as premultiplied R, G, B,
 * A normalised to 0..1; float sources keep their full HDR range and
 * their channel names. Multi-part EXR parts matching the base
 * dimensions merge in with their part name as the prefix.
 */
struct ChannelData {
  int width = 0;
  int height = 0;
  bool floatingPoint = false;      ///< source was float/HDR
  std::vector<std::string> names;  ///< source order
  std::vector<float> data;         ///< interleaved: w*h*names.size()

  /** Index of a channel by exact name; -1 when absent. */
  int index(std::string_view name) const;

  /** One channel of one texel.
   *  @trap The caller states a texel inside the raster and a channel
   *  this data carries; anything else is a programming error, caught in
   *  a debug build. */
  float at(int x, int y, int channel) const {
    assert(x >= 0 && x < width && y >= 0 && y < height);
    assert(channel >= 0 && (size_t)channel < names.size());
    return data[((size_t)y * width + x) * names.size() + channel];
  }

  /** Composites channels into an SkImage. @p layer selects a channel
   *  group exactly as DecodeOptions::layer does: empty is plain
   *  R/G/B/A, a luminance channel repeats, a missing alpha is 1. Float
   *  data lands as RGBA_F32 and LDR as N32; null when the layer names
   *  nothing. */
  sk_sp<SkImage> makeImage(std::string_view layer = {}) const;

  /** Composites explicit channel indices. An index this data does not
   *  carry — negative, or past the channels it holds — is missing, so
   *  alpha fills with 1 and green and blue repeat red. */
  sk_sp<SkImage> makeImage(int r, int g, int b, int a) const;
};

}  // namespace sigil::image
