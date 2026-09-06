/** @file
 * Colour, named: the key, the ramp strip and the chip.
 */

#include <gtest/gtest.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <vector>

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

/** An outlined key: a map whose own marks are outlines cannot be keyed
 *  with filled patches. */
TEST(SketchKitLegend, AnOutlinedSwatchIsNotAFilledOne) {
  const Fill mark = Fill::color({0.4f, 0.9f, 0.55f, 1});
  EXPECT_FALSE(sameDrawing(
      kit::legend({.entries = {{mark, u8"live"}}}),
      kit::legend({.entries = {{mark, u8"live"}}, .strokeWidth = 1.4f})));
}

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
                                .width(compose::Dim(house.spacing.swatchSide))
                                .height(compose::Dim(house.spacing.swatchSide))
                                .fill(warm)
                                .shrink(0))
                     .child(compose::text(u8"lit",
                                          house.style(house.type.captionNote,
                                                      house.palette.ink))));
  EXPECT_TRUE(sameDrawing(std::move(byHand),
                          kit::legend({.entries = {{warm, u8"lit"}}})));
}

/** THE LADDER KEY: a dim body inside a bright edge, the word set in the
 *  colour it names, and the swatch and its word at the caller's own
 *  density. Three facts about ONE entry that a key to a tier ladder
 *  cannot state without them, asserted in pixels against the hand
 *  spelling. */
TEST(SketchKitLegend, AnEntryCanCarryItsOwnEdgeAndItsOwnInk) {
  const kit::Theme& house = kit::houseTheme();
  const SkColor4f rare{0.98f, 0.86f, 0.32f, 1};
  const Fill body =
      Fill::color({rare.fR * 0.35f, rare.fG * 0.35f, rare.fB * 0.35f, 1});
  Element byHand =
      compose::box()
          .column()
          .gap(house.spacing.rowGap)
          .alignItems(compose::Align::Start)
          .child(compose::box()
                     .row()
                     .alignItems(compose::Align::Center)
                     .gap(6)
                     .child(compose::box()
                                .width(compose::Dim(9))
                                .height(compose::Dim(9))
                                .fill(body)
                                .shrink(0)
                                .corners(compose::Corners{1.5f})
                                .foreground(
                                    compose::stroke(1.0f, Fill::color(rare))))
                     .child(compose::text(
                         u8"rare", house.style(house.type.captionNote, rare))));
  Element byKit = kit::legend(
      {.entries = {{body, u8"rare", {}, Fill::color(rare), Fill::color(rare)}},
       .swatchSide = 9,
       .corners = 1.5f,
       .labelGap = 6});
  EXPECT_TRUE(sameDrawing(std::move(byHand), std::move(byKit)));
}

/** An entry with no ink of its own is still the theme's ink, and an
 *  entry with no keyline still draws a flat patch — so the two fields
 *  cost nothing to a key that does not want them. */
TEST(SketchKitLegend, AnEntryWithoutThemDrawsWhatItAlwaysDid) {
  const Fill warm = Fill::color({0.9f, 0.6f, 0.3f, 1});
  EXPECT_TRUE(sameDrawing(
      kit::legend({.entries = {{warm, u8"lit"}}}),
      kit::legend(
          {.entries = {{warm, u8"lit", {}, std::nullopt, std::nullopt}}})));
  EXPECT_FALSE(sameDrawing(
      kit::legend({.entries = {{warm, u8"lit"}}}),
      kit::legend(
          {.entries = {{warm, u8"lit", {}, Fill::color({1, 1, 1, 1})}}})));
}

/** A KEY'S MARK IS WHATEVER THE CALLER DREW: a quarried sample at its own
 *  two dimensions with its own edge stands where the swatch would, and
 *  none of the swatch's dressing is read for it. */
TEST(SketchKitLegend, AnEntrysMarkIsWhateverTheCallerDrew) {
  const kit::Theme& house = kit::houseTheme();
  Element sample = compose::box()
                       .width(compose::Dim(20))
                       .height(compose::Dim(13))
                       .fill(Fill::color({0.45f, 0.29f, 0.29f, 1}))
                       .foreground(compose::stroke(
                           1.0f, Fill::color({0.87f, 0.84f, 0.77f, 0.55f})));
  Element byHand =
      compose::box()
          .column()
          .gap(house.spacing.rowGap)
          .alignItems(compose::Align::Start)
          .child(compose::box()
                     .row()
                     .alignItems(compose::Align::Center)
                     .gap(house.spacing.captionNoteGap)
                     .child(sample)
                     .child(compose::text(u8"porphyry",
                                          house.style(house.type.captionNote,
                                                      house.palette.ink))));
  EXPECT_TRUE(sameDrawing(
      std::move(byHand),
      kit::legend({.entries = {{.label = u8"porphyry", .mark = sample}}})));
}

/** A strip that names only its ends keeps the unnamed steps butted, so
 *  the ramp reads as one band rather than as a row of tiles. */
TEST(SketchKitLegend, AStripNamesTheStepsItHasWordsFor) {
  std::vector<kit::Ground> steps;
  for (int i = 0; i < 4; ++i)
    steps.push_back(Fill::color({0.2f * (float)i, 0.3f, 0.4f, 1}));
  EXPECT_FALSE(sameDrawing(kit::swatchStrip({.swatches = steps,
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
          .child(compose::text(u8"PINNED", house.style(house.type.eyebrow,
                                                       house.palette.ground)));
  EXPECT_TRUE(sameDrawing(std::move(byHand), kit::chip({.label = u8"PINNED"})));
}

}  // namespace
