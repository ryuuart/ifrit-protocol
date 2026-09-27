// path/Arrange.h — where item i of n goes: the run, the ring with its
// heading, the grid of modules; and the polar frame and the radial
// arrangement answering through it.

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "sigilgeometry/path/Arrange.h"
#include "sigilgeometry/path/Frame.h"
#include "sigilgeometry/path/Radial.h"

namespace arrange = sigil::geometry::arrange;
using sigil::geometry::path::PolarFrame;

TEST(Arrange, ARunDividesClosedAndOpenExtents) {
  EXPECT_FLOAT_EQ(arrange::step(360, 4, arrange::Turn::Closed), 90);
  EXPECT_FLOAT_EQ(arrange::step(360, 4, arrange::Turn::Open), 120);
  EXPECT_FLOAT_EQ(arrange::step(360, 1, arrange::Turn::Open), 0);
  EXPECT_FLOAT_EQ(arrange::along(10, 90, 3, 4, arrange::Turn::Open), 100);
}

TEST(Arrange, ARingStartsAtTwelveAndTurnsClockwise) {
  const arrange::Ring ring{.center = {100, 50}, .radii = {80, 40}};
  const glm::vec2 top = arrange::onRing(0, 4, ring);
  EXPECT_NEAR(top.x, 100, 1e-4f);
  EXPECT_NEAR(top.y, 10, 1e-4f);
  const arrange::Placement right = arrange::placeOnRing(1, 4, ring);
  EXPECT_NEAR(right.position.x, 180, 1e-4f);
  EXPECT_NEAR(right.position.y, 50, 1e-4f);
  // Standing along its spoke: upright at twelve, a quarter turned at three.
  EXPECT_NEAR(arrange::placeOnRing(0, 4, ring).headingDegrees, 0, 1e-4f);
  EXPECT_NEAR(right.headingDegrees, 90, 1e-4f);
  // A fan reaches both ends of its sweep.
  const arrange::Ring fan{.radii = {1, 1},
                          .fromDegrees = 180,
                          .sweepDegrees = 180,
                          .turn = arrange::Turn::Open};
  EXPECT_NEAR(arrange::onRing(2, 3, fan).x, 1, 1e-6f);
}

TEST(Arrange, AHeadingFollowsTheDirectionOfTravel) {
  EXPECT_NEAR(arrange::heading({0, 1}), 90.0f, 1e-4f);
  EXPECT_EQ(arrange::heading({0, 0}), 0.0f);
  EXPECT_NEAR(arrange::placeAlong({3, 4}, {-1, 0}).headingDegrees, 180.0f,
              1e-4f);
}

TEST(Arrange, APolarFrameResolvesThroughTheSameEllipse) {
  // A frame owns its convention only: its point for an angle is the
  // arrangement's point at the screen angle the convention names.
  const PolarFrame frame{.centre = {40, 60}, .radius = 25};
  for (float degrees : {0.0f, 33.0f, 200.0f}) {
    const glm::vec2 expected = arrange::onEllipse(
        frame.centre, glm::vec2(25.0f), frame.screenRadians(degrees));
    EXPECT_EQ(frame.at(degrees), expected);
  }
}

TEST(Arrange, RadialVerticesInABoxAreTheOutlinesOwn) {
  // Seven vertices on an oblong box stand where the outline drawn in the
  // same box turns its corners.
  const std::vector<glm::vec2> vertices =
      sigil::geometry::path::radialPoints(7, {}, glm::vec2{300, 180});
  ASSERT_EQ(vertices.size(), 7u);
  EXPECT_NEAR(vertices[0].x, 150, 1e-4f);
  EXPECT_NEAR(vertices[0].y, 0, 1e-4f);
  for (const glm::vec2 vertex : vertices) {
    const float x = (vertex.x - 150) / 150, y = (vertex.y - 90) / 90;
    EXPECT_NEAR(x * x + y * y, 1.0f, 1e-4f);
  }
}

TEST(Arrange, ACellBlockSwallowsTheGapsItCrosses) {
  EXPECT_EQ(arrange::cellAt(7, 3), (arrange::Cell{1, 2}));
  const glm::vec2 module = arrange::moduleSize({100, 50}, 4, 2, {4, 2});
  EXPECT_FLOAT_EQ(module.x, 22);
  EXPECT_FLOAT_EQ(module.y, 24);
  const auto rect = arrange::cellRect(
      {1, 1}, module, {.gap = {4, 2}, .origin = {10, 10}, .columnSpan = 2});
  EXPECT_FLOAT_EQ(rect.min.x, 36);
  EXPECT_FLOAT_EQ(rect.min.y, 36);
  EXPECT_FLOAT_EQ(rect.width(), 48);
  EXPECT_FLOAT_EQ(rect.height(), 24);
}
