/** @file
 * The stepper: a fixed step is the same run twice, gravity lands on the
 * closed form, a pinned pair settles on its rest length, a band does
 * nothing until it is taut, a constraint takes the speed it took, and a
 * flock keeps every bird it started with.
 */

#include <gtest/gtest.h>
#include <sigilmotion/physics/Physics.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace sigil::motion::physics;

namespace {

/** How far apart two points are. */
float apart(const Points& points, size_t a, size_t b) {
  return (points.position[b] - points.position[a]).length();
}

}  // namespace

TEST(Physics, TheSameStepTwiceIsTheSameRun) {
  const Verlet stepper{.dt = 1.0f / 120.0f, .damping = 0.4f, .iterations = 6};
  const std::vector<Force> forces{gravity({0, 900}), drag(0.2f),
                                  attract({40, 40}, 300.0f, 200.0f)};
  const std::vector<Constraint> sticks{distance(0, 1, 30.0f)};

  auto run = [&] {
    Points points;
    points.add({0, 0}, {}, 1.0f, true);
    points.add({30, 0}, {12, -4});
    points.add({-18, 22}, {-3, 9}, 2.0f);
    for (int frame = 0; frame < 200; ++frame)
      stepper.step(points, forces, sticks);
    return points.position;
  };
  EXPECT_EQ(run(), run());
}

TEST(Physics, GravityLandsOnTheClosedForm) {
  const float acceleration = 980.0f;
  const Verlet stepper{.dt = 1.0f / 240.0f};
  const std::vector<Force> forces{gravity({0, acceleration})};
  Points points;
  points.add({0, 0});
  const int steps = 240;
  for (int i = 0; i < steps; ++i) stepper.step(points, forces);
  const float seconds = (float)steps * stepper.dt;
  // Half a t squared, to within the step's own first-order error: the
  // sum of a constant acceleration over n steps is the closed form
  // times (1 + 1/n), so a finer step is a closer answer and no step at
  // all is exact.
  EXPECT_NEAR(points.position[0].y, 0.5f * acceleration * seconds * seconds,
              0.5f * acceleration * seconds * seconds * 0.01f);
  EXPECT_NEAR(points.velocity[0].y, acceleration * seconds,
              acceleration * seconds * 0.01f);
  EXPECT_FLOAT_EQ(points.position[0].x, 0.0f);
}

TEST(Physics, APinnedSpringSettlesOnItsRestLength) {
  const Verlet stepper{.dt = 1.0f / 120.0f, .damping = 2.0f, .iterations = 8};
  const std::vector<Force> forces{gravity({0, 400})};
  const std::vector<Constraint> sticks{spring(0, 1, 50.0f, 0.6f)};
  Points points;
  points.add({0, 0}, {}, 1.0f, true);
  points.add({120, 0});
  for (int frame = 0; frame < 600; ++frame)
    stepper.step(points, forces, sticks);
  EXPECT_NEAR(apart(points, 0, 1), 50.0f, 0.5f);
  // The pinned end never moved, whatever was pulling on it.
  EXPECT_EQ(points.position[0], (Vec2{0, 0}));
  // And it hangs below the anchor, because that is where the gravity is.
  EXPECT_GT(points.position[1].y, 40.0f);
}

TEST(Physics, ABandDoesNothingUntilItIsTaut) {
  const Verlet stepper{.dt = 1.0f / 120.0f, .iterations = 8};
  Points points;
  points.add({0, 0}, {}, 1.0f, true);
  points.add({60, 0});
  const std::vector<Constraint> rope{range(0, 1, 20.0f, 100.0f)};
  const std::vector<Force> none;

  // Inside the band the rope says nothing: the point falls freely.
  const std::vector<Force> pull{gravity({200, 0})};
  for (int frame = 0; frame < 30; ++frame) stepper.step(points, pull, rope);
  EXPECT_GT(points.position[1].x, 60.0f);
  EXPECT_LE(apart(points, 0, 1), 100.0f + 1e-3f);

  // And past it the rope is a stick.
  for (int frame = 0; frame < 400; ++frame) stepper.step(points, pull, rope);
  EXPECT_NEAR(apart(points, 0, 1), 100.0f, 1e-2f);
  // Which means the speed it had is gone: the rope took it, and no
  // force said so.
  EXPECT_LT(points.velocity[1].length(), 5.0f);
  stepper.step(points, none, rope);
  EXPECT_NEAR(apart(points, 0, 1), 100.0f, 1e-2f);
}

