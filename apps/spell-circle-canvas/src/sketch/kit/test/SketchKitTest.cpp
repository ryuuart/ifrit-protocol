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
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Feed.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Plate.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilsketch/kit/Kit.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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

// ---------------------------------------------------------------------------
// The headings

/** The card is the header half of a page, standing alone: three lines in
 *  the theme's registers, spaced by its subtitle gap. */
TEST(SketchKitHeading, TitleCardDrawsTheHandSpelledColumn) {
  const kit::Theme& house = kit::houseTheme();
  const auto line = [&](const kit::Register& r, SkColor4f c) {
    return house.style(r, c);
  };
  Element byHand =
      compose::box()
          .column()
          .alignItems(compose::Align::Start)
          .child(compose::text(u8"SIGIL · COMPOSE",
                               line(house.type.eyebrow, house.palette.ash)))
          .child(compose::text(u8"THE STROKE ATLAS",
                               line(house.type.title, house.palette.ink))
                     .margin(0, house.spacing.subtitleGap, 0, 0))
          .child(compose::text(u8"every rail, at one width",
                               line(house.type.subtitle, house.palette.ash))
                     .margin(0, house.spacing.subtitleGap, 0, 0));
  Element byKit = kit::titleCard({.eyebrow = u8"SIGIL · COMPOSE",
                                  .title = u8"THE STROKE ATLAS",
                                  .subtitle = u8"every rail, at one width"});
  EXPECT_TRUE(sameDrawing(std::move(byHand), std::move(byKit)));
}

/** A missing line is absent and spends no gap behind it, so a card of two
 *  lines is not a card of three with one blank. */
TEST(SketchKitHeading, AMissingLineSpendsNoGap) {
  EXPECT_TRUE(sameDrawing(
      kit::titleCard({.title = u8"T", .subtitle = u8"S"}),
      compose::box()
          .column()
          .alignItems(compose::Align::Start)
          .child(compose::text(
              u8"T", kit::houseTheme().style(kit::houseTheme().type.title,
                                             kit::houseTheme().palette.ink)))
          .child(compose::text(
                     u8"S",
                     kit::houseTheme().style(kit::houseTheme().type.subtitle,
                                             kit::houseTheme().palette.ash))
                     .margin(0, kit::houseTheme().spacing.subtitleGap, 0, 0))));
}

/** The rule is what grows, so the note stands at the far edge however
 *  wide the header is — which is the whole reason it is a component. */
TEST(SketchKitHeading, TheSectionRuleFillsWhatTheTwoLinesLeave) {
  EXPECT_FALSE(sameDrawing(
      kit::sectionHeader({.label = u8"DYNAMICS", .note = u8"6 presets"})
          .width(compose::Dim(360)),
      kit::sectionHeader({.label = u8"DYNAMICS",
                          .note = u8"6 presets",
                          .ruled = false})
          .width(compose::Dim(360))));
}

// ---------------------------------------------------------------------------
// The rows

TEST(SketchKitRows, ALabelRowRangesItsFigureToTheMeasure) {
  const kit::Theme& house = kit::houseTheme();
  Element byHand =
      compose::box()
          .row()
          .alignItems(compose::Align::Center)
          .gap(house.spacing.labelGap)
          .width(compose::Dim(220))
          .child(compose::text(u8"nodes", house.style(house.type.captionNote,
                                                      house.palette.ash)))
          .child(compose::box().grow(1))
          .child(compose::text(u8"1 248",
                               house.style(house.type.captionLabel,
                                           house.palette.figure)));
  EXPECT_TRUE(sameDrawing(
      std::move(byHand),
      kit::labelRow({.name = u8"nodes", .value = u8"1 248"},
                    {.measure = 220})));
}

/** The table is the rows stacked at the theme's row gap — which is what
 *  makes two of them line up on their figures. */
