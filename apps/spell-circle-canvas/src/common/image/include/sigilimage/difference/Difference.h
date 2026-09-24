#pragma once

/** @file
 * @ingroup image-difference
 * HOW FAR TWO PICTURES OF ONE SIZE STAND APART: how many pixels differ,
 * the widest gap on any one channel, and where it was found. A
 * compositing-order difference shows as a wide gap somewhere; a rounding
 * difference does not. It is the one comparison a test that holds a
 * render against a baseline needs, and it answers for any two rasters
 * whatever drew them.
 */

/** @defgroup image-difference Pixel difference
 *  How many pixels two pictures disagree on and how widely — the reading
 *  a render is held against its baseline by.
 *  @{ */
/** @} */

#include <include/core/SkPixmap.h>

namespace sigil::image {

/** Two pictures held against each other, pixel by pixel. */
struct PixelDifference {
  int differingPixels = 0;  ///< pixels whose colour differs at all
  int worst = 0;            ///< largest absolute difference on any one channel
  int x = -1;               ///< where that difference was found
  int y = -1;
  [[nodiscard]] bool identical() const { return differingPixels == 0; }
  bool operator==(const PixelDifference&) const = default;
};

/** Holds @p actual against @p expected over the extent both cover,
 *  channel by channel, alpha included, as unpremultiplied 8-bit colours.
 *  Two pixmaps of one colour type and alpha type are compared row by row
 *  as stored first, so identical rows cost a memory comparison.
 *  @silent the sizes differ: the part outside the smaller one is not
 *  read, so a caller that cares compares the sizes first. */
[[nodiscard]] PixelDifference difference(const SkPixmap& actual,
                                         const SkPixmap& expected);

}  // namespace sigil::image
