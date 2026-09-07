/** @file
 * The mass gesture: inside its surface, and across a collection.
 */

#include <gtest/gtest.h>
#include <sigildraw/Draw.h>
#include <sigildraw/brush/Engine.h>
#include <sigildraw/brush/Mass.h>
#include <sigildraw/brush/Polygon.h>

#include <array>
#include <vector>

#include "support/Paper.h"

namespace {

namespace brush = sigil::draw::brush;
using namespace sigil::draw;
using sigil::draw::testing::Paper;

TEST(Mass, StaysInsideItsSurface) {
  Paper paper(120, 100, SK_ColorWHITE);
  paper.begin();
  paper.pen.randomSeed(88);
  const std::vector<SkPoint> polygon = {{30, 20}, {94, 28}, {86, 78}, {24, 70}};
  brush::Tool tool = brush::pencil(SkColors::kBlack, 2.0f);
  brush::mass(paper.pen, tool, polygon,
              {.precision = 0.35f,
               .strength = 0.4f,
               .gradient = 0.2f,
               .outline = false});
  paper.end();

  const SkBitmap pixels = paper.pixels();
  EXPECT_NE(pixels.getColor(60, 50), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(4, 4), SK_ColorWHITE);
}

TEST(Mass, ArraysLeaveTheHoleEmptyAndTheEngineClipsThem) {
  const std::array<brush::Polygon, 2> polygons{
      brush::Polygon({{10, 10}, {110, 10}, {110, 110}, {10, 110}}),
      brush::Polygon({{40, 40}, {80, 40}, {80, 80}, {40, 80}})};
  Paper paper(120, 120, SK_ColorWHITE);
  paper.begin();
  paper.pen.randomSeed(5);
  paper.pen.noiseSeed(5);
  brush::Tool tool = brush::marker(SkColors::kBlack, 1.5f);
  tool.opacity = 1.0f;
  tool.scatter = 0.0f;
  tool.markerTip = false;
  tool.pressure = {1, 1, 1};

  brush::Engine engine;
  ASSERT_NE(engine.add("fine", tool), nullptr);
  ASSERT_NE(engine.mass("fine", SkColors::kBlack,
                        {.precision = 1.0f, .strength = 1.0f}),
            nullptr);
  engine.clip(SkRect::MakeLTRB(0, 0, 60, 120));
  engine.massArray(paper.pen, polygons);
  paper.end();

  const SkBitmap pixels = paper.pixels();
  int leftInk = 0;
  int rightInk = 0;
  int holeInk = 0;
  for (int y = 12; y < 108; ++y) {
    for (int x = 12; x < 108; ++x) {
      if (pixels.getColor(x, y) == SK_ColorWHITE) continue;
      if (x > 43 && x < 77 && y > 43 && y < 77)
        ++holeInk;
      else if (x < 60)
        ++leftInk;
      else
        ++rightInk;
    }
  }
  EXPECT_GT(leftInk, 50);
  EXPECT_EQ(rightInk, 0);
  EXPECT_EQ(holeInk, 0);
}

}  // namespace
