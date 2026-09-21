/** @file
 * What a specimen stands in: the well, the plate, the caption beside it,
 * and the runs and grids several of them are set in.
 */

#include <gtest/gtest.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/PixelStyles.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcore/reconcile/Environment.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Paint.h>
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

TEST(SketchKitCells, CaptionDrawsTheHandSpelledCell) {
  const kit::Theme& house = kit::houseTheme();
  const compose::kit::Caption voice{
      .where = compose::kit::Caption::Where::Split,
      .gap = 7,
      .noteMeasure = 160};
  // By hand the cell's two lines are two classes of a sheet stated on the
  // cell. The kit's caption names the same two classes and states no
  // sheet of its own: a page states the theme's on its root, and here the
  // test states it on the cell, where the theme's registers resolve to
  // the two rules spelled out below.
  const sigil::weave::StyleSheet classes{
      {"label",
       {.face = house.type.mono, .size = 10.5f, .color = house.palette.ink}},
      {"caption",
       {.face = house.type.sans,
        .size = 10,
        .color = house.palette.ash,
        .track = 0.2f}}};
  Element byHand =
      compose::kit::cell(voice, "border(1.8, ink, inset 7)",
                         "an ordinary rule 7 px inside the outline", subject())
          .styleSheet(classes);
  EXPECT_TRUE(sameDrawing(
      std::move(byHand),
      kit::caption(160, "border(1.8, ink, inset 7)",
                   "an ordinary rule 7 px inside the outline", subject())
          .styleSheet(house.styleSheet())));
}

TEST(SketchKitCells, WellTakesTheThemesCellGround) {
  const kit::Theme& house = kit::houseTheme();
  EXPECT_TRUE(sameDrawing(
      compose::kit::well({.width = compose::Dimension(163),
                          .height = compose::Dimension(176),
                          .ground = Fill::color(house.palette.cellGround)},
                         compose::box().children({subject()})),
      kit::well(
          {.width = compose::Dimension(163), .height = compose::Dimension(176)},
          compose::box().children({subject()}))));
}

/** THE SHEET'S OWN CELL: the plate and the measure stated once, against
 *  the caption-over-a-well every sheet of pictures writes by hand. */
TEST(SketchKitCells, ACellIsTheSheetsPlateAndMeasureStatedOnce) {
  const kit::Cell sheet{.plate = {.width = compose::Dimension(200),
                                  .height = compose::Dimension(140),
                                  .padding = 12}};
  EXPECT_TRUE(sameDrawing(
      kit::caption(200, "shapes::chamfer(9)", "the corner taken off square",
                   kit::well({.width = compose::Dimension(200),
                              .height = compose::Dimension(140),
                              .padding = 12})
                       .children({subject()})),
      kit::cell(sheet, "shapes::chamfer(9)", "the corner taken off square",
                subject())));
}

/** A plate that RANGES its picture holds it where `Well::content` says,
 *  which is the middle where it says nothing else. */
TEST(SketchKitCells, ACellsPlateRangesThePictureWhereItsContentSays) {
  const kit::Cell sheet{.plate = {.width = compose::Dimension(200),
                                  .height = compose::Dimension(140),
                                  .content = kit::Well::Content{}}};
  EXPECT_TRUE(
      sameDrawing(kit::caption(200, "call", "note",
                               kit::well({.width = compose::Dimension(200),
                                          .height = compose::Dimension(140),
                                          .content = kit::Well::Content{}},
                                         subject())),
                  kit::cell(sheet, "call", "note", subject())));
}

/** THE WELL THAT HOLDS: a plate of the theme's ground with the picture
 *  standing in the middle of it at its own measure, against the centring
 *  container a sheet of pictures writes by hand. */
TEST(SketchKitCells, AWellHoldsItsPictureAtItsOwnMeasure) {
  const kit::Theme& house = kit::houseTheme();
  EXPECT_TRUE(
      sameDrawing(compose::kit::well(
                      {.width = compose::Dimension(200),
                       .height = compose::Dimension(200),
                       .ground = Fill::color(house.palette.cellGround)},
                      compose::kit::centred(subject().width(132).height(132))),
                  kit::well({.width = compose::Dimension(200),
                             .height = compose::Dimension(200),
                             .content = kit::Well::Content{}},
                            subject().width(132).height(132))));
}

/** THE PLATE: a grounded well with rounded corners and one hairline round
 *  it, against the four calls a sketch writes by hand for the same
 *  picture. */
TEST(SketchKitCells, APlateIsAGroundedWellWithCornersAndOneKeyline) {
  const Fill ground = Fill::color({0.10f, 0.11f, 0.14f, 1});
  const Fill edge = Fill::color({0.42f, 0.38f, 0.22f, 1});
  EXPECT_TRUE(
      sameDrawing(compose::box()
                      .width(163)
                      .height(176)
                      .borderRadius(compose::Corners{8})
                      .padding(16)
                      .overflow(compose::Overflow::Clip)
                      .fill(ground)
                      .stroke(compose::stroke(
                          1.0f, edge, compose::PathFormat::Align::Inner))
                      .children({subject()}),
                  kit::well({.width = compose::Dimension(163),
                             .height = compose::Dimension(176),
                             .ground = ground,
                             .padding = 16,
                             .corners = 8,
                             .keyline = edge},
                            compose::box().children({subject()}))));
}

