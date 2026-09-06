/** @file
 * The sheet a sketch stands on: the canvas a stage declares, and the page
 * that is the hand-spelled header, content and footer.
 */

#include <gtest/gtest.h>
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

// The stage

TEST(SketchKitStage, DeclaresTheWholeCanvas) {
  sigil::motion::Ticker ticker;
  compose::Composer composer(ticker, fonts());
  sigil::sketch::CanvasSpec spec;
  sigil::sketch::SketchContext ctx(composer, ticker, assets(), {0, 0}, &spec,
                                   &fonts());

  kit::stage(ctx, {.size = {1100, 424}, .captureAt = 0.05});
  EXPECT_EQ(spec.size, (SkSize{1100, 424}));
  EXPECT_EQ(spec.captureSeconds, 0.05);
  EXPECT_EQ(spec.background, kit::houseTheme().palette.ground);
  EXPECT_EQ(spec.oversample, 0);
  EXPECT_FALSE(spec.plateOnly);
  EXPECT_EQ(ctx.size, (SkSize{1100, 424}));
}

TEST(SketchKitStage, TheGroundIsTheThemesUnlessTheStageSaysOtherwise) {
  sigil::motion::Ticker ticker;
  compose::Composer composer(ticker, fonts());
  sigil::sketch::CanvasSpec spec;
  sigil::sketch::SketchContext ctx(composer, ticker, assets(), {0, 0}, &spec,
                                   &fonts());

  kit::Theme paper = kit::houseTheme();
  paper.palette.ground = {0.945f, 0.937f, 0.918f, 1};
  {
    const kit::Provide bound(paper);
    kit::stage(ctx, {.size = {100, 100}});
    EXPECT_EQ(spec.background, paper.palette.ground);
  }
  kit::stage(ctx, {.size = {100, 100}, .background = SkColor4f{1, 0, 0, 1}});
  EXPECT_EQ(spec.background, (SkColor4f{1, 0, 0, 1}));
}

// The components, against what they replace

/** The claim every migrated sheet rests on: `page()` under the house
 *  theme is the sheet those sheets spelled by hand. */
TEST(SketchKitPage, DrawsTheHandSpelledSheet) {
  const kit::Theme& house = kit::houseTheme();
  const auto label = [&](float size, SkColor4f color, float track) {
    return sigil::weave::textStyle(
        {.size = size, .color = color, .track = track});
  };
  Element byHand = compose::kit::sheet(
                       {.title = u8"THE RULE AND THE STRANDS",
                        .subtitle = u8"dials · the width and the inset",
                        .footer = u8"a crossing is discovered",
                        .titleStyle = label(14, house.palette.ink, 2.4f),
                        .subtitleStyle = label(11.5f, house.palette.ash, 0.8f),
                        .footerStyle = label(11, house.palette.ash, 0.4f),
                        .marginX = 24,
                        .marginTop = 20,
                        .marginBottom = 16,
                        .ground = Fill::color(house.palette.ground),
                        .rule = Fill::color(house.palette.rule)},
                       subject())
                       .absolute()
                       .inset(0);
  Element byKit = kit::page({.title = u8"THE RULE AND THE STRANDS",
                             .subtitle = u8"dials · the width and the inset",
                             .footer = u8"a crossing is discovered"},
                            subject());
  EXPECT_TRUE(sameDrawing(std::move(byHand), std::move(byKit)));
}

/** A bound theme reaches the page four levels down without being handed
 *  to it — and moves what it draws. */
TEST(SketchKitPage, ReadsTheThemeInScope) {
  Element house = kit::page({.title = u8"TITLE"}, subject());
  kit::Theme paper = kit::houseTheme();
  paper.palette.ground = {0.945f, 0.937f, 0.918f, 1};
  paper.palette.ink = {0.114f, 0.106f, 0.098f, 1};
  Element other;
  {
    const kit::Provide bound(paper);
    other = kit::page({.title = u8"TITLE"}, subject());
  }
  EXPECT_FALSE(sameDrawing(std::move(house), std::move(other)));
}

TEST(SketchKitPage, UnruledRulesNeither) {
  EXPECT_FALSE(sameDrawing(
      kit::page({.title = u8"T", .footer = u8"F"}, subject()),
      kit::page({.title = u8"T", .footer = u8"F", .ruled = false}, subject())));
}

}  // namespace
