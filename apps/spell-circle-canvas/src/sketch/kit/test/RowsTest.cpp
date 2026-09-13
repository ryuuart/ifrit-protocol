/** @file
 * A name and the figure that answers it: the row, the readout several of
 * them make, and the fixed-column table.
 */

#include <gtest/gtest.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/kit/Rows.h>
#include <sigildata/table/Table.h>
#include <sigilmaterial/skia/Color.h>
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

// The rows

TEST(SketchKitRows, ALabelRowRangesItsFigureToTheMeasure) {
  const kit::Theme& house = kit::houseTheme();
  Element byHand =
      compose::box()
          .row()
          .alignItems(compose::Align::Center)
          .gap(house.spacing.labelGap)
          .width(220)
          .children(
              {compose::text(u8"nodes", house.style(house.type.captionNote,
                                                    house.palette.ash)),
               compose::box().grow(1),
               compose::text(u8"1 248", house.style(house.type.captionLabel,
                                                    house.palette.figure))});
  EXPECT_TRUE(sameDrawing(std::move(byHand),
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
          .width(220)
          .children({kit::labelRow({.name = u8"nodes", .value = u8"1 248"},
                                   {.measure = 220}),
                     kit::labelRow({.name = u8"instances", .value = u8"96"},
                                   {.measure = 220})});
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
      kit::labelRow({.name = u8"Promoted", .value = u8"18"}, {.measure = 200}),
      kit::labelRow({.name = u8"Promoted", .value = u8"18", .swatch = tier},
                    {.measure = 200})));
}

/** A name measure is what makes a table a table: with one, two rows whose
 *  names differ in length still start their figures on one line. */
TEST(SketchKitRows, ANameMeasureRangesTheFiguresOfUnequalNames) {
  const kit::Readout table{.measure = 240, .nameMeasure = 120};
  Element wide =
      kit::labelRow({.name = u8"describedNodes", .value = u8"7"}, table);
  Element narrow = kit::labelRow({.name = u8"memoHits", .value = u8"7"}, table);
  SkBitmap a = Drawn(std::move(wide)).pixels();
  SkBitmap b = Drawn(std::move(narrow)).pixels();
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
      compose::box()
          .column()
          .gap(house.spacing.rowGap)
          .children(
              {compose::box()
                   .row()
                   .alignItems(compose::Align::Center)
                   .gap(8)
                   .children(
                       {compose::box().width(9).height(9).fill(tier).shrink(0)})
                   .children(
                       {compose::text(u8"cellPanel", figure()).width(126)})
                   .children({compose::text(u8"0.00", figure()).width(46)})
                   .children({compose::text(u8"Promoted", quiet()).width(66)})
                   .children(
                       {compose::text(u8"baked by the library", quiet())})});
  Element byKit = kit::table(
      {{{u8"cellPanel", u8"0.00", u8"Promoted", u8"baked by the library"},
        tier}},
      {.columns = {{126, true}, {46, true}, {66}, {}},
       .gap = 8,
       .swatchSide = 9});
  EXPECT_TRUE(sameDrawing(std::move(byHand), std::move(byKit)));
}

/** A row with more words than there are columns sets the surplus in the
 *  last column's register, at its own width — the shape a table whose
 *  final column is prose already has. */
TEST(SketchKitRows, ASurplusWordTakesTheLastColumnsRegister) {
  const kit::Theme& house = kit::houseTheme();
  Element byHand =
      compose::box()
          .column()
          .gap(house.spacing.rowGap)
          .children(
              {compose::box()
                   .row()
                   .alignItems(compose::Align::Center)
                   .gap(house.spacing.labelGap)
                   .children({compose::text(u8"key",
                                            house.style(house.type.captionNote,
                                                        house.palette.ash))
                                  .width(60)})
                   .children({compose::text(u8"0.00",
                                            house.style(house.type.captionLabel,
                                                        house.palette.figure))})
                   .children({compose::text(
                       u8"12", house.style(house.type.captionLabel,
                                           house.palette.figure))})});
  EXPECT_TRUE(sameDrawing(std::move(byHand),
                          kit::table({{{u8"key", u8"0.00", u8"12"}}},
                                     {.columns = {{60}, {0, true}}})));
}

// The bars a column of values is drawn as

/** One row per value, the bar against the largest of them, drawn in the
 *  theme's figure colour on a track of the same dimmed. */
TEST(SketchKitRows, BarsStandAgainstTheLargestValue) {
  const kit::Theme& house = kit::houseTheme();
  const std::vector<compose::Utf8> labels = {u8"Tokyo", u8"Lagos"};
  const std::vector<double> values = {37.0, 15.0};
  compose::kit::Bars how{.length = 150,
                         .labelMeasure = 96,
                         .barHeight = house.spacing.barHeight,
                         .gap = house.spacing.labelGap,
                         .rowGap = house.spacing.rowGap,
                         .bar = Fill::color(house.palette.figure),
                         .rest = Fill::color(sigil::material::skia::withAlpha(
                             house.palette.figure, 0.25f))};
  const auto quiet = house.style(house.type.captionNote, house.palette.ash);
  const auto number =
      house.style(house.type.captionLabel, house.palette.figure);
  how.labelLine = [quiet](const compose::Utf8& words) {
    return compose::text(words, quiet);
  };
  how.figureLine = [number](double value) {
    return compose::text(compose::kit::formatted("%.0f", value), number);
  };
  EXPECT_TRUE(sameDrawing(compose::kit::bars(labels, values, how),
                          kit::bars(labels, values, {})));
}

/** The same, read straight off two columns of a table — which is what a
 *  sketch holding a decoded CSV has. */
TEST(SketchKitRows, BarsReadTwoColumnsOfATable) {
  sigil::data::Table cities;
  cities.add<std::string>("city", {"Tokyo", "Lagos"});
  cities.add<double>("population", {37.0, 15.0});
  const std::vector<compose::Utf8> labels = {u8"Tokyo", u8"Lagos"};
  const std::vector<double> values = {37.0, 15.0};
  EXPECT_TRUE(sameDrawing(kit::bars(labels, values, {}),
                          kit::bars(cities, "city", "population", {})));
  // A column that is not there draws no row rather than guessing one.
  EXPECT_TRUE(
      sameDrawing(compose::box(), kit::bars(cities, "city", "deaths", {})));
}

}  // namespace
