/** @file
 * A window's share of what it scrolls: the thumb's arithmetic, and the bar
 * it stands in.
 */

#include <gtest/gtest.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilsketch/canvas/Sketch.h>
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

/** A stepper of the kind a bar puts at each end — the caller's chrome,
 *  which is all a stepper ever is. */
Element stepper() {
  return compose::box()
      .width(compose::Dimension(16))
      .height(compose::Dimension(16))
      .shrink(0)
      .fill(Fill::color({0.4f, 0.4f, 0.5f, 1}));
}

/** The bar the component has to draw: two steppers, a track that takes
 *  what is left, and the thumb standing absolutely in it. */
Element barByHand(float top, float length) {
  const kit::Theme& house = kit::houseTheme();
  return compose::box()
      .column()
      .width(compose::Dimension(16))
      .height(compose::Dimension(232))
      .child(stepper())
      .child(compose::box()
                 .grow(1)
                 .fill(Fill::color(house.palette.cellGround))
                 .child(compose::box()
                            .absolute()
                            .left(compose::Dimension(0))
                            .right(compose::Dimension(0))
                            .top(compose::Dimension(top))
                            .height(compose::Dimension(length))
                            .fill(Fill::color(house.palette.figure))))
      .child(stepper());
}

Element bar(float at) {
  return kit::scrollbar(
             {.leading = stepper(),
              .trailing = stepper(),
              .scrolled = {.view = 100, .content = 400, .track = 200},
              .at = at})
      .width(compose::Dimension(16))
      .height(compose::Dimension(232));
}

TEST(SketchKitScrollbar, TheBarIsTheHandSpelledStack) {
  EXPECT_TRUE(sameDrawing(barByHand(0, 50), bar(0)));
}

/** The reading itself: a window over a quarter of what it scrolls asks
 *  for a thumb a quarter of the track long, and the rest of the track is
 *  how far it goes. */
TEST(SketchKitScrollbar, TheThumbIsTheWindowsShareOfWhatItScrolls) {
  const kit::Thumb quarter =
      kit::Scrolled{.view = 100, .content = 400, .track = 200}.thumb();
  EXPECT_FLOAT_EQ(quarter.length, 50);
  EXPECT_FLOAT_EQ(quarter.travel, 150);
  EXPECT_TRUE(sameDrawing(barByHand(150, 50), bar(1)));
  EXPECT_TRUE(sameDrawing(barByHand(75, 50), bar(0.5f)));
}

/** A window over everything has nothing to scroll, so its thumb is the
 *  whole track and goes nowhere — which a division by the share would
 *  answer too, and a content of nothing would not. */
TEST(SketchKitScrollbar, EverythingShowingFillsTheTrack) {
  EXPECT_EQ((kit::Scrolled{.view = 200, .content = 200, .track = 200}.thumb()),
            (kit::Thumb{200, 0}));
  EXPECT_EQ((kit::Scrolled{.view = 200, .content = 0, .track = 200}.thumb()),
            (kit::Thumb{200, 0}));
  EXPECT_EQ((kit::Scrolled{.view = 0, .content = 0, .track = 0}.thumb()),
            (kit::Thumb{0, 0}));
}

/** A minimum keeps a long document's thumb visible, and it takes the
 *  travel with it: a thumb held longer than its share has less track left
 *  to run along. */
TEST(SketchKitScrollbar, AShortThumbIsHeldAtItsMinimum) {
  const kit::Thumb held =
      kit::Scrolled{.view = 10, .content = 4000, .track = 200, .minLength = 24}
          .thumb();
  EXPECT_FLOAT_EQ(held.length, 24);
  EXPECT_FLOAT_EQ(held.travel, 176);
}

/** A measured thumb is stated rather than read off a ratio — which is
 *  what a reconstruction of a bar someone else drew has. */
TEST(SketchKitScrollbar, AStatedLengthReplacesTheShare) {
  EXPECT_TRUE(sameDrawing(
      barByHand(0, 90), kit::scrollbar({.leading = stepper(),
                                        .trailing = stepper(),
                                        .thumbLength = compose::Dimension(90)})
                            .width(compose::Dimension(16))
                            .height(compose::Dimension(232))));
}

}  // namespace
