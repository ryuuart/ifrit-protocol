/** @file
 * A name and the figure that answers it: the row, the readout several of
 * them make, and the fixed-column table.
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

// The rows

TEST(SketchKitRows, ALabelRowRangesItsFigureToTheMeasure) {
  const kit::Theme& house = kit::houseTheme();
  Element byHand =
      compose::box()
          .row()
          .alignItems(compose::Align::Center)
          .gap(house.spacing.labelGap)
          .width(compose::Dimension(220))
          .child(compose::text(u8"nodes", house.style(house.type.captionNote,
                                                      house.palette.ash)))
          .child(compose::box().grow(1))
          .child(compose::text(u8"1 248", house.style(house.type.captionLabel,
                                                      house.palette.figure)));
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
          .width(compose::Dimension(220))
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
          .child(compose::box()
                     .row()
                     .alignItems(compose::Align::Center)
                     .gap(8)
                     .child(compose::box()
                                .width(compose::Dimension(9))
                                .height(compose::Dimension(9))
                                .fill(tier)
                                .shrink(0))
                     .child(compose::text(u8"cellPanel", figure())
                                .width(compose::Dimension(126)))
                     .child(compose::text(u8"0.00", figure())
                                .width(compose::Dimension(46)))
                     .child(compose::text(u8"Promoted", quiet())
                                .width(compose::Dimension(66)))
                     .child(compose::text(u8"baked by the library", quiet())));
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
          .child(compose::box()
                     .row()
                     .alignItems(compose::Align::Center)
                     .gap(house.spacing.labelGap)
                     .child(compose::text(u8"key",
                                          house.style(house.type.captionNote,
                                                      house.palette.ash))
                                .width(compose::Dimension(60)))
                     .child(compose::text(u8"0.00",
                                          house.style(house.type.captionLabel,
                                                      house.palette.figure)))
                     .child(compose::text(u8"12",
                                          house.style(house.type.captionLabel,
                                                      house.palette.figure))));
  EXPECT_TRUE(sameDrawing(std::move(byHand),
                          kit::table({{{u8"key", u8"0.00", u8"12"}}},
                                     {.columns = {{60}, {0, true}}})));
}

}  // namespace
