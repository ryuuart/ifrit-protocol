/** @file
 * The four crossings out of the colour leaf: the same stops as a
 * gradient over a span of node-local pixels and as a paint over the unit
 * square, and a palette as a table sampled nearest.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/Ramp.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::render;

namespace {

sk_sp<SkRuntimeEffect> effectFor(const char* src) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(src));
  return effect;
}

/** Red at the top, blue at the bottom, with green halfway — three stops
 *  a wrong span shows as a flat band of the last one. */
std::vector<RampStop> threeStops() {
  return {{0.0f, Color{1, 0, 0, 1}},
          {0.5f, Color{0, 1, 0, 1}},
          {1.0f, Color{0, 0, 1, 1}}};
}

}  // namespace

TEST(SkiaRamp, TheUnitRampFillsWhateverBoxItIsGiven) {
  // The unit square is what a text fill and a mask paint in: a ramp
  // measured in node-local PIXELS instead runs out one pixel down and
  // paints every row below it the last stop, so the picture is flat but
  // for its first line. Asked over a 100 px box, the ramp is spread over
  // all 100 rows: red at the top, green halfway, blue at the bottom.
  const skia::Paint ramp = skia::unitRamp(threeStops());
  EXPECT_TRUE(ramp.geometryDependent());
  const SkBitmap bm =
      render(ramp.shaderFor(skia::PaintFrame{.size = {100, 100}}), 100, 100);
  EXPECT_GT(SkColorGetR(bm.getColor(50, 2)), 200u);
  EXPECT_LT(SkColorGetB(bm.getColor(50, 2)), 60u);
  EXPECT_GT(SkColorGetG(bm.getColor(50, 50)), 180u);
  EXPECT_GT(SkColorGetB(bm.getColor(50, 97)), 200u);
  EXPECT_LT(SkColorGetR(bm.getColor(50, 97)), 60u);
}

TEST(SkiaRamp, TheVerticalRampRunsBetweenTheTwoRowsItIsGiven) {
  // The other crossing measures in the coordinates a node is painted in,
  // so a caller who knows its span says it, and both ends clamp because
  // a ramp carries no answer beyond its stops.
  const sk_sp<SkShader> shader = skia::verticalRamp(20, 80, threeStops());
  ASSERT_NE(shader, nullptr);
  const SkBitmap bm = render(shader, 100, 100);
  EXPECT_GT(SkColorGetR(bm.getColor(50, 22)), 180u);
  EXPECT_GT(SkColorGetG(bm.getColor(50, 50)), 180u);
  EXPECT_GT(SkColorGetB(bm.getColor(50, 78)), 180u);
  // Above the span is the first stop and below it the last, not a repeat
  // and not a hole.
  EXPECT_EQ(bm.getColor(50, 2), bm.getColor(50, 19));
  EXPECT_GT(SkColorGetR(bm.getColor(50, 2)), 200u);
  EXPECT_GT(SkColorGetB(bm.getColor(50, 98)), 200u);
}

TEST(SkiaRamp, APaletteCrossesToAShaderAsATableSampledNearest) {
  const Palette pal{{Color{1, 0, 0, 1}, Color{0, 1, 0, 1}, Color{0, 0, 1, 1}}};
  const sk_sp<SkImage> table = skia::paletteImage(pal);
  ASSERT_NE(table, nullptr);
  EXPECT_EQ(table->width(), 3);
  EXPECT_EQ(table->height(), 1);
  EXPECT_EQ(skia::paletteImage(Palette{}), nullptr);

  // The body reads the table at texel centres, so entry n is entry n. The
  // index runs past the end deliberately: clamped, it is the last entry,
  // which is what Palette::at answers on the CPU for the same index.
  skia::Paint lut = skia::Paint::sksl(
      effectFor("uniform shader uPalette;\n"
                "uniform float uIndex;\n"
                "half4 main(float2 p) {\n"
                "  return uPalette.eval(float2(uIndex + 0.5, 0.5));\n"
                "}"));
  lut.child("uPalette", skia::paletteLookup(pal));
  for (int i : {0, 1, 2, 9}) {
    lut.uniform("uIndex", (float)i);
    const SkColor got = render(lut.staticShader()).getColor(1, 1);
    const Color want = pal.at(i);
    EXPECT_EQ(SkColorGetR(got), (uint32_t)std::lround(want.r * 255.0f)) << i;
    EXPECT_EQ(SkColorGetG(got), (uint32_t)std::lround(want.g * 255.0f)) << i;
    EXPECT_EQ(SkColorGetB(got), (uint32_t)std::lround(want.b * 255.0f)) << i;
  }
}
