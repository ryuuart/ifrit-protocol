#pragma once

/** @file
 * Reading a rendered surface back: whether any pixel answers a question,
 * and how many do. A pixel scan written out by hand is a loop nobody
 * reads; written once it is the question the case is actually asking.
 * How far two renders stand apart is SigilMedia's reading,
 * `sigil::media::difference`.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPixmap.h>

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

}  // namespace sigil::weave::test
