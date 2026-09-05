/** @file
 * The interval algebra: what the normal form does to an unsorted,
 * overlapping, backwards, out-of-range pile; what the complement leaves;
 * what two sets share; and where the epsilon changes the answer.
 */

#include <gtest/gtest.h>
#include <sigilcore/compute/Intervals.h>

#include <cstdint>
#include <vector>

namespace {

/** A run over a fraction, with its endpoints under the default names. */
struct Arc {
  float low = 0, high = 0;
  bool operator==(const Arc&) const = default;
};

/** A half-open run of code units, whose endpoints are named otherwise. */
struct Range {
  uint32_t start = 0, end = 0;
  bool operator==(const Range&) const = default;
};

}  // namespace

template <>
struct sigil::core::IntervalEnds<Range> {
  using Value = uint32_t;
  static uint32_t& low(Range& r) { return r.start; }
  static uint32_t& high(Range& r) { return r.end; }
  static const uint32_t& low(const Range& r) { return r.start; }
  static const uint32_t& high(const Range& r) { return r.end; }
};

using namespace sigil::core;

TEST(Intervals, TheNormalFormClampsSortsAndMerges) {
  const std::vector<Arc> normal = normalizeIntervals<Arc>(
      {{0.6f, 0.9f}, {-0.2f, 0.3f}, {0.25f, 0.5f}, {0.4f, 0.4f}, {0.8f, 1.4f}},
      0.0f, 1.0f, 1e-6f);
  ASSERT_EQ(normal.size(), 2u);
  // Clamped to the bounds, sorted, and the two that touch merged into one.
  EXPECT_EQ(normal[0], (Arc{0.0f, 0.5f}));
  EXPECT_EQ(normal[1], (Arc{0.6f, 1.0f}));
}

TEST(Intervals, ABackwardsRunIsEitherTurnedRoundOrDropped) {
  const std::vector<Arc> swapped = normalizeIntervals<Arc>(
      {{0.7f, 0.2f}}, 0.0f, 1.0f, 1e-6f, Inverted::Swap);
  ASSERT_EQ(swapped.size(), 1u);
  EXPECT_EQ(swapped[0], (Arc{0.2f, 0.7f}));
  EXPECT_TRUE(normalizeIntervals<Arc>({{0.7f, 0.2f}}, 0.0f, 1.0f, 1e-6f,
                                      Inverted::Drop)
                  .empty());
}

TEST(Intervals, TheComplementIsWhatTheSetLeavesInsideTheBounds) {
  const std::vector<Arc> gaps =
      complementIntervals<Arc>({{0.2f, 0.5f}, {0.7f, 0.9f}}, 0.0f, 1.0f, 1e-6f);
  ASSERT_EQ(gaps.size(), 3u);
  EXPECT_EQ(gaps[0], (Arc{0.0f, 0.2f}));
  EXPECT_EQ(gaps[1], (Arc{0.5f, 0.7f}));
  EXPECT_EQ(gaps[2], (Arc{0.9f, 1.0f}));
  // A set that covers everything leaves nothing.
  EXPECT_TRUE(complementIntervals<Arc>({{0.0f, 1.0f}}, 0.0f, 1.0f, 1e-6f)
                  .empty());
}

TEST(Intervals, TheIntersectionIsOneSweepAndTouchingIsNotSharing) {
  const std::vector<Arc> shared = intersectIntervals<Arc>(
      {{0.0f, 0.4f}, {0.6f, 1.0f}}, {{0.3f, 0.7f}}, 1e-6f);
  ASSERT_EQ(shared.size(), 2u);
  EXPECT_EQ(shared[0], (Arc{0.3f, 0.4f}));
  EXPECT_EQ(shared[1], (Arc{0.6f, 0.7f}));
  // Two runs meeting at a point share no length.
  EXPECT_TRUE(
      intersectIntervals<Arc>({{0.0f, 0.5f}}, {{0.5f, 1.0f}}, 1e-6f).empty());
}

TEST(Intervals, TheOverlapReportReadsByItsOwnThreshold) {
  const std::vector<Arc> a = {{0.0f, 0.5f}};
  const std::vector<Arc> b = {{0.49999f, 1.0f}};
  // A sliver is an intersection at the tight threshold…
  EXPECT_FALSE(intersectIntervals<Arc>(a, b, 1e-6f).empty());
  // …and not a conflict at the loose one a report reads by.
  EXPECT_FALSE(firstOverlap<Arc>(a, b, 1e-4f).has_value());
  EXPECT_EQ(firstOverlap<Arc>(a, {{0.2f, 1.0f}}, 1e-4f), (Arc{0.2f, 0.5f}));
}

TEST(Intervals, AnIntegerEndpointNeedsNoEpsilonAndItsOwnNames) {
  const std::vector<Range> normal = normalizeIntervals<Range>(
      {{10, 20}, {3, 3}, {0, 5}, {5, 8}}, 0u, 100u);
  ASSERT_EQ(normal.size(), 2u);
  // 0..5 and 5..8 are adjacent half-open runs, so they merge.
  EXPECT_EQ(normal[0], (Range{0, 8}));
  EXPECT_EQ(normal[1], (Range{10, 20}));
  const std::vector<Range> gaps =
      complementIntervals<Range>(normal, 0u, 100u);
  ASSERT_EQ(gaps.size(), 2u);
  EXPECT_EQ(gaps[0], (Range{8, 10}));
  EXPECT_EQ(gaps[1], (Range{20, 100}));
  EXPECT_EQ(intersectIntervals<Range>(normal, {{6, 12}}).size(), 2u);
}
