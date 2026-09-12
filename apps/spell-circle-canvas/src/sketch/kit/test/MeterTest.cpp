/** @file
 * How much of something there is: the bar and the dial.
 */

#include <gtest/gtest.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilsketch/canvas/Sketch.h>
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
        Drawn(kit::meter({.level = level, .width = compose::Dimension(200)}))
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
                       .width(compose::Dimension(220))
                       .height(compose::Dimension(house.spacing.barHeight))
                       .fill(Fill::color(house.palette.cellGround))
                       .clip()
                       .child(compose::box()
                                  .width(compose::pct(40))
                                  .height(compose::pct(100))
                                  .fill(Fill::color(house.palette.figure))
                                  .alignSelf(compose::Align::Stretch));
  EXPECT_TRUE(sameDrawing(
      std::move(byHand),
      kit::meter({.fraction = 0.4f, .width = compose::Dimension(220)})));
}

/** THE FILL STATES ITS OWN HEIGHT. A rail is laid out in whichever
 *  direction the tree around it runs, and a fill that took its height
 *  from the cross-axis stretch would be a hairline wherever that axis is
 *  the horizontal one. */
TEST(SketchKitMeter, TheBarFillsItsRailInsideAColumn) {
  const SkColor4f figure = kit::houseTheme().palette.figure;
  SkBitmap drawn = Drawn(compose::box().column().child(
                             kit::meter({.fraction = 0.5f,
                                         .width = compose::Dimension(200),
                                         .height = compose::Dimension(20)})))
                       .pixels();
  const SkColor4f pixel = drawn.getColor4f(40, 10);
  EXPECT_NEAR(pixel.fR, figure.fR, 0.02f);
  EXPECT_NEAR(pixel.fG, figure.fG, 0.02f);
}

/** A KEYLINE AND AN INSET make the rail a bezelled gauge: the line is
 *  drawn inside the rail's own box and the fill is held off it. */
TEST(SketchKitMeter, ABezelHoldsTheFillOffTheFrame) {
  const SkColor4f figure = kit::houseTheme().palette.figure;
  SkBitmap drawn = Drawn(kit::meter({.fraction = 1.0f,
                                     .width = compose::Dimension(200),
                                     .height = compose::Dimension(24),
                                     .keyline = Fill::color(SkColors::kRed),
                                     .keylineWidth = 2.0f,
                                     .inset = 6.0f}))
                       .pixels();
  // Inside the inset the bar; on the edge the keyline, and neither is
  // the other.
  const SkColor4f inside = drawn.getColor4f(100, 12);
  EXPECT_NEAR(inside.fR, figure.fR, 0.02f);
  EXPECT_NEAR(inside.fG, figure.fG, 0.02f);
  const SkColor4f edge = drawn.getColor4f(100, 1);
  EXPECT_GT(edge.fR, 0.5f);
  EXPECT_LT(edge.fG, 0.3f);
}

/** A fraction outside 0..1 is clamped: a bar past its own end is a
 *  drawing error rather than a reading. */
TEST(SketchKitMeter, AFractionOutsideTheTrackIsClamped) {
  EXPECT_TRUE(sameDrawing(
      kit::meter({.fraction = 3.0f, .width = compose::Dimension(220)}),
      kit::meter({.fraction = 1.0f, .width = compose::Dimension(220)})));
  EXPECT_TRUE(sameDrawing(
      kit::meter({.fraction = -1.0f, .width = compose::Dimension(220)}),
      kit::meter({.fraction = 0.0f, .width = compose::Dimension(220)})));
}

TEST(SketchKitMeter, TheDialSweepsWithItsFraction) {
  EXPECT_FALSE(sameDrawing(kit::gauge({.fraction = 0.25f}),
                           kit::gauge({.fraction = 0.75f})));
  EXPECT_TRUE(sameDrawing(kit::gauge({.fraction = 2.0f}),
                          kit::gauge({.fraction = 1.0f})));
}

}  // namespace
