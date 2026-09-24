#include "sigilweave/testing/Difference.h"

#include <include/core/SkColor.h>

#include <algorithm>
#include <cstdlib>

namespace sigil::weave::testing {

PixelDifference difference(const SkPixmap& actual, const SkPixmap& expected) {
  PixelDifference found;
  const int height = std::min(actual.height(), expected.height());
  const int width = std::min(actual.width(), expected.width());
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const SkColor a = actual.getColor(x, y);
      const SkColor b = expected.getColor(x, y);
      if (a == b) continue;
      ++found.differingPixels;
      const int widest =
          std::max({std::abs((int)SkColorGetA(a) - (int)SkColorGetA(b)),
                    std::abs((int)SkColorGetR(a) - (int)SkColorGetR(b)),
                    std::abs((int)SkColorGetG(a) - (int)SkColorGetG(b)),
                    std::abs((int)SkColorGetB(a) - (int)SkColorGetB(b))});
      if (widest > found.worst) {
        found.worst = widest;
        found.x = x;
        found.y = y;
      }
    }
  return found;
}

}  // namespace sigil::weave::testing
