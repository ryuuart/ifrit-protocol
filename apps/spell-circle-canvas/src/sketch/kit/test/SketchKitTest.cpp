/** @file
 * The kit's claim, asserted: a theme is a comparable value that a scope
 * binds and a component reads, and every component here draws exactly
 * what the compose kit spelled by hand with the same values draws — which
 * is what lets a sketch adopt the kit without moving a pixel of its
 * plate.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilsketch/kit/Kit.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "support/Fixtures.h"

namespace {

namespace kit = sigil::sketch::kit;
namespace compose = sigil::compose;
using compose::Element;
using compose::Fill;
using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;

constexpr int kWide = 420;
constexpr int kTall = 300;

/** A composer over a raster surface, so a tree can be compared to another
 *  tree by the pixels the two produce. */
struct Host {
  sigil::motion::Ticker ticker;
  compose::Composer composer{ticker, fonts()};
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWide, kTall));

  explicit Host(Element tree) {
    composer.setSize({(float)kWide, (float)kTall});
    composer.render(std::move(tree));
    surface->getCanvas()->clear(SK_ColorBLACK);
    composer.draw(*surface->getCanvas());
  }

  SkBitmap pixels() {
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(kWide, kTall));
    EXPECT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
    return bitmap;
  }
};

/** Every pixel of one tree against every pixel of the other. */
bool sameDrawing(Element left, Element right) {
  SkBitmap a = Host(std::move(left)).pixels();
  SkBitmap b = Host(std::move(right)).pixels();
  for (int y = 0; y < kTall; ++y)
    if (std::memcmp(a.getAddr32(0, y), b.getAddr32(0, y),
                    (size_t)kWide * 4) != 0)
      return false;
  return true;
}

/** A subject with nothing of the theme in it, so a difference between two
 *  drawings is a difference in what the kit put around it. */
Element subject() {
  return compose::box().width(compose::Dim(60)).height(compose::Dim(40)).fill(
      Fill::color({0.9f, 0.3f, 0.4f, 1}));
}

// ---------------------------------------------------------------------------
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
  EXPECT_EQ(kit::houseTheme().type.mono.get(), kit::houseTheme().type.mono.get());
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

// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// The components, against what they replace

/** The claim every migrated sheet rests on: `page()` under the house
 *  theme is the sheet those sheets spelled by hand. */
TEST(SketchKitPage, DrawsTheHandSpelledSheet) {
  const kit::Theme& house = kit::houseTheme();
  const auto label = [&](float size, SkColor4f color, float track) {
    return sigil::weave::textStyle(
        {.size = size, .color = color, .track = track});
  };
  Element byHand =
      compose::kit::sheet({.title = u8"THE RULE AND THE STRANDS",
                           .subtitle = u8"dials · the width and the inset",
                           .footer = u8"a crossing is discovered",
                           .titleStyle = label(14, house.palette.ink, 2.4f),
                           .subtitleStyle = label(11.5f, house.palette.ash,
                                                  0.8f),
                           .footerStyle = label(11, house.palette.ash, 0.4f),
                           .marginX = 24,
                           .marginTop = 20,
                           .marginBottom = 16,
                           .ground = Fill::color(house.palette.ground),
                           .rule = Fill::color(house.palette.rule)},
                          subject())
          .absolute()
          .inset(0);
  Element byKit =
      kit::page({.title = u8"THE RULE AND THE STRANDS",
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
  EXPECT_FALSE(sameDrawing(kit::page({.title = u8"T", .footer = u8"F"},
                                     subject()),
                           kit::page({.title = u8"T",
                                      .footer = u8"F",
                                      .ruled = false},
                                     subject())));
}

TEST(SketchKitCells, CaptionDrawsTheHandSpelledCell) {
  const kit::Theme& house = kit::houseTheme();
  const compose::kit::Caption voice{
      .where = compose::kit::Caption::Where::Split,
      .label = sigil::weave::textStyle({.face = house.type.mono,
                                        .size = 10.5f,
                                        .color = house.palette.ink}),
      .note = sigil::weave::textStyle(
          {.size = 10, .color = house.palette.ash, .track = 0.2f}),
      .gap = 7,
      .noteMeasure = 160};
  EXPECT_TRUE(sameDrawing(
      compose::kit::cell(voice, u8"border(1.8, ink, inset 7)",
                         u8"an ordinary rule 7 px inside the outline",
                         subject()),
      kit::caption(160, u8"border(1.8, ink, inset 7)",
                   u8"an ordinary rule 7 px inside the outline", subject())));
}

TEST(SketchKitCells, WellTakesTheThemesCellGround) {
  const kit::Theme& house = kit::houseTheme();
  EXPECT_TRUE(sameDrawing(
      compose::kit::well({.width = compose::Dim(163),
                          .height = compose::Dim(176),
                          .ground = Fill::color(house.palette.cellGround)},
                         compose::box().child(subject())),
      kit::well({.width = compose::Dim(163), .height = compose::Dim(176)},
                compose::box().child(subject()))));
}

TEST(SketchKitCells, AnExplicitGroundWinsOverTheThemes) {
  EXPECT_FALSE(sameDrawing(
      kit::well({.width = compose::Dim(163), .height = compose::Dim(176)},
                compose::box().child(subject())),
      kit::well({.width = compose::Dim(163),
                 .height = compose::Dim(176),
                 .ground = Fill::color({0.4f, 0.1f, 0.1f, 1})},
                compose::box().child(subject()))));
}

// ---------------------------------------------------------------------------
// The passage

/** A sketch's assets directory holding one passage, so what `passage`
 *  answers can be compared to what was written. */
struct Beside {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() / "sketch_kit_passage";
  sigil::sketch::Assets store{root};
  sigil::motion::Ticker ticker;
  compose::Composer composer{ticker, fonts()};
  sigil::sketch::CanvasSpec spec;
  sigil::sketch::SketchContext ctx{composer, ticker, store, {0, 0}, &spec,
                                   &fonts()};

  explicit Beside(std::string_view text) {
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "passages");
    std::ofstream(root / "passages" / "one.txt", std::ios::binary)
        << text;
  }
  ~Beside() { std::filesystem::remove_all(root); }
};

/** Narrowed for the failure message: the passage's bytes are what is
 *  being asserted, and a test framework prints them as text. */
std::string read(const std::u8string& text) {
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

TEST(SketchKitPassage, TheTextIsTheFilesWithoutItsLastNewline) {
  Beside beside("one line\nand another\n");
  EXPECT_EQ(read(kit::passage(beside.ctx, "one.txt")), "one line\nand another");
}

TEST(SketchKitPassage, AMissingPassageIsEmptyRatherThanAStandIn) {
  Beside beside("anything");
  EXPECT_TRUE(kit::passage(beside.ctx, "absent.txt").empty());
}

}  // namespace
