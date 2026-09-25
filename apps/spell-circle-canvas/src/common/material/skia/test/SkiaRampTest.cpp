/** @file
 * The gradients a list of colour stops reaches Skia through, in box units
 * and in pixels, and a palette as a table sampled nearest.
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

#include <array>
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
std::vector<ColorStop> threeStops() {
  return {{0.0f, Color{1, 0, 0, 1}},
          {0.5f, Color{0, 1, 0, 1}},
          {1.0f, Color{0, 0, 1, 1}}};
}

}  // namespace

TEST(SkiaGradient, ABoxUnitGradientFillsWhateverBoxItIsGiven) {
  // Box units are the default and what a text fill and a mask paint in: a
  // gradient measured in node-local PIXELS instead runs out one pixel down
  // and paints every row below it the last stop, so the picture is flat
  // but for its first line. Asked over a 100 px box, the gradient is
  // spread over all 100 rows: red at the top, green halfway, blue at the
  // bottom.
  const Paint ramp =
      Paint::linearGradient({0, 0}, {0, 1}, threeStops());
  EXPECT_TRUE(ramp.geometryDependent());
  const SkBitmap bm =
      render(skia::shader(ramp, FrameData{.resolution = {100, 100}}), 100, 100);
  EXPECT_GT(SkColorGetR(bm.getColor(50, 2)), 200u);
  EXPECT_LT(SkColorGetB(bm.getColor(50, 2)), 60u);
  EXPECT_GT(SkColorGetG(bm.getColor(50, 50)), 180u);
  EXPECT_GT(SkColorGetB(bm.getColor(50, 97)), 200u);
  EXPECT_LT(SkColorGetR(bm.getColor(50, 97)), 60u);
}

TEST(SkiaGradient, APixelGradientRunsBetweenTheTwoPointsItIsGiven) {
  // Pixel units measure in the coordinates a node is painted in, so a
  // caller who knows its span says it, and both ends pad because the
  // stops carry no answer beyond themselves.
  const Paint ramp = Paint::linearGradient(
      {0, 20}, {0, 80}, threeStops(), {.units = GradientUnits::Pixels});
  EXPECT_FALSE(ramp.geometryDependent());
  const SkBitmap bm = render(skia::staticShader(ramp), 100, 100);
  EXPECT_GT(SkColorGetR(bm.getColor(50, 22)), 180u);
  EXPECT_GT(SkColorGetG(bm.getColor(50, 50)), 180u);
  EXPECT_GT(SkColorGetB(bm.getColor(50, 78)), 180u);
  // Above the span is the first stop and below it the last, not a repeat
  // and not a hole.
  EXPECT_EQ(bm.getColor(50, 2), bm.getColor(50, 19));
  EXPECT_GT(SkColorGetR(bm.getColor(50, 2)), 200u);
  EXPECT_GT(SkColorGetB(bm.getColor(50, 98)), 200u);
}

TEST(SkiaGradient, TheStopsComeFromAListPlainColoursOrARamp) {
  // Three spellings of one gradient: the stops written out, the colours
  // alone (spaced evenly), and a ramp read straight in sRGB, which hands
  // over its own stops.
  const GradientOptions pixels{.units = GradientUnits::Pixels};
  const SkBitmap wanted =
      render(skia::staticShader(Paint::linearGradient({0, 0}, {0, 64}, threeStops(), pixels)),
             8, 64);
  const Ramp straight{.stops = threeStops(), .space = RampSpace::Srgb};
  for (const Paint& other :
       {Paint::linearGradient(
            {0, 0}, {0, 64},
            {Color{1, 0, 0, 1}, Color{0, 1, 0, 1}, Color{0, 0, 1, 1}}, pixels),
        Paint::linearGradient({0, 0}, {0, 64}, straight, pixels)}) {
    const SkBitmap got = render(skia::staticShader(other), 8, 64);
    for (int y : {4, 20, 32, 44, 60})
      EXPECT_EQ(wanted.getColor(4, y), got.getColor(4, y)) << y;
  }
  // A ramp walked in OKLab is read at offsets, so its middle is the
  // perceptual midpoint rather than the straight sRGB one.
  const Ramp perceptual{
      .stops = {{0.0f, Color{1, 0, 0, 1}}, {1.0f, Color{0, 0, 1, 1}}}};
  const ColorStops read(perceptual);
  EXPECT_GT(read.size(), 2u);
  EXPECT_EQ(read.stops()[read.size() / 2].color, perceptual.at(0.5f));
}

TEST(SkiaGradient, ARepeatingBoxGradientStartsOverPastItsEnd) {
  // Half the box long, repeated: the lower half paints what the upper
  // half does, where the default pad would paint it flat blue.
  const Paint repeated = Paint::linearGradient(
      {0, 0}, {0, 0.5f}, threeStops(), {.repeat = Repeat::Repeat});
  EXPECT_TRUE(repeated.geometryDependent());
  const SkBitmap bm = render(
      skia::shader(repeated, FrameData{.resolution = {100, 100}}), 100, 100);
  EXPECT_EQ(bm.getColor(50, 10), bm.getColor(50, 60));
  EXPECT_GT(SkColorGetR(bm.getColor(50, 52)), 200u);
}

TEST(SkiaGradient, TheExtentSaysWhatARadiusOfOneReaches) {
  // Centred on a 100 px box, a radius of 1 reaches the corners at the
  // default and the middle of each side with ClosestSide: at the right
  // edge's middle the first has not yet run out and the second has.
  const std::vector<ColorStop> whiteToBlack{{0.0f, Color{1, 1, 1, 1}},
                                            {1.0f, Color{0, 0, 0, 1}}};
  const FrameData frame{.resolution = {100, 100}};
  const SkBitmap corner =
      render(skia::shader(Paint::radialGradient({0.5f, 0.5f}, 1, whiteToBlack), frame),
             100, 100);
  const SkBitmap side =
      render(skia::shader(Paint::radialGradient({0.5f, 0.5f}, 1, whiteToBlack,
                                         {.extent = RadialExtent::ClosestSide}), frame),
             100, 100);
  EXPECT_GT(SkColorGetR(corner.getColor(99, 50)), 60u);
  EXPECT_LT(SkColorGetR(side.getColor(99, 50)), 8u);
  EXPECT_LT(SkColorGetR(corner.getColor(99, 99)), 8u);
}

TEST(SkiaGradient, AConicInBoxUnitsTurnsAroundThePointOfTheBoxItNames) {
  // Centred at a quarter across, the sweep's seam runs right from there:
  // just below it is the start of the stops and just above it the end.
  const Paint conic = Paint::conicGradient(
      {0.25f, 0.5f}, {Color{1, 0, 0, 1}, Color{0, 0, 1, 1}});
  EXPECT_TRUE(conic.geometryDependent());
  const SkBitmap bm =
      render(skia::shader(conic, FrameData{.resolution = {100, 100}}), 100, 100);
  EXPECT_GT(SkColorGetR(bm.getColor(60, 52)), 200u);
  EXPECT_GT(SkColorGetB(bm.getColor(60, 48)), 200u);
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
  Paint lut = skia::sksl(
      effectFor("uniform shader uPalette;\n"
                "uniform float uIndex;\n"
                "half4 main(float2 p) {\n"
                "  return uPalette.eval(float2(uIndex + 0.5, 0.5));\n"
                "}"));
  lut.slot("uPalette", skia::paletteLookup(pal));
  for (int i : {0, 1, 2, 9}) {
    lut.set("uIndex", (float)i);
    const SkColor got = render(skia::staticShader(lut)).getColor(1, 1);
    const Color want = pal.at(i);
    EXPECT_EQ(SkColorGetR(got), (uint32_t)std::lround(want.r * 255.0f)) << i;
    EXPECT_EQ(SkColorGetG(got), (uint32_t)std::lround(want.g * 255.0f)) << i;
    EXPECT_EQ(SkColorGetB(got), (uint32_t)std::lround(want.b * 255.0f)) << i;
  }
}
