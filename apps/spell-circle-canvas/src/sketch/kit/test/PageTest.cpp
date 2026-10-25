/** @file
 * The sheet a sketch stands on: the canvas a stage declares, and the page
 * that is the hand-spelled header, content and footer.
 */

#include <gtest/gtest.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>

#include <cstring>
#include <optional>
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
  sigil::motion::Engine engine;
  compose::Composer composer(engine, fonts());
  sigil::sketch::CanvasSpecification specification;
  sigil::sketch::Scene2DContext ctx(composer, engine, assets(), {0, 0},
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
  sigil::motion::Engine engine;
  compose::Composer composer(engine, fonts());
  sigil::sketch::CanvasSpecification specification;
  sigil::sketch::Scene2DContext ctx(composer, engine, assets(), {0, 0},
                                    &specification, &fonts());

  kit::Theme paper = kit::houseTheme();
  paper.palette.ground = {0.945f, 0.937f, 0.918f, 1};
  kit::stage(ctx, {.theme = paper, .size = {100, 100}});
  EXPECT_EQ(specification.background, paper.palette.ground);
  kit::stage(ctx, {.size = {100, 100},
                   .background = sigil::material::Color{1, 0, 0, 1}});
  EXPECT_EQ(specification.background, (sigil::material::Color{1, 0, 0, 1}));
}

TEST(SketchKitStage, AScene3DDeclaresTheSameThreeThings) {
  // A scene3d sketch says the canvas, the ground and the moment through three
  // calls of its own; the stage is the same value for both kinds of sketch.
  sigil::sketch::CanvasSpecification specification;
  sigil::geometry::mesh::camera::Camera eye;
  sigil::sketch::Scene3DContext ctx{.assets = assets(),
                                    .fonts = fonts(),
                                    .specification = &specification,
                                    .eye = &eye};

  kit::stage(ctx,
             {.size = {640, 440},
              .captureAt = 1.3,
              .background = sigil::material::Color{0.03f, 0.035f, 0.05f, 1}});
  EXPECT_EQ(specification.size, (SkSize{640, 440}));
  EXPECT_EQ(specification.captureSeconds, 1.3);
  EXPECT_EQ(specification.background,
            (sigil::material::Color{0.03f, 0.035f, 0.05f, 1}));
  // The ground falls back to the theme's, as a scene2d sketch's does.
  kit::stage(ctx, {.size = {10, 10}});
  EXPECT_EQ(specification.background, kit::houseTheme().palette.ground);
}

// The components, against what they replace

/** The claim every migrated sheet rests on: `page()` under the house
 *  theme is the sheet those sheets spelled by hand. */
TEST(SketchKitPage, DrawsTheHandSpelledSheet) {
  const kit::Theme& house = kit::houseTheme();
  const auto label = [&](float size, sigil::material::Color color,
                         float track) {
    return sigil::weave::Type{
        .face = house.type.sans, .size = size, .color = color, .track = track};
  };
  // By hand the three lines are three classes of a sheet stated on the
  // page, which is exactly what the register names resolve to under the
  // theme.
  const compose::StyleSheet classes{
      compose::rule("h1").font(label(14, house.palette.ink, 2.4f)),
      compose::rule("lead").font(label(11.5f, house.palette.ash, 0.8f)),
      compose::rule("footer").font(label(11, house.palette.ash, 0.4f))};
  Element byHand =
      compose::kit::sheet({.title = "THE RULE AND THE STRANDS",
                           .subtitle = "dials · the width and the inset",
                           .footer = "a crossing is discovered",
                           .marginHorizontal = 24,
                           .marginTop = 20,
                           .marginBottom = 16,
                           .ground = Fill::color(house.palette.ground),
                           .rule = Fill::color(house.palette.rule)},
                          subject())
          .applyStyleSheet(classes)
          .absolute()
          .inset(0);
  Element byKit = kit::page({.title = "THE RULE AND THE STRANDS",
                             .subtitle = "dials · the width and the inset",
                             .footer = "a crossing is discovered"},
                            subject())
                      .applyStyleSheet(house.styleSheet());
  EXPECT_TRUE(sameDrawing(std::move(byHand), std::move(byKit)));
}

