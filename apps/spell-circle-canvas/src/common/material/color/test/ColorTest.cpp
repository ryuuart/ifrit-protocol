/** @file
 * The colour feature: an exponent transform bakes to a LUT material that
 * darkens mid-grey by the expected amount and leaves white alone, a bad
 * config fails soft to an empty material, and the sRGB curve round-trips.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Texture.h>

#include <cmath>

#ifdef SIGILMATERIAL_ENABLE_OCIO
#include <sigilmaterial/color/Ocio.h>
#endif

using namespace sigil::material;

TEST(Color, SrgbRoundTrips) {
  for (float v : {0.0f, 0.02f, 0.25f, 0.5f, 0.75f, 1.0f})
    EXPECT_NEAR(linearToSrgb(srgbToLinear(v)), v, 1e-5f);
  const Color mid{0.5f, 0.5f, 0.5f, 1};
  const Color back = fromOklab(toOklab(mid));
  EXPECT_NEAR(back.r, 0.5f, 1e-4f);
  EXPECT_NEAR(back.g, 0.5f, 1e-4f);
}

#ifdef SIGILMATERIAL_ENABLE_OCIO
TEST(Color, ExponentBakesToALutThatGradesTheContent) {
  if (!color::available()) GTEST_SKIP() << "OCIO raw config unavailable";
  skia::install();
  Material grade = color::exponent(2.2f);
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
  const Material bad = color::viewTransform("ocio://no-such-config", "x", "y");
  EXPECT_EQ(bad.leaf("lut"), nullptr);
}
#endif
