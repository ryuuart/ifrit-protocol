/** @file
 * The pixel difference: two pixels counted and the wider gap located,
 * identical pictures reading identical, colour types compared as the
 * colours they store rather than as their bytes, and only the extent both
 * pictures cover read.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImageInfo.h>
#include <sigilimage/difference/Difference.h>

using namespace sigil::image;

namespace {

SkBitmap white(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorWHITE);
  return bitmap;
}

}  // namespace

TEST(PixelDifference, CountsPixelsAndLocatesTheWidestChannel) {
  const SkBitmap first = white(8, 8);
  SkBitmap second = white(8, 8);
  *second.getAddr32(2, 3) = SkPreMultiplyColor(SkColorSetRGB(255, 250, 255));
  *second.getAddr32(5, 6) = SkPreMultiplyColor(SkColorSetRGB(255, 200, 255));

  const PixelDifference apart = difference(first.pixmap(), second.pixmap());
  EXPECT_EQ(apart.differingPixels, 2);
  EXPECT_EQ(apart.worst, 55);
  EXPECT_EQ(apart.x, 5);
  EXPECT_EQ(apart.y, 6);
  EXPECT_FALSE(apart.identical());
  EXPECT_TRUE(difference(first.pixmap(), first.pixmap()).identical());
}

TEST(PixelDifference, ColourTypesAreComparedAsTheColoursTheyStore) {
  // The same white in BGRA and RGBA order: different bytes, one colour.
  SkBitmap bgra;
  bgra.allocPixels(SkImageInfo::Make(4, 4, kBGRA_8888_SkColorType,
                                     kPremul_SkAlphaType));
  bgra.eraseColor(SkColorSetRGB(10, 20, 30));
  SkBitmap rgba;
  rgba.allocPixels(SkImageInfo::Make(4, 4, kRGBA_8888_SkColorType,
                                     kPremul_SkAlphaType));
  rgba.eraseColor(SkColorSetRGB(10, 20, 30));
  EXPECT_TRUE(difference(bgra.pixmap(), rgba.pixmap()).identical());
}

TEST(PixelDifference, OnlyTheExtentBothCoverIsRead) {
  const SkBitmap small = white(4, 4);
  SkBitmap large = white(8, 8);
  *large.getAddr32(6, 6) = SkPreMultiplyColor(SK_ColorBLACK);
  EXPECT_TRUE(difference(small.pixmap(), large.pixmap()).identical());
}
