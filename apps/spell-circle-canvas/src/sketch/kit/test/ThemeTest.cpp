/** @file
 * The theme as a value: the house sheet, the registers it sets a line in,
 * and the scope that binds one.
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

// The theme as a value

TEST(SketchKitTheme, TheHouseSheetIsTheHouseColours) {
  const kit::Theme& house = kit::houseTheme();
  EXPECT_EQ(house.palette.ground, (SkColor4f{0.07f, 0.07f, 0.085f, 1}));
  EXPECT_EQ(house.palette.cellGround, (SkColor4f{0.105f, 0.11f, 0.125f, 1}));
  EXPECT_EQ(house.palette.ink, (SkColor4f{0.90f, 0.90f, 0.92f, 1}));
  EXPECT_EQ(house.palette.ash, (SkColor4f{0.55f, 0.56f, 0.62f, 1}));
  EXPECT_EQ(house.palette.rule, (SkColor4f{0.20f, 0.21f, 0.25f, 1}));
}

/** The mono face is resolved once. Two reads that answered two faces
 *  would compare unequal, and every memo under the theme would miss
 *  forever. */
TEST(SketchKitTheme, TheMonoFaceIsOneFace) {
  EXPECT_EQ(kit::houseTheme().type.mono.get(),
            kit::houseTheme().type.mono.get());
  EXPECT_NE(kit::houseTheme().type.mono.get(), nullptr);
}

/** What the reconciler's prune needs: equal exactly when everything
 *  described under them describes the same. */
TEST(SketchKitTheme, ComparesExactly) {
  kit::Theme one = kit::houseTheme();
  const kit::Theme two = kit::houseTheme();
  EXPECT_EQ(one, two);
  one.palette.ink.fR += 0.001f;
  EXPECT_NE(one, two);

  kit::Theme spaced = kit::houseTheme();
  spaced.spacing.marginX += 1;
  EXPECT_NE(spaced, two);

  kit::Theme quiet = kit::houseTheme();
  quiet.type.title.track += 0.1f;
  EXPECT_NE(quiet, two);
}

TEST(SketchKitTheme, NothingBoundIsTheHouseTheme) {
  EXPECT_EQ(kit::theme(), kit::houseTheme());
}

TEST(SketchKitTheme, AScopeBindsAndAnInnerScopeShadows) {
  kit::Theme paper = kit::houseTheme();
  paper.palette.ground = {0.945f, 0.937f, 0.918f, 1};
  {
    const kit::Provide bound(paper);
    EXPECT_EQ(kit::theme().palette.ground, paper.palette.ground);

    kit::Theme darker = paper;
    darker.palette.ground = {0, 0, 0, 1};
    {
      const kit::Provide inner(darker);
      EXPECT_EQ(kit::theme().palette.ground, (SkColor4f{0, 0, 0, 1}));
    }
    EXPECT_EQ(kit::theme().palette.ground, paper.palette.ground);
  }
  EXPECT_EQ(kit::theme(), kit::houseTheme());
}

TEST(SketchKitTheme, ARegisterAsAFontIsItsFaceSizeAndTrackAndNoColour) {
  const kit::Theme& house = kit::houseTheme();
  const sigil::weave::Type eyebrow = house.font(house.type.eyebrow);
  EXPECT_EQ(eyebrow.face, house.type.mono) << "a mono register, the mono face";
  ASSERT_TRUE(eyebrow.size.has_value());
  EXPECT_FLOAT_EQ(eyebrow.size->value, house.type.eyebrow.size);
  ASSERT_TRUE(eyebrow.track.has_value());
  EXPECT_FLOAT_EQ(eyebrow.track->value, house.type.eyebrow.track);
  EXPECT_FALSE(eyebrow.color.has_value()) << "the ink in force paints it";
  EXPECT_EQ(house.font(house.type.footer).face, house.type.sans);
}

TEST(SketchKitTheme, TheRegistersAreClassesUnderABoundTheme) {
  namespace environment = sigil::core::environment;
  EXPECT_EQ(environment::inherited<sigil::weave::StyleSheet>(), nullptr);
  const kit::Theme& house = kit::houseTheme();
  {
    const kit::Provide bound(house);
    const sigil::weave::StyleSheet* classes =
        environment::inherited<sigil::weave::StyleSheet>();
    ASSERT_NE(classes, nullptr);
    EXPECT_EQ(classes->size(), 7u);
    ASSERT_NE(classes->find("eyebrow"), nullptr);
    EXPECT_EQ(*classes->find("eyebrow"), house.font(house.type.eyebrow));
    EXPECT_EQ(*classes->find("captionLabel"),
              house.font(house.type.captionLabel));

    sigil::weave::StyleSheet own = house.styleSheet();
    own.set("value", {.size = 13.0f});
    {
      const kit::Provide inner(house, own);
      const sigil::weave::StyleSheet* bound =
          environment::inherited<sigil::weave::StyleSheet>();
      ASSERT_NE(bound, nullptr);
      EXPECT_EQ(bound->size(), 8u) << "the registers and the sketch's own";
      EXPECT_NE(bound->find("value"), nullptr);
    }
    EXPECT_EQ(environment::inherited<sigil::weave::StyleSheet>()->size(), 7u);
  }
  EXPECT_EQ(environment::inherited<sigil::weave::StyleSheet>(), nullptr);
}

/** The four registers a sheet is set in, spelled the long way at the call
 *  site and the theme's way here, must be the same style. */
TEST(SketchKitTheme, ARegisterIsTheStyleTheCallSiteWouldHaveWritten) {
  const kit::Theme& house = kit::houseTheme();
  const sigil::weave::TextStyle byHand = sigil::weave::textStyle(
      {.size = 14, .color = house.palette.ink, .track = 2.4f});
  const sigil::weave::TextStyle byTheme =
      house.style(house.type.title, house.palette.ink);
  EXPECT_EQ(byTheme.shaping.fontSize, byHand.shaping.fontSize);
  EXPECT_EQ(byTheme.shaping.letterSpacing, byHand.shaping.letterSpacing);
  EXPECT_EQ(byTheme.shaping.typeface.get(), byHand.shaping.typeface.get());
  EXPECT_EQ(byTheme.paint.foreground.getColor(),
            byHand.paint.foreground.getColor());
}

}  // namespace
