/** @file
 * The signed-distance surfaces: a star fills its centre and misses its
 * corners at the resolution the frame supplies, the pad follows the
 * style's reach, a bound glow makes the material live, and equal styles
 * make equal materials.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

using namespace sigil::material;

namespace {

SkBitmap render(const Material& m, int w, int h) {
  skia::install();
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  SkCanvas canvas(bm);
  canvas.clear(SK_ColorTRANSPARENT);
  SkPaint paint;
  paint.setShader(skia::shader(m, {.resolution = {(float)w, (float)h}}));
  canvas.drawPaint(paint);
  return bm;
}

}  // namespace

TEST(Sdf, StarFillsCenterMissesCorners) {
  sdf::Style style;
  style.fill = {1, 1, 1, 1};
  const SkBitmap bm = render(sdf::material(sdf::star(5, 2.5f), style), 64, 64);
  EXPECT_EQ(bm.getColor(32, 32) & 0xff000000, 0xff000000u);
  EXPECT_EQ(bm.getColor(2, 2) & 0xff000000, 0u);
  // The same style on every kind compiles and covers the centre.
  EXPECT_NE(
      render(sdf::material(sdf::circle(), style), 32, 32).getColor(16, 16), 0u);
  EXPECT_NE(
      render(sdf::material(sdf::roundBox(6), style), 32, 32).getColor(16, 16),
      0u);
}

TEST(Sdf, PadReservesTheStylesReach) {
  sdf::Style plain;
  EXPECT_FLOAT_EQ(sdf::pad(plain), 1.0f);
  sdf::Style glow;
  glow.glowRadius = 5;
  EXPECT_FLOAT_EQ(sdf::pad(glow), 17.0f);
  EXPECT_FLOAT_EQ(sdf::minBoxFor(glow, 20), 54.0f);
  sdf::Style shadow;
  shadow.shadowColor = {0, 0, 0, 1};
  shadow.shadowOffset = {3, -4};
  shadow.shadowBlur = 2;
  shadow.borderWidth = 2;
  EXPECT_FLOAT_EQ(sdf::pad(shadow), 1 + 4 + 3 + 1);
}

TEST(Sdf, StyleIsTheRecipeAndAGlowBindingIsLive) {
  sdf::Style a;
  a.glowRadius = 4;
  const Material x = sdf::material(sdf::circle(), a);
  EXPECT_EQ(x, sdf::material(sdf::circle(), a));
  EXPECT_FALSE(x == sdf::material(sdf::roundBox(2), a));
  EXPECT_TRUE(x.geometryDependent());
  EXPECT_FALSE(x.isAnimated());
  EXPECT_FLOAT_EQ(x.get<float>("uPad"), sdf::pad(a));
  choreograph::Output<float> glow;
  Material bound = x;
  bound.bind("uGlowR", &glow);
  EXPECT_TRUE(bound.isAnimated());
  // Star's pointiness clamps into [2, points].
  EXPECT_EQ(sdf::star(5, 9.0f), sdf::star(5, 5.0f));
}
