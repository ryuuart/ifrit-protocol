/** @file
 * How much of something there is: the bar and the dial.
 */

#include <gtest/gtest.h>
#include <sigilsketch/kit/Kit.h>

#include <cmath>

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

/** A bound level SCALES the filled part rather than sizing it, which is
 *  what keeps the bed's recording valid while the bar moves. Full, the
 *  scale is the identity and the two spellings are one drawing; part
 *  way, the scaled edge is resolved by the transform rather than by
 *  layout, so the two are close and not equal. */
TEST(SketchKitMeter, ABoundLevelFillsTheRailAsAFractionDoes) {
  const SkColor4f figure = kit::houseTheme().palette.figure;
  const auto barAt = [&](float level, int x) {
    SkBitmap drawn =
        Drawn(kit::meter({.level = level, .width = compose::Dim(200)}))
            .pixels();
    const SkColor4f pixel = drawn.getColor4f(x, 2);
    return std::abs(pixel.fR - figure.fR) < 0.02f &&
           std::abs(pixel.fG - figure.fG) < 0.02f;
  };
  EXPECT_TRUE(barAt(1.0f, 190));
  EXPECT_FALSE(barAt(0.25f, 190));
  EXPECT_TRUE(barAt(0.25f, 20));
}

// A fraction drawn

TEST(SketchKitMeter, TheBarIsTheFractionOfTheTrack) {
  const kit::Theme& house = kit::houseTheme();
  Element byHand = compose::box()
                       .width(compose::Dim(220))
                       .height(compose::Dim(house.spacing.barHeight))
                       .fill(Fill::color(house.palette.cellGround))
                       .clip()
                       .child(compose::box()
                                  .width(compose::pct(40))
                                  .fill(Fill::color(house.palette.figure))
                                  .alignSelf(compose::Align::Stretch));
  EXPECT_TRUE(
      sameDrawing(std::move(byHand),
                  kit::meter({.fraction = 0.4f, .width = compose::Dim(220)})));
}

/** A fraction outside 0..1 is clamped: a bar past its own end is a
 *  drawing error rather than a reading. */
TEST(SketchKitMeter, AFractionOutsideTheTrackIsClamped) {
  EXPECT_TRUE(
      sameDrawing(kit::meter({.fraction = 3.0f, .width = compose::Dim(220)}),
                  kit::meter({.fraction = 1.0f, .width = compose::Dim(220)})));
  EXPECT_TRUE(
      sameDrawing(kit::meter({.fraction = -1.0f, .width = compose::Dim(220)}),
                  kit::meter({.fraction = 0.0f, .width = compose::Dim(220)})));
}

TEST(SketchKitMeter, TheDialSweepsWithItsFraction) {
  EXPECT_FALSE(sameDrawing(kit::gauge({.fraction = 0.25f}),
                           kit::gauge({.fraction = 0.75f})));
  EXPECT_TRUE(sameDrawing(kit::gauge({.fraction = 2.0f}),
                          kit::gauge({.fraction = 1.0f})));
}

}  // namespace
