/** @file
 * The woven cloth: a sett expands to its threadcount, a reflective sett
 * is the half between its pivots, the pivots are read back off the
 * count, a plain weave alternates and a twill runs on a diagonal, the
 * rib darkens only the weft floats, the repeat comes round with the
 * weave, one pixel per thread reads what the CPU reads, and gingham and
 * houndstooth are the same generator at another sett and another weave.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilmaterial/pattern/Weave.h>

#include <algorithm>
#include <set>
#include <vector>

using namespace sigil::material;
using pattern::Cloth;
using pattern::ThreadRun;
using pattern::Weave;

namespace {

// The register's codes, as a shade card's places.
enum Shade : int { K = 0, B = 1, G = 2 };

/** BLACK WATCH (42nd Regiment), the whole repeat unit by unit as a
 *  weaving register prints it: A + B + C + B, 252 ends. */
const std::vector<ThreadRun>& blackWatch() {
  static const std::vector<ThreadRun> runs{
      {18, K}, {6, B},  {2, K},  {6, B},  {2, K}, {18, B}, {2, K},
      {6, B},  {2, K},  {6, B},  {18, K},                            // A, 86
      {18, G}, {6, K},  {18, G},                                     // B, 42
      {18, K}, {18, B}, {2, K},  {6, B},  {2, K}, {18, B}, {18, K},  // C, 82
      {18, G}, {6, K},  {18, G}};                                    // B, 42
  return runs;
}

/** A run of threads back into the runs that spell it. */
std::vector<ThreadRun> runsOf(const std::vector<uint8_t>& threads) {
  std::vector<ThreadRun> runs;
  for (uint8_t shade : threads)
    if (!runs.empty() && runs.back().shade == shade)
      ++runs.back().threads;
    else
      runs.push_back({1, shade});
  return runs;
}

/** The cloth read back out of a baked window, one pixel per thread. */
SkBitmap raster(const Cloth& cloth, SkIPoint origin, SkISize size) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(size.width(), size.height()));
  const sk_sp<SkImage> woven = pattern::clothImage(cloth, origin, size);
  if (woven) woven->readPixels(nullptr, bitmap.pixmap(), 0, 0);
  return bitmap;
}

SkColor toSkColor(Color c) { return SkColor4f{c.r, c.g, c.b, c.a}.toSkColor(); }

Cloth gingham() {
  const std::vector<uint8_t> count = pattern::threadcount({{8, 0}, {8, 1}});
  return {.warp = count,
          .weft = count,
          .shades = {rgb(0xFFFFFF), rgb(0xCC2222)},
          .weave = Weave::plain()};
}

}  // namespace

TEST(Weave, TheBlackWatchSettCountsTwoHundredAndFiftyTwoEnds) {
  const std::vector<uint8_t> count = pattern::threadcount(blackWatch());
  EXPECT_EQ(count.size(), 252u);
  // The three units the register prints, in the order it prints them.
  EXPECT_EQ(count[0], K);
  EXPECT_EQ(count[18], B);
  EXPECT_EQ(count[86], G);   // unit B opens on green
  EXPECT_EQ(count[128], K);  // unit C opens on black
  EXPECT_EQ(count[210], G);  // unit B again
  // Blue is exactly a third of the cloth — the register's own ratio.
  int blue = 0;
  for (uint8_t shade : count) blue += shade == B;
  EXPECT_EQ(blue * 3, 252);
}

TEST(Weave, TheBlackWatchSettPivotsTwiceHalfARepeatApart) {
  const std::vector<int> found =
      pattern::pivots(pattern::threadcount(blackWatch()));
  ASSERT_EQ(found.size(), 2u);
  // The middle of the wide blue of unit A, and the middle of the narrow
  // blue of unit C: the two lines the design reflects about.
  EXPECT_EQ(found[0], 43);
  EXPECT_EQ(found[1], 169);
  EXPECT_EQ(found[1] - found[0], 126);
}

TEST(Weave, AReflectiveSettIsTheHalfBetweenItsPivots) {
  const std::vector<uint8_t> whole = pattern::threadcount(blackWatch());
  const std::vector<int> pivot = pattern::pivots(whole);
  ASSERT_EQ(pivot.size(), 2u);
  // The half sett the register would print: from one pivot up to the
  // other, the pivots being the gaps at its two ends.
  std::vector<uint8_t> half;
  for (int i = pivot[0]; i < pivot[1]; ++i) half.push_back(whole[(size_t)i]);
  EXPECT_EQ(half.size(), 126u);

  const std::vector<uint8_t> mirrored =
      pattern::threadcount(runsOf(half), pattern::Symmetry::Reflective);
  ASSERT_EQ(mirrored.size(), whole.size());
  // The same cloth, read from the first pivot instead of from the
  // register's own starting thread.
  for (size_t i = 0; i < mirrored.size(); ++i)
    ASSERT_EQ(mirrored[i], whole[(i + (size_t)pivot[0]) % whole.size()])
        << "thread " << i;
  // And the mirroring is what put the pivots where they now are.
  EXPECT_EQ(pattern::pivots(mirrored), (std::vector<int>{0, 126}));
}

