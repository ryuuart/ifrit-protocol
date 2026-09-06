/** @file
 * The uniform grid, judged against the brute-force answer: a radius query
 * returns exactly the set a full scan returns, the nearest is the nearest,
 * k-nearest come back in distance order, and the degenerate shapes — one
 * point, coincident points, a flat sheet, a query far outside — answer
 * what they should rather than crashing.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>
#include <numbers>
#include <vector>

#include "sigilcore/compute/Chance.h"
#include "sigilgeometry/path/Neighbours.h"

using namespace sigil::geometry::path;
using sigil::core::chance::Stream;

namespace {

std::vector<glm::vec3> cloud(size_t count, float extent, uint64_t seed) {
  Stream stream = Stream::pcg(seed);
  std::vector<glm::vec3> points;
  points.reserve(count);
  for (size_t i = 0; i < count; ++i)
    points.push_back({stream.range(0, extent), stream.range(0, extent),
                      stream.range(0, extent)});
  return points;
}

std::vector<uint32_t> bruteWithin(const std::vector<glm::vec3>& points,
                                  glm::vec3 p, float radius) {
  std::vector<uint32_t> found;
  for (size_t i = 0; i < points.size(); ++i) {
    const glm::vec3 delta = points[i] - p;
    if (glm::dot(delta, delta) <= radius * radius) found.push_back((uint32_t)i);
  }
  return found;
}

std::vector<uint32_t> sorted(std::vector<uint32_t> indices) {
  std::sort(indices.begin(), indices.end());
  return indices;
}

}  // namespace

TEST(Neighbours, WithinMatchesBruteForceAtEveryRadius) {
  const std::vector<glm::vec3> points = cloud(2000, 500.0f, 11);
  const Neighbours index(points);
  Stream stream = Stream::pcg(3);
  for (int trial = 0; trial < 40; ++trial) {
    const glm::vec3 query{stream.range(-50, 550), stream.range(-50, 550),
                          stream.range(-50, 550)};
    const float radius = stream.range(1, 120);
    EXPECT_EQ(sorted(index.within(query, radius)),
              sorted(bruteWithin(points, query, radius)))
        << "radius " << radius;
  }
}

TEST(Neighbours, WithinIsEmptyForANonPositiveRadius) {
  const Neighbours index(cloud(100, 10.0f, 5));
  EXPECT_TRUE(index.within(glm::vec3{5, 5, 5}, 0.0f).empty());
  EXPECT_TRUE(index.within(glm::vec3{5, 5, 5}, -1.0f).empty());
}

TEST(Neighbours, NearestIsTheNearest) {
  const std::vector<glm::vec3> points = cloud(1500, 300.0f, 21);
  const Neighbours index(points);
  Stream stream = Stream::pcg(9);
  for (int trial = 0; trial < 60; ++trial) {
    const glm::vec3 query{stream.range(-100, 400), stream.range(-100, 400),
                          stream.range(-100, 400)};
    size_t best = 0;
    float bestSquared = std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < points.size(); ++i) {
      const glm::vec3 delta = points[i] - query;
      const float squared = glm::dot(delta, delta);
      if (squared < bestSquared) {
        bestSquared = squared;
        best = i;
      }
    }
    const auto found = index.nearest(query);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, best);
  }
}

TEST(Neighbours, NearestKComesBackNearestFirstAndMatchesASort) {
  const std::vector<glm::vec3> points = cloud(800, 200.0f, 31);
  const Neighbours index(points);
  const glm::vec3 query{77, 33, 121};

  std::vector<uint32_t> all(points.size());
  for (size_t i = 0; i < all.size(); ++i) all[i] = (uint32_t)i;
  std::sort(all.begin(), all.end(), [&](uint32_t a, uint32_t b) {
    return glm::length(points[a] - query) < glm::length(points[b] - query);
  });

  const std::vector<uint32_t> found = index.nearest(query, 12);
  ASSERT_EQ(found.size(), 12u);
  for (size_t i = 0; i < found.size(); ++i) EXPECT_EQ(found[i], all[i]);
  for (size_t i = 1; i < found.size(); ++i)
    EXPECT_LE(glm::length(points[found[i - 1]] - query),
              glm::length(points[found[i]] - query));
}

TEST(Neighbours, NearestKAsksForMoreThanItHolds) {
  const std::vector<glm::vec3> points = cloud(7, 10.0f, 4);
  const Neighbours index(points);
  EXPECT_EQ(index.nearest(glm::vec3{0, 0, 0}, 50).size(), 7u);
  EXPECT_TRUE(index.nearest(glm::vec3{0, 0, 0}, 0).empty());
}

TEST(Neighbours, NearestOtherSkipsThePointItself) {
  const std::vector<glm::vec3> points = {
      {0, 0, 0}, {1, 0, 0}, {5, 0, 0}, {50, 0, 0}};
  const Neighbours index(points);
  EXPECT_EQ(index.nearestOther(points[0], 0).value(), 1u);
  EXPECT_EQ(index.nearestOther(points[2], 2).value(), 1u);
}

TEST(Neighbours, FlatPointsCostOneLayerOfCells) {
  std::vector<glm::vec2> flat;
  for (int i = 0; i < 400; ++i) {
    const float angle = 2.0f * std::numbers::pi_v<float> * (float)i / 400.0f;
    flat.push_back({100.0f * std::cos(angle), 100.0f * std::sin(angle)});
  }
  const Neighbours index(flat);
  EXPECT_EQ(index.dimensions().z, 1);
  EXPECT_EQ(index.size(), 400u);
  // Neighbours on a ring of this radius sit about 1.57 apart, so a radius
  // of 4 reaches two on each side and no more.
  EXPECT_EQ(index.within(flat[0], 4.0f).size(), 5u);
}

TEST(Neighbours, CoincidentPointsAllAnswerOneQuery) {
  const std::vector<glm::vec3> points(500, glm::vec3{3, 4, 5});
  const Neighbours index(points);
  EXPECT_EQ(index.within(glm::vec3{3, 4, 5}, 0.5f).size(), 500u);
  EXPECT_TRUE(index.within(glm::vec3{30, 4, 5}, 0.5f).empty());
  EXPECT_TRUE(index.nearest(glm::vec3{0, 0, 0}).has_value());
}

TEST(Neighbours, EmptyIndexAnswersNothing) {
  const Neighbours index;
  EXPECT_TRUE(index.empty());
  EXPECT_TRUE(index.within(glm::vec3{0, 0, 0}, 10.0f).empty());
  EXPECT_FALSE(index.nearest(glm::vec3{0, 0, 0}).has_value());
  EXPECT_TRUE(index.nearest(glm::vec3{0, 0, 0}, 4).empty());
}

TEST(Neighbours, ARequestedCellIsCoarsenedRatherThanBlowingTheTable) {
  // Ten points across a huge extent at a cell of one would want a table of
  // 10^12 cells; the grid coarsens until it fits and says what it used.
  std::vector<glm::vec3> points;
  for (int i = 0; i < 10; ++i)
    points.push_back({(float)i * 100000.0f, 0, 0});
  const Neighbours index(points, 1.0f);
  EXPECT_GT(index.cell(), 1.0f);
  EXPECT_EQ(index.within(points[4], 1.0f).size(), 1u);
  EXPECT_EQ(index.nearest(points[4]).value(), 4u);
}

TEST(Neighbours, ForEachWithinVisitsWhatWithinReturns) {
  const std::vector<glm::vec3> points = cloud(600, 80.0f, 77);
  const Neighbours index(points);
  const glm::vec3 query{40, 40, 40};
  std::vector<uint32_t> visited;
  index.forEachWithin(query, 15.0f,
                      [&](uint32_t i) { visited.push_back(i); });
  EXPECT_EQ(sorted(visited), sorted(index.within(query, 15.0f)));
}
