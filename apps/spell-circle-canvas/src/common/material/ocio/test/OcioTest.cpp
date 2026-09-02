/** @file
 * The ocio feature: an exponent transform bakes to a LUT material that
 * darkens mid-grey by the expected amount and leaves white alone, and a
 * bad config fails soft to an empty material. Skips where the transforms
 * are unavailable.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilmaterial/ocio/Ocio.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Texture.h>

#include <cmath>

using namespace sigil::material;

TEST(Ocio, ExponentBakesToALutThatGradesTheContent) {
  if (!ocio::available()) GTEST_SKIP() << "OCIO raw config unavailable";
  skia::install();
  Material grade = ocio::exponent(2.2f);
  ASSERT_NE(grade.leaf("lut"), nullptr);
  EXPECT_FLOAT_EQ(grade.get<float>("lutSize"), 33.0f);
  // Content: a 4x4 mid-grey with a white pixel.
  SkBitmap content;
  content.allocPixels(SkImageInfo::MakeN32Premul(4, 4));
  content.eraseColor(SkColorSetARGB(255, 128, 128, 128));
  content.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1, 1));
  content.setImmutable();
  grade.child("content", Texture::of(content.asImage()));
  SkBitmap out;
  out.allocPixels(SkImageInfo::MakeN32Premul(4, 4));
  SkCanvas canvas(out);
  SkPaint paint;
  paint.setShader(skia::shader(grade, {}));
  canvas.drawPaint(paint);
  const int grey = (int)SkColorGetR(out.getColor(2, 2));
  const int expected = (int)std::lround(255.0 * std::pow(128.0 / 255.0, 2.2));
  EXPECT_NEAR(grey, expected, 3);
  EXPECT_EQ(SkColorGetR(out.getColor(0, 0)), 255u);
  // A bad config fails soft: an empty LUT slot, no throw.
  const Material bad = ocio::viewTransform("ocio://no-such-config", "x", "y");
  EXPECT_EQ(bad.leaf("lut"), nullptr);
}
