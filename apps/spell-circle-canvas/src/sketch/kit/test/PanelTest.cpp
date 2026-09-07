/** @file
 * The sheet's ground, and the chrome a device's screen is inset into.
 */

#include <gtest/gtest.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Paint.h>
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

/** A shell whose only rule runs round its OUTER edge asks the frame for
 *  none, and gets none — where a `Fill` with no kind would otherwise paint
 *  the stroke's own default. */
TEST(SketchKitPanel, AKeylineOfNoneDrawsNoKeyline) {
  const Fill shell = Fill::color({0.22f, 0.20f, 0.17f, 1});
  const Fill screen = Fill::color({0.86f, 0.84f, 0.78f, 1});
  EXPECT_TRUE(sameDrawing(
      compose::box()
          .width(compose::Dim(220))
          .height(compose::Dim(180))
          .padding(20)
          .fill(shell)
          .child(compose::box().column().grow(1).fill(screen).clip()),
      kit::frame({.width = compose::Dim(220),
                  .height = compose::Dim(180),
                  .shell = shell,
                  .corners = 0,
                  .bezel = 20,
                  .screen = screen,
                  .screenCorners = 0,
                  .keyline = Fill::none()})));
}

// What stands behind and around

TEST(SketchKitPanel, TheBackdropIsTheThemesGround) {
  SkBitmap flat = Drawn(kit::backdrop({.over = {kWide, kTall}})).pixels();
  const SkColor4f ground = kit::houseTheme().palette.ground;
  const SkColor4f drawn = flat.getColor4f(4, 4);
  EXPECT_NEAR(drawn.fR, ground.fR, 0.01f);
  EXPECT_NEAR(drawn.fG, ground.fG, 0.01f);
  EXPECT_NEAR(drawn.fB, ground.fB, 0.01f);
}

/** A vignette darkens the corners and leaves the middle alone, which is
 *  what makes it a vignette rather than a wash. */
TEST(SketchKitPanel, AVignetteDarkensTheCornersAndNotTheMiddle) {
  kit::Theme paper = kit::houseTheme();
  paper.palette.ground = {0.6f, 0.6f, 0.6f, 1};
  const kit::Provide bound(paper);
  SkBitmap shaded =
      Drawn(kit::backdrop({.over = {kWide, kTall}, .vignette = 0.9f})).pixels();
  const SkColor4f corner = shaded.getColor4f(1, 1);
  const SkColor4f middle = shaded.getColor4f(kWide / 2, kTall / 2);
  EXPECT_LT(corner.fR, middle.fR);
  EXPECT_NEAR(middle.fR, paper.palette.ground.fR, 0.02f);
}

/** A grain moves pixels that a flat ground leaves identical. */
TEST(SketchKitPanel, AGrainIsNotAFlatGround) {
  SkBitmap grained =
      Drawn(kit::backdrop({.over = {kWide, kTall}, .grain = 0.5f})).pixels();
  bool moved = false;
  const uint32_t first = *grained.getAddr32(0, 8);
  for (int x = 1; x < kWide && !moved; ++x)
    moved = *grained.getAddr32(x, 8) != first;
  EXPECT_TRUE(moved);
}

/** The screen is inset into the shell by the bezel on every side — the
 *  arithmetic every reconstruction did in four places by hand. */
TEST(SketchKitPanel, TheScreenIsInsetByTheBezel) {
  const kit::Theme& house = kit::houseTheme();
  Element byHand =
      compose::box()
          .column()
          .padding(8)
          .width(compose::Dim(200))
          .height(compose::Dim(120))
          .fill(Fill::color(house.palette.cellGround))
          .corners(compose::Corners{6})
          .child(compose::box()
                     .column()
                     .grow(1)
                     .fill(Fill::color(house.palette.ground))
                     .clip()
                     .corners(compose::Corners{2})
                     .stroke(compose::stroke(1, Fill::color(house.palette.rule),
                                             compose::PathFormat::Align::Inner))
                     .child(subject()));
  EXPECT_TRUE(sameDrawing(
      std::move(byHand),
      kit::frame(
          {.width = compose::Dim(200), .height = compose::Dim(120), .bezel = 8},
          subject())));
}

/** A shell quarried rather than coloured: the frame's two grounds each
 *  take a material, and the chrome is what the same materials on the two
 *  hand-spelled nodes draw. */
TEST(SketchKitPanel, AFrameShellAndScreenTakeAMaterial) {
  const sigil::material::Material purbeck = sigil::material::kit::stone(
      {.hi = {0.47f, 0.46f, 0.42f, 1}, .lo = {0.31f, 0.31f, 0.28f, 1}});
  const sigil::material::Material mortar =
      sigil::material::kit::stone({.hi = {0.42f, 0.41f, 0.37f, 1},
                                   .lo = {0.28f, 0.27f, 0.25f, 1},
                                   .bedAngle = 60.0f});
  const kit::Theme& house = kit::houseTheme();
  Element byHand =
      compose::box()
          .column()
          .padding(8)
          .width(compose::Dim(200))
          .height(compose::Dim(120))
          .fill(sigil::material::skia::Paint::recipe(purbeck))
          .corners(compose::Corners{6})
          .child(compose::box()
                     .column()
                     .grow(1)
                     .fill(sigil::material::skia::Paint::recipe(mortar))
                     .clip()
                     .corners(compose::Corners{2})
                     .stroke(compose::stroke(1, Fill::color(house.palette.rule),
                                             compose::PathFormat::Align::Inner))
                     .child(subject()));
  EXPECT_TRUE(
      sameDrawing(std::move(byHand), kit::frame({.width = compose::Dim(200),
                                                 .height = compose::Dim(120),
                                                 .shell = purbeck,
                                                 .bezel = 8,
                                                 .screen = mortar},
                                                subject())));
}

}  // namespace
