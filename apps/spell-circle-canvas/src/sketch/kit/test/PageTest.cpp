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

TEST(SketchKitStage, TheGroundIsTheStyleSheetsUnlessTheStageSaysOtherwise) {
  sigil::motion::Engine engine;
  compose::Composer composer(engine, fonts());
  sigil::sketch::CanvasSpecification specification;
  sigil::sketch::Scene2DContext ctx(composer, engine, assets(), {0, 0},
                                    &specification, &fonts());

  kit::Theme paper = kit::houseTheme();
  paper.palette.ground = {0.945f, 0.937f, 0.918f, 1};
  kit::stage(ctx, {.styleSheet = paper.styleSheet(), .size = {100, 100}});
  EXPECT_EQ(specification.background, paper.palette.ground);
  kit::stage(ctx, {.styleSheet = kit::darkStyleSheet(), .size = {100, 100}});
  EXPECT_EQ(specification.background, kit::darkTheme().palette.ground);
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
  // The ground falls back to the house ground, as a scene2d sketch's does.
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
    return sigil::weave::Type{.face = house.type.sans,
                              .size = size,
                              .ink = color,
                              .letterSpacing = track};
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

/** THE STAGE HAS ONE SLOT, ITS STYLESHEET, set whole at the root of what
 *  the sketch renders, so a page under it is the page under that
 *  stylesheet; a bare stage sets the house properties alone, and its role
 *  rules stay the sketch's own to state. */
TEST(SketchKitStage, TheStageSetsItsStyleSheetAtTheRoot) {
  kit::Theme paper = kit::houseTheme();
  paper.palette.ground = {0.945f, 0.937f, 0.918f, 1};
  const auto staged = [](std::optional<compose::StyleSheet> look) {
    sigil::motion::Engine engine;
    compose::Composer composer(engine, fonts());
    sigil::sketch::CanvasSpecification specification;
    sigil::sketch::Scene2DContext ctx(composer, engine, assets(), {0, 0},
                                      &specification, &fonts());
    kit::stage(ctx, {.styleSheet = std::move(look),
                     .size = {(float)kWide, (float)kTall}});
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
  const auto drawn = [](const compose::StyleSheet* look) {
    Element page = kit::page({.title = "TITLE", .footer = "FOOT"}, subject());
    if (look != nullptr) page.applyStyleSheet(*look);
    return Drawn(std::move(page)).pixels();
  };
  const compose::StyleSheet paperSheet = paper.styleSheet();
  EXPECT_TRUE(
      same(staged(kit::houseStyleSheet()), drawn(&kit::houseStyleSheet())));
  EXPECT_TRUE(same(staged(paperSheet), drawn(&paperSheet)));
  EXPECT_TRUE(same(staged(kit::featureStyleSheet(kit::Density::Spacious)),
                   drawn(&kit::featureStyleSheet(kit::Density::Spacious))));
  EXPECT_TRUE(same(staged(std::nullopt), drawn(nullptr)))
      << "a bare stage is the house properties and no role rules";
  const compose::StyleSheet houseProperties = kit::houseTheme().properties();
  EXPECT_TRUE(same(staged(std::nullopt), drawn(&houseProperties)))
      << "a bare stage is the house properties and no role rules";
  EXPECT_FALSE(same(staged(std::nullopt), staged(kit::houseStyleSheet())));
}

/** A STOCK STYLESHEET IS ITS BUILDER'S: each one is the stylesheet its
 *  `Theme` makes, held, so two stages naming it hand the composer one
 *  sheet. */
TEST(SketchKitStage, AStockStyleSheetIsTheStyleSheetItsBuilderMakes) {
  EXPECT_EQ(kit::houseStyleSheet(), kit::houseTheme().styleSheet());
  EXPECT_EQ(kit::studyStyleSheet(), kit::studyTheme().styleSheet());
  EXPECT_EQ(kit::specimenStyleSheet(), kit::specimenTheme().styleSheet());
  EXPECT_EQ(kit::featureStyleSheet(),
            kit::featureTheme(kit::Density::Compact).styleSheet());
  EXPECT_EQ(kit::featureStyleSheet(kit::Density::Spacious),
            kit::featureTheme(kit::Density::Spacious).styleSheet());
  EXPECT_EQ(kit::darkStyleSheet(), kit::darkTheme().styleSheet());
  EXPECT_EQ(&kit::featureStyleSheet(kit::Density::Spacious),
            &kit::featureStyleSheet(kit::Density::Spacious));
  EXPECT_NE(kit::featureStyleSheet(kit::Density::Compact),
            kit::featureStyleSheet(kit::Density::Spacious));
}

/** A STYLESHEET OF A FEW RULES STANDS ON THE HOUSE PROPERTIES: a stage
 *  whose stylesheet sets one property draws the page the house properties
 *  with that one field changed draw. */
TEST(SketchKitStage, AStyleSheetOfOneRuleStandsOnTheHouseProperties) {
  kit::Theme copied = kit::houseTheme();
  copied.spacing.contentGap = 30;
  sigil::motion::Engine engine;
  compose::Composer composer(engine, fonts());
  sigil::sketch::CanvasSpecification specification;
  sigil::sketch::Scene2DContext ctx(composer, engine, assets(), {0, 0},
                                    &specification, &fonts());
  kit::stage(ctx, {.styleSheet = compose::rule(":root").var(
                       "spacing-content-gap", compose::Dimension(30.0f)),
                   .size = {(float)kWide, (float)kTall}});
  EXPECT_EQ(specification.background, kit::houseTheme().palette.ground);
  composer.setSize({(float)kWide, (float)kTall});
  composer.render(kit::page({.title = "TITLE", .footer = "FOOT"}, subject()));
  const sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWide, kTall));
  surface->getCanvas()->clear(SK_ColorBLACK);
  composer.draw(*surface->getCanvas());
  SkBitmap staged;
  staged.allocPixels(SkImageInfo::MakeN32Premul(kWide, kTall));
  ASSERT_TRUE(surface->readPixels(staged.pixmap(), 0, 0));

  const SkBitmap byCopy =
      Drawn(kit::page({.title = "TITLE", .footer = "FOOT"}, subject())
                .applyStyleSheet(copied.properties()))
          .pixels();
  for (int y = 0; y < kTall; ++y)
    ASSERT_EQ(std::memcmp(staged.getAddr32(0, y), byCopy.getAddr32(0, y),
                          (size_t)kWide * 4),
              0)
        << "row " << y;
}