TEST(SketchKitRows, AReadoutStacksItsRowsAtTheThemesGap) {
  const kit::Theme& house = kit::houseTheme();
  Element byHand =
      compose::box()
          .column()
          .gap(house.spacing.rowGap)
          .width(compose::Dim(220))
          .child(kit::labelRow({.name = u8"nodes", .value = u8"1 248"},
                               {.measure = 220}))
          .child(kit::labelRow({.name = u8"instances", .value = u8"96"},
                               {.measure = 220}));
  EXPECT_TRUE(sameDrawing(
      std::move(byHand),
      kit::readout({{u8"nodes", u8"1 248"}, {u8"instances", u8"96"}},
                   {.measure = 220})));
}

/** A reading's swatch stands before its name, so a table that is also a
 *  key needs no second column of marks beside it. */
TEST(SketchKitRows, ASwatchStandsBeforeTheName) {
  const Fill tier = Fill::color({0.4f, 0.9f, 0.55f, 1});
  EXPECT_FALSE(sameDrawing(
      kit::labelRow({.name = u8"Promoted", .value = u8"18"},
                    {.measure = 200}),
      kit::labelRow({.name = u8"Promoted", .value = u8"18", .swatch = tier},
                    {.measure = 200})));
}

/** A name measure is what makes a table a table: with one, two rows whose
 *  names differ in length still start their figures on one line. */
TEST(SketchKitRows, ANameMeasureRangesTheFiguresOfUnequalNames) {
  const kit::Readout table{.measure = 240, .nameMeasure = 120};
  Element wide = kit::labelRow({.name = u8"describedNodes", .value = u8"7"},
                               table);
  Element narrow = kit::labelRow({.name = u8"memoHits", .value = u8"7"}, table);
  SkBitmap a = Host(std::move(wide)).pixels();
  SkBitmap b = Host(std::move(narrow)).pixels();
  // The figure is at the far edge of the measure in both, so the last
  // painted column is the same one.
  const auto lastInk = [](const SkBitmap& bm) {
    for (int x = 239; x > 0; --x)
      for (int y = 0; y < 24; ++y)
        if (*bm.getAddr32(x, y) != 0xFF000000) return x;
    return 0;
  };
  EXPECT_EQ(lastInk(a), lastInk(b));
}

/** A bound level SCALES the filled part rather than sizing it, which is
 *  what keeps the bed's recording valid while the bar moves. Full, the
 *  scale is the identity and the two spellings are one drawing; part
 *  way, the scaled edge is resolved by the transform rather than by
 *  layout, so the two are close and not equal. */
TEST(SketchKitMeter, ABoundLevelFillsTheRailAsAFractionDoes) {
  const SkColor4f figure = kit::houseTheme().palette.figure;
  const auto barAt = [&](float level, int x) {
    SkBitmap drawn =
        Host(kit::meter({.level = level, .width = compose::Dim(200)})).pixels();
    const SkColor4f pixel = drawn.getColor4f(x, 2);
    return std::abs(pixel.fR - figure.fR) < 0.02f &&
           std::abs(pixel.fG - figure.fG) < 0.02f;
  };
  EXPECT_TRUE(barAt(1.0f, 190));
  EXPECT_FALSE(barAt(0.25f, 190));
  EXPECT_TRUE(barAt(0.25f, 20));
}

/** An outlined key: a map whose own marks are outlines cannot be keyed
 *  with filled patches. */
TEST(SketchKitLegend, AnOutlinedSwatchIsNotAFilledOne) {
  const Fill mark = Fill::color({0.4f, 0.9f, 0.55f, 1});
  EXPECT_FALSE(sameDrawing(kit::legend({.entries = {{mark, u8"live"}}}),
                           kit::legend({.entries = {{mark, u8"live"}},
                                        .strokeWidth = 1.4f})));
}

/** The four-column reading a name-and-figure pair cannot hold: each
 *  column at its own width, the last one taking what is left. */
