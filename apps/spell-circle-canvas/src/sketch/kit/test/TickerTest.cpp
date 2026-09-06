/** @file
 * Things along an axis: the crawl past a window, and the rail a scale is
 * marked off on.
 */

#include <gtest/gtest.h>
#include <sigilcompose/kit/Marquee.h>
#include <sigilsketch/kit/Kit.h>

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
          .width(compose::Dim(200)),
      kit::ticker({.content = strip,
                   .contentWidth = 60,
                   .phase = 0.0f,
                   .width = compose::Dim(200)})));
}

/** A minor mark draws a shorter tick and no word, so a scale reads its
 *  major divisions before its subdivisions. */
TEST(SketchKitTicker, AMinorMarkIsShorterAndUnnamed) {
  EXPECT_FALSE(sameDrawing(
      kit::timeline({.marks = {{0, u8"0 ms"}, {0.5f, u8"half", true}},
                     .width = compose::Dim(300)}),
      kit::timeline({.marks = {{0, u8"0 ms"}, {0.5f, u8"half", false}},
                     .width = compose::Dim(300)})));
}

/** THE INK IS THE MARK'S, NOT THE TICK'S: a scale given a colour of its
 *  own sets its words in it as well as its ticks, and a mark that states
 *  its own colour keeps its word out of it. */
TEST(SketchKitTicker, TheInkColoursTheWordsAsWellAsTheTicks) {
  const kit::Theme& house = kit::houseTheme();
  kit::Timeline scale{.marks = {{0.5f, u8"half"}}, .width = compose::Dim(300)};

  // Below the rail and past a tick's reach, the only thing drawn is the
  // word, so an amber pixel in those rows is a word set in amber.
  const int wordRows =
      (int)house.spacing.barHeight + (int)house.spacing.tickReach + 1;
  auto amberIn = [](const SkBitmap& shot, int fromRow) {
    for (int y = fromRow; y < kTall; ++y)
      for (int x = 0; x < kWide; ++x) {
        const SkColor4f pixel = shot.getColor4f(x, y);
        if (pixel.fR > 0.5f && pixel.fR > pixel.fB * 2) return true;
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

}  // namespace