/** A VARIANT IS A STOCK STYLESHEET PLUS RULES: a stage naming a stock
 *  stylesheet with a rule joined by `+` that sets a few of its properties
 *  on `:root` draws the page a copied builder with those fields changed
 *  draws, and the canvas takes the ground the rule states. */
TEST(SketchKitStage, AVariantIsRulesJoinedAtTheStage) {
  const sigil::material::Color paperGround{0.945f, 0.937f, 0.918f, 1};
  kit::Theme copied = kit::houseTheme();
  copied.palette.ground = paperGround;
  copied.spacing.marginHorizontal = 40;
  copied.spacing.contentGap = 30;
  const compose::Rule variant =
      compose::rule(":root")
          .var("palette-ground", paperGround)
          .var("spacing-margin-horizontal", compose::Dimension(40.0f))
          .var("spacing-content-gap", compose::Dimension(30.0f));

  sigil::motion::Engine engine;
  compose::Composer composer(engine, fonts());
  sigil::sketch::CanvasSpecification specification;
  sigil::sketch::Scene2DContext ctx(composer, engine, assets(), {0, 0},
                                    &specification, &fonts());
  kit::stage(ctx, {.styleSheet = kit::houseStyleSheet() + variant,
                   .size = {(float)kWide, (float)kTall}});
  EXPECT_EQ(specification.background, paperGround);
  composer.setSize({(float)kWide, (float)kTall});
  composer.render(kit::page({.title = "TITLE", .footer = "FOOT"}, subject()));
  const sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWide, kTall));
  surface->getCanvas()->clear(SK_ColorBLACK);
  composer.draw(*surface->getCanvas());
  SkBitmap staged;
  staged.allocPixels(SkImageInfo::MakeN32Premul(kWide, kTall));
  ASSERT_TRUE(surface->readPixels(staged.pixmap(), 0, 0));

  const SkBitmap byCopy =
      Drawn(kit::page({.title = "TITLE", .footer = "FOOT"}, subject())
                .applyStyleSheet(copied.styleSheet()))
          .pixels();
  for (int y = 0; y < kTall; ++y)
    ASSERT_EQ(std::memcmp(staged.getAddr32(0, y), byCopy.getAddr32(0, y),
                          (size_t)kWide * 4),
              0)
        << "row " << y;

  // An explicit background still stands over the sheet's ground.
  kit::stage(ctx, {.styleSheet = kit::houseStyleSheet() + variant,
                   .size = {10, 10},
                   .background = sigil::material::Color{1, 0, 0, 1}});
  EXPECT_EQ(specification.background, (sigil::material::Color{1, 0, 0, 1}));
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
