/** @file
 * Filling a shape with points: what the region says is inside, what each
 * rate means, what each spread guarantees, and that one seed is one point
 * set.
 */

#include <gtest/gtest.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>
#include <numbers>
#include <vector>

#include "sigilgeometry/path/Neighbours.h"
#include "sigilgeometry/path/Scatter.h"

using namespace sigil::geometry::path;

namespace {

/** The closest any two of the points come to each other. */
float minimumSpacing(const std::vector<glm::vec2>& points) {
  if (points.size() < 2) return 0;
  const Neighbours index(points);
  float closest = std::numeric_limits<float>::infinity();
  for (size_t i = 0; i < points.size(); ++i) {
    const auto other =
        index.nearestOther(glm::vec3(points[i], 0.0f), (uint32_t)i);
    if (!other) continue;
    closest = std::min(closest, glm::length(points[*other] - points[i]));
  }
  return closest;
}

/** A square with a square hole: the shape that proves the even-odd rule
 *  is what a region is read by. */
Region annulus() {
  SkPathBuilder builder;
  builder.addRect(SkRect::MakeXYWH(0, 0, 200, 200));
  builder.addRect(SkRect::MakeXYWH(75, 75, 50, 50));
  return Region::of(builder.detach());
}

}  // namespace

// ---------------------------------------------------------------------------
// Region

TEST(Region, RectIsOneRingOfItsOwnArea) {
  const Region region = Region::of(SkRect::MakeXYWH(10, 20, 40, 30));
  ASSERT_EQ(region.rings.size(), 1u);
  EXPECT_NEAR(region.area(), 1200.0f, 1e-3f);
  EXPECT_TRUE(region.contains({30, 35}));
  EXPECT_FALSE(region.contains({5, 35}));
}

TEST(Region, DiscApproachesItsCircleFromInside) {
  const Region region = Region::disc({0, 0}, 100.0f, 512);
  const float exact = std::numbers::pi_v<float> * 100.0f * 100.0f;
  EXPECT_LT(region.area(), exact);
  EXPECT_GT(region.area(), exact * 0.999f);
}

TEST(Region, AHoleSubtractsFromTheArea) {
  const Region region = annulus();
  EXPECT_NEAR(region.area(), 200.0f * 200.0f - 50.0f * 50.0f, 1e-2f);
  EXPECT_TRUE(region.contains({10, 10}));
  EXPECT_FALSE(region.contains({100, 100}));
}

// ---------------------------------------------------------------------------
// The rates

TEST(Scatter, RandomAtACountAnswersExactlyThatCount) {
  const Region region = Region::of(SkRect::MakeXYWH(0, 0, 300, 300));
  EXPECT_EQ(sample(region, uniform(500)).size(), 500u);
  EXPECT_EQ(sample(region, uniform(1)).size(), 1u);
  EXPECT_TRUE(sample(region, uniform(0)).empty());
}

TEST(Scatter, DensityIsPointsPerUnitArea) {
  const Region region = Region::of(SkRect::MakeXYWH(0, 0, 100, 100));
  Distribution distribution;
  distribution.rate = Rate::Density;
  distribution.amount = 0.05f;  // 10000 units of area, so 500 points
  EXPECT_EQ(sample(region, distribution).size(), 500u);
}

TEST(Scatter, SpacingIsTheSideOfTheSquareEachPointOwns) {
  const Region region = Region::of(SkRect::MakeXYWH(0, 0, 100, 100));
  Distribution distribution;
  distribution.rate = Rate::Spacing;
  distribution.amount = 10.0f;  // 10 x 10 squares over 100 x 100
  EXPECT_EQ(sample(region, distribution).size(), 100u);
}

TEST(Scatter, MaxPointsBoundsTheAnswer) {
  const Region region = Region::of(SkRect::MakeXYWH(0, 0, 1000, 1000));
  Distribution distribution;
  distribution.rate = Rate::Density;
  distribution.amount = 1.0f;  // a million points asked for
  distribution.maxPoints = 250;
  EXPECT_EQ(sample(region, distribution).size(), 250u);
}

// ---------------------------------------------------------------------------
// The spreads

TEST(Scatter, EveryPointLandsInsideTheRegionAndNoneInTheHole) {
  const Region region = annulus();
  for (const Distribution& distribution :
       {uniform(400), poisson(9.0f), grid(11.0f), jittered(11.0f),
        blueNoise(400)}) {
    const std::vector<glm::vec2> points = sample(region, distribution);
    EXPECT_FALSE(points.empty());
    for (const glm::vec2 point : points) {
      EXPECT_TRUE(region.contains(point))
          << point.x << ", " << point.y;
      EXPECT_FALSE(point.x > 75 && point.x < 125 && point.y > 75 &&
                   point.y < 125);
    }
  }
}

