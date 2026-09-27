#pragma once

/** @file
 * @ingroup media-difference
 * HOW FAR TWO PICTURES OF ONE SIZE STAND APART: how many pixels differ,
 * the widest gap on any one channel, and where it was found. A
 * compositing-order difference shows as a wide gap somewhere; a rounding
 * difference does not. It is the one comparison a test that holds a
 * render against a baseline needs, and it answers for any two rasters
 * whatever drew them.
 */

/** @defgroup media-difference Pixel difference
 *  How many pixels two pictures disagree on and how widely — the reading
 *  a render is held against its baseline by.
 *  @{ */
/** @} */

#include "sigilmedia/core/Picture.h"

namespace sigil::media {

class Image;

/** Two pictures held against each other, pixel by pixel. */
struct PixelDifference {
  int differingPixels = 0;  ///< pixels whose colour differs at all
  int worst = 0;            ///< largest absolute difference on any one channel
  int x = -1;               ///< where that difference was found
  int y = -1;
  [[nodiscard]] bool identical() const { return differingPixels == 0; }
  bool operator==(const PixelDifference&) const = default;
};

/** Holds @p actual's first frame against @p expected's over the extent
 *  both cover, channel by channel, alpha included, as unpremultiplied
 *  8-bit colours, each read back to the CPU first where it is not there.
 *  @silent the sizes differ: the part outside the smaller one is not
 *  read, so a caller that cares compares the sizes first. An empty
 *  document, or a picture that cannot be read back, differs nowhere. */
[[nodiscard]] PixelDifference difference(const Image& actual,
                                         const Image& expected);

/** The same for two pictures in hand. */
[[nodiscard]] PixelDifference difference(const Picture& actual,
                                         const Picture& expected);

}  // namespace sigil::media
