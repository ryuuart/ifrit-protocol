#include "sigilimage/difference/Difference.h"

#include <include/core/SkColor.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace sigil::image {

namespace {

/// Whether row @p y of both pixmaps is stored alike byte for byte over
/// @p width pixels, which only a shared colour and alpha type can say.
bool rowsStoredAlike(const SkPixmap& actual, const SkPixmap& expected, int y,
                     int width) {
  if (actual.colorType() != expected.colorType() ||
      actual.alphaType() != expected.alphaType())
    return false;
  const size_t bytes = static_cast<size_t>(width) * actual.info().bytesPerPixel();
  return std::memcmp(actual.addr(0, y), expected.addr(0, y), bytes) == 0;
}

}  // namespace

PixelDifference difference(const SkPixmap& actual, const SkPixmap& expected) {
  PixelDifference found;
  const int height = std::min(actual.height(), expected.height());
  const int width = std::min(actual.width(), expected.width());
  if (!actual.addr() || !expected.addr()) return found;
  for (int y = 0; y < height; ++y) {
    if (rowsStoredAlike(actual, expected, y, width)) continue;
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
  }
  return found;
}

}  // namespace sigil::image