TEST(SketchKitRows, ATableDrawsTheHandSpelledColumns) {
  const kit::Theme& house = kit::houseTheme();
  const Fill tier = Fill::color({0.4f, 0.9f, 0.55f, 1});
  const auto figure = [&] {
    return house.style(house.type.captionLabel, house.palette.figure);
  };
  const auto quiet = [&] {
    return house.style(house.type.captionNote, house.palette.ash);
  };
  Element byHand =
      compose::box().column().gap(house.spacing.rowGap).child(
      compose::box()
          .row()
          .alignItems(compose::Align::Center)
          .gap(8)
          .child(compose::box()
                     .width(compose::Dim(9))
                     .height(compose::Dim(9))
                     .fill(tier)
                     .shrink(0))
          .child(compose::text(u8"cellPanel", figure())
                     .width(compose::Dim(126)))
          .child(compose::text(u8"0.00", figure()).width(compose::Dim(46)))
          .child(compose::text(u8"Promoted", quiet()).width(compose::Dim(66)))
          .child(compose::text(u8"baked by the library", quiet())));
  Element byKit = kit::table(
      {{{u8"cellPanel", u8"0.00", u8"Promoted", u8"baked by the library"},
        tier}},
      {.columns = {{126, true}, {46, true}, {66}, {}},
       .gap = 8,
       .swatch = 9});
  EXPECT_TRUE(sameDrawing(std::move(byHand), std::move(byKit)));
}

/** A row with more words than there are columns sets the surplus in the
 *  last column's register, at its own width — the shape a table whose
 *  final column is prose already has. */
TEST(SketchKitRows, ASurplusWordTakesTheLastColumnsRegister) {
  const kit::Theme& house = kit::houseTheme();
  Element byHand =
      compose::box().column().gap(house.spacing.rowGap).child(
          compose::box()
              .row()
              .alignItems(compose::Align::Center)
              .gap(house.spacing.labelGap)
              .child(compose::text(u8"key",
                                   house.style(house.type.captionNote,
                                               house.palette.ash))
                         .width(compose::Dim(60)))
              .child(compose::text(u8"0.00",
                                   house.style(house.type.captionLabel,
                                               house.palette.figure)))
              .child(compose::text(u8"12",
                                   house.style(house.type.captionLabel,
                                               house.palette.figure))));
  EXPECT_TRUE(sameDrawing(
      std::move(byHand),
      kit::table({{{u8"key", u8"0.00", u8"12"}}},
                 {.columns = {{60}, {0, true}}})));
}

// ---------------------------------------------------------------------------
// Colour, named

TEST(SketchKitLegend, AnEntryIsASwatchAndItsWords) {
  const kit::Theme& house = kit::houseTheme();
  const Fill warm = Fill::color({0.9f, 0.6f, 0.3f, 1});
  Element byHand =
      compose::box()
          .column()
          .gap(house.spacing.rowGap)
          .alignItems(compose::Align::Start)
          .child(compose::box()
                     .row()
                     .alignItems(compose::Align::Center)
                     .gap(house.spacing.captionNoteGap)
                     .child(compose::box()
                                .width(compose::Dim(house.spacing.swatch))
                                .height(compose::Dim(house.spacing.swatch))
                                .fill(warm)
                                .shrink(0))
                     .child(compose::text(u8"lit",
                                          house.style(house.type.captionNote,
                                                      house.palette.ink))));
  EXPECT_TRUE(sameDrawing(std::move(byHand),
                          kit::legend({.entries = {{warm, u8"lit"}}})));
}

/** A strip that names only its ends keeps the unnamed steps butted, so
 *  the ramp reads as one band rather than as a row of tiles. */
TEST(SketchKitLegend, AStripNamesTheStepsItHasWordsFor) {
  std::vector<Fill> steps;
  for (int i = 0; i < 4; ++i)
    steps.push_back(Fill::color({0.2f * (float)i, 0.3f, 0.4f, 1}));
  EXPECT_FALSE(sameDrawing(
      kit::swatchStrip({.swatches = steps,
                        .width = compose::Dim(28),
                        .height = compose::Dim(14),
                        .gap = 0}),
      kit::swatchStrip({.swatches = steps,
                        .labels = {u8"0", {}, {}, u8"1"},
                        .width = compose::Dim(28),
                        .height = compose::Dim(14),
                        .gap = 0})));
}

