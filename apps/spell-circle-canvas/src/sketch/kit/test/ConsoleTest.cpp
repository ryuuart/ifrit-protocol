/** @file
 * The log panel a study prints into.
 */

#include <gtest/gtest.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/kit/Feed.h>
#include <sigilcompose/kit/Plate.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

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
                 .styles =
                     compose::kit::tinted({.face = house.type.mono,
                                           .size = house.type.captionLabel.size,
                                           .ink = house.palette.ink},
                                          {})},
       .plate = {.paddingHorizontal = house.spacing.panelPadding,
                 .paddingVertical = house.spacing.panelPadding * 0.6f,
                 .gap = house.spacing.labelGap,
                 .fill = Fill::color(house.palette.cellGround),
                 .border = border,
                 .divider = border}});
  // The console's voice is the `.console` rule of the sheet it stands
  // under; the plate spelled by hand carries whole styles and reads none.
  EXPECT_TRUE(sameDrawing(std::move(byHand.applyStyleSheet(house.styleSheet())),
                          std::move(kit::console({.feeds = {&rows}})
                                        .applyStyleSheet(house.styleSheet()))));
}

}  // namespace