/** A STAGE NAMING A THEME SETS ITS WHOLE SHEET at the root of what the
 *  sketch renders, so a page under it is the page under that sheet; a
 *  stage naming none sets the house properties alone, and its role rules
 *  stay the sketch's own to state. */
TEST(SketchKitStage, TheStageSetsItsThemeAtTheRoot) {
  kit::Theme paper = kit::houseTheme();
  paper.palette.ground = {0.945f, 0.937f, 0.918f, 1};
  const auto staged = [](std::optional<kit::Theme> look) {
    sigil::motion::Engine engine;
    compose::Composer composer(engine, fonts());
    sigil::sketch::CanvasSpecification specification;
    sigil::sketch::Scene2DContext ctx(composer, engine, assets(), {0, 0},
                                      &specification, &fonts());
    kit::stage(
        ctx, {.theme = std::move(look), .size = {(float)kWide, (float)kTall}});
    composer.setSize({(float)kWide, (float)kTall});
    composer.render(kit::page({.title = "TITLE", .footer = "FOOT"}, subject()));
    const sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWide, kTall));
    surface->getCanvas()->clear(SK_ColorBLACK);
    composer.draw(*surface->getCanvas());
    SkBitmap pixels;
    pixels.allocPixels(SkImageInfo::MakeN32Premul(kWide, kTall));
    EXPECT_TRUE(surface->readPixels(pixels.pixmap(), 0, 0));
    return pixels;
  };
  const auto same = [](const SkBitmap& one, const SkBitmap& other) {
    for (int y = 0; y < kTall; ++y)
      if (std::memcmp(one.getAddr32(0, y), other.getAddr32(0, y),
                      (size_t)kWide * 4) != 0)
        return false;
    return true;
  };
  const auto drawn = [](const kit::Theme* look) {
    Element page = kit::page({.title = "TITLE", .footer = "FOOT"}, subject());
    if (look != nullptr) page.applyStyleSheet(look->styleSheet());
    return Drawn(std::move(page)).pixels();
  };
  EXPECT_TRUE(same(staged(kit::houseTheme()), drawn(&kit::houseTheme())));
  EXPECT_TRUE(same(staged(paper), drawn(&paper)));
  EXPECT_TRUE(same(staged(std::nullopt), drawn(nullptr)))
      << "no theme named is the house properties and no role rules";
  EXPECT_FALSE(same(staged(std::nullopt), staged(kit::houseTheme())));
}

/** The theme a page names reaches a component four levels down without
 *  being handed to it — and moves what it draws. */
TEST(SketchKitPage, AComponentUnderThePageReadsItsTheme) {
  kit::Theme paper = kit::houseTheme();
  paper.palette.cellGround = {0.945f, 0.937f, 0.918f, 1};
  paper.spacing.cellGap = 44;
  const auto content = [] {
    return compose::box().children({compose::box().children(
        {kit::cells({.cells = {kit::well({.width = compose::Dimension(60),
                                          .height = compose::Dimension(40)}),
                               subject().key("second")}})})});
  };
  const auto under = [&](const kit::Theme& look) {
    return kit::page({.title = "TITLE"}, content())
        .applyStyleSheet(look.styleSheet());
  };
  Drawn house(under(kit::houseTheme()));
  Drawn other(under(paper));
  const auto before = house.composer.bounds("second");
  const auto after = other.composer.bounds("second");
  ASSERT_TRUE(before && after);
  EXPECT_FLOAT_EQ(after->left() - before->left(),
                  paper.spacing.cellGap - kit::houseTheme().spacing.cellGap);
  EXPECT_FALSE(sameDrawing(under(kit::houseTheme()), under(paper)));
}

TEST(SketchKitPage, UnruledRulesNeither) {
  EXPECT_FALSE(sameDrawing(
      kit::page({.title = "T", .footer = "F"}, subject()),
      kit::page({.title = "T", .footer = "F", .ruled = false}, subject())));
}

}  // namespace
