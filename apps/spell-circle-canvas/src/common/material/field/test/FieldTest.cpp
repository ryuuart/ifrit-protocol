/** @file
 * The fields: the halftone ramp swells downward and its band remaps,
 * grain is monochrome and varies, noise compares by its parameters and
 * shades, and a ripple displaces the content it is handed.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilshaders/MaterialField.h>

#include <algorithm>
#include <vector>

#include "ShaderTable.h"
#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::render;

namespace {

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
  // The swell is confined to the last tenth: the top reads as the minimum radius.
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
  r.slot("content", Texture(content.asImage()));
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

// ---- the embedded shader table --------------------------------------------

TEST(Field, EveryStockBodyCompiles) {
  // One of each recipe the feature ships, with the grain at every octave
  // count up to the default: each count is its own program.
  // The ripple's content slot is dressed: a slot left empty and a slot
  // holding an image are not the same program.
  Material warp = field::ripple(4, 32);
  warp.slot("content", Texture(test::solid(SK_ColorMAGENTA, 4, 4)));
  std::vector<Material> all{
      field::halftoneRamp(8, 1, 3, {1, 1, 1, 1}, 15.0f, 0.1f, 0.9f),
      field::noise(0.03f), warp};
  for (int octaves = 1; octaves <= 4; ++octaves)
    all.push_back(field::grain(0.05f, octaves));
  for (const Material& m : all) {
    if (!m.recipe().has(Target::SkSL)) continue;
    EXPECT_TRUE(skia::shader(m, {.resolution = {64, 64}})) << m.recipe().name();
  }
}

TEST(Field, TheShaderTableHoldsEveryFileTheDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(
      field::shaderSources(), SIGIL_MATERIAL_FIELD_SHADER_DIR);
}
