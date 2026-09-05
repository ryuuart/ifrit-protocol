/** @file
 * The wash: a textured translucent interior, composited once.
 */

#include <gtest/gtest.h>
#include <sigildraw/Draw.h>
#include <sigildraw/brush/Wash.h>

#include <vector>

#include "support/Paper.h"

namespace {

namespace brush = sigil::draw::brush;
using namespace sigil::draw;
using sigil::draw::testing::Paper;

TEST(Wash, BuildsATexturedTranslucentInterior) {
  Paper paper(100, 100, SK_ColorWHITE);
  paper.begin();
  paper.pen.randomSeed(12);
  paper.pen.noiseSeed(12);
  const std::vector<SkPoint> polygon = {{24, 22}, {78, 27}, {82, 75}, {20, 80}};
  brush::wash(paper.pen,
              {.color = SkColors::kBlue,
               .opacity = 0.5f,
               .bleed = 0.3f,
               .texture = 0.4f,
               .border = 0.3f,
               .layers = 12},
              polygon);
  paper.end();

  const SkBitmap pixels = paper.pixels();
  EXPECT_NE(pixels.getColor(50, 50), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(2, 2), SK_ColorWHITE);
}

TEST(Wash, RestoresThePenAndLeavesNoLayerOpen) {
  Paper paper(100, 100, SK_ColorWHITE);
  paper.begin();
  paper.pen.fill(255, 0, 0);
  paper.pen.noStroke();
  const int saves = paper.pen.canvas()->getSaveCount();
  const std::vector<SkPoint> polygon = {{20, 20}, {80, 20}, {80, 80}, {20, 80}};
  brush::wash(paper.pen, {.color = SkColors::kBlue, .opacity = 0.9f}, polygon);
  EXPECT_EQ(paper.pen.canvas()->getSaveCount(), saves);
  paper.pen.rect(0, 90, 10, 10);
  paper.end();
  EXPECT_EQ(paper.pixel(5, 95), SK_ColorRED);
}

}  // namespace
