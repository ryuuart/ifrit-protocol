/** @file
 * The palette side of the colour leaf: the polar round trip, the hue
 * schemes read around one colour, the threshold a dither answers at a
 * pixel, and the table a run of pixels is made of.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/color/Dither.h>
#include <sigilmaterial/color/Extract.h>
#include <sigilmaterial/color/Harmony.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

using namespace sigil::material;

namespace {

/** The shorter way from one hue to another, in degrees. */
float hueApart(float a, float b) {
  float delta = std::fmod(std::abs(a - b), 360.0f);
  return delta > 180.0f ? 360.0f - delta : delta;
}

}  // namespace

TEST(Harmony, ThePolarFormRoundTripsAndAGreyHasNoDirection) {
  for (const Color& c : {Color{0.9f, 0.1f, 0.2f, 1.0f},
                         Color{0.1f, 0.3f, 0.8f, 0.5f},
                         Color{0.42f, 0.42f, 0.42f, 1.0f}}) {
    const Color back = fromOklch(toOklch(c));
    EXPECT_NEAR(back.r, c.r, 1e-4f);
    EXPECT_NEAR(back.g, c.g, 1e-4f);
    EXPECT_NEAR(back.b, c.b, 1e-4f);
    EXPECT_NEAR(back.a, c.a, 1e-6f);
  }
  // A grey is at the axis: no chroma, and therefore no hue to preserve.
  EXPECT_NEAR(toOklch(Color{0.5f, 0.5f, 0.5f, 1.0f}).chroma, 0.0f, 1e-3f);
}

TEST(Harmony, ARotationHoldsTheLightnessAndGivesUpOnlyChroma) {
  // A muted colour: every hue at this chroma is inside sRGB, so the
  // rotation is exact in all three numbers.
  const Color muted = rgb(0x8E6E7E);
  const Oklch from = toOklch(muted);
  for (float degrees : {30.0f, 120.0f, 210.0f, 330.0f}) {
    const Oklch to = toOklch(rotateHue(muted, degrees));
    // The whole reason the rotation is in OKLCH: an HSV rotation holds
    // the largest channel, which is not a brightness, and the family
    // falls apart. Here only the direction changed.
    EXPECT_NEAR(to.L, from.L, 2e-3f) << degrees;
    EXPECT_NEAR(to.chroma, from.chroma, 2e-3f) << degrees;
    EXPECT_NEAR(hueApart(to.hueDegrees, from.hueDegrees + degrees), 0.0f, 1.0f)
        << degrees;
  }

  // A vivid one: the green sRGB cannot mix at a saturated red's chroma
  // comes back duller, and still at the hue and the weight it was asked
  // for — which is what the component-wise cut would have lost.
  const Color vivid = rgb(0xC80000);
  const Oklch vividFrom = toOklch(vivid);
  const Oklch vividTo = toOklch(rotateHue(vivid, 120.0f));
  EXPECT_NEAR(vividTo.L, vividFrom.L, 2e-3f);
  EXPECT_LT(vividTo.chroma, vividFrom.chroma);
  EXPECT_NEAR(hueApart(vividTo.hueDegrees, vividFrom.hueDegrees), 120.0f, 1.0f);
  // Where the plain clamp lands instead, for the same ask: several
  // degrees off the hue that was asked for, because three channels are
  // cut by three different amounts.
  const float clamped =
      hueApart(toOklch(fromOklch({vividFrom.L, vividFrom.chroma,
                                  vividFrom.hueDegrees + 120.0f, 1.0f}))
                   .hueDegrees,
               vividFrom.hueDegrees);
  EXPECT_GT(std::abs(clamped - 120.0f), 3.0f);
}

