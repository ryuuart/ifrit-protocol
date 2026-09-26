/** @file
 * The instruments a live stream is read with: the window of its last few
 * values, the smoothed reading that follows it, and the rate of its
 * events. Every stamp here is stated, never read off a clock, so each
 * claim is arithmetic.
 */

#include <gtest/gtest.h>
#include <sigilmeasure/stats/Rate.h>
#include <sigilmeasure/stats/Smoothed.h>
#include <sigilmeasure/stats/Window.h>

#include <chrono>
#include <cmath>
#include <vector>

using namespace sigil::measure;
using namespace std::chrono_literals;

namespace {
std::vector<double> held(const Window<>& window) {
  return {window.values().begin(), window.values().end()};
}
}  // namespace

TEST(Window, EmptyReadsZeroEverywhere) {
  const Window<> window(4);
  EXPECT_TRUE(window.empty());
  EXPECT_EQ(window.size(), 0u);
  EXPECT_DOUBLE_EQ(window.mean(), 0.0);
  EXPECT_DOUBLE_EQ(window.quantile(0.99), 0.0);
  EXPECT_DOUBLE_EQ(window.min(), 0.0);
  EXPECT_DOUBLE_EQ(window.max(), 0.0);
  EXPECT_DOUBLE_EQ(window.latest(), 0.0);
  EXPECT_TRUE(window.values().empty());
}

TEST(Window, ACountKeepsTheNewestInOrder) {
  Window window{3};  // a bare count deduces a window of doubles
  static_assert(std::is_same_v<decltype(window), Window<double>>);
  for (double value : {1.0, 2.0, 3.0, 4.0, 5.0}) window.add(value);
  EXPECT_EQ(window.size(), 3u);
  EXPECT_EQ(held(window), (std::vector<double>{3, 4, 5}));
  EXPECT_DOUBLE_EQ(window.mean(), 4.0);
  EXPECT_DOUBLE_EQ(window.min(), 3.0);
  EXPECT_DOUBLE_EQ(window.max(), 5.0);
  EXPECT_DOUBLE_EQ(window.latest(), 5.0);
  EXPECT_DOUBLE_EQ(window.quantile(0.5), 4.0);
}

TEST(Window, StaysContiguousAndInOrderAcrossManyLaps) {
  // The held run is released from the front in blocks; a long stream
  // still reads as the last `count` values, oldest first, every time.
  Window<> window(5);
  for (int value = 0; value < 1000; ++value) {
    window.add((double)value);
    const std::vector<double> run = held(window);
    ASSERT_EQ(run.back(), (double)value);
    for (size_t index = 1; index < run.size(); ++index)
      ASSERT_EQ(run[index], run[index - 1] + 1.0);
  }
  EXPECT_EQ(held(window), (std::vector<double>{995, 996, 997, 998, 999}));
}

TEST(Window, ASpanDropsWhatIsOlderThanItsReach) {
  Window<> recent({.span = 4s});
  recent.add(1.0, 0s);
  recent.add(2.0, 1s);
  recent.add(3.0, 3s);
  EXPECT_EQ(recent.size(), 3u);
  recent.add(4.0, 5s);  // 0 s is now more than four seconds back
  EXPECT_EQ(held(recent), (std::vector<double>{2, 3, 4}));
  ASSERT_EQ(recent.times().size(), 3u);
  EXPECT_EQ(recent.times().front(), Duration(1s));
  recent.advance(8s);  // a silent stream empties as its time moves on
  EXPECT_EQ(held(recent), (std::vector<double>{4}));
  recent.advance(20s);
  EXPECT_TRUE(recent.empty());
}

TEST(Window, AStampFromThePastDoesNotWindTheWindowBack) {
  Window<> recent({.span = 2s});
  recent.add(1.0, 10s);
  recent.add(2.0, 3s);  // late, and already out of reach of 10 s
  EXPECT_EQ(held(recent), (std::vector<double>{1}));
}

TEST(Window, OfSpansOfTimeReadsInTheirUnit) {
  Window<Duration> frames(4);
  frames.add(10ms);
  frames.add(20ms);
  EXPECT_DOUBLE_EQ(Milliseconds(frames.mean()).count(), 15.0);
  EXPECT_DOUBLE_EQ(Milliseconds(frames.max()).count(), 20.0);
}

TEST(Window, ClearForgetsButKeepsItsBounds) {
  Window<> window(2);
  window.add(1.0);
  window.clear();
  EXPECT_TRUE(window.empty());
  EXPECT_EQ(window.options().count, 2u);
  window.add(9.0);
  EXPECT_DOUBLE_EQ(window.latest(), 9.0);
}

TEST(Smoothed, TakesTheFirstValueWholeThenFollowsAtTwoOverNPlusOne) {
  Smoothed level{3};  // weight 2 / (3 + 1) = 0.5
  EXPECT_TRUE(level.empty());
  level.add(10.0);
  EXPECT_DOUBLE_EQ(level.value(), 10.0);
  level.add(20.0);
  EXPECT_DOUBLE_EQ(level.value(), 15.0);
  level.add(20.0);
  EXPECT_DOUBLE_EQ(level.value(), 17.5);
}

TEST(Smoothed, AStatedWeightIsTheWeightUsed) {
  // `old · 0.85 + new · 0.15`, written as an option, is that number.
  Smoothed build({.weight = 0.15});
  build.add(100.0);
  build.add(200.0);
  EXPECT_DOUBLE_EQ(build.value(), 100.0 * 0.85 + 200.0 * 0.15);
}

TEST(Smoothed, ATimeConstantFollowsTheSameWhateverTheStep) {
  // One second in one step and one second in ten land in the same place.
  Smoothed once({.timeConstant = 1s});
  Smoothed tenTimes({.timeConstant = 1s});
  once.add(0.0, 0s);
  tenTimes.add(0.0, 0s);
  once.add(1.0, 1s);
  for (int step = 0; step < 10; ++step) tenTimes.add(1.0, 100ms);
  EXPECT_NEAR(once.value(), 1.0 - std::exp(-1.0), 1e-12);
  EXPECT_NEAR(tenTimes.value(), once.value(), 1e-12);
}

TEST(Smoothed, APeakRisesAtOnceAndFallsSlowly) {
  Smoothed peak({.over = 3, .peak = true});
  peak.add(1.0);
  peak.add(9.0);
  EXPECT_DOUBLE_EQ(peak.value(), 9.0);
  peak.add(1.0);
  EXPECT_DOUBLE_EQ(peak.value(), 5.0);
  peak.clear();
  EXPECT_TRUE(peak.empty());
}

TEST(Rate, CountsTheEventsOfTheLastSpanPerSecond) {
  Rate arrivals{4s};
  for (int second = 0; second < 8; ++second)
    arrivals.mark(Duration(second * 1.0));
  // At 7 s the span reaches back to 3 s: 3, 4, 5, 6 and 7.
  EXPECT_EQ(arrivals.count(), 5u);
  EXPECT_DOUBLE_EQ(arrivals.perSecond(), 5.0 / 4.0);
  EXPECT_EQ(arrivals.times().front(), Duration(3s));
  arrivals.advance(30s);
  EXPECT_EQ(arrivals.count(), 0u);
  EXPECT_DOUBLE_EQ(arrivals.perSecond(), 0.0);
}
