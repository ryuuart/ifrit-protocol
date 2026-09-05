/** @file
 * The cursor: through its field, through a plot, and inside its bounds.
 */

#include <gtest/gtest.h>
#include <sigildraw/Draw.h>
#include <sigildraw/brush/Engine.h>
#include <sigildraw/brush/Plot.h>
#include <sigildraw/brush/Position.h>

#include "support/Paper.h"

namespace {

namespace brush = sigil::draw::brush;
using namespace sigil::draw;
using sigil::draw::testing::Paper;

TEST(Position, WalksAndRemembersHowFar) {
  brush::Position cursor(2, 3);
  const brush::Stroke moved = cursor.moveTo(0.0f, 9.0f, 2.0f);
  EXPECT_EQ(moved.size(), 6u);
  EXPECT_FLOAT_EQ(cursor.x(), 11.0f);
  EXPECT_FLOAT_EQ(cursor.y(), 3.0f);
  EXPECT_FLOAT_EQ(cursor.plotted(), 9.0f);
  cursor.reset();
  EXPECT_FLOAT_EQ(cursor.plotted(), 0.0f);

  brush::Position down(4, 5);
  const brush::Stroke turned = down.moveTo(HALF_PI, 10.0f, 2.0f);
  EXPECT_GT(turned.size(), 1u);
  EXPECT_NEAR(down.x(), 4.0f, 1e-5f);
  EXPECT_NEAR(down.y(), 15.0f, 1e-5f);
}

TEST(Position, TheFieldTurnsEveryStep) {
  brush::Engine engine;
  ASSERT_TRUE(engine.addField("down", [](SkPoint, float) { return HALF_PI; }));
  ASSERT_TRUE(engine.field("down"));
  brush::Position cursor = engine.position(3, 4);
  const brush::Stroke first = cursor.moveTo(0.0f, 8.0f, 2.0f);
  const brush::Stroke second = cursor.moveTo(0.0f, 4.0f, 2.0f);
  EXPECT_GT(first.size(), 1u);
  EXPECT_GT(second.size(), 1u);
  EXPECT_NEAR(cursor.x(), 3.0f, 1e-4f);
  EXPECT_NEAR(cursor.y(), 16.0f, 1e-4f);
  EXPECT_NEAR(cursor.angle(), HALF_PI, 1e-6f);

  brush::Engine degreeField;
  ASSERT_TRUE(degreeField.addField(
      "down-degrees", [](SkPoint, float) { return 90.0f; }, DEGREES));
  ASSERT_TRUE(degreeField.field("down-degrees"));
  EXPECT_NEAR(degreeField.position().angle(), HALF_PI, 1e-6f);
}

TEST(Position, StopsOutsideTheCanvasAndFollowsAScaledPlot) {
  brush::Engine engine;
  Paper paper(100, 80);
  paper.begin();
  brush::Position bounded = engine.position(paper.pen, 10, 10);
  EXPECT_TRUE(bounded.isIn());
  bounded.place(-60, 10);
  EXPECT_FALSE(bounded.isInCanvas());
  const brush::Stroke stopped = bounded.moveTo(0.0f, 20.0f);
  EXPECT_EQ(stopped.size(), 1u);
  EXPECT_FLOAT_EQ(bounded.x(), -60.0f);
  EXPECT_FLOAT_EQ(bounded.plotted(), 1.0f);
  paper.end();

  brush::Plot plot(brush::PlotType::Segments);
  plot.addSegment(0.0f, 20.0f);
  plot.endPlot(0.0f);
  brush::Position plotting(10, 10);
  const brush::Stroke plotted = plotting.plotTo(plot, 10.0f, 2.0f, 2.0f);
  EXPECT_GT(plotted.size(), 1u);
  EXPECT_FLOAT_EQ(plotting.x(), 20.0f);
  EXPECT_FLOAT_EQ(plotting.plotted(), 5.0f);
}

}  // namespace
