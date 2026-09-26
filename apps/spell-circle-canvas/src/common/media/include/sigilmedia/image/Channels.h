#pragma once

/** @file
 * @ingroup media-image
 * `Channels`: every channel a source carries as named interleaved float
 * planes — the format-neutral bridge between a decoder and a consumer
 * that wants more than colour — and the compositing that turns a channel
 * group back into an image.
 */

#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::media {

class Image;

/** WHICH CHANNELS MAKE A PICTURE, by index. A channel the data does not
 *  carry — negative, or past the channels it holds — is missing: green
 *  and blue then repeat red, and a missing alpha is opaque. */
struct ChannelPick {
  int red = 0;
  int green = -1;
  int blue = -1;
  int alpha = -1;
};

/**
 * EVERY CHANNEL A SOURCE CARRIES, as named interleaved float planes: what
 * `hub.load<media::Channels>(uri)` answers. LDR sources arrive as
 * premultiplied R, G, B, A normalized to 0..1; float sources keep their
 * full HDR range and their channel names. Multi-part EXR parts matching
 * the base dimensions merge in with their part name as the prefix.
 */
struct Channels {
  int width = 0;
  int height = 0;
  bool floatingPoint = false;      ///< the source was float/HDR
  std::vector<std::string> names;  ///< in source order
  std::vector<float> data;         ///< interleaved: width*height*names.size()

  /** The index of a channel by exact name; -1 when absent. */
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

  /** A CHANNEL GROUP COMPOSITED INTO A ONE-FRAME IMAGE: @p layer selects
   *  the group exactly as `ImageOptions::layer` does — empty is plain
   *  R/G/B/A, a luminance channel repeats, a missing alpha is 1. Float
   *  data lands as RGBA float and LDR as N32; null when the layer names
   *  nothing. */
  std::shared_ptr<const Image> image(std::string_view layer = {}) const;

  /** The channels @p pick names, composited the same way. */
  std::shared_ptr<const Image> image(ChannelPick pick) const;
};

}  // namespace sigil::media
