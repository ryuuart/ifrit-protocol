#pragma once

/** @file
 * Reading a rendered surface back: whether any pixel answers a question,
 * how many do, and how far two renders of the same size stand apart. A
 * pixel scan written out by hand is a loop nobody reads; written once it is
 * the question the case is actually asking.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPixmap.h>

#include <algorithm>
#include <cstdlib>

namespace sigil::weave::test {

/// True when some pixel of `pixmap` satisfies `predicate(SkColor)`.
template <typename Predicate>
inline bool anyPixel(const SkPixmap& pixmap, Predicate&& predicate) {
  for (int y = 0; y < pixmap.height(); ++y)
    for (int x = 0; x < pixmap.width(); ++x)
      if (predicate(pixmap.getColor(x, y))) return true;
  return false;
}

/// How many pixels of `pixmap` satisfy `predicate(SkColor)`.
template <typename Predicate>
inline int countPixels(const SkPixmap& pixmap, Predicate&& predicate) {
  int count = 0;
  for (int y = 0; y < pixmap.height(); ++y)
    for (int x = 0; x < pixmap.width(); ++x)
      if (predicate(pixmap.getColor(x, y))) ++count;
  return count;
}

/// The widest per-channel gap between two renders of the same size, and
/// where it was found. A compositing-order difference shows up as a large
/// worst channel somewhere; a rounding difference does not.
struct PixelDifference {
  int worst = 0;  ///< largest absolute difference on any one channel
  int x = -1;     ///< where that difference was found
  int y = -1;
};

inline PixelDifference worstPixelDifference(const SkPixmap& actual,
                                            const SkPixmap& expected) {
  PixelDifference found;
  const int height = std::min(actual.height(), expected.height());
  const int width = std::min(actual.width(), expected.width());
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const SkColor a = actual.getColor(x, y);
      const SkColor b = expected.getColor(x, y);
      const int difference =
          std::max({std::abs((int)SkColorGetR(a) - (int)SkColorGetR(b)),
                    std::abs((int)SkColorGetG(a) - (int)SkColorGetG(b)),
                    std::abs((int)SkColorGetB(a) - (int)SkColorGetB(b))});
      if (difference > found.worst) found = {difference, x, y};
    }
  return found;
}

}  // namespace sigil::weave::test
