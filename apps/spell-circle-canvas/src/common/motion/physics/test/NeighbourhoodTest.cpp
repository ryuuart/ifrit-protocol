/** @file
 * The grid held against the walk it replaces: the same neighbours on a
 * seeded cloud whatever the cell size, an answer in index order, the
 * degenerate sets a caller can hand in (nothing, one point, a line, a
 * heap of coincident points, a coordinate that is not a number), and the
 * flock over it — the same steering the pair walk computes, reaching
 * only inside its radius, and the same run twice.
 */

#include <gtest/gtest.h>
#include <sigilcore/compute/Chance.h>
#include <sigilmotion/physics/Physics.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "support/StandsAlone.h"

using namespace sigil::motion::physics;

namespace {

/** A cloud of `count` points spread over a box, from a seed. */
std::vector<Vec2> cloud(int count, uint64_t seed, float spread = 400.0f) {
  sigil::core::chance::Stream stream = sigil::core::chance::Stream::pcg(seed);
  std::vector<Vec2> points;
  points.reserve((size_t)count);
  for (int i = 0; i < count; ++i)
    points.push_back(
        {stream.signedUnit() * spread, stream.signedUnit() * spread * 0.6f});
  return points;
}

/** Every index within `radius` of `at`, found by asking all of them. */
std::vector<uint32_t> byWalking(const std::vector<Vec2>& points, Vec2 at,
                                float radius) {
  std::vector<uint32_t> found;
  const float reachSquared = radius * radius;
  for (size_t i = 0; i < points.size(); ++i)
    if ((points[i] - at).lengthSquared() <= reachSquared)
      found.push_back((uint32_t)i);
  return found;
}

/** A point set standing where `at` says, at rest. */
Points restingAt(const std::vector<Vec2>& at) {
  Points points;
  for (const Vec2 place : at) points.add(place);
  return points;
}

}  // namespace

TEST(Neighbourhood, AnswersWhatTheWalkOverEveryPairAnswers) {
  const std::vector<Vec2> points = cloud(2000, 71);
  // Three cell sizes against one radius: the size the queries will use,
  // one far finer and one far coarser. A grid that swept a cell too few
  // would lose a neighbour at exactly one of these.
  for (const float cell : {0.0f, 6.0f, 90.0f, 4000.0f}) {
    const Neighbourhood near(points, cell);
    ASSERT_EQ(near.size(), points.size());
    std::vector<uint32_t> found;
    for (size_t i = 0; i < points.size(); ++i) {
      near.within(points[i], 40.0f, found);
      EXPECT_EQ(found, byWalking(points, points[i], 40.0f))
          << "point " << i << " at cell " << cell;
    }
    // And about places no point stands, where an empty answer is as much
    // of an answer as a full one.
    near.within({10000.0f, 10000.0f}, 40.0f, found);
    EXPECT_TRUE(found.empty());
  }
}

TEST(Neighbourhood, TheAnswerIsInIndexOrder) {
  const std::vector<Vec2> points = cloud(1500, 12);
  const Neighbourhood near(points);
  std::vector<uint32_t> found;
  size_t crowded = 0;
  for (size_t i = 0; i < points.size(); ++i) {
    near.within(points[i], 60.0f, found);
    crowded = std::max(crowded, found.size());
    EXPECT_TRUE(std::is_sorted(found.begin(), found.end()));
    EXPECT_TRUE(std::adjacent_find(found.begin(), found.end()) == found.end());
    // The point itself is inside every radius and comes back with the
    // rest; dropping it is the caller's.
    EXPECT_TRUE(std::binary_search(found.begin(), found.end(), (uint32_t)i));
  }
  // A radius that reached nothing would satisfy everything above.
  EXPECT_GT(crowded, 3u);
}

TEST(Neighbourhood, ARadiusOfNothingAndAnEmptyIndexAnswerNothing) {
  const Neighbourhood nothing;
  EXPECT_TRUE(nothing.empty());
  EXPECT_EQ(nothing.size(), 0u);
  EXPECT_TRUE(nothing.within({0, 0}, 100.0f).empty());

  const std::vector<Vec2> points = cloud(64, 5);
  const Neighbourhood near(points);
  EXPECT_TRUE(near.within(points[0], 0.0f).empty());
  EXPECT_TRUE(near.within(points[0], -1.0f).empty());
}

TEST(Neighbourhood, TheSetsAGridCannotSpaceItselfOverStillAnswer) {
  // One point: no extent on either axis, so no cell size can be derived
  // from the set and the grid falls back to one cell.
  const std::vector<Vec2> alone{{3.0f, -7.0f}};
  const Neighbourhood one(alone);
  EXPECT_EQ(one.within({3.0f, -7.0f}, 1.0f), std::vector<uint32_t>{0u});
  EXPECT_TRUE(one.within({300.0f, -7.0f}, 1.0f).empty());

  // A line: one axis has no extent at all, and a cell size taken off the
  // area of that box would be zero.
  std::vector<Vec2> line;
  for (int i = 0; i < 500; ++i) line.push_back({(float)i * 2.0f, 0.0f});
  const Neighbourhood strung(line);
  EXPECT_EQ(strung.within({100.0f, 0.0f}, 5.0f),
            byWalking(line, {100.0f, 0.0f}, 5.0f));

  // A heap: every point in one place, which is one bucket however fine
  // the grid is asked to be.
  const std::vector<Vec2> heap(300, Vec2{12.0f, 12.0f});
  const Neighbourhood piled(heap, 0.001f);
  EXPECT_EQ(piled.within({12.0f, 12.0f}, 0.5f).size(), heap.size());

  // A coordinate that is not a number takes no part in the bounds and
  // still has a bucket, so every index the caller handed in is
  // answerable rather than lost.
  std::vector<Vec2> withNaN = cloud(200, 9);
  withNaN.push_back({std::numeric_limits<float>::quiet_NaN(), 0.0f});
  const Neighbourhood ragged(withNaN);
  EXPECT_EQ(ragged.size(), withNaN.size());
  EXPECT_EQ(ragged.within(withNaN[7], 30.0f),
            byWalking(withNaN, withNaN[7], 30.0f));
}