TEST(Harmony, EachSchemeIsItsOwnSetOfAnglesWithTheBaseFirst) {
  const Color base = rgb(0x6E86A2);
  const float baseHue = toOklch(base).hueDegrees;

  const Palette complement = harmony(base, Scheme::Complement);
  ASSERT_EQ(complement.size(), 2u);
  EXPECT_EQ(complement.at(0), base);
  EXPECT_NEAR(hueApart(toOklch(complement.at(1)).hueDegrees, baseHue), 180.0f,
              1.0f);

  const Palette triad = harmony(base, Scheme::Triad);
  ASSERT_EQ(triad.size(), 3u);
  EXPECT_NEAR(hueApart(toOklch(triad.at(1)).hueDegrees, baseHue), 120.0f, 1.0f);
  EXPECT_NEAR(hueApart(toOklch(triad.at(2)).hueDegrees, baseHue), 120.0f, 1.0f);

  const Palette analogous = harmony(base, Scheme::Analogous, 25.0f);
  ASSERT_EQ(analogous.size(), 3u);
  EXPECT_NEAR(hueApart(toOklch(analogous.at(1)).hueDegrees, baseHue), 25.0f,
              1.0f);
  EXPECT_NEAR(hueApart(toOklch(analogous.at(2)).hueDegrees, baseHue), 25.0f,
              1.0f);

  EXPECT_EQ(harmony(base, Scheme::SplitComplement).size(), 3u);
  EXPECT_EQ(harmony(base, Scheme::Tetrad, 90.0f).size(), 4u);
  // The spread is not read by the schemes whose angles are fixed.
  EXPECT_EQ(harmony(base, Scheme::Triad, 10.0f).entries,
            harmony(base, Scheme::Triad, 80.0f).entries);
}

TEST(Dither, TheOrderedMatrixHoldsEveryThresholdOnceAndAveragesAHalf) {
  const Dither dither{.matrix = 4};
  std::set<float> seen;
  double total = 0.0;
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x) {
      const float t = dither.threshold(x, y);
      seen.insert(t);
      total += t;
      EXPECT_GE(t, 0.0f);
      EXPECT_LT(t, 1.0f);
    }
  EXPECT_EQ(seen.size(), 16u);
  EXPECT_NEAR(total / 16.0, 0.5, 1e-6);
  // The 2 x 2 matrix is the one every recursion starts from, and its
  // four cells are the quarters in the order that spreads them.
  const Dither small{.matrix = 2};
  EXPECT_NEAR(small.threshold(0, 0), 0.125f, 1e-6f);
  EXPECT_NEAR(small.threshold(1, 0), 0.625f, 1e-6f);
  EXPECT_NEAR(small.threshold(0, 1), 0.875f, 1e-6f);
  EXPECT_NEAR(small.threshold(1, 1), 0.375f, 1e-6f);
  // It tiles, and a negative coordinate folds forward rather than
  // mirroring.
  EXPECT_FLOAT_EQ(dither.threshold(4, 4), dither.threshold(0, 0));
  EXPECT_FLOAT_EQ(dither.threshold(-4, -4), dither.threshold(0, 0));
}

TEST(Dither, TheNoiseHasNoPeriodAndStillAveragesAHalf) {
  const Dither dither{.kind = DitherKind::Noise};
  double total = 0.0;
  int cells = 0;
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 64; ++x) {
      const float t = dither.threshold(x, y);
      EXPECT_GE(t, 0.0f);
      EXPECT_LT(t, 1.0f);
      total += t;
      ++cells;
    }
  EXPECT_NEAR(total / cells, 0.5, 0.02);
  EXPECT_NE(dither.threshold(0, 0), dither.threshold(4, 4));
}

TEST(Dither, ARoundedRampAveragesToTheValueItWasAskedFor) {
  const Dither dither{.matrix = 4, .levels = 2};
  // One bit per channel: every pixel is black or white, and the average
  // over one tile is the grey that was asked for. That is the whole
  // claim a dither makes.
  double total = 0.0;
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x) {
      const Color out = dither.at({0.25f, 0.25f, 0.25f, 1.0f}, x, y);
      EXPECT_TRUE(out.r == 0.0f || out.r == 1.0f);
      EXPECT_FLOAT_EQ(out.a, 1.0f);
      total += out.r;
    }
  EXPECT_NEAR(total / 16.0, 0.25, 1e-6);

  // No amount is plain rounding: the same answer at every pixel.
  const Dither flat{.matrix = 4, .levels = 2, .amount = 0.0f};
  EXPECT_FLOAT_EQ(flat.at({0.4f, 0, 0, 1}, 3, 1).r, 0.0f);
  EXPECT_FLOAT_EQ(flat.at({0.6f, 0, 0, 1}, 3, 1).r, 1.0f);
  EXPECT_TRUE(flat.on(0.6f, 3, 1));
  EXPECT_FALSE(flat.on(0.4f, 3, 1));
}