TEST(Weave, AnAsymmetricSettHasNoPivot) {
  EXPECT_TRUE(
      pattern::pivots(pattern::threadcount({{3, 0}, {1, 1}, {2, 2}})).empty());
}

TEST(Weave, APlainWeaveAlternatesEveryThread) {
  const Weave plain = Weave::plain();
  for (int y = -2; y < 6; ++y)
    for (int x = -2; x < 6; ++x) {
      EXPECT_EQ(pattern::warpUp(plain, x, y), ((x - y) % 2 + 2) % 2 == 0)
          << x << "," << y;
      // No float anywhere: every neighbour along either axis is the
      // other face.
      EXPECT_NE(pattern::warpUp(plain, x, y), pattern::warpUp(plain, x + 1, y));
      EXPECT_NE(pattern::warpUp(plain, x, y), pattern::warpUp(plain, x, y + 1));
    }
}

TEST(Weave, ATwillFloatsItsRunAndCarriesItOnTheDiagonal) {
  const Weave twill = Weave::twill(2, 2);
  // The interlacing is unchanged one thread down the "\" diagonal, which
  // is the rib the cloth shows.
  for (int y = -3; y < 8; ++y)
    for (int x = -3; x < 8; ++x)
      EXPECT_EQ(pattern::warpUp(twill, x, y),
                pattern::warpUp(twill, x + 1, y + 1))
          << x << "," << y;
  // Two over, two under, and nothing floats longer than two either way.
  int warpFloat = 0, weftFloat = 0, run = 0;
  for (int x = 0; x < 32; ++x) {
    run = pattern::warpUp(twill, x, 0) ? run + 1 : 0;
    warpFloat = std::max(warpFloat, run);
  }
  run = 0;
  for (int y = 0; y < 32; ++y) {
    run = pattern::warpUp(twill, 0, y) ? 0 : run + 1;
    weftFloat = std::max(weftFloat, run);
  }
  EXPECT_EQ(warpFloat, 2);
  EXPECT_EQ(weftFloat, 2);
  // A warp-faced twill floats three and its reverse floats one.
  EXPECT_TRUE(pattern::warpUp(Weave::twill(3, 1), 2, 0));
  EXPECT_FALSE(pattern::warpUp(Weave::twill(1, 3), 2, 0));
  // A negative step lays the rib on the other diagonal.
  const Weave mirrored = Weave::twill(2, 2, -1);
  EXPECT_EQ(pattern::warpUp(mirrored, 1, -1), pattern::warpUp(mirrored, 0, 0));
  EXPECT_NE(pattern::warpUp(mirrored, 1, 1), pattern::warpUp(mirrored, 0, 0));
}

TEST(Weave, TheRepeatComesRoundWithTheWeaveAndNotOnlyTheSett) {
  const std::vector<uint8_t> count = pattern::threadcount(blackWatch());
  const Cloth tartan{.warp = count, .weft = count};
  // 252 is a multiple of the twill's four, so the cloth repeats on the
  // sett itself.
  EXPECT_EQ(pattern::clothRepeat(tartan), SkISize::Make(252, 252));
  // A six-thread sett under the same twill does not: it tiles at twelve.
  const std::vector<uint8_t> six = pattern::threadcount({{3, 0}, {3, 1}});
  EXPECT_EQ(pattern::clothRepeat({.warp = six, .weft = six}),
            SkISize::Make(12, 12));
  // Under a plain weave the same six threads tile at six.
  EXPECT_EQ(
      pattern::clothRepeat({.warp = six, .weft = six, .weave = Weave::plain()}),
      SkISize::Make(6, 6));
}

TEST(Weave, TheRibDarkensTheWeftFloatsAndNothingElse) {
  const Color white = rgb(0xFFFFFF);
  const Cloth flat{.warp = {0}, .weft = {0}, .shades = {white}, .rib = 0.25f};
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x) {
      const Color c = flat.at(x, y);
      if (pattern::warpUp(flat.weave, x, y))
        EXPECT_FLOAT_EQ(c.r, 1.0f);
      else
        EXPECT_FLOAT_EQ(c.r, 0.75f);
      EXPECT_FLOAT_EQ(c.a, 1.0f);
    }
}

TEST(Weave, OnePixelPerThreadReadsWhatTheClothReads) {
  const std::vector<uint8_t> count =
      pattern::threadcount({{5, 0}, {4, 1}, {3, 2}});
  const Cloth cloth{.warp = count,
                    .weft = count,
                    .shades = {rgb(0x101010), rgb(0x2C2C80), rgb(0x006818)},
                    .rib = 0.22f};
  // A window taken at a negative origin, so the wrap is exercised too.
  const SkIPoint origin{-7, -3};
  const SkISize size = SkISize::Make(23, 19);
  const SkBitmap baked = raster(cloth, origin, size);
  ASSERT_FALSE(baked.drawsNothing());
  for (int y = 0; y < size.height(); ++y)
    for (int x = 0; x < size.width(); ++x)
      ASSERT_EQ(baked.getColor(x, y),
                toSkColor(cloth.at(origin.x() + x, origin.y() + y)))
          << x << "," << y;
}

