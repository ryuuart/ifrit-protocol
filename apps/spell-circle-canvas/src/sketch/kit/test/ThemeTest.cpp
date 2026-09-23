/** @file
 * The theme as a value: the house sheet, the registers it sets a line in,
 * and the scope that binds one.
 */

#include <gtest/gtest.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include "Drawn.h"

namespace {

/** The rule of @p sheet whose selector reads as @p cssText, or null. */
const sigil::compose::Rule* ruleFor(const sigil::compose::StyleSheet& sheet,
                                    std::string_view cssText) {
  const sigil::compose::ElementSelector wanted =
      sigil::compose::selector(cssText);
  for (const sigil::compose::Rule& rule : sheet.rules())
    if (rule.selector() == wanted) return &rule;
  return nullptr;
}

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
  EXPECT_EQ(house.palette.ground,
            (sigil::material::Color{0.07f, 0.07f, 0.085f, 1}));
  EXPECT_EQ(house.palette.cellGround,
            (sigil::material::Color{0.105f, 0.11f, 0.125f, 1}));
  EXPECT_EQ(house.palette.ink,
            (sigil::material::Color{0.90f, 0.90f, 0.92f, 1}));
  EXPECT_EQ(house.palette.ash,
            (sigil::material::Color{0.55f, 0.56f, 0.62f, 1}));
  EXPECT_EQ(house.palette.rule,
            (sigil::material::Color{0.20f, 0.21f, 0.25f, 1}));
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
  one.palette.ink.r += 0.001f;
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
      EXPECT_EQ(kit::theme().palette.ground,
                (sigil::material::Color{0, 0, 0, 1}));
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

TEST(SketchKitTheme, ARegisterInTheHouseSansStatesTheDefaultFamily) {
  // The house theme leaves its sans as the font context's default family,
  // and a register set in it SAYS so: under an ancestor that named a face,
  // a caption is still set in the register's own family.
  const kit::Theme& house = kit::houseTheme();
  const sigil::weave::Type note = house.font(house.type.captionNote);
  ASSERT_TRUE(note.face.has_value());
  EXPECT_EQ(*note.face, nullptr);
  sigil::weave::Type serif = sigil::weave::initialType();
  serif.face = SkTypeface::MakeEmpty();
  const sigil::weave::Type under = sigil::weave::overlay(serif, note);
  ASSERT_TRUE(under.face.has_value());
  EXPECT_EQ(*under.face, nullptr) << "not the serif above it";
}

TEST(SketchKitTheme, TheRegistersStyleDocumentRoles) {
  const kit::Theme& house = kit::houseTheme();
  {
    const compose::StyleSheet sheet = house.styleSheet();
    // The seven registers, `readout`, and the eight a chart's parts are
    // dressed in.
    EXPECT_EQ(sheet.size(), 16u);
    ASSERT_NE(ruleFor(sheet, "eyebrow"), nullptr);
    EXPECT_EQ(ruleFor(sheet, "eyebrow")->type(),
              house.font(house.type.eyebrow))
        << "a register a sheet sets inside its content names no colour";
    EXPECT_EQ(ruleFor(sheet, "label, .label")->type(),
              house.font(house.type.captionLabel, house.palette.ink))
        << "a rule carries its whole look, colour included";
    EXPECT_EQ(ruleFor(sheet, "caption, .caption")->type(),
              house.font(house.type.captionNote, house.palette.ash));
    EXPECT_EQ(ruleFor(sheet, "h1")->type(),
              house.font(house.type.title, house.palette.ink));
    EXPECT_EQ(ruleFor(sheet, "lead")->type(),
              house.font(house.type.subtitle, house.palette.ash));
    EXPECT_EQ(ruleFor(sheet, "footer")->type(),
              house.font(house.type.footer, house.palette.ash));
    // The one class that is not a register of its own: a MEASURED FIGURE,
    // set in the register a call is set in, in the figure colour.
    ASSERT_NE(ruleFor(sheet, ".readout"), nullptr);
    EXPECT_EQ(ruleFor(sheet, ".readout")->type(),
              house.font(house.type.captionLabel, house.palette.figure));

    compose::StyleSheet own =
        house.styleSheet() +
        compose::StyleSheet{compose::rule(".value").font({.size = 13.0f})};
    EXPECT_EQ(own.size(), 17u) << "the registers and the sketch's own";
    EXPECT_NE(ruleFor(own, ".value"), nullptr);
    EXPECT_EQ(house.styleSheet().size(), 16u) << "a copy, not the theme's";
  }
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
