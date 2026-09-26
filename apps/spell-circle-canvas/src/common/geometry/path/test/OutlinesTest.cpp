/** @file
 * The 2D answer type and the general operations over it: an outline
 * measured, cut, combined and carried through a transform; the path
 * through points; a rail and a band at a width law; points placed by a
 * pattern; the radial arrangement; the projection seam; and the
 * constrained triangulation a flat fill stands on.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "sigilgeometry/path/Offset.h"
#include "sigilgeometry/path/Outline.h"
#include "sigilgeometry/path/Points.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Projection.h"
#include "sigilgeometry/path/Radial.h"
#include "sigilgeometry/path/Through.h"
#include "sigilgeometry/path/Transform.h"
#include "sigilgeometry/path/Triangulate.h"

using namespace sigil::geometry::path;

namespace {

const Outline kSquare = Outline::rectangle(Rect::of({0, 0}, {100, 100}));

::testing::AssertionResult near(glm::vec2 a, glm::vec2 b, float tolerance) {
  const float gap = std::hypot(a.x - b.x, a.y - b.y);
  if (gap <= tolerance) return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure()
         << "(" << a.x << ", " << a.y << ") vs (" << b.x << ", " << b.y
         << ") — " << gap << " apart";
}

TEST(Outline, IsMeasuredByDistanceAlongEveryContour) {
  EXPECT_NEAR(kSquare.length(), 400.0f, 1e-3f);
  EXPECT_TRUE(near(kSquare.pointAt(150), {100, 50}, 1e-3f));
  EXPECT_TRUE(near(kSquare.tangentAt(150), {0, 1}, 1e-4f));
  // The normal is the tangent turned toward +y: to the right of travel.
  EXPECT_TRUE(near(kSquare.normalAt(150), {-1, 0}, 1e-4f));
  // Past the end a closed outline comes round when asked to.
  EXPECT_TRUE(near(kSquare.pointAt(450, Wrap::Around), {50, 0}, 1e-3f));
  EXPECT_TRUE(near(kSquare.pointAt(450), {0, 0}, 1e-3f));
}

TEST(Outline, CutsSplitsAndFindsTheNearestPoint) {
  EXPECT_NEAR(kSquare.segment(50, 250).length(), 200.0f, 1e-2f);
  const auto [before, after] = kSquare.split(100);
  EXPECT_NEAR(before.length(), 100.0f, 1e-2f);
  EXPECT_NEAR(after.length(), 300.0f, 1e-2f);
  const Nearest nearest = kSquare.nearest({130, 40});
  EXPECT_TRUE(near(nearest.position, {100, 40}, 0.1f));
  EXPECT_NEAR(nearest.distance, 140.0f, 0.1f);
  EXPECT_NEAR(nearest.gap, 30.0f, 0.1f);
}

TEST(Outline, CombinesContainsAndTransformsAsAValue) {
  const Outline shifted =
      kSquare.transformed(Transform::translate({50, 0}));
  EXPECT_NEAR(kSquare.united(shifted).area(), 15000.0f, 1.0f);
  EXPECT_NEAR(kSquare.intersected(shifted).area(), 5000.0f, 1.0f);
  EXPECT_NEAR(kSquare.subtracted(shifted).area(), 5000.0f, 1.0f);
  EXPECT_NEAR(kSquare.excluded(shifted).area(), 10000.0f, 1.0f);
  EXPECT_TRUE(kSquare.contains({50, 50}));
  EXPECT_FALSE(kSquare.contains({150, 50}));
  EXPECT_EQ(kSquare.winding(), Winding::OutersClockwise);
  EXPECT_EQ(kSquare.reversed().winding(), Winding::OutersCounterClockwise);
  EXPECT_NEAR(kSquare.joined(shifted).length(), 800.0f, 1e-2f);
  // Equal outlines are the same verbs through the same points.
  EXPECT_EQ(kSquare, Outline::rectangle(Rect::of({0, 0}, {100, 100})));
  EXPECT_NE(kSquare, shifted);
}

TEST(Outline, ResamplesByCountOrBySpacing) {
  const std::vector<Polyline> counted = kSquare.resampled({.count = 8});
  ASSERT_EQ(counted.size(), 1u);
  EXPECT_EQ(counted[0].points.size(), 8u);
  const std::vector<Polyline> spaced = kSquare.resampled({.spacing = 10});
  ASSERT_EQ(spaced.size(), 1u);
  EXPECT_GE(spaced[0].points.size(), 40u);
}

TEST(Transform, ComposesRightToLeftAndInverts) {
  const Transform turn = Transform::rotate(90, {10, 10});
  EXPECT_TRUE(near(turn({20, 10}), {10, 20}, 1e-4f));  // clockwise on screen
  const Transform both = Transform::translate({5, 0}) * Transform::scale({2, 2});
  EXPECT_TRUE(near(both({1, 1}), {7, 2}, 1e-5f));
  EXPECT_TRUE(near(both.inverse()(both({3, 4})), {3, 4}, 1e-4f));
  const Transform fitting =
      Transform::fit(Rect::of({0, 0}, {10, 20}), Rect::of({0, 0}, {100, 100}), true);
  EXPECT_TRUE(near(fitting({10, 20}), {75, 100}, 1e-3f));
}

TEST(Through, JoinsPointsStraightOrPassingThroughThem) {
  const std::vector<glm::vec2> points{{0, 0}, {50, 40}, {100, 0}};
  const Outline straight = through(points);
  EXPECT_NEAR(straight.length(), 2.0f * std::hypot(50.0f, 40.0f), 1e-2f);
  const Outline smooth = curveThrough(points);
  EXPECT_LT(smooth.nearest({50, 40}).gap, 1e-2f);  // it passes through
  EXPECT_GT(smooth.length(), straight.length() - 1e-2f);
  EXPECT_TRUE(through(std::vector<glm::vec2>{{0, 0}}).empty());
  const Outline closed = through(points, {.closed = true});
  EXPECT_NEAR(closed.length(),
              2.0f * std::hypot(50.0f, 40.0f) + 100.0f, 1e-2f);
}

TEST(Offset, WalksARailOrGrowsTheRegion) {
  const Outline line = through(std::vector<glm::vec2>{{0, 0}, {100, 0}});
  const Rect rail = offset(line, 10).bounds();
  EXPECT_NEAR(std::abs(rail.min.y), 10.0f, 0.5f);
  const Outline grown = offset(kSquare, 10, {.region = true});
  EXPECT_NEAR(grown.bounds().width(), 120.0f, 0.5f);
  const Outline narrowing = band(line, Profile{{0, 20}, {1, 0}});
  EXPECT_NEAR(narrowing.bounds().height(), 20.0f, 0.5f);
  // Stops read between as asked.
  const Profile stepped({{0, 4}, {0.5f, 8}}, {.between = Between::Step});
  EXPECT_FLOAT_EQ(stepped.across(0.25f), 4.0f);
  EXPECT_FLOAT_EQ(stepped.across(0.75f), 8.0f);
}

TEST(Points, PlacesByEveryPattern) {
  EXPECT_EQ(points(kSquare, random(40)).size(), 40u);
  for (const glm::vec2 p : points(kSquare, grid(10)))
    EXPECT_TRUE(kSquare.contains(p));
  EXPECT_EQ(points(kSquare, along(100)).size(), 5u);  // both ends of the run
  const std::vector<glm::vec2> seeds =
      points(Rect::of({0, 0}, {100, 100}),
             radial(200, {.stepDegrees = 137.508f, .growth = Growth::SquareRoot}));
  ASSERT_EQ(seeds.size(), 200u);
  for (const glm::vec2 p : seeds)
    EXPECT_LE(std::hypot(p.x - 50, p.y - 50), 50.0f + 1e-3f);
  EXPECT_NEAR(heading({0, 1}), 90.0f, 1e-4f);
}

TEST(Radial, DealsLoopsChordsAndMarks) {
  const PolarFrame frame{.centre = {0, 0}, .radius = 100};
  const std::vector<glm::vec2> hexagon = radialPoints(6, {}, frame);
  ASSERT_EQ(hexagon.size(), 6u);
  EXPECT_TRUE(near(hexagon[0], {0, -100}, 1e-3f));  // first vertex up
  EXPECT_TRUE(near(hexagon[1], {86.6025f, -50}, 1e-3f));  // clockwise
  const Outline chords =
      radialOutline(6, {.skip = 2, .connect = Connect::Each}, frame);
  EXPECT_NEAR(chords.length(), 6 * 100 * std::sqrt(3.0f), 0.5f);
  const Outline ticks = radialOutline(
      12, {.connect = Connect::None, .marks = {Mark::line(0.9f, 1.0f)}}, frame);
  EXPECT_NEAR(ticks.length(), 12 * 10.0f, 1e-2f);
  const Outline stars = radialOutline(
      4, {.connect = Connect::None,
          .marks = {Mark::shape(Outline::rectangle(Rect::of({0, 0}, {4, 4})))}},
      frame);
  EXPECT_NEAR(stars.area(), 4 * 16.0f, 1e-2f);
}

TEST(Projection, IsASeamWithAGnomonicAndACustomDoor) {
  static_assert(ProjectionScheme<Projection>);
  static_assert(ProjectionScheme<CustomProjection>);
  const Projection sundial = projection::gnomonic({.centre = {0, 90}, .scale = 100});
  EXPECT_NEAR(sundial.radiusAt(45), 100.0f, 1e-3f);
  EXPECT_NEAR(sundial.arcAtRadius(100), 45.0f, 1e-3f);
  const Spherical back = sundial.from(sundial.at({30, 60}));
  EXPECT_NEAR(back.lonDeg, 30.0f, 1e-2f);
  EXPECT_NEAR(back.latDeg, 60.0f, 1e-2f);
  const CustomProjection flat = projection::custom(
      "flat", [](Spherical s) { return glm::vec2{s.lonDeg, s.latDeg}; },
      [](glm::vec2 p) { return Spherical{p.x, p.y}; });
  EXPECT_TRUE(near(flat.at({10, 20}), {10, 20}, 0));
  EXPECT_EQ(flat, projection::custom("flat", nullptr, nullptr));
}

TEST(Triangulate, KeepsTheHole) {
  const Outline ring = Outline::rectangle(Rect::of({0, 0}, {100, 100}))
                           .joined(Outline::rectangle(Rect::of({25, 25}, {50, 50})))
                           .withFillRule(FillRule::EvenOdd);
  const Triangulation inside = triangulate(ring);
  // Eight vertices, two rings: n + 2h - 2 triangles.
  EXPECT_EQ(inside.points.size(), 8u);
  EXPECT_EQ(inside.triangles.size(), 8u);
  float area = 0;
  for (const glm::uvec3 t : inside.triangles) {
    const glm::vec2 a = inside.points[t.x], b = inside.points[t.y],
                    c = inside.points[t.z];
    area += std::abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) * 0.5f;
  }
  EXPECT_NEAR(area, 10000.0f - 2500.0f, 1e-2f);
}

}  // namespace
