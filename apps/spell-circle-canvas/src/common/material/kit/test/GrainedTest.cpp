/** @file
 * The grained surfaces: every recipe compiles and shades, two seeds are
 * two pieces of stone, the timber lights the arris the flip names, the
 * latten sits on its ladder and sheens along its run, and the board is
 * its paint under a tooth.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <cmath>

#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::differing;
using sigil::material::test::luminance;
using sigil::material::test::shade;

TEST(Grained, EveryRecipeCompilesAndTwoSeedsAreTwoPieces) {
  skia::install();
  for (const Material& m :
       {kit::stone(), kit::timber(), kit::latten(), kit::board()}) {
    EXPECT_TRUE(skia::shader(m, {}));
    EXPECT_TRUE(m.recipe().has(Target::SkSL));
    EXPECT_TRUE(m.recipe().has(Target::Slang));
  }
  kit::StoneParams a;
  kit::StoneParams b = a;
  b.seed = 3;
  // One seed twice is one material; two seeds are two pieces of one
  // quarry — the same tones, different flecks and veins.
  EXPECT_EQ(kit::stone(a), kit::stone(a));
  EXPECT_FALSE(kit::stone(a) == kit::stone(b));
  const SkBitmap first = shade(kit::stone(a), 64, 64);
  const SkBitmap second = shade(kit::stone(b), 64, 64);
  EXPECT_GT(differing(first, second), 200);
  EXPECT_EQ(differing(first, shade(kit::stone(a), 64, 64)), 0);
  // The grain is luminance: a coloured stone stays its own hue.
  kit::StoneParams red;
  red.hi = {0.8f, 0.2f, 0.2f, 1};
  red.lo = {0.5f, 0.1f, 0.1f, 1};
  red.speckle = 0;
  const SkBitmap ruddy = shade(kit::stone(red), 32, 32);
  for (int y = 0; y < 32; y += 5)
    for (int x = 0; x < 32; x += 5) {
      const SkColor c = ruddy.getColor(x, y);
      EXPECT_GT(SkColorGetR(c), SkColorGetG(c) * 2);
    }
}

TEST(Grained, TimberLightsTheNearArrisAndFlipLightsTheFar) {
  kit::TimberParams t;
  t.span = 40;
  t.tooth = 0;
  t.figure = 0;
  const SkBitmap near = shade(kit::timber(t), 60, 40);
  // The lit arris along the top, the shaded one along the bottom.
  EXPECT_GT(luminance(near.getColor(30, 1)), luminance(near.getColor(30, 20)));
  EXPECT_LT(luminance(near.getColor(30, 38)), luminance(near.getColor(30, 20)));
  t.flip = 1;
  const SkBitmap far = shade(kit::timber(t), 60, 40);
  EXPECT_LT(luminance(far.getColor(30, 1)), luminance(far.getColor(30, 20)));
  EXPECT_GT(luminance(far.getColor(30, 38)), luminance(far.getColor(30, 20)));
  // Turned to run down y, the arrises stand at the sides.
  t.flip = 0;
  t.along = 1;
  const SkBitmap post = shade(kit::timber(t), 40, 60);
  EXPECT_GT(luminance(post.getColor(1, 30)), luminance(post.getColor(20, 30)));
  EXPECT_LT(luminance(post.getColor(38, 30)), luminance(post.getColor(20, 30)));
}

TEST(Grained, LattenSitsOnItsLadderAndSheensAlongItsRun) {
  kit::LattenParams p;
  p.tooth = 0;
  p.from = {0, 0};
  p.to = {64, 0};
  p.sheen = 0.2f;
  p.level = 0.1f;
  const SkBitmap low = shade(kit::latten(p), 64, 8);
  p.level = 0.9f;
  const SkBitmap high = shade(kit::latten(p), 64, 8);
  // A high level is brighter than a low one at every pixel …
  EXPECT_GT(luminance(high.getColor(32, 4)),
            luminance(low.getColor(32, 4)) + 40);
  // … and along the run the sheen climbs the ladder.
  EXPECT_GT(luminance(low.getColor(60, 4)), luminance(low.getColor(3, 4)));
  // A patina is flecks of its colour, at its alpha.
  p.patina = 1.0f;
  p.patinaCell = 8;
  p.patinaColor = {0, 1, 0, 1};
  const SkBitmap green = shade(kit::latten(p), 64, 8);
  int greened = 0;
  for (int x = 0; x < 64; ++x)
    greened += SkColorGetG(green.getColor(x, 4)) > 200 &&
               SkColorGetR(green.getColor(x, 4)) < 60;
  EXPECT_GT(greened, 4);
}

TEST(Grained, BoardIsItsPaintUnderATooth) {
  kit::BoardParams b;
  b.paint = {0.5f, 0.5f, 0.5f, 1};
  const SkBitmap card = shade(kit::board(b), 48, 48);
  int lo = 255, hi = 0;
  for (int y = 0; y < 48; y += 3)
    for (int x = 0; x < 48; x += 3) {
      const int l = luminance(card.getColor(x, y));
      lo = std::min(lo, l);
      hi = std::max(hi, l);
    }
  // Around the paint, and varying: a tooth, not a flat.
  EXPECT_GT(lo, 100);
  EXPECT_LT(hi, 156);
  EXPECT_GT(hi - lo, 4);
  b.tooth = 0;
  b.wear = 0;
  const SkBitmap flat = shade(kit::board(b), 8, 8);
  EXPECT_EQ(luminance(flat.getColor(4, 4)), luminance(flat.getColor(1, 1)));
}

// ---- the embedded shader table --------------------------------------------
