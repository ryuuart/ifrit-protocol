#pragma once

/** @file
 * @ingroup weave-testing
 *
 * How far two renders of one size stand apart: how many pixels differ,
 * the widest gap on any one channel, and where it was found. A
 * compositing-order difference shows as a wide gap somewhere; a rounding
 * difference does not.
 */

#include <include/core/SkPixmap.h>

namespace sigil::weave::testing {

/** Two renders held against each other, pixel by pixel. */
struct PixelDifference {
  int differingPixels = 0;  ///< pixels whose colour differs at all
  int worst = 0;            ///< largest absolute difference on any one channel
  int x = -1;               ///< where that difference was found
  int y = -1;
  [[nodiscard]] bool identical() const { return differingPixels == 0; }
  bool operator==(const PixelDifference&) const = default;
};

/** Holds @p actual against @p expected over the extent both cover,
 *  channel by channel, alpha included, as unpremultiplied colours.
 *  @silent the sizes differ: the part outside the smaller one is not
 *  read, so a caller that cares compares the sizes first. */
[[nodiscard]] PixelDifference difference(const SkPixmap& actual,
                                         const SkPixmap& expected);

}  // namespace sigil::weave::testing