/** A RECESSED well is the flush one with the shadow inside its edge and
 *  the sunken lip under it — a hole punched in what holds it. */
TEST(SketchKitCells, ARecessIsAShadowInsideTheEdgeAndASunkenLip) {
  const Fill ground = Fill::color({0.10f, 0.11f, 0.14f, 1});
  const kit::Well::Recess hole{
      .lipLight = SkColor4f{0.42f, 0.38f, 0.31f, 0.30f},
      .lipDark = SkColor4f{0, 0, 0, 0.55f}};
  EXPECT_TRUE(sameDrawing(
      compose::box()
          .width(140)
          .height(90)
          .overflow(compose::Overflow::Clip)
          .fill(ground)
          .foreground(compose::styles::InnerShadow{hole.shade.colorValue,
                                                   hole.offset, hole.blur})
          .overlay(compose::styles::bevelPair(*hole.lipLight, *hole.lipDark,
                                              hole.lipWidth,
                                              /*sunken=*/true)),
      kit::well({.width = compose::Dimension(140),
                 .height = compose::Dimension(90),
                 .ground = ground,
                 .recess = hole})));
  // …and it is not the flush well: the recess draws something.
  EXPECT_FALSE(sameDrawing(kit::well({.width = compose::Dimension(140),
                                      .height = compose::Dimension(90),
                                      .ground = ground}),
                           kit::well({.width = compose::Dimension(140),
                                      .height = compose::Dimension(90),
                                      .ground = ground,
                                      .recess = hole})));
}

/** A plate set tighter down than across, which one distance cannot say. */
TEST(SketchKitCells, APaddingDownOfItsOwn) {
  const Fill ground = Fill::color({0.10f, 0.11f, 0.14f, 1});
  EXPECT_TRUE(sameDrawing(
      compose::box()
          .width(163)
          .height(176)
          .padding(10, 13)
          .overflow(compose::Overflow::Clip)
          .fill(ground)
          .children({subject()}),
      kit::well({.width = compose::Dimension(163),
                 .height = compose::Dimension(176),
                 .ground = ground,
                 .padding = 13,
                 .paddingY = 10},
                compose::box().children({subject()}))));
}

/** THE GROUND'S OTHER FORM: a well grounded in a material draws what the
 *  same material put on a compose node by hand draws. The recipe is
 *  geometry-dependent SkSL, so it rides the node's material slot rather
 *  than collapsing to a Fill — which is the half a `Fill` alone could not
 *  say. */
TEST(SketchKitCells, AWellGroundedInAMaterialIsTheHandSpelledFill) {
  const sigil::material::Material quarry = sigil::material::kit::stone(
      {.hi = {0.47f, 0.29f, 0.29f, 1}, .lo = {0.30f, 0.19f, 0.19f, 1}});
  EXPECT_TRUE(
      sameDrawing(compose::box()
                      .width(163)
                      .height(176)
                      .overflow(compose::Overflow::Clip)
                      .fill(sigil::material::skia::Paint::recipe(quarry))
                      .children({subject()}),
                  kit::well({.width = compose::Dimension(163),
                             .height = compose::Dimension(176),
                             .ground = quarry},
                            compose::box().children({subject()}))));
}

/** A well carrying neither draws exactly what it always did. */
TEST(SketchKitCells, AWellWithoutThemDrawsWhatItAlwaysDid) {
  const kit::Theme& house = kit::houseTheme();
  EXPECT_TRUE(sameDrawing(
      compose::kit::well({.width = compose::Dimension(163),
                          .height = compose::Dimension(176),
                          .ground = Fill::color(house.palette.cellGround),
                          .padding = house.spacing.wellPadding},
                         compose::box().children({subject()})),
      kit::well(
          {.width = compose::Dimension(163), .height = compose::Dimension(176)},
          compose::box().children({subject()}))));
}

TEST(SketchKitCells, AnExplicitGroundWinsOverTheThemes) {
  EXPECT_FALSE(sameDrawing(
      kit::well(
          {.width = compose::Dimension(163), .height = compose::Dimension(176)},
          compose::box().children({subject()})),
      kit::well({.width = compose::Dimension(163),
                 .height = compose::Dimension(176),
                 .ground = Fill::color({0.4f, 0.1f, 0.1f, 1})},
                compose::box().children({subject()}))));
}

// The runs

