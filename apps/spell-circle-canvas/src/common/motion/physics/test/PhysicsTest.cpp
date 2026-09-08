/** @file
 * The stepper: a fixed step is the same run twice, gravity lands on the
 * closed form, a pinned pair settles on its rest length, a band does
 * nothing until it is taut, a constraint takes the speed it took, and a
 * flock keeps every bird it started with, a force a caller pre-loaded
 * moves the point it was written on and is spent once — and the
 * degenerate settings a
 * caller can hand in (no time, the clock's biggest step, no iterations, a
 * stiffness past rigid, an attractor arrived at, a body with nothing in
 * it) answer rather than dividing.
 */

#include <gtest/gtest.h>
#include <sigilmotion/physics/Physics.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "support/StandsAlone.h"

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

TEST(Physics, TheApproximateStickConvergesOnTheSameLength) {
  // The square-root-free solve is a different path to the same band: one
  // pass of it is not one pass of the exact solve, and a few passes bring
  // the pair to the rest length either way.
  auto run = [](const Constraint& held, int passes) {
    Points points;
    points.add({0, 0}, {}, 1.0f, true);
    points.add({70, 0});
    for (int pass = 0; pass < passes; ++pass) held.project(points);
    return points;
  };
  // Far from the rest length the two solves do not agree: the exact one
  // lands on the band in a single pass, the approximation overshoots and
  // comes back.
  EXPECT_NEAR(apart(run(distance(0, 1, 40.0f), 1), 0, 1), 40.0f, 0.001f);
  EXPECT_GT(std::abs(apart(run(stick(0, 1, 40.0f), 1), 0, 1) - 40.0f), 1.0f);

  Points points = run(stick(0, 1, 40.0f), 60);
  const Constraint held = stick(0, 1, 40.0f);
  EXPECT_NEAR(apart(points, 0, 1), 40.0f, 0.05f);
  // The held point did not move: the correction is shared by inverse
  // mass here exactly as it is in the exact solve.
  EXPECT_FLOAT_EQ(points.position[0].x, 0.0f);
  EXPECT_FLOAT_EQ(points.position[0].y, 0.0f);
}

