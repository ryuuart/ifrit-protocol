#include "sigilmedia/difference/Difference.h"
#include "sigilmedia/advanced/Skia.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>

#include <include/core/SkColor.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "sigilmedia/core/Image.h"

namespace sigil::media {

namespace {

/// Whether row @p y of both pixmaps is stored alike byte for byte over
/// @p width pixels, which only a shared colour and alpha type can say.
bool rowsStoredAlike(const SkPixmap& actual, const SkPixmap& expected, int y,
                     int width) {
  if (actual.colorType() != expected.colorType() ||
      actual.alphaType() != expected.alphaType())
    return false;
  const size_t bytes =
      static_cast<size_t>(width) * actual.info().bytesPerPixel();
  return std::memcmp(actual.addr(0, y), expected.addr(0, y), bytes) == 0;
}

/// @p picture's pixels on the CPU: peeked where they already stand
/// there, read back into @p storage otherwise. False when neither can be
/// done.
bool pixelsOf(const SkImage& picture, SkBitmap& storage, SkPixmap& pixels) {
  if (picture.peekPixels(&pixels)) return true;
  if (!storage.tryAllocPixels(picture.imageInfo().makeColorType(kN32_SkColorType)))
    return false;
  if (!picture.readPixels(nullptr, storage.pixmap(), 0, 0)) return false;
  pixels = storage.pixmap();
  return true;
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

PixelDifference difference(const Picture& actualPicture,
                           const Picture& expectedPicture) {
  const sk_sp<SkImage> actualImage = toSk(actualPicture);
  const sk_sp<SkImage> expectedImage = toSk(expectedPicture);
  if (!actualImage || !expectedImage) return {};
  const SkImage& actual = *actualImage;
  const SkImage& expected = *expectedImage;
  SkBitmap actualStorage;
  SkBitmap expectedStorage;
  SkPixmap actualPixels;
  SkPixmap expectedPixels;
  if (!pixelsOf(actual, actualStorage, actualPixels) ||
      !pixelsOf(expected, expectedStorage, expectedPixels))
    return {};
  return difference(actualPixels, expectedPixels);
}

PixelDifference difference(const Image& actual, const Image& expected) {
  if (actual.frames().empty() || expected.frames().empty()) return {};
  const sk_sp<SkImage> left = deviceImage(actual.frames().front(), nullptr);
  const sk_sp<SkImage> right = deviceImage(expected.frames().front(), nullptr);
  if (!left || !right) return {};
  return difference(fromSk(left), fromSk(right));
}

}  // namespace sigil::media
