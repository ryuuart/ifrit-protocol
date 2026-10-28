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
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/Ramp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "SkiaPaintTestSupport.h"
#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::render;

namespace {

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
  const Paint ramp = Paint::linearGradient({0, 0}, {0, 1}, threeStops());
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
  const Paint ramp = Paint::linearGradient({0, 20}, {0, 80}, threeStops(),
                                           {.units = GradientUnits::Pixels});
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
  const SkBitmap wanted = render(skia::staticShader(Paint::linearGradient(
                                     {0, 0}, {0, 64}, threeStops(), pixels)),
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
  const Paint repeated = Paint::linearGradient({0, 0}, {0, 0.5f}, threeStops(),
                                               {.repeat = Repeat::Repeat});
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
  const SkBitmap corner = render(
      skia::shader(Paint::radialGradient({0.5f, 0.5f}, 1, whiteToBlack), frame),
      100, 100);
  const SkBitmap side = render(
      skia::shader(Paint::radialGradient({0.5f, 0.5f}, 1, whiteToBlack,
                                         {.extent = RadialExtent::ClosestSide}),
                   frame),
      100, 100);
  EXPECT_GT(SkColorGetR(corner.getColor(99, 50)), 60u);
  EXPECT_LT(SkColorGetR(side.getColor(99, 50)), 8u);
  EXPECT_LT(SkColorGetR(corner.getColor(99, 99)), 8u);
}

TEST(SkiaGradient, AConicInBoxUnitsTurnsAroundThePointOfTheBoxItNames) {
  // Centred at a quarter across, the sweep's seam runs up from there:
  // just right of it is the start of the stops and just left of it the
  // end.
  const Paint conic = Paint::conicGradient(
      {0.25f, 0.5f}, {Color{1, 0, 0, 1}, Color{0, 0, 1, 1}});
  EXPECT_TRUE(conic.geometryDependent());
  const SkBitmap bm = render(
      skia::shader(conic, FrameData{.resolution = {100, 100}}), 100, 100);
  EXPECT_GT(SkColorGetR(bm.getColor(28, 20)), 200u);
  EXPECT_GT(SkColorGetB(bm.getColor(22, 20)), 200u);
}

namespace {

const std::vector<ColorStop> kWhiteToBlack{{0.0f, Color{1, 1, 1, 1}},
                                           {1.0f, Color{0, 0, 0, 1}}};

/** @p material rendered over a box of @p width by @p height. */
SkBitmap renderBox(const Material& material, int width, int height) {
  return render(
      skia::shader(skia::paint(material),
                   FrameData{.resolution = {(float)width, (float)height}}),
      width, height);
}

}  // namespace

TEST(SkiaGradient, ACssLinearGradientRunsToTheBottomUnlessTold) {
  const SkBitmap down = renderBox(linearGradient(kWhiteToBlack), 200, 50);
  EXPECT_GT(SkColorGetR(down.getColor(100, 0)), 240u);
  EXPECT_LT(SkColorGetR(down.getColor(100, 49)), 15u);
  const SkBitmap right = renderBox(linearGradient(kWhiteToBlack, 90), 200, 50);
  EXPECT_GT(SkColorGetR(right.getColor(0, 25)), 240u);
  EXPECT_LT(SkColorGetR(right.getColor(199, 25)), 15u);
}

TEST(SkiaGradient, ACssAngleSpansTheBoxCornerToCorner) {
  // At 45 degrees on a box four times wider than tall, the gradient line
  // is long enough that the bottom-left corner is the first stop and the
  // top-right the last, as CSS's is.
  const SkBitmap bm = renderBox(linearGradient(kWhiteToBlack, 45), 200, 50);
  EXPECT_GT(SkColorGetR(bm.getColor(0, 49)), 245u);
  EXPECT_LT(SkColorGetR(bm.getColor(199, 0)), 10u);
}

TEST(SkiaGradient, TowardACornerPutsTheMiddleOnTheOtherDiagonal) {
  // Toward the top-right of a wide box, the top-left and bottom-right
  // corners both sit at the middle stop.
  const SkBitmap bm =
      renderBox(linearGradient(kWhiteToBlack, Toward::TopRight), 200, 50);
  const int topLeft = (int)SkColorGetR(bm.getColor(0, 0));
  const int bottomRight = (int)SkColorGetR(bm.getColor(199, 49));
  EXPECT_NEAR(topLeft, 128, 8);
  EXPECT_NEAR(bottomRight, 128, 8);
  EXPECT_GT(SkColorGetR(bm.getColor(0, 49)), 245u);
}

TEST(SkiaGradient, ACssCircleKeepsItsRadiusAndMeasuresFromItsOwnCentre) {
  // Centred a quarter across a 200 by 100 box, the closest side is 50 px
  // away in every direction, and the circle is as tall as it is wide.
  const SkBitmap bm = renderBox(
      radialGradient(kWhiteToBlack, {.shape = RadialShape::Circle,
                                     .extent = RadialExtent::ClosestSide,
                                     .at = {0.25f, 0.5f}}),
      200, 100);
  EXPECT_LT(SkColorGetR(bm.getColor(50 + 50, 50)), 10u);
  EXPECT_NEAR((int)SkColorGetR(bm.getColor(50 + 30, 50)),
              (int)SkColorGetR(bm.getColor(50, 50 + 30)), 2);
  EXPECT_GT(SkColorGetR(bm.getColor(50 + 30, 50)), 80u);
}

TEST(SkiaGradient, ACssEllipseTakesTheBoxsProportions) {
  // The farthest side from the middle of a 200 by 100 box: 100 px across
  // and 50 down, so half way out reads the same in both directions.
  const SkBitmap bm = renderBox(
      radialGradient(kWhiteToBlack, {.extent = RadialExtent::FarthestSide}),
      200, 100);
  EXPECT_NEAR((int)SkColorGetR(bm.getColor(150, 50)),
              (int)SkColorGetR(bm.getColor(100, 75)), 3);
  EXPECT_LT(SkColorGetR(bm.getColor(199, 50)), 10u);
}

TEST(SkiaGradient, ACssConicStartsFromTheAngleItIsGiven) {
  // From 90 degrees the seam is at 3 o'clock: just below it is the first
  // stop and just above it the last.
  const SkBitmap bm =
      renderBox(conicGradient(kWhiteToBlack, {.fromDegrees = 90}), 100, 100);
  EXPECT_GT(SkColorGetR(bm.getColor(90, 53)), 230u);
  EXPECT_LT(SkColorGetR(bm.getColor(90, 47)), 25u);
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

namespace {

/** @p count stops from @p from to @p to, evenly spaced along a straight
 *  mix — the same ramp as the two ends alone, spelled long. */
std::vector<ColorStop> spelledLong(int count, Color from, Color to) {
  std::vector<ColorStop> stops;
  for (int i = 0; i < count; ++i) {
    const float t = (float)i / (float)(count - 1);
    stops.push_back(
        {t, Color{from.r + (to.r - from.r) * t, from.g + (to.g - from.g) * t,
                  from.b + (to.b - from.b) * t, from.a + (to.a - from.a) * t}});
  }
  return stops;
}

int channelDifference(SkColor a, SkColor b) {
  return std::max({std::abs((int)SkColorGetR(a) - (int)SkColorGetR(b)),
                   std::abs((int)SkColorGetG(a) - (int)SkColorGetG(b)),
                   std::abs((int)SkColorGetB(a) - (int)SkColorGetB(b)),
                   std::abs((int)SkColorGetA(a) - (int)SkColorGetA(b))});
}

}  // namespace

TEST(SkiaGradient, ALongRampReadsAsTheStopsItIsSpelledWith) {
  // A ramp of many stops is read from a lookup made once rather than
  // mixed stop by stop per pixel, and reads the same picture: sixty-five
  // stops along a straight mix are the two ends' gradient, and so is a
  // two hundred and fifty-six stop one, horizontally and radially.
  const Color red{1, 0, 0, 1};
  const Color blue{0, 0.4f, 1, 0.5f};
  for (const bool radial : {false, true})
    for (const int count : {65, 256}) {
      const auto ramp = [&](std::vector<ColorStop> stops) {
        return radial
                   ? Paint::radialGradient({0.5f, 0.5f}, 0.5f, std::move(stops))
                   : Paint::linearGradient({0, 0}, {1, 0}, std::move(stops));
      };
      const FrameData frame{.resolution = {256, 64}};
      const SkBitmap two =
          render(skia::shader(ramp({{0, red}, {1, blue}}), frame), 256, 64);
      const SkBitmap many = render(
          skia::shader(ramp(spelledLong(count, red, blue)), frame), 256, 64);
      int worst = 0;
      for (int y = 0; y < 64; y += 3)
        for (int x = 0; x < 256; ++x)
          worst = std::max(worst, channelDifference(two.getColor(x, y),
                                                    many.getColor(x, y)));
      EXPECT_LE(worst, 1) << count << " stops, radial " << radial;
    }
}