TEST(SketchKitLegend, AChipIsItsWordOnTheThemesFigureGround) {
  const kit::Theme& house = kit::houseTheme();
  Element byHand =
      compose::box()
          .padding(house.spacing.chipPaddingX, house.spacing.chipPaddingY)
          .fill(Fill::color(house.palette.figure))
          .corners(compose::Corners{2})
          .child(compose::text(u8"PINNED",
                               house.style(house.type.eyebrow,
                                           house.palette.ground)));
  EXPECT_TRUE(sameDrawing(std::move(byHand), kit::chip({.label = u8"PINNED"})));
}

// ---------------------------------------------------------------------------
// A fraction drawn

TEST(SketchKitMeter, TheBarIsTheFractionOfTheTrack) {
  const kit::Theme& house = kit::houseTheme();
  Element byHand =
      compose::box()
          .width(compose::Dim(220))
          .height(compose::Dim(house.spacing.barHeight))
          .fill(Fill::color(house.palette.cellGround))
          .clip()
          .child(compose::box()
                     .width(compose::pct(40))
                     .fill(Fill::color(house.palette.figure))
                     .alignSelf(compose::Align::Stretch));
  EXPECT_TRUE(sameDrawing(std::move(byHand),
                          kit::meter({.fraction = 0.4f,
                                      .width = compose::Dim(220)})));
}

/** A fraction outside 0..1 is clamped: a bar past its own end is a
 *  drawing error rather than a reading. */
TEST(SketchKitMeter, AFractionOutsideTheTrackIsClamped) {
  EXPECT_TRUE(sameDrawing(
      kit::meter({.fraction = 3.0f, .width = compose::Dim(220)}),
      kit::meter({.fraction = 1.0f, .width = compose::Dim(220)})));
  EXPECT_TRUE(sameDrawing(
      kit::meter({.fraction = -1.0f, .width = compose::Dim(220)}),
      kit::meter({.fraction = 0.0f, .width = compose::Dim(220)})));
}

TEST(SketchKitMeter, TheDialSweepsWithItsFraction) {
  EXPECT_FALSE(sameDrawing(kit::gauge({.fraction = 0.25f}),
                           kit::gauge({.fraction = 0.75f})));
  EXPECT_TRUE(sameDrawing(kit::gauge({.fraction = 2.0f}),
                          kit::gauge({.fraction = 1.0f})));
}

// ---------------------------------------------------------------------------
// What stands behind and around

TEST(SketchKitPanel, TheBackdropIsTheThemesGround) {
  SkBitmap flat = Host(kit::backdrop({.over = {kWide, kTall}})).pixels();
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
      Host(kit::backdrop({.over = {kWide, kTall}, .vignette = 0.9f})).pixels();
  const SkColor4f corner = shaded.getColor4f(1, 1);
  const SkColor4f middle = shaded.getColor4f(kWide / 2, kTall / 2);
  EXPECT_LT(corner.fR, middle.fR);
  EXPECT_NEAR(middle.fR, paper.palette.ground.fR, 0.02f);
}

/** A grain moves pixels that a flat ground leaves identical. */
TEST(SketchKitPanel, AGrainIsNotAFlatGround) {
  SkBitmap grained =
      Host(kit::backdrop({.over = {kWide, kTall}, .grain = 0.5f})).pixels();
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
                     .stroke(compose::stroke(
                         1, Fill::color(house.palette.rule),
                         compose::PathFormat::Align::Inner))
                     .child(subject()));
  EXPECT_TRUE(sameDrawing(std::move(byHand),
                          kit::frame({.width = compose::Dim(200),
                                      .height = compose::Dim(120),
                                      .bezel = 8},
                                     subject())));
}

// ---------------------------------------------------------------------------
// The log panel

