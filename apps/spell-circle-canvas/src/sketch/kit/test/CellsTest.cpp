/** @file
 * What a specimen stands in: the well, the plate, the caption beside it,
 * and the runs and grids several of them are set in.
 */

#include <gtest/gtest.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Paint.h>
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

TEST(SketchKitCells, CaptionDrawsTheHandSpelledCell) {
  const kit::Theme& house = kit::houseTheme();
  const compose::kit::Caption voice{
      .where = compose::kit::Caption::Where::Split,
      .label = sigil::weave::textStyle(
          {.face = house.type.mono, .size = 10.5f, .color = house.palette.ink}),
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

/** THE PLATE: a grounded well with rounded corners and one hairline round
 *  it, against the four calls a sketch writes by hand for the same
 *  picture. */
TEST(SketchKitCells, APlateIsAGroundedWellWithCornersAndOneKeyline) {
  const Fill ground = Fill::color({0.10f, 0.11f, 0.14f, 1});
  const Fill edge = Fill::color({0.42f, 0.38f, 0.22f, 1});
  EXPECT_TRUE(
      sameDrawing(compose::box()
                      .width(compose::Dim(163))
                      .height(compose::Dim(176))
                      .corners(compose::Corners{8})
                      .padding(16)
                      .clip()
                      .fill(ground)
                      .stroke(compose::stroke(
                          1.0f, edge, compose::PathFormat::Align::Inner))
                      .child(subject()),
                  kit::well({.width = compose::Dim(163),
                             .height = compose::Dim(176),
                             .ground = ground,
                             .padding = 16,
                             .corners = 8,
                             .keyline = edge},
                            compose::box().child(subject()))));
}

/** A plate set tighter down than across, which one distance cannot say. */
TEST(SketchKitCells, APaddingDownOfItsOwn) {
  const Fill ground = Fill::color({0.10f, 0.11f, 0.14f, 1});
  EXPECT_TRUE(sameDrawing(compose::box()
                              .width(compose::Dim(163))
                              .height(compose::Dim(176))
                              .padding(13, 10)
                              .clip()
                              .fill(ground)
                              .child(subject()),
                          kit::well({.width = compose::Dim(163),
                                     .height = compose::Dim(176),
                                     .ground = ground,
                                     .padding = 13,
                                     .paddingY = 10},
                                    compose::box().child(subject()))));
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
                      .width(compose::Dim(163))
                      .height(compose::Dim(176))
                      .clip()
                      .fill(sigil::material::skia::Paint::recipe(quarry))
                      .child(subject()),
                  kit::well({.width = compose::Dim(163),
                             .height = compose::Dim(176),
                             .ground = quarry},
                            compose::box().child(subject()))));
}

/** A well carrying neither draws exactly what it always did. */
TEST(SketchKitCells, AWellWithoutThemDrawsWhatItAlwaysDid) {
  const kit::Theme& house = kit::houseTheme();
  EXPECT_TRUE(sameDrawing(
      compose::kit::well({.width = compose::Dim(163),
                          .height = compose::Dim(176),
                          .ground = Fill::color(house.palette.cellGround),
                          .padding = house.spacing.wellPadding},
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
  Element wide = compose::box()
                     .width(compose::Dim(300))
                     .height(compose::Dim(20))
                     .fill(Fill::color({0.9f, 0.3f, 0.4f, 1}));
  Element narrow = compose::box()
                       .width(compose::Dim(10))
                       .height(compose::Dim(20))
                       .fill(Fill::color({0.9f, 0.3f, 0.4f, 1}));
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
