/** @file
 * Hatching: clipped to its polygon, even-odd across a collection, and
 * its angle in radians whatever the pen's mode.
 */

#include <gtest/gtest.h>
#include <sigildraw/Draw.h>
#include <sigildraw/brush/Engine.h>
#include <sigildraw/brush/Hatch.h>
#include <sigildraw/brush/Polygon.h>

#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "Recorder.h"
#include "support/Paper.h"

namespace {

namespace brush = sigil::draw::brush;
using namespace sigil::draw;
using sigil::draw::brush::testing::Recording;
using sigil::draw::brush::testing::recorder;
using sigil::draw::testing::Paper;

TEST(Hatch, StaysInsideItsPolygon) {
  Paper paper(120, 100, SK_ColorWHITE);
  paper.begin();
  paper.pen.randomSeed(9);
  brush::Tool tool = brush::marker(SkColors::kBlack, 2.5f);
  tool.opacity = 1.0f;
  tool.bristles = 1;
  tool.scatter = 0.0f;
  tool.pressure = {1, 1, 1};
  const std::vector<SkPoint> polygon = {{25, 20}, {98, 28}, {88, 82}, {18, 72}};
  brush::hatch(paper.pen, tool, polygon, {.spacing = 7.0f, .angle = 0.35f});
  paper.end();

  const SkBitmap pixels = paper.pixels();
  int interiorInk = 0;
  int distantInk = 0;
  for (int y = 0; y < pixels.height(); ++y) {
    for (int x = 0; x < pixels.width(); ++x) {
      if (pixels.getColor(x, y) == SK_ColorWHITE) continue;
      if (x > 30 && x < 80 && y > 30 && y < 65) ++interiorInk;
      if (x < 8 || x > 112 || y < 8 || y > 92) ++distantInk;
    }
  }
  EXPECT_GT(interiorInk, 100);
  EXPECT_EQ(distantInk, 0);
}

TEST(Hatch, ArraysUseInnerPolygonsAsEvenOddHoles) {
  const std::array<brush::Polygon, 2> polygons{
      brush::Polygon({{10, 10}, {90, 10}, {90, 90}, {10, 90}}),
      brush::Polygon({{35, 35}, {65, 35}, {65, 65}, {35, 65}})};
  Paper paper(100, 100, SK_ColorWHITE);
  paper.begin();
  paper.pen.randomSeed(22);
  brush::Tool tool = brush::marker(SkColors::kBlack, 2.0f);
  tool.opacity = 1.0f;
  tool.bristles = 1;
  tool.scatter = 0.0f;
  tool.pressure = {1, 1, 1};
  brush::hatchArray(paper.pen, tool, polygons, {.spacing = 5.0f, .angle = 0.0f});
  paper.end();

  SkBitmap pixels = paper.pixels();
  int outerInk = 0;
  int holeInk = 0;
  for (int y = 15; y < 85; ++y) {
    for (int x = 15; x < 85; ++x) {
      if (pixels.getColor(x, y) == SK_ColorWHITE) continue;
      if (x > 37 && x < 63 && y > 37 && y < 63)
        ++holeInk;
      else
        ++outerInk;
    }
  }
  EXPECT_GT(outerInk, 100);
  EXPECT_EQ(holeInk, 0);

  const std::array<brush::Polygon, 2> islands{
      brush::Polygon({{5, 5}, {30, 5}, {30, 30}, {5, 30}}),
      brush::Polygon({{70, 70}, {95, 70}, {95, 95}, {70, 95}})};
  paper.surface->getCanvas()->clear(SK_ColorWHITE);
  paper.begin();
  paper.pen.randomSeed(22);
  brush::hatchArray(paper.pen, tool, islands, {.spacing = 3.0f, .angle = HALF_PI});
  paper.end();
  pixels = paper.pixels();
  bool secondIslandPainted = false;
  for (int y = 70; y < 95; ++y)
    for (int x = 70; x < 95; ++x)
      secondIslandPainted |= pixels.getColor(x, y) != SK_ColorWHITE;
  EXPECT_TRUE(secondIslandPainted);
}

/** The heading of the first mark a hatch lays through a square, as the
 *  tool records it. */
float firstMarkHeading(brush::Engine& engine, Pen& pen,
                       const std::shared_ptr<Recording>& recording) {
  recording->dabs.clear();
  const brush::Polygon square({{10, 10}, {90, 10}, {90, 90}, {10, 90}});
  engine.hatch(pen, square);
  EXPECT_GT(recording->dabs.size(), 1u);
  return recording->dabs.empty() ? 0.0f : recording->dabs.front().direction;
}

TEST(Hatch, TheDefaultAngleIsAQuarterTurnWhateverThePensMode) {
  brush::Engine engine;
  auto recording = std::make_shared<Recording>();
  ASSERT_NE(engine.add("recorder", recorder(recording)), nullptr);
  ASSERT_NE(engine.hatchStyle("recorder", SkColors::kBlack), nullptr);
  Paper paper(100, 100, SK_ColorWHITE);
  paper.begin();
  paper.pen.angleMode(DEGREES);

  engine.hatch(brush::Hatch{});
  const float byDefault = firstMarkHeading(engine, paper.pen, recording);
  EXPECT_NEAR(std::abs(std::tan(byDefault)), 1.0f, 0.05f);

  engine.hatch(brush::Hatch{.spacing = 12.0f, .angle = HALF_PI});
  const float byValue = firstMarkHeading(engine, paper.pen, recording);
  EXPECT_NEAR(std::abs(std::cos(byValue)), 0.0f, 0.05f);

  engine.hatch(paper.pen, 12.0f, 90.0f);
  const float byScalar = firstMarkHeading(engine, paper.pen, recording);
  EXPECT_NEAR(std::abs(std::cos(byScalar)), 0.0f, 0.05f);
  paper.end();
}

}  // namespace
