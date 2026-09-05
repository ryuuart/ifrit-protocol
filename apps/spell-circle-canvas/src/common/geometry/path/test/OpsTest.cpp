/** @file
 * The path operators — the booleans that combine two outlines, the offset
 * that grows and shrinks one, and the distorts that displace one without
 * moving the shape — with the numeric routines and the value noise under
 * them.
 */

#include <gtest/gtest.h>
#include <include/core/SkPath.h>
#include <include/core/SkRect.h>

#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "sigilgeometry/path/Noise.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Ops.h"
#include "support/Paths.h"

using namespace sigil::geometry::path;
using sigil::geometry::test::rect;

namespace {

// ---------------------------------------------------------------------------
// The booleans. Two unit squares overlapping by half: each operator names
// one region of that pair, and the width alone cannot tell exclude from
// unite — they share an outline — so every row also probes the overlap.

struct Boolean {
  const char* name;
  SkPath (*combine)(const SkPath&, const SkPath&);
  float width;
  bool keepsTheOverlap;
};

class PathBoolean : public ::testing::TestWithParam<Boolean> {};

TEST_P(PathBoolean, NamesItsRegionOfTwoOverlappingSquares) {
  const SkPath a = rect(0, 0, 100, 100);
  const SkPath b = rect(50, 0, 100, 100);
  const SkPath combined = GetParam().combine(a, b);
  EXPECT_NEAR(combined.computeTightBounds().width(), GetParam().width, 1e-3);
  EXPECT_EQ(combined.contains(75, 50), GetParam().keepsTheOverlap);
}

INSTANTIATE_TEST_SUITE_P(
    PathOps, PathBoolean,
    ::testing::Values(
        Boolean{"Unite", [](const SkPath& a,
                            const SkPath& b) { return ops::unite(a, b); },
                150.0f, true},
        Boolean{"Subtract", [](const SkPath& a,
                               const SkPath& b) { return ops::subtract(a, b); },
                50.0f, false},
        Boolean{"Intersect",
                [](const SkPath& a, const SkPath& b) {
                  return ops::intersect(a, b);
                },
                50.0f, true},
        Boolean{"Exclude", [](const SkPath& a,
                              const SkPath& b) { return ops::exclude(a, b); },
                150.0f, false}),
    [](const ::testing::TestParamInfo<Boolean>& info) {
      return std::string(info.param.name);
    });

TEST(PathOps, UnitingAListOfOutlinesReachesEveryOneOfThem) {
  const SkPath a = rect(0, 0, 100, 100);
  const SkPath b = rect(50, 0, 100, 100);
  EXPECT_TRUE(ops::unite({a, b, rect(140, 0, 100, 100)}).contains(200, 50));
}

// Offset distance is a radius, not a diameter: a positive amount grows the
// outline by that much on every side, a negative one eats into it, so a
// circle of radius 50 offset by 10 spans 120 across and by -15 spans 70.
TEST(PathOps, OffsetIsARadiusThatGrowsAndShrinksTheOutline) {
  const SkPath circle = SkPath::Circle(0, 0, 50);
  const SkRect grown = ops::offset(circle, 10).computeTightBounds();
  EXPECT_NEAR(grown.width(), 120, 1.5f);
  const SkRect shrunk = ops::offset(circle, -15).computeTightBounds();
  EXPECT_NEAR(shrunk.width(), 70, 1.5f);
}

// ---------------------------------------------------------------------------
// The distorts. Each is shape-preserving in the large: it displaces the
// outline but must not translate the shape or run away in size, and how
// far it may reach is its own amplitude budget. A Roughen of amplitude 6
// can move a point at most 6 outward, so the width cannot exceed the
// diameter plus twice that.

struct Distort {
  const char* name;
  std::function<SkPath(const SkPath&)> apply;
  float reach;  // how far outward this distort's dials allow a point to go
};

class PathDistort : public ::testing::TestWithParam<Distort> {};

TEST_P(PathDistort, DisplacesTheOutlineWithoutMovingOrGrowingTheShape) {
  const SkPath base = SkPath::Circle(100, 100, 60);
  const SkPath distorted = GetParam().apply(base);
  ASSERT_FALSE(distorted.isEmpty());
  const SkRect bounds = distorted.computeTightBounds();
  EXPECT_LT(std::abs(bounds.centerX() - 100), 4);
  EXPECT_LT(std::abs(bounds.centerY() - 100), 4);
  EXPECT_LT(bounds.width(), 120 + 2 * GetParam().reach + 2);
}

INSTANTIATE_TEST_SUITE_P(
    PathOps, PathDistort,
    ::testing::Values(
        Distort{"Roughen",
                [](const SkPath& p) { return ops::Roughen{6, 8, 42}.apply(p); },
                6.0f},
        Distort{"Twirl",
                [](const SkPath& p) { return ops::Twirl{90}.apply(p); }, 0.0f},
        Distort{"Zigzag",
                [](const SkPath& p) { return ops::Zigzag{4, 20}.apply(p); },
                4.0f},
        Distort{"AChainOfTwo",
                [](const SkPath& p) {
                  return ops::chain({ops::offsetBy(6), ops::Zigzag{4, 20}})(p);
                },
                10.0f}),
    [](const ::testing::TestParamInfo<Distort>& info) {
      return std::string(info.param.name);
    });

TEST(PathOps, BloatPushesOutwardAndNeverInward) {
  const SkPath base = SkPath::Circle(100, 100, 60);
  const SkRect bloated = ops::PuckerBloat{0.8f}.apply(base).computeTightBounds();
  EXPECT_GT(bloated.width(), 118);
}

// ---------------------------------------------------------------------------
// Numeric

TEST(Numeric, BisectReturnsTheFarSideOfTheTransition) {
  const float at = bisect(0.0f, 1.0f, [](float x) { return x < 0.3f; }, 20);
  EXPECT_GE(at, 0.3f);
  EXPECT_NEAR(at, 0.3f, 1e-5f);
}

TEST(Numeric, WrapIsPeriodicAndNonNegative) {
  EXPECT_FLOAT_EQ(wrap(5, 4), 1);
  EXPECT_FLOAT_EQ(wrap(-1, 4), 3);
  EXPECT_FLOAT_EQ(wrap(4, 4), 0);
}

// ---------------------------------------------------------------------------
// Value noise, which is this leaf's own: the seeded mixers it stands on
// live one library down and are held to their own promises there.

TEST(PathNoise, ValueNoiseIsBoundedAndMovesSmoothlyWithItsInput) {
  float prev = valueNoise({0.5f, 0.5f, 0.5f}, 1);
  for (int i = 1; i <= 100; ++i) {
    const float v = valueNoise({0.5f + i * 0.01f, 0.5f, 0.5f}, 1);
    EXPECT_GE(v, -1.0f);
    EXPECT_LE(v, 1.0f);
    EXPECT_LT(std::abs(v - prev), 0.1f);  // 0.01 steps never jump
    prev = v;
  }
}

}  // namespace
