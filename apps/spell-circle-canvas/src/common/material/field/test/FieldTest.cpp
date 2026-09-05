/** @file
 * The fields: the halftone ramp swells downward and its band remaps,
 * grain is monochrome and varies, noise compares by its parameters and
 * shades, a ripple displaces the content it is handed, and the CRT
 * overlay stripes and vignettes in alpha alone.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilshaders/MaterialField.h>

#include "ShaderTable.h"

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

int coverage(const SkBitmap& bm, int y) {
  int n = 0;
  for (int x = 0; x < bm.width(); ++x)
    if (SkColorGetA(bm.getColor(x, y)) > 128) ++n;
  return n;
}

}  // namespace

TEST(Field, HalftoneRampSwellsDownwardAndBandRemaps) {
  const Material ramp = field::halftoneRamp(8, 0.5f, 3.5f, {0, 0, 0, 1});
  EXPECT_TRUE(ramp.geometryDependent());
  const SkBitmap bm = render(ramp, 64, 64);
  EXPECT_LT(coverage(bm, 4), coverage(bm, 60));
  const Material band =
      field::halftoneRamp(8, 0.5f, 3.5f, {0, 0, 0, 1}, 0, 0.9f, 1);
  const SkBitmap bb = render(band, 64, 64);
  // The swell is confined to the last tenth: the top reads as rMin.
  EXPECT_LE(coverage(bb, 30), coverage(bm, 30));
}

TEST(Field, GrainIsMonochromeAndVaries) {
  const Material g = field::grain(0.3f, 3, 4.0f, 1.0f);
  const SkBitmap bm = render(g, 32, 32);
  bool varies = false;
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 32; ++x) {
      const SkColor c = bm.getColor(x, y);
      EXPECT_EQ(SkColorGetR(c), SkColorGetG(c));
      EXPECT_EQ(SkColorGetG(c), SkColorGetB(c));
      varies |= c != bm.getColor(0, 0);
    }
  EXPECT_TRUE(varies);
  EXPECT_EQ(g, field::grain(0.3f, 3, 4.0f, 1.0f));
  EXPECT_FALSE(g == field::grain(0.3f, 4, 4.0f, 1.0f));
  EXPECT_EQ(field::grainRecipe(3).get(), field::grainRecipe(3).get());
}

TEST(Field, NoiseComparesByParametersAndShades) {
  const Material a = field::noise(0.05f, 3, 1);
  EXPECT_EQ(a, field::noise(0.05f, 3, 1));
  EXPECT_FALSE(a == field::noise(0.05f, 3, 2));
  EXPECT_FALSE(a == field::noise(0.05f, 3, 1, true));
  const SkBitmap bm = render(a, 16, 16);
  bool varies = false;
  for (int i = 1; i < 16; ++i) varies |= bm.getColor(i, i) != bm.getColor(0, 0);
  EXPECT_TRUE(varies);
}

TEST(Field, RippleDisplacesTheContent) {
  // Content: a horizontal edge at y = 8. A vertical sine of x moves the
  // edge up and down along x.
  SkBitmap content;
  content.allocPixels(SkImageInfo::MakeN32Premul(32, 16));
  content.eraseColor(SK_ColorTRANSPARENT);
  content.erase(SK_ColorRED, SkIRect::MakeXYWH(0, 8, 32, 8));
  content.setImmutable();
  Material r = field::ripple(3, 16);
  r.child("content", Texture::of(content.asImage()));
  const SkBitmap bm = render(r, 32, 16);
  int firstRow[2] = {16, 16};
  for (int k = 0; k < 2; ++k) {
    const int x = k == 0 ? 4 : 12;  // a quarter wave apart
    for (int y = 0; y < 16; ++y)
      if (SkColorGetA(bm.getColor(x, y)) > 0) {
        firstRow[k] = y;
        break;
      }
  }
  EXPECT_NE(firstRow[0], firstRow[1]);
  EXPECT_EQ(r, r);
}

TEST(Field, CrtOverlayStripesEveryOtherHalfPitchAndDarkensTheCorners) {
  const Material crt = field::crtOverlay();
  EXPECT_TRUE(crt.geometryDependent());
  const SkBitmap bm = render(crt, 128, 128);
  // Black at every pixel; the whole picture is in the alpha.
  EXPECT_EQ(SkColorGetR(bm.getColor(64, 64)), 0u);
  // The default pitch is 4 px with the first half dark, so rows 0 and 1
  // carry the scanline and rows 2 and 3 do not.
  const unsigned lit = SkColorGetA(bm.getColor(64, 2));
  const unsigned dark = SkColorGetA(bm.getColor(64, 0));
  EXPECT_GT(dark, lit);
  EXPECT_EQ(SkColorGetA(bm.getColor(64, 1)), dark);
  EXPECT_EQ(SkColorGetA(bm.getColor(64, 3)), lit);
  // The corner falloff: a corner is further out than the centre row.
  EXPECT_GT(SkColorGetA(bm.getColor(1, 1)), SkColorGetA(bm.getColor(64, 64)));
}

TEST(Field, CrtOverlayScanStrengthAndVignetteAreTheCallersNumbers) {
  const SkBitmap none =
      render(field::crtOverlay(4.0f, 0.0f, 1.45f, 2.15f, 0.0f), 64, 64);
  // Every parameter off: the overlay is fully transparent and changes
  // nothing about what it sits over.
  for (int y = 0; y < 4; ++y) EXPECT_EQ(SkColorGetA(none.getColor(32, y)), 0u);
  const SkBitmap strong =
      render(field::crtOverlay(4.0f, 0.5f, 1.45f, 2.15f, 0.0f), 64, 64);
  EXPECT_GT(SkColorGetA(strong.getColor(32, 0)), 100u);
  EXPECT_EQ(SkColorGetA(strong.getColor(32, 2)), 0u);
}

// ---- the embedded shader table --------------------------------------------

TEST(Field, EveryStockBodyCompiles) {
  skia::install();
  for (const Material& m : field::everyRecipe()) {
    if (!m.recipe().has(Target::SkSL)) continue;
    EXPECT_TRUE(skia::shader(m, {.resolution = {64, 64}})) << m.recipe().name();
  }
}

TEST(Field, TheShaderTableHoldsEveryFileTheDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(
      field::shaderSources(), SIGIL_MATERIAL_FIELD_SHADER_DIR);
}
