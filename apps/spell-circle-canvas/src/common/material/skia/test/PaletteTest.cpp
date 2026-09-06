/** @file
 * The crossing from a picture Skia holds to the table it is made of: the
 * colours come back, a picture bigger than the read size is scaled
 * rather than sampled, and a null image answers nothing.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <sigilmaterial/skia/Palette.h>

using namespace sigil::material;

namespace {

/** A picture of vertical bands, one per colour given. */
sk_sp<SkImage> bands(std::initializer_list<SkColor> colors, int side = 64) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(side, side));
  SkCanvas canvas(bitmap);
  const float width = (float)side / (float)colors.size();
  int index = 0;
  for (SkColor color : colors) {
    SkPaint paint;
    paint.setColor(color);
    canvas.drawRect(SkRect::MakeXYWH((float)index * width, 0, width, (float)side),
                    paint);
    ++index;
  }
  bitmap.setImmutable();
  return bitmap.asImage();
}

}  // namespace

TEST(SkiaPalette, ThePicturesOwnColoursComeBack) {
  const sk_sp<SkImage> image =
      bands({SkColorSetRGB(0xD0, 0x15, 0x15), SkColorSetRGB(0x15, 0x30, 0xD0),
             SkColorSetRGB(0xE8, 0xD9, 0xA0)});
  const Palette table = skia::palette(image, {.entries = 3});
  ASSERT_EQ(table.size(), 3u);
  for (const Color& wanted : {rgb(0xD01515), rgb(0x1530D0), rgb(0xE8D9A0)}) {
    const int entry = closestEntry(table, wanted);
    ASSERT_GE(entry, 0);
    EXPECT_LT(deltaE(table.at(entry), wanted), 3.0f);
  }
}

TEST(SkiaPalette, APictureLargerThanTheReadSizeIsScaledIntoIt) {
  const sk_sp<SkImage> image =
      bands({SK_ColorBLACK, SK_ColorWHITE}, 512);
  // Scaled rather than sampled: the filtered reduction averages the
  // pixels it drops, so both bands are still in the table at a read size
  // a stride could have stepped over one of them at.
  const Palette table = skia::palette(image, {.entries = 2}, 16);
  ASSERT_EQ(table.size(), 2u);
  EXPECT_LT(luminance(table.at(0)), 0.15f);
  EXPECT_GT(luminance(table.at(1)), 0.6f);
}

TEST(SkiaPalette, NothingToReadIsAnEmptyTable) {
  EXPECT_TRUE(skia::palette(nullptr).empty());
}
