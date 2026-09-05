/** @file
 * The plot: relative turns replayed anywhere, clockwise on the y-down
 * canvas.
 */

#include <gtest/gtest.h>
#include <sigildraw/Math.h>
#include <sigildraw/brush/Plot.h>

#include <array>
#include <cmath>

namespace {

namespace brush = sigil::draw::brush;
using namespace sigil::draw;

TEST(Plot, ReplaysItsTurnsFromAnyOriginAtAnyScale) {
  brush::Plot plot(brush::PlotType::Segments);
  plot.addSegment(0.0f, 20.0f, 0.4f);
  plot.addSegment(HALF_PI, 10.0f, 0.8f);
  plot.endPlot(HALF_PI, 0.2f);
  brush::Stroke path = plot.path({5, 6});
  ASSERT_EQ(path.size(), 3u);
  EXPECT_NEAR(path.back().position.fX, 25.0f, 1e-5f);
  EXPECT_NEAR(path.back().position.fY, 16.0f, 1e-5f);
  EXPECT_FLOAT_EQ(path.back().pressure, 0.2f);
  EXPECT_NEAR(plot.length(), 30.0f, 1e-5f);

  plot.rotate(HALF_PI);
  path = plot.path({0, 0});
  EXPECT_NEAR(path[1].position.fX, 0.0f, 1e-5f);
  EXPECT_NEAR(path[1].position.fY, 20.0f, 1e-5f);
  const brush::Stroke scaled = plot.path({0, 0}, 1.0f, 0.5f, 2.0f);
  EXPECT_NEAR(scaled[1].position.fY, 40.0f, 1e-5f);
}

TEST(Plot, InterpolatesAnglesAcrossTheWrapAndTurnsWithRotate) {
  brush::Plot wrapped(brush::PlotType::Curve);
  wrapped.addSegment(radians(170.0f), 10.0f);
  wrapped.endPlot(radians(-170.0f));
  EXPECT_NEAR(std::abs(wrapped.angle(5.0f)), PI, 1e-5f);
  wrapped.rotate(radians(90.0f));
  EXPECT_NEAR(std::remainder(wrapped.angle(0.0f), TWO_PI), radians(-100.0f),
              1e-5f);
  wrapped.rotate(radians(180.0f));
  EXPECT_NEAR(std::remainder(wrapped.angle(0.0f), TWO_PI), radians(-10.0f),
              1e-5f);
}

TEST(Plot, FromStrokeIsRelativeSoTheCallerPlacesAndScalesIt) {
  const std::array<brush::Sample, 2> absolute{{{{4, 7}, 1.0f}, {{14, 7}, 1.0f}}};
  const brush::Plot placed =
      brush::Plot::fromStroke(absolute, brush::PlotType::Segments);
  EXPECT_FALSE(placed.empty());
  const brush::Stroke moved = placed.path({50, 50}, 1.0f, 0.5f, 3.0f);
  ASSERT_EQ(moved.size(), 2u);
  EXPECT_EQ(moved.front().position, SkPoint::Make(50, 50));
  EXPECT_NEAR(moved.back().position.fX, 80.0f, 1e-5f);
  EXPECT_NEAR(moved.back().position.fY, 50.0f, 1e-5f);
  const brush::Polygon polygon = placed.polygon(1, 2, 1.0f, 0.5f, 1.0f);
  ASSERT_EQ(polygon.vertices.size(), 2u);
  EXPECT_EQ(polygon.vertices.front(), SkPoint::Make(1, 2));
}

}  // namespace
