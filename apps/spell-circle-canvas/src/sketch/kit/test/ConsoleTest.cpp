/** @file
 * The log panel a study prints into.
 */

#include <gtest/gtest.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Feed.h>
#include <sigilcompose/kit/Plate.h>
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

}  // namespace