TEST(Neighbourhood, ARebuiltIndexAnswersAboutWhereThePointsAreNow) {
  std::vector<Vec2> points = cloud(400, 33);
  Neighbourhood near(points);
  const std::vector<uint32_t> before = near.within({0, 0}, 50.0f);

  // A snapshot: moving the caller's points does not move the answer.
  for (Vec2& at : points) at += Vec2{1000.0f, 0.0f};
  EXPECT_EQ(near.within({0, 0}, 50.0f), before);

  near.build(points);
  EXPECT_TRUE(near.within({0, 0}, 50.0f).empty());
  EXPECT_EQ(near.within({1000.0f, 0.0f}, 50.0f),
            byWalking(points, {1000.0f, 0.0f}, 50.0f));
}

TEST(Neighbourhood, AFlockSteersOnTheSameSumTheWalkOverEveryPairMakes) {
  const std::vector<Vec2> places = cloud(600, 4181);
  const Flocking weights{
      .separation = 1.5f, .alignment = 0.8f, .cohesion = 1.2f};
  const float reach = 55.0f;

  Points indexed = restingAt(places);
  for (size_t i = 0; i < indexed.size(); ++i)
    indexed.velocity[i] = {std::sin((float)i), std::cos((float)i * 0.7f)};
  Points walked = indexed;

  boids(weights, reach, 2.0f).apply(indexed, 1.0f / 60.0f);

  // The same three steerings, over every pair, in index order — which is
  // the body the force had before there was a grid to ask.
  const float reachSquared = reach * reach;
  for (size_t i = 0; i < walked.size(); ++i) {
    Vec2 away{}, heading{}, centre{};
    int neighbours = 0;
    for (size_t j = 0; j < walked.size(); ++j) {
      if (j == i) continue;
      const Vec2 offset = walked.position[j] - walked.position[i];
      const float distanceSquared = offset.lengthSquared();
      if (distanceSquared > reachSquared || distanceSquared <= 0.0f) continue;
      ++neighbours;
      heading += walked.velocity[j];
      centre += walked.position[j];
      away -= offset * (1.0f / distanceSquared);
    }
    if (neighbours == 0) continue;
    const float share = 1.0f / (float)neighbours;
    const Vec2 alignment = heading * share - walked.velocity[i];
    const Vec2 cohesion = centre * share - walked.position[i];
    walked.force[i] +=
        (away * weights.separation + alignment * weights.alignment +
         cohesion * weights.cohesion) *
        (2.0f * walked.mass[i]);
  }

  size_t pushed = 0;
  for (size_t i = 0; i < indexed.size(); ++i) {
    EXPECT_FLOAT_EQ(indexed.force[i].x, walked.force[i].x) << "point " << i;
    EXPECT_FLOAT_EQ(indexed.force[i].y, walked.force[i].y) << "point " << i;
    if (indexed.force[i].lengthSquared() > 0.0f) ++pushed;
  }
  EXPECT_GT(pushed, indexed.size() / 2);
}

TEST(Neighbourhood, AFlockReachesNoFurtherThanItsRadius) {
  Points points;
  const size_t left = points.add({0, 0});
  const size_t beside = points.add({10.0f, 0.0f});
  const size_t away = points.add({500.0f, 0.0f});
  points.velocity[beside] = {0.0f, 20.0f};

  boids({}, 60.0f, 1.0f).apply(points, 1.0f / 60.0f);

  // The pair inside the radius sees each other and is pushed; the loner
  // is outside every reach and is not touched at all.
  EXPECT_GT(points.force[left].lengthSquared(), 0.0f);
  EXPECT_GT(points.force[beside].lengthSquared(), 0.0f);
  EXPECT_EQ(points.force[away], Vec2{});

  // And a radius that reaches nobody pushes nobody, however many points
  // there are.
  Points tight = restingAt(cloud(300, 8));
  boids({}, 0.001f, 1.0f).apply(tight, 1.0f / 60.0f);
  for (size_t i = 0; i < tight.size(); ++i) EXPECT_EQ(tight.force[i], Vec2{});
}

TEST(Neighbourhood, TheSameCloudFlocksTheSameWayTwice) {
  const std::vector<Vec2> places = cloud(500, 2718);
  const std::vector<Force> forces{boids({}, 70.0f), drag(0.3f)};
  const Verlet stepper{.dt = 1.0f / 60.0f, .damping = 0.2f};

  auto run = [&] {
    Points points = restingAt(places);
    for (size_t i = 0; i < points.size(); ++i)
      points.velocity[i] = {std::cos((float)i * 0.3f), std::sin((float)i)};
    for (int step = 0; step < 40; ++step) stepper.step(points, forces);
    return points.position;
  };

  const std::vector<Vec2> once = run();
  const std::vector<Vec2> twice = run();
  ASSERT_EQ(once.size(), twice.size());
  for (size_t i = 0; i < once.size(); ++i) EXPECT_EQ(once[i], twice[i]);
}