TEST(Physics, TheApproximateStickPushesApartWhenTooClose) {
  Points points;
  points.add({0, 0});
  points.add({4, 0});
  const Constraint held = stick(0, 1, 40.0f);
  for (int pass = 0; pass < 200; ++pass) held.project(points);
  EXPECT_NEAR(apart(points, 0, 1), 40.0f, 0.05f);
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

TEST(Physics, APreLoadedForceMovesThePointItWasWrittenOn) {
  // The lane is the caller's to pre-load. A push written into it between
  // two steps is what the step's own forces accumulate onto, and the
  // step clears it once it has integrated, so the push is spent once.
  const Verlet stepper{.dt = 1.0f / 60.0f};
  Points points;
  points.add({0, 0});
  points.add({0, 0});
  points.force[0] = {600, 0};

  stepper.step(points, {});
  // One step of a constant force on a unit mass: the velocity it buys is
  // force times the step, and the distance that velocity covers is the
  // force times the step squared.
  const float expected = 600.0f * stepper.dt * stepper.dt;
  EXPECT_NEAR(points.position[0].x, expected, expected * 1e-4f);
  EXPECT_FLOAT_EQ(points.velocity[0].x, 600.0f * stepper.dt);
  // The point nobody pushed stands where it was.
  EXPECT_FLOAT_EQ(points.position[1].x, 0.0f);
  EXPECT_FLOAT_EQ(points.velocity[1].x, 0.0f);
  // And the lane is empty again, so the push is not spent a second time.
  EXPECT_EQ(points.force[0], Vec2{});
  const Vec2 carried = points.velocity[0];
  stepper.step(points, {});
  EXPECT_FLOAT_EQ(points.velocity[0].x, carried.x);
}

TEST(Physics, AStepOfNoTimeMovesNothing) {
  // The step covers no time, so nothing happened in it — and answering
  // by dividing the forces through a zero would be a NaN in every lane
  // instead of the point set the caller handed in.
  const std::vector<Force> forces{gravity({0, 980}), drag(0.5f)};
  const std::vector<Constraint> stick{distance(0, 1, 5.0f)};
  Points points;
  points.add({0, 0}, {7, -3});
  points.add({40, 0}, {-2, 1});
  const std::vector<Vec2> before = points.position;
  const std::vector<Vec2> speeds = points.velocity;

  for (float dt : {0.0f, -1.0f / 60.0f}) {
    const Verlet stepper{.dt = dt};
    stepper.step(points, forces, stick);
    EXPECT_EQ(points.position, before) << "at dt " << dt;
    EXPECT_EQ(points.velocity, speeds) << "at dt " << dt;
  }
}

TEST(Physics, AStepAsLongAsTheClocksCeilingIsCoarseAndNotExploded) {
  // A quarter of a second is what a stalled host or a debugger break
  // hands over, and it is the biggest step there is. A constraint is a
  // projection onto POSITIONS and runs last, so it is satisfied at the
  // end of the pass whatever the step before it did — the coarse step
  // costs accuracy in the middle of the motion, not the structure.
  const Verlet stepper{.dt = 0.25f, .damping = 0.5f, .iterations = 8};
  const std::vector<Force> forces{gravity({0, 980}), drag(0.3f)};
  const std::vector<Constraint> stick{distance(0, 1, 20.0f)};
  Points pair;
  pair.add({0, 0}, {}, 1.0f, true);
  pair.add({20, 0});
  for (int frame = 0; frame < 20; ++frame) stepper.step(pair, forces, stick);
  EXPECT_NEAR(apart(pair, 0, 1), 20.0f, 1e-3f);
  EXPECT_EQ(pair.position[0], (Vec2{0, 0}));

  // A CHAIN of them at that step is coarse — a walk of the list only
  // carries a correction one link along, so a long chain under a step
  // that big is left stretched rather than converged — but it is a
  // number: every lane is finite, and the constraint the pass projected
  // LAST is satisfied exactly, whatever the step before it did.
  std::vector<Constraint> chain;
  Points hanging;
  hanging.add({0, 0}, {}, 1.0f, true);
  for (int i = 1; i < 12; ++i) {
    hanging.add({(float)i * 20.0f, 0});
    chain.push_back(distance(i - 1, i, 20.0f));
  }
  for (int frame = 0; frame < 40; ++frame) stepper.step(hanging, forces, chain);
  for (size_t i = 0; i < hanging.size(); ++i) {
    EXPECT_TRUE(std::isfinite(hanging.position[i].x)) << "at " << i;
    EXPECT_TRUE(std::isfinite(hanging.position[i].y)) << "at " << i;
    EXPECT_TRUE(std::isfinite(hanging.velocity[i].x)) << "at " << i;
    EXPECT_TRUE(std::isfinite(hanging.velocity[i].y)) << "at " << i;
  }
  EXPECT_NEAR(apart(hanging, 10, 11), 20.0f, 1e-3f);
}

TEST(Physics, NoIterationsIsStillOnePassOverTheList) {
  // `iterations` is how many walks, and a walk is the least a constraint
  // list can be given: zero would be a list silently ignored, which is
  // the one answer a caller cannot tell from a constraint that does not
  // work.
  const std::vector<Constraint> stick{distance(0, 1, 40.0f)};
  auto run = [&](int iterations) {
    const Verlet stepper{.dt = 1.0f / 60.0f, .iterations = iterations};
    Points points;
    points.add({0, 0}, {}, 1.0f, true);
    points.add({10, 0});
    stepper.step(points, {}, stick);
    return points.position[1];
  };
  EXPECT_EQ(run(0), run(1));
  EXPECT_NEAR((run(0) - Vec2{0, 0}).length(), 40.0f, 1e-3f);
}

TEST(Physics, AStiffnessAboveOneIsHeldAtRigidRatherThanOvershooting) {
  // Above one, a pass would take out MORE than the whole error and land
  // the pair on the far side of the band; the number is held at one, so
  // the softest reading of "stiffer than rigid" is rigid.
  const Verlet stepper{.dt = 1.0f / 60.0f, .iterations = 1};
  auto once = [&](float stiffness) {
    Points points;
    points.add({0, 0}, {}, 1.0f, true);
    points.add({10, 0});
    const std::vector<Constraint> soft{spring(0, 1, 40.0f, stiffness)};
    stepper.step(points, {}, soft);
    return points.position[1].x;
  };
  EXPECT_FLOAT_EQ(once(2.0f), once(1.0f));
  EXPECT_NEAR(once(2.0f), 40.0f, 1e-3f);
  // …and below zero is held at nothing rather than pushing the error
  // wider.
  EXPECT_FLOAT_EQ(once(-3.0f), 10.0f);
}

TEST(Physics, AnAttractorPullsNoHarderThanItsStrengthAtTheCentre) {
  // The falloff is 1/distance, so without a floor a point that arrives
  // is pulled by an arbitrarily large number: at 1e-6 away, a millionth
  // of the strength would become a million times it. `strength` is the
  // pull at one unit away and is the most there is.
  const float strength = 300.0f;
  const std::vector<Force> forces{attract({0, 0}, strength)};
  const Verlet stepper{.dt = 1.0f / 60.0f};
  // A step leaves the force lane empty, so what the pull WAS is read off
  // the speed it bought: a unit mass gains the push times the step, and
  // nothing here takes any of it back.
  const auto pushOn = [&](const Points& points) {
    return points.velocity[0].length() / stepper.dt;
  };
  for (float away : {1e-6f, 1e-3f, 0.5f, 1.0f}) {
    Points points;
    points.add({away, 0});
    stepper.step(points, forces);
    EXPECT_LE(pushOn(points), strength + 1e-2f) << "at " << away;
    EXPECT_TRUE(std::isfinite(points.position[0].x)) << "at " << away;
  }
  // And it IS the strength there, rather than nothing: the floor holds
  // the pull, it does not switch it off.
  Points arrived;
  arrived.add({1e-6f, 0});
  stepper.step(arrived, forces);
  EXPECT_NEAR(pushOn(arrived), strength, 1e-2f);
}

TEST(Physics, ABodyForceWithNothingInItPushesNothing) {
  // `Body` is a plain function pointer, so a default-constructed force
  // of that kind carries a null one — a value a describe can hand over
  // before it has decided what the push is.
  const Force empty{.kind = ForceKind::Body, .strength = 400.0f};
  ASSERT_EQ(empty.body, nullptr);
  Points points;
  points.add({3, 4}, {1, 1});
  const std::vector<Vec2> before = points.position;
  const Verlet stepper{.dt = 1.0f / 60.0f};
  const std::vector<Force> forces{empty};
  stepper.step(points, forces);
  EXPECT_EQ(points.force[0], (Vec2{0, 0}));
  // It coasts on the speed it had and nothing else touched it.
  EXPECT_EQ(points.position[0], (before[0] + Vec2{1, 1} * stepper.dt));
}
