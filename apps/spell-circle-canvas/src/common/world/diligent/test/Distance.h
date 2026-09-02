#pragma once

/** @file
 * THE WORST CHANNEL two plates differ by, in 0..255.
 *
 * It is read here as an INEQUALITY and never as a tolerance. What a case
 * in this binary asks of two pictures is whether an operation reached
 * the pixels AT ALL, which is a lower bound on the disagreement and is
 * false only if the operation did nothing. How CLOSE two rasterisers
 * stand is a different question — its answer is a different number per
 * subject, it moves with the scene rather than with this code, and it is
 * asked of the whole registry against a committed baseline by the plate
 * ledger's device tier rather than here.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>

#include <algorithm>
#include <cmath>

namespace sigil::world::diligent {

/** How far @p a stands from @p b at the channel they disagree about
 *  most. Two plates of different sizes are as far apart as two plates
 *  can be. */
inline int worstChannel(const SkBitmap& a, const SkBitmap& b) {
  if (a.width() != b.width() || a.height() != b.height()) return 255;
  int worst = 0;
  for (int y = 0; y < a.height(); ++y) {
    for (int x = 0; x < a.width(); ++x) {
      const SkColor4f left = a.getColor4f(x, y);
      const SkColor4f right = b.getColor4f(x, y);
      const float channels[4][2] = {{left.fR, right.fR},
                                    {left.fG, right.fG},
                                    {left.fB, right.fB},
                                    {left.fA, right.fA}};
      for (const auto& pair : channels) {
        const int diff = (int)std::lround(std::abs(pair[0] - pair[1]) * 255.0f);
        worst = std::max(worst, std::clamp(diff, 0, 255));
      }
    }
  }
  return worst;
}

}  // namespace sigil::world::diligent