TEST(Scatter, PoissonKeepsItsRadiusApart) {
  const Region region = Region::of(SkRect::MakeXYWH(0, 0, 400, 400));
  const std::vector<glm::vec2> points = sample(region, poisson(12.0f));
  ASSERT_GT(points.size(), 200u);
  EXPECT_GE(minimumSpacing(points), 12.0f);
  // The fill is dense: a poisson disc packs to well over half the points a
  // square lattice of the same pitch would hold.
  EXPECT_GT(points.size(), (size_t)(400.0f * 400.0f / (12.0f * 12.0f) * 0.5f));
}

TEST(Scatter, AnExactLatticeIsAnExactLattice) {
  const Region region = Region::of(SkRect::MakeXYWH(0, 0, 100, 100));
  const std::vector<glm::vec2> points = sample(region, grid(10.0f));
  // Ten cells of ten units on each side, each holding its own centre — the
  // same count the spacing asks for, and no point on the boundary.
  ASSERT_EQ(points.size(), 100u);
  EXPECT_NEAR(minimumSpacing(points), 10.0f, 1e-3f);
  EXPECT_NEAR(points.front().x, 5.0f, 1e-3f);
  EXPECT_NEAR(points.front().y, 5.0f, 1e-3f);
}

TEST(Scatter, JitterMovesEachPointInsideItsOwnCellAndNoFurther) {
  const Region region = Region::of(SkRect::MakeXYWH(0, 0, 100, 100));
  const std::vector<glm::vec2> exact = sample(region, grid(10.0f));
  const std::vector<glm::vec2> moved = sample(region, jittered(10.0f, 1, 0.8f));
  // The lattice is the same lattice; the jitter is bounded by a fraction of
  // half a cell on each axis, so nothing crosses into a neighbour's cell.
  ASSERT_EQ(moved.size(), exact.size());
  for (size_t i = 0; i < moved.size(); ++i) {
    EXPECT_LE(std::abs(moved[i].x - exact[i].x), 4.0f + 1e-3f);
    EXPECT_LE(std::abs(moved[i].y - exact[i].y), 4.0f + 1e-3f);
  }
  EXPECT_GT(minimumSpacing(moved), 0.0f);
  EXPECT_LT(minimumSpacing(moved), 10.0f);
}

TEST(Scatter, RelaxingSpreadsAnIndependentScatterOut) {
  const Region region = Region::of(SkRect::MakeXYWH(0, 0, 300, 300));
  const std::vector<glm::vec2> raw = sample(region, uniform(600));
  const std::vector<glm::vec2> even = sample(region, blueNoise(600));
  ASSERT_EQ(raw.size(), even.size());
  // Relaxation is exactly the claim that the closest pair gets further
  // apart: an independent scatter puts two points almost on top of each
  // other, and pushing them apart is what blue noise means.
  EXPECT_GT(minimumSpacing(even), minimumSpacing(raw) * 4.0f);
}

// ---------------------------------------------------------------------------
// Determinism

TEST(Scatter, OneSeedIsOnePointSet) {
  const Region region = annulus();
  for (const Distribution& distribution :
       {uniform(300), poisson(8.0f), jittered(9.0f), blueNoise(300)}) {
    EXPECT_EQ(sample(region, distribution), sample(region, distribution));
  }
}

TEST(Scatter, AnotherSeedIsAnotherPointSet) {
  const Region region = Region::of(SkRect::MakeXYWH(0, 0, 200, 200));
  EXPECT_NE(sample(region, uniform(200, 1)), sample(region, uniform(200, 2)));
  EXPECT_NE(sample(region, poisson(9.0f, 1)), sample(region, poisson(9.0f, 2)));
}

TEST(Scatter, ALowDiscrepancySourceSpreadsBetterThanAMixer) {
  const Region region = Region::of(SkRect::MakeXYWH(0, 0, 200, 200));
  Distribution halton = uniform(400);
  halton.source = sigil::core::chance::Source::Halton;
  halton.parameter = 2;
  // A Halton stream is not random at all: it is the sequence that fills an
  // interval most evenly for the count drawn so far, so the closest pair
  // it places is further apart than a mixer's.
  EXPECT_GT(minimumSpacing(sample(region, halton)),
            minimumSpacing(sample(region, uniform(400))));
}

TEST(Scatter, AnEmptyRegionAnswersNothing) {
  const Region region;
  EXPECT_TRUE(sample(region, uniform(100)).empty());
  EXPECT_TRUE(sample(region, poisson(4.0f)).empty());
  EXPECT_TRUE(sample(region, grid(4.0f)).empty());
}