TEST(Weave, TheTileBakesTheWholeRepeatOfTheSameCloth) {
  const std::vector<uint8_t> count = pattern::threadcount({{3, 0}, {3, 1}});
  const Cloth cloth{
      .warp = count, .weft = count, .shades = {rgb(0x000000), rgb(0xFFFFFF)}};
  pattern::Tile tile = pattern::clothTile(cloth);
  EXPECT_EQ(tile.size().width(), 12.0f);
  EXPECT_EQ(tile.size().height(), 12.0f);
  EXPECT_EQ(tile.filter(), SkFilterMode::kNearest);
  const sk_sp<SkImage> baked = tile.image();
  ASSERT_TRUE(baked);
  SkBitmap read;
  read.allocPixels(SkImageInfo::MakeN32Premul(12, 12));
  ASSERT_TRUE(baked->readPixels(nullptr, read.pixmap(), 0, 0));
  for (int y = 0; y < 12; ++y)
    for (int x = 0; x < 12; ++x)
      ASSERT_EQ(read.getColor(x, y), toSkColor(cloth.at(x, y)))
          << x << "," << y;
}

TEST(Weave, GinghamIsTheSameGeneratorAtATwoColourSett) {
  const Cloth cloth = gingham();
  EXPECT_EQ(pattern::clothRepeat(cloth), SkISize::Make(16, 16));
  // Two dyes and no third: gingham's middle tone is not a colour the
  // cloth holds, it is the two threads alternating faster than an eye
  // separates them, which is what the mixed quarter below asserts.
  std::set<SkColor> tones;
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x) tones.insert(toSkColor(cloth.at(x, y)));
  EXPECT_EQ(tones.size(), 2u);
  // The solid quarters are solid.
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x)
      ASSERT_EQ(toSkColor(cloth.at(x, y)), toSkColor(cloth.shades[0]));
  for (int y = 8; y < 16; ++y)
    for (int x = 8; x < 16; ++x)
      ASSERT_EQ(toSkColor(cloth.at(x, y)), toSkColor(cloth.shades[1]));
  // The mixed quarter is exactly half of each, thread by thread.
  int light = 0, dark = 0;
  for (int y = 8; y < 16; ++y)
    for (int x = 0; x < 8; ++x) {
      if (toSkColor(cloth.at(x, y)) == toSkColor(cloth.shades[0]))
        ++light;
      else
        ++dark;
    }
  EXPECT_EQ(light, 32);
  EXPECT_EQ(dark, 32);
}

TEST(Weave, HoundstoothIsThatSettUnderTheTwillInstead) {
  const std::vector<uint8_t> count = pattern::threadcount({{4, 0}, {4, 1}});
  const Cloth plain{.warp = count,
                    .weft = count,
                    .shades = {rgb(0x000000), rgb(0xFFFFFF)},
                    .weave = Weave::plain()};
  Cloth tooth = plain;
  tooth.weave = Weave::twill(2, 2);
  EXPECT_EQ(pattern::clothRepeat(tooth), SkISize::Make(8, 8));
  // Where warp and weft carry the same shade the weave cannot be seen:
  // both quarters are solid under either interlacing.
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x) {
      ASSERT_EQ(toSkColor(tooth.at(x, y)), toSkColor(tooth.shades[0]));
      ASSERT_EQ(toSkColor(tooth.at(x + 4, y + 4)), toSkColor(tooth.shades[1]));
    }
  // Where they differ the weave IS the pattern, and that is the whole
  // difference between a check and a tooth: the plain weave alternates
  // thread by thread, the twill floats two and steps them along, which
  // is the point that runs out of the corner of the block.
  const SkColor dark = toSkColor(tooth.shades[0]);
  EXPECT_EQ(toSkColor(tooth.at(0, 4)), dark);
  EXPECT_EQ(toSkColor(tooth.at(1, 4)), dark);
  EXPECT_NE(toSkColor(tooth.at(2, 4)), dark);
  EXPECT_NE(toSkColor(tooth.at(3, 4)), dark);
  EXPECT_EQ(toSkColor(plain.at(0, 4)), dark);
  EXPECT_NE(toSkColor(plain.at(1, 4)), dark);
  EXPECT_EQ(toSkColor(plain.at(2, 4)), dark);
}

TEST(Weave, AClothWithNoThreadsPaintsNothing) {
  const Cloth empty;
  EXPECT_TRUE(pattern::clothRepeat(empty).isEmpty());
  EXPECT_EQ(pattern::clothImage(empty, {0, 0}, SkISize::Make(4, 4)), nullptr);
  EXPECT_EQ(empty.at(0, 0).a, 0.0f);
  // A thread naming a shade the card does not carry is transparent, not
  // a read past the end.
  const Cloth unnamed{.warp = {3}, .weft = {3}};
  EXPECT_EQ(unnamed.at(0, 0).a, 0.0f);
}