TEST(SketchKitCells, ComparisonAlignsFiguresAfterWrappedTitlesAndControls) {
  const kit::Provide look(kit::studyTheme());
  Drawn drawn(
      kit::comparison(
          {.cases = {{.title = "THE REFERENCE WITH A LONGER TWO LINE TITLE",
                      .control = "radius = 22; corners = topLeft | bottomRight",
                      .figure =
                          subject().key("reference").width(140).height(40),
                      .note = "The reference outline."},
                     {.title = "RESULT",
                      .control = "radius = 0",
                      .figure = subject().key("result").width(160).height(60),
                      .note = "Its authored height is retained."}},
           .measure = 400,
           .gap = 20})
          .styleSheet(kit::theme().styleSheet()));
  const auto reference = drawn.composer.bounds("reference");
  const auto result = drawn.composer.bounds("result");
  ASSERT_TRUE(reference);
  ASSERT_TRUE(result);
  EXPECT_FLOAT_EQ(reference->top(), result->top());
  EXPECT_GT(reference->top(), 40);
  EXPECT_FLOAT_EQ(reference->width(), 140);
  EXPECT_FLOAT_EQ(result->width(), 160);
  EXPECT_FLOAT_EQ(reference->height(), 40);
  EXPECT_FLOAT_EQ(result->height(), 60);
  EXPECT_GE(result->left() - reference->right(), 20);
}

TEST(SketchKitCells, ComparisonInAPagePreservesFiguresAndWrapsNotes) {
  const kit::Provide look(kit::studyTheme());
  sigil::motion::Ticker ticker;
  compose::Composer composer(ticker, fonts());
  composer.setSize({900, 1050});
  const auto result = [](const char* key) {
    return compose::box().key(key).width(160).height(240).fill(
        Fill::color({0.9f, 0.3f, 0.4f, 1}));
  };
  Element first = compose::box().column().gap(12).children(
      {compose::box().key("map").width(160).height(74), result("first")});
  Element comparison =
      kit::comparison(
          {.cases =
               {{.title = "A LONG REFERENCE TITLE THAT WRAPS",
                 .control = "blur(map, maximum = 14)",
                 .figure = std::move(first),
                 .note =
                     "The reference note is deliberately long enough to wrap "
                     "across several lines inside its own narrow column."},
                {.title = "SECOND",
                 .control = "same control",
                 .figure = result("second"),
                 .note = "Short note."},
                {.title = "THIRD",
                 .control = "same control",
                 .figure = result("third"),
                 .note = "Short note."},
                {.title = "FOURTH",
                 .control = "same control",
                 .figure = result("fourth"),
                 .note = "Short note."}},
           .measure = 700,
           .gap = 20})
          .key("comparison");
  composer.render(
      kit::page({.title = "A complete page", .footer = "End"},
                compose::box().column().gap(24).children(
                    {compose::box()
                         .row()
                         .alignItems(compose::Align::Start)
                         .gap(20)
                         .children({std::move(comparison),
                                    compose::box().width(100).height(80)}),
                     compose::box().key("following").width(700).height(90)})));
  const sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(900, 1050));
  composer.draw(*surface->getCanvas());
  const auto map = composer.bounds("map");
  const auto firstBounds = composer.bounds("first");
  const auto second = composer.bounds("second");
  const auto fourth = composer.bounds("fourth");
  const auto band = composer.bounds("comparison");
  const auto following = composer.bounds("following");
  ASSERT_TRUE(map && firstBounds && second && fourth && band && following);
  EXPECT_FLOAT_EQ(map->height(), 74);
  EXPECT_FLOAT_EQ(firstBounds->height(), 240);
  EXPECT_FLOAT_EQ(second->height(), 240);
  EXPECT_FLOAT_EQ(fourth->height(), 240);
  EXPECT_FLOAT_EQ(firstBounds->top() - map->bottom(), 12);
  EXPECT_FLOAT_EQ(map->top(), second->top());
  EXPECT_FLOAT_EQ(second->top(), fourth->top());
  EXPECT_FLOAT_EQ(fourth->right() - map->left(), 700);
  // The longest note occupies multiple lines below the entire nested figure.
  EXPECT_GT(band->bottom() - firstBounds->bottom(), 50);
  EXPECT_GE(following->top() - band->bottom(), 24);
  EXPECT_LT(following->bottom(), 1000);
}

TEST(SketchKitCells, ComparisonOmitsTracksEmptyInEveryCase) {
  Drawn drawn(kit::comparison({.cases = {{.figure = subject().key("first")},
                                         {.figure = subject().key("second")}},
                               .measure = 400,
                               .gap = 20}));
  const auto first = drawn.composer.bounds("first");
  const auto second = drawn.composer.bounds("second");
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  EXPECT_FLOAT_EQ(first->top(), 0);
  EXPECT_FLOAT_EQ(first->top(), second->top());
  EXPECT_FLOAT_EQ(first->width(), 60);
}

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
  Element wide = compose::box().width(300).height(20).fill(
      Fill::color({0.9f, 0.3f, 0.4f, 1}));
  Element narrow = compose::box().width(10).height(20).fill(
      Fill::color({0.9f, 0.3f, 0.4f, 1}));
  SkBitmap shared =
      Drawn(kit::panelGrid({.cells = {wide, narrow}, .columns = 0})).pixels();
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
      kit::cells(
          {.cells = {kit::panelGrid({.cells = {subject(), subject(), subject()},
                                     .columns = 0}),
                     kit::panelGrid(
                         {.cells = {subject(), compose::box(), compose::box()},
                          .columns = 0})},
           .column = true,
           .align = compose::Align::Stretch})));
}

}  // namespace
