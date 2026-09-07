#pragma once

/** @file
 * The readings a sketch test takes off a picture: what a surface or an
 * image holds, whether two plates are one picture, and where the drawn
 * pixels stand in one.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <cstring>

namespace sigil::sketch::test {

/** What @p surface holds now. */
inline SkBitmap plateOf(SkSurface& surface) {
  SkBitmap bitmap;
  bitmap.allocPixels(surface.imageInfo());
  if (!surface.readPixels(bitmap.pixmap(), 0, 0)) bitmap.reset();
  return bitmap;
}

/** The pixels of @p image, read back. */
inline SkBitmap pixelsOf(const sk_sp<SkImage>& image) {
  SkBitmap bitmap;
  if (!image) return bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(image->width(), image->height()));
  if (!image->readPixels(nullptr, bitmap.pixmap(), 0, 0)) bitmap.reset();
  return bitmap;
}

/** Whether two plates are the same picture, byte for byte. */
inline bool samePicture(const SkBitmap& a, const SkBitmap& b) {
  return a.getPixels() && b.getPixels() &&
         a.computeByteSize() == b.computeByteSize() &&
         std::memcmp(a.getPixels(), b.getPixels(), a.computeByteSize()) == 0;
}

/** THE BOX THE DRAWN PIXELS STAND IN: every pixel that is not @p ground.
 *  Empty when nothing but the ground was drawn. */
inline SkIRect silhouetteOf(const SkBitmap& plate, SkColor ground) {
  SkIRect box = SkIRect::MakeEmpty();
  for (int y = 0; y < plate.height(); ++y)
    for (int x = 0; x < plate.width(); ++x)
      if (plate.getColor(x, y) != ground)
        box.join(SkIRect::MakeXYWH(x, y, 1, 1));
  return box;
}

/** WHERE THE PICTURE STANDS, as fractions of the plate, so two plates of
 *  different sizes can be asked whether they show the same thing in the
 *  same place. The ground is one flat colour and everything drawn stands
 *  against it, so what is not the corner pixel is the picture. */
inline SkRect inkBounds(const SkBitmap& plate) {
  const SkIRect box = silhouetteOf(plate, plate.getColor(0, 0));
  if (box.isEmpty()) return SkRect::MakeEmpty();
  return SkRect::MakeLTRB((float)box.left() / (float)plate.width(),
                          (float)box.top() / (float)plate.height(),
                          (float)box.right() / (float)plate.width(),
                          (float)box.bottom() / (float)plate.height());
}

}  // namespace sigil::sketch::test
