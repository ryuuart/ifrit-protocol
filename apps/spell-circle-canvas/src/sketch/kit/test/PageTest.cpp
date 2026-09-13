/** @file
 * The sheet a sketch stands on: the canvas a stage declares, and the page
 * that is the hand-spelled header, content and footer.
 */

#include <gtest/gtest.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcore/reconcile/Environment.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/layout/StyleSheet.h>

#include <utility>

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

// The stage

TEST(SketchKitStage, DeclaresTheWholeCanvas) {
  sigil::motion::Ticker ticker;
  compose::Composer composer(ticker, fonts());
  sigil::sketch::CanvasSpecification specification;
  sigil::sketch::SketchContext ctx(composer, ticker, assets(), {0, 0},
                                   &specification, &fonts());

  kit::stage(ctx, {.size = {1100, 424}, .captureAt = 0.05});
  EXPECT_EQ(specification.size, (SkSize{1100, 424}));
  EXPECT_EQ(specification.captureSeconds, 0.05);
  EXPECT_EQ(specification.background, kit::houseTheme().palette.ground);
  EXPECT_EQ(specification.oversample, 0);
  EXPECT_FALSE(specification.plateOnly);
  EXPECT_EQ(ctx.size, (SkSize{1100, 424}));
}

TEST(SketchKitStage, TheGroundIsTheThemesUnlessTheStageSaysOtherwise) {
  sigil::motion::Ticker ticker;
  compose::Composer composer(ticker, fonts());
  sigil::sketch::CanvasSpecification specification;
  sigil::sketch::SketchContext ctx(composer, ticker, assets(), {0, 0},
                                   &specification, &fonts());

  kit::Theme paper = kit::houseTheme();
  paper.palette.ground = {0.945f, 0.937f, 0.918f, 1};
  {
    const kit::Provide bound(paper);
    kit::stage(ctx, {.size = {100, 100}});
    EXPECT_EQ(specification.background, paper.palette.ground);
  }
  kit::stage(ctx, {.size = {100, 100}, .background = SkColor4f{1, 0, 0, 1}});
  EXPECT_EQ(specification.background, (SkColor4f{1, 0, 0, 1}));
}

// The components, against what they replace

/** The claim every migrated sheet rests on: `page()` under the house
 *  theme is the sheet those sheets spelled by hand. */
TEST(SketchKitPage, DrawsTheHandSpelledSheet) {
  const kit::Theme& house = kit::houseTheme();
  const auto label = [&](float size, SkColor4f color, float track) {
    return sigil::weave::Type{
        .face = house.type.sans, .size = size, .color = color, .track = track};
  };
  // By hand the three lines are three classes of a sheet stated on the
  // page, which is exactly what the register names resolve to under the
  // theme.
  const sigil::weave::StyleSheet classes{
      {"title", label(14, house.palette.ink, 2.4f)},
      {"subtitle", label(11.5f, house.palette.ash, 0.8f)},
      {"footer", label(11, house.palette.ash, 0.4f)}};
  Element byHand =
      compose::kit::sheet({.title = "THE RULE AND THE STRANDS",
                           .subtitle = "dials · the width and the inset",
                           .footer = "a crossing is discovered",
                           .marginX = 24,
                           .marginTop = 20,
                           .marginBottom = 16,
                           .ground = Fill::color(house.palette.ground),
                           .rule = Fill::color(house.palette.rule)},
                          subject())
          .styleSheet(classes)
          .absolute()
          .inset(0);
  Element byKit = kit::page({.title = "THE RULE AND THE STRANDS",
                             .subtitle = "dials · the width and the inset",
                             .footer = "a crossing is discovered"},
                            subject());
  EXPECT_TRUE(sameDrawing(std::move(byHand), std::move(byKit)));
}

/** The page's own lines are set in the theme's registers with NO sheet
 *  bound around the call: the component binds them for the lines it
 *  writes, so a sketch that bound no theme still gets the sheet's voice. */
TEST(SketchKitPage, SetsItsLinesWithNoSheetBound) {
  namespace environment = sigil::core::environment;
  EXPECT_EQ(environment::inherited<sigil::weave::StyleSheet>(), nullptr);
  const kit::Theme& house = kit::houseTheme();
  Element unbound = kit::page({.title = "TITLE", .footer = "FOOT"}, subject());
  Element bound;
  {
    const kit::Provide look(house);
    bound = kit::page({.title = "TITLE", .footer = "FOOT"}, subject());
  }
  EXPECT_TRUE(sameDrawing(std::move(unbound), std::move(bound)));
}

/** A bound theme reaches the page four levels down without being handed
 *  to it — and moves what it draws. */
TEST(SketchKitPage, ReadsTheThemeInScope) {
  Element house = kit::page({.title = "TITLE"}, subject());
  kit::Theme paper = kit::houseTheme();
  paper.palette.ground = {0.945f, 0.937f, 0.918f, 1};
  paper.palette.ink = {0.114f, 0.106f, 0.098f, 1};
  Element other;
  {
    const kit::Provide bound(paper);
    other = kit::page({.title = "TITLE"}, subject());
  }
  EXPECT_FALSE(sameDrawing(std::move(house), std::move(other)));
}

TEST(SketchKitPage, UnruledRulesNeither) {
  EXPECT_FALSE(sameDrawing(
      kit::page({.title = "T", .footer = "F"}, subject()),
      kit::page({.title = "T", .footer = "F", .ruled = false}, subject())));
}

}  // namespace