TEST(Extract, TheTableIsTheColoursThePicturesActuallyHolds) {
  const Color red = rgb(0xD01515), blue = rgb(0x1530D0), sand = rgb(0xE8D9A0);
  std::vector<Color> pixels;
  for (int i = 0; i < 400; ++i) pixels.push_back(red);
  for (int i = 0; i < 300; ++i) pixels.push_back(blue);
  for (int i = 0; i < 40; ++i) pixels.push_back(sand);

  const Palette table = palette(pixels, {.entries = 3});
  ASSERT_EQ(table.size(), 3u);
  // What the moved means claim: every colour the picture is made of is
  // in the table, to well inside the difference an eye can see — the
  // 40-pixel sand included, which is the case a table chosen by
  // population alone loses.
  for (const Color& wanted : {red, blue, sand}) {
    const int entry = closestEntry(table, wanted);
    ASSERT_GE(entry, 0);
    EXPECT_LT(deltaE(table.at(entry), wanted), 2.0f);
  }
  // Darkest first, so two runs can be compared entry by entry.
  for (size_t i = 1; i < table.size(); ++i)
    EXPECT_LE(toOklab(table.at((int)i - 1)).L, toOklab(table.at((int)i)).L);
}

TEST(Extract, TheDividedBoxesCoverTheRangeTheyWereGiven) {
  // What median cut claims, on the picture it is claimed for: a run of
  // colours with no clusters in it, where the answer wanted is a table
  // spread over the range rather than the colours it dwells on.
  std::vector<Color> ramp;
  for (int i = 0; i < 256; ++i)
    ramp.push_back({(float)i / 255.0f, (float)i / 255.0f, (float)i / 255.0f, 1});
  const Palette table =
      palette(ramp, {.entries = 4, .method = PaletteMethod::MedianCut});
  ASSERT_EQ(table.size(), 4u);
  for (size_t i = 1; i < table.size(); ++i)
    EXPECT_LT(toOklab(table.at((int)i - 1)).L, toOklab(table.at((int)i)).L);
  // The ends of the run are covered: no entry stands for more than a
  // quarter of it, so the darkest is well inside the dark quarter and
  // the lightest well inside the light one.
  EXPECT_LT(table.at(0).r, 0.35f);
  EXPECT_GT(table.at(3).r, 0.65f);
}

TEST(Extract, ItReadsTheSamePixelsTheSameWayAndSkipsTheOnesItIsTold) {
  std::vector<Color> pixels;
  for (int i = 0; i < 64; ++i)
    pixels.push_back({(float)i / 64.0f, 0.2f, 0.7f, 1.0f});
  // Deterministic: no seed, and the farthest-first start makes the two
  // runs the same table rather than merely a similar one.
  EXPECT_EQ(palette(pixels, {.entries = 4}).entries,
            palette(pixels, {.entries = 4}).entries);

  // A cut-out's transparent surround is not one of its colours.
  std::vector<Color> withHole = pixels;
  for (int i = 0; i < 500; ++i) withHole.push_back({0, 0, 0, 0});
  EXPECT_EQ(palette(withHole, {.entries = 4}).entries,
            palette(pixels, {.entries = 4}).entries);

  // A picture with fewer colours than the table asked for answers a
  // shorter table rather than repeating one.
  const std::vector<Color> two{{1, 0, 0, 1}, {0, 0, 1, 1}, {1, 0, 0, 1}};
  EXPECT_EQ(palette(two, {.entries = 6}).size(), 2u);
  EXPECT_TRUE(palette({}, {.entries = 4}).empty());
  EXPECT_EQ(closestEntry({}, {1, 1, 1, 1}), -1);
}