TEST(Physics, AnImmovablePointTakesNoneOfTheCorrection) {
  const Verlet stepper{.dt = 1.0f / 60.0f, .iterations = 4};
  Points points;
  points.add({0, 0}, {}, 0.0f);  // a mass of zero is a wall
  points.add({10, 0});
  const std::vector<Constraint> stick{distance(0, 1, 40.0f)};
  for (int frame = 0; frame < 20; ++frame) stepper.step(points, {}, stick);
  EXPECT_EQ(points.position[0], (Vec2{0, 0}));
  EXPECT_NEAR(apart(points, 0, 1), 40.0f, 1e-3f);
}

TEST(Physics, APinIsWhereTheCallerPutsItThisFrame) {
  const Verlet stepper{.dt = 1.0f / 60.0f, .iterations = 4};
  Points points;
  points.add({0, 0});
  std::vector<Constraint> held{pin(0, {12, -7})};
  const std::vector<Force> forces{gravity({0, 500})};
  stepper.step(points, forces, held);
  EXPECT_EQ(points.position[0], (Vec2{12, -7}));
  held[0].at = {60, 5};
  stepper.step(points, forces, held);
  EXPECT_EQ(points.position[0], (Vec2{60, 5}));
}

TEST(Physics, AFlockKeepsEveryBirdAndStaysWhereItCanBeDrawn) {
  const Verlet stepper{.dt = 1.0f / 60.0f, .damping = 0.5f, .iterations = 1};
  const std::vector<Force> forces{
      boids({.separation = 2.0f, .alignment = 1.0f, .cohesion = 1.0f}, 60.0f,
            0.8f),
      drag(0.4f)};
  Points points;
  for (int i = 0; i < 48; ++i) {
    const float angle = (float)i * 0.7f;
    points.add({std::cos(angle) * 120.0f, std::sin(angle) * 90.0f},
               {std::sin(angle) * 20.0f, std::cos(angle) * 20.0f});
  }
  const size_t before = points.size();
  float spreadBefore = 0.0f;
  for (size_t i = 0; i < points.size(); ++i)
    spreadBefore = std::max(spreadBefore, points.position[i].length());

  for (int frame = 0; frame < 600; ++frame) stepper.step(points, forces);

  // A force adds nothing and drops nothing: the count is the caller's,
  // and only `add` and `remove` change it.
  EXPECT_EQ(points.size(), before);
  float spreadAfter = 0.0f;
  for (size_t i = 0; i < points.size(); ++i) {
    EXPECT_TRUE(std::isfinite(points.position[i].x));
    EXPECT_TRUE(std::isfinite(points.position[i].y));
    spreadAfter = std::max(spreadAfter, points.position[i].length());
  }
  // Cohesion beat separation over that many frames, so the flock is
  // together rather than scattered — which is the one thing the three
  // weights are read for.
  EXPECT_LT(spreadAfter, spreadBefore);

  // And no two of them are sitting on each other, which is what
  // separation is there to prevent.
  int touching = 0;
  for (size_t i = 0; i < points.size(); ++i)
    for (size_t j = i + 1; j < points.size(); ++j)
      if (apart(points, i, j) < 0.5f) ++touching;
  EXPECT_EQ(touching, 0);
}

TEST(Physics, TheLanesStayTheSameLengthAndRemovingRenumbers) {
  Points points;
  points.add({0, 0});
  const size_t middle = points.add({1, 1}, {2, 2}, 3.0f);
  points.add({9, 9});
  EXPECT_EQ(points.size(), 3u);
  EXPECT_EQ(points.previous[middle], (Vec2{1, 1}));
  EXPECT_FLOAT_EQ(points.inverseMass(middle), 1.0f / 3.0f);

  points.remove(middle);
  EXPECT_EQ(points.size(), 2u);
  EXPECT_EQ(points.velocity.size(), 2u);
  EXPECT_EQ(points.mass.size(), 2u);
  EXPECT_EQ(points.pinned.size(), 2u);
  // The last one moved into the hole, which is why anything holding an
  // index into a set is holding the wrong point after a removal.
  EXPECT_EQ(points.position[middle], (Vec2{9, 9}));

  points.clear();
  EXPECT_TRUE(points.empty());
}

TEST(Physics, ACallersOwnForceIsAValueLikeTheOthers) {
  Force custom{.kind = ForceKind::Body, .strength = 25.0f};
  custom.body = [](Points& points, float, const Force& force) {
    for (size_t i = 0; i < points.size(); ++i)
      points.force[i] += Vec2{force.strength, 0};
  };
  Points points;
  points.add({0, 0});
  const Verlet stepper{.dt = 1.0f / 60.0f};
  const std::vector<Force> forces{custom};
  stepper.step(points, forces);
  EXPECT_GT(points.position[0].x, 0.0f);
  // A captureless body is a plain pointer, so a force carrying one still
  // compares — which is what lets a describe carry a force list.
  EXPECT_EQ(custom, forces[0]);
  EXPECT_NE(custom, Force{});
}
