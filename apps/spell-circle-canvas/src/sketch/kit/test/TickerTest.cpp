/** @file
 * Things along an axis: the crawl past a window, and the rail a scale is
 * marked off on.
 */

#include <gtest/gtest.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/kit/Marquee.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <optional>

#include "Drawn.h"

namespace {

namespace kit = sigil::sketch::kit;
namespace compose = sigil::compose;
using compose::Element;
using compose::Fill;
using sigil::sketch::kit::test::Drawn;
using sigil::sketch::kit::test::kTall;
using sigil::sketch::kit::test::kWide;
using sigil::sketch::kit::test::sameDrawing;
using sigil::sketch::kit::test::samePixels;
using sigil::sketch::kit::test::subject;
using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;

// Along an axis

TEST(SketchKitTicker, TheCrawlIsTheHandSpelledMarquee) {
  const kit::Theme& house = kit::houseTheme();
  Element strip = subject();
  EXPECT_TRUE(sameDrawing(
      compose::kit::marquee(
          strip,
          {.phase = 0.0f, .gap = house.spacing.labelGap, .contentWidth = 60.0f})
          .width(200),
      kit::ticker({.content = strip,
                   .contentWidth = 60,
                   .phase = 0.0f,
                   .width = compose::Dimension(200)})));
}

/** A minor mark draws a shorter tick and no word, so a scale reads its
 *  major divisions before its subdivisions. */
TEST(SketchKitTicker, AMinorMarkIsShorterAndUnnamed) {
  EXPECT_FALSE(sameDrawing(
      kit::timeline({.marks = {{0, u8"0 ms"}, {0.5f, u8"half", true}},
                     .width = compose::Dimension(300)}),
      kit::timeline({.marks = {{0, u8"0 ms"}, {0.5f, u8"half", false}},
                     .width = compose::Dimension(300)})));
}

/** THE INK IS THE MARK'S, NOT THE TICK'S: a scale given a colour of its
 *  own sets its words in it as well as its ticks, and a mark that states
 *  its own colour keeps its word out of it. */
TEST(SketchKitTicker, TheInkColoursTheWordsAsWellAsTheTicks) {
  const kit::Theme& house = kit::houseTheme();
  kit::Timeline scale{.marks = {{0.5f, u8"half"}},
                      .width = compose::Dimension(300)};

  // Below the rail and past a tick's reach, the only thing drawn is the
  // word, so an amber pixel in those rows is a word set in amber.
  const int wordRows =
      (int)house.spacing.barHeight + (int)house.spacing.tickReach + 1;
  auto amberIn = [](const SkBitmap& shot, int fromRow) {
    for (int y = fromRow; y < kTall; ++y)
      for (int x = 0; x < kWide; ++x) {
        const sigil::material::Color pixel = shot.getColor4f(x, y);
        if (pixel.r > 0.5f && pixel.r > pixel.b * 2) return true;
      }
    return false;
  };

  EXPECT_FALSE(amberIn(Drawn(kit::timeline(scale)).pixels(), wordRows));
  scale.ink = Fill::color({0.95f, 0.62f, 0.15f, 1});
  EXPECT_TRUE(amberIn(Drawn(kit::timeline(scale)).pixels(), wordRows));

  scale.marks[0].ink = Fill::color(house.palette.ash);
  const SkBitmap quieted = Drawn(kit::timeline(scale)).pixels();
  EXPECT_FALSE(amberIn(quieted, wordRows));
  // The ticks are still the scale's, which is what makes the word's own
  // ink an exception rather than a second timeline ink.
  EXPECT_TRUE(amberIn(quieted, 0));
}

/** A SCALE UNDER NO SHEET IS THE STOCK SHEET'S: its rail's thickness, its
 *  ticks' reach and its colours fall back to the stock values, so it
 *  draws as it does under the stock sheet rather than with ticks of no
 *  reach on a rail of no thickness. */
TEST(SketchKitTicker, AScaleUnderNoSheetIsTheStockSheets) {
  const auto scale = [] {
    return kit::timeline({.marks = {{0, u8"0 ms"},
                                    {0.25f, {}, false},
                                    {0.5f, u8"half"},
                                    {1, u8"640 ms"}},
                          .width = compose::Dimension(300)});
  };
  const compose::StyleSheet stock = kit::houseTheme().properties();
  EXPECT_TRUE(samePixels(Drawn(scale(), compose::StyleSheet{}).pixels(),
                         Drawn(scale(), stock).pixels()));
  // …and a sheet stating another reach still moves it.
  kit::Theme longer = kit::houseTheme();
  longer.spacing.tickReach = 14;
  EXPECT_FALSE(samePixels(Drawn(scale(), longer.properties()).pixels(),
                          Drawn(scale(), stock).pixels()));
}

/** AN UNSET CRAWL GAP IS THE SHEET'S `spacing-ticker-gap`, read where the
 *  strip lays out. */
TEST(SketchKitTicker, AnUnsetGapIsTheSheets) {
  kit::Theme wider = kit::houseTheme();
  wider.spacing.tickerGap = 36;
  const auto strip = [](std::optional<float> gap) {
    return kit::ticker({.content = subject(),
                        .contentWidth = 60,
                        .phase = 0.0f,
                        .gap = gap,
                        .width = compose::Dimension(200)});
  };
  EXPECT_TRUE(sameDrawing(
      sigil::sketch::kit::test::withProperties(strip(36.0f), wider),
      sigil::sketch::kit::test::withProperties(strip(std::nullopt), wider)));
  EXPECT_FALSE(sameDrawing(
      strip(std::nullopt),
      sigil::sketch::kit::test::withProperties(strip(std::nullopt), wider)));
}

/** AN UNSET-GAP CRAWL LOOPS WITHOUT A SEAM under a sheet stating another
 *  `spacing-ticker-gap`: its driver runs the phase on without knowing the
 *  sheet's gap, and the crawl folds it over the content and that gap, so a
 *  phase laps on draws what its place within the first lap draws. */
TEST(SketchKitTicker, AnUnsetGapCrawlLoopsWithoutASeamUnderAnySheet) {
  kit::Theme wider = kit::houseTheme();
  wider.spacing.tickerGap = 36;
  const auto strip = [&](float phase) {
    return sigil::sketch::kit::test::withProperties(
        kit::ticker({.content = subject(),
                     .contentWidth = 60,
                     .phase = phase,
                     .width = compose::Dimension(200)}),
        wider);
  };
  // One loop is the strip's 60 and the sheet's 36.
  EXPECT_TRUE(sameDrawing(strip(-20.0f), strip(-20.0f - 96.0f * 4)));
  EXPECT_TRUE(sameDrawing(strip(-20.0f), strip(-20.0f + 96.0f)));
  EXPECT_FALSE(sameDrawing(strip(-20.0f), strip(-30.0f)));
}

}  // namespace