TEST(SketchKitConsole, DrawsTheHandSpelledPlate) {
  const kit::Theme& house = kit::houseTheme();
  compose::feed::TextRing rows;
  rows.append({u8"probe 1 · ok", ""});
  rows.append({u8"probe 2 · ok", ""});
  const Fill border = Fill::color(house.palette.rule);
  Element byHand = compose::kit::console(
      {.feeds = {&rows},
       .style = {.window = {.visible = 24, .gap = house.spacing.rowGap},
                 .styles = compose::kit::tinted(house.type.mono,
                                                house.type.captionLabel.size,
                                                house.palette.ink, {})},
       .plate = {.paddingX = house.spacing.panelPadding,
                 .paddingY = house.spacing.panelPadding * 0.6f,
                 .gap = house.spacing.labelGap,
                 .fill = Fill::color(house.palette.cellGround),
                 .border = border,
                 .divider = border}});
  EXPECT_TRUE(sameDrawing(std::move(byHand), kit::console({.feeds = {&rows}})));
}

// ---------------------------------------------------------------------------
// Along an axis

TEST(SketchKitTicker, TheCrawlIsTheHandSpelledMarquee) {
  const kit::Theme& house = kit::houseTheme();
  Element strip = subject();
  EXPECT_TRUE(sameDrawing(
      compose::kit::marquee(strip, 60.0f, 0.0f, house.spacing.labelGap)
          .width(compose::Dim(200)),
      kit::ticker({.content = strip,
                   .contentWidth = 60,
                   .phase = 0.0f,
                   .width = compose::Dim(200)})));
}

/** A minor mark draws a shorter tick and no word, so a scale reads its
 *  major divisions before its subdivisions. */
TEST(SketchKitTicker, AMinorMarkIsShorterAndUnnamed) {
  EXPECT_FALSE(sameDrawing(
      kit::timeline({.marks = {{0, u8"0 ms"}, {0.5f, u8"half", true}},
                     .width = compose::Dim(300)}),
      kit::timeline({.marks = {{0, u8"0 ms"}, {0.5f, u8"half", false}},
                     .width = compose::Dim(300)})));
}

// ---------------------------------------------------------------------------
// The runs

TEST(SketchKitCells, ARunIsTheHandSpelledRunAtTheThemesGutter) {
  EXPECT_TRUE(sameDrawing(
      compose::kit::cells({.cells = {subject(), subject()},
                           .gap = kit::houseTheme().spacing.cellGap}),
      kit::cells({.cells = {subject(), subject()}})));
}

/** Equal shares, whatever the cells carry: a wide cell and a narrow one
 *  come out the same width, which is what a run of fixed widths cannot
 *  do because it does not know how wide the page is. */
TEST(SketchKitCells, ColumnsTakeEqualShares) {
  Element wide = compose::box().width(compose::Dim(300)).height(
      compose::Dim(20)).fill(Fill::color({0.9f, 0.3f, 0.4f, 1}));
  Element narrow = compose::box().width(compose::Dim(10)).height(
      compose::Dim(20)).fill(Fill::color({0.9f, 0.3f, 0.4f, 1}));
  SkBitmap shared = Host(kit::columns({.cells = {wide, narrow}})).pixels();
  const int gutter = (int)kit::houseTheme().spacing.cellGap;
  const int share = (kWide - gutter) / 2;
  // The far end of the second share is painted, which it could not be if
  // the wide cell had kept its own 300 px.
  EXPECT_NE(*shared.getAddr32(share + gutter + share - 2, 4),
            *shared.getAddr32(share + gutter / 2, 4));
}

/** A short last row keeps its cells at one share rather than stretching
 *  them across the whole width. */
TEST(SketchKitCells, AShortGridRowKeepsItsShare) {
  EXPECT_TRUE(sameDrawing(
      kit::panelGrid({.cells = {subject(), subject(), subject(), subject()},
                      .columns = 3}),
      kit::cells({.cells = {kit::columns({.cells = {subject(), subject(),
                                                    subject()}}),
                            kit::columns({.cells = {subject(), compose::box(),
                                                    compose::box()}})},
                  .column = true,
                  .align = compose::Align::Stretch})));
}

}  // namespace
