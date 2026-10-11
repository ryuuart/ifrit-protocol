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
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/slang/SlangCompiler.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilshaders/MaterialField.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "ShaderTable.h"
#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::render;

namespace {

/** The standard deviation of the picture's luminance, in eight-bit
 *  levels, read on the Rec.709 weights over the eight-bit channels. */
double luminanceDeviation(const SkBitmap& bitmap) {
  double sum = 0, squares = 0;
  const double count = (double)bitmap.width() * bitmap.height();
  for (int y = 0; y < bitmap.height(); ++y)
    for (int x = 0; x < bitmap.width(); ++x) {
      const SkColor colour = bitmap.getColor(x, y);
      const double light = 0.2126 * SkColorGetR(colour) +
                           0.7152 * SkColorGetG(colour) +
                           0.0722 * SkColorGetB(colour);
      sum += light;
      squares += light * light;
    }
  const double mean = sum / count;
  return std::sqrt(std::max(0.0, squares / count - mean * mean));
}

/** @p material lowered to the one Skia paint a layered material becomes,
 *  and painted over a @p width × @p height surface. */
SkBitmap painted(const Material& material, int width, int height) {
  return render(skia::shader(skia::paint(material),
                             {.resolution = {(float)width, (float)height}}),
                width, height);
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
  // The swell is confined to the last tenth: the top reads as the minimum
  // radius.
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

// A grain moves a near-black ground as far as it moves a mid-tone one,
// since a dark ground is where a grain is most wanted, and no grain at
// all is the ground exactly.
TEST(Field, AGrainHoldsItsStrengthOnADarkGround) {
  const Color night = hexColor(0x0B111A), clay = hexColor(0x8A7560);
  const double dark =
      luminanceDeviation(painted(grained(night, 0.1f), 256, 256));
  const double middle =
      luminanceDeviation(painted(grained(clay, 0.1f), 256, 256));
  EXPECT_GT(middle, 1.0);
  EXPECT_GE(dark, 0.75 * middle) << "dark " << dark << ", mid-tone " << middle;

  for (const Color ground : {night, clay}) {
    EXPECT_EQ(grained(ground, 0.0f), from(ground));
    const SkBitmap still = painted(grained(ground, 0.0f), 64, 64);
    const SkBitmap flat = painted(from(ground), 64, 64);
    EXPECT_TRUE(sigil::material::test::identical(still, flat));
  }
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

// With no jitter every seed stands at its cell's centre, so at a cell's
// centre the nearest seed is zero away and the distance rises toward the
// cell's edge; the wall between two cells is where the nearest and the
// second are equally far, so blue falls to zero there.
TEST(Field, CellularNoiseIsZeroAtASeedAndRisesAwayFromIt) {
  const Material cells = noise(1.0f / 32.0f, {.cellular = true, .jitter = 0});
  EXPECT_EQ(cells, field::cellular(1.0f / 32.0f, 0.0f));
  const SkBitmap bm = painted(cells, 64, 64);
  const auto red = [&](int x, int y) { return SkColorGetR(bm.getColor(x, y)); };
  const auto blue = [&](int x, int y) {
    return SkColorGetB(bm.getColor(x, y));
  };
  EXPECT_LE(red(16, 16), 6);
  EXPECT_LT(red(16, 16), red(20, 16));
  EXPECT_LT(red(20, 16), red(24, 16));
  EXPECT_LT(red(24, 16), red(28, 16));
  EXPECT_LT(red(16, 16), red(24, 24));
  EXPECT_LE(blue(32, 16), 12);
  EXPECT_GT(blue(16, 16), blue(30, 16));

  // A jittered field moves its seeds and still finds some point at zero.
  const Material jittered = field::cellular(1.0f / 16.0f, 1.0f, 3.0f);
  EXPECT_FALSE(jittered == field::cellular(1.0f / 16.0f, 1.0f, 4.0f));
  const SkBitmap moved = painted(jittered, 64, 64);
  int lowest = 255;
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 64; ++x)
      lowest = std::min<int>(lowest, SkColorGetR(moved.getColor(x, y)));
  EXPECT_LE(lowest, 8);
  EXPECT_FALSE(sigil::material::test::identical(bm, moved));
}

// The device body is the same field: it compiles as a surface whose
// parameters and frame inputs the compiler reports.
TEST(Field, CellularNoiseHasASlangSurface) {
  const auto& recipe = *field::cellularRecipe();
  ASSERT_TRUE(recipe.has(Target::Slang));
  const std::string source = recipe.source(Target::Slang) + R"SLANG(
struct VSOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };

[shader("vertex")]
VSOut vsCellular(uint id : SV_VertexID) {
  VSOut out;
  out.position = float4(float(id), 0, 0, 1);
  out.uv = float2(0.5, 0.5);
  return out;
}

[shader("fragment")]
float4 fsCellular(VSOut input) : SV_Target {
  return surface(input.uv);
}
)SLANG";
  slang::Compiled built;
  std::string error;
  ASSERT_TRUE(slang::compileModule(source, "vsCellular", "fsCellular",
                                   /*lit=*/false, &built, &error))
      << error;
  EXPECT_NE(built.uniform("uFrequency"), nullptr);
  EXPECT_NE(built.uniform("uJitter"), nullptr);
  EXPECT_NE(built.uniform("uResolution"), nullptr);
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
      field::noise(0.03f), field::cellular(0.05f), warp};
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
