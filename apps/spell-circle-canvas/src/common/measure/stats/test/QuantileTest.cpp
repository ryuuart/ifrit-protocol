/** @file
 * The one quantile every window and every one-off run shares, and the
 * several-fractions call that must agree with it off one sort.
 */

#include <gtest/gtest.h>
#include <sigilmeasure/advanced/Quantiles.h>
#include <sigilmeasure/stats/Quantile.h>

#include <chrono>
#include <span>
#include <vector>

using namespace sigil::measure;

TEST(Quantile, EmptyReadsZero) {
  EXPECT_DOUBLE_EQ(quantile(std::vector<double>{}, 0.5), 0.0);
}

TEST(Quantile, OneSampleReadsItselfEverywhere) {
  const double one[] = {7.5};
  EXPECT_DOUBLE_EQ(quantile(one, 0.0), 7.5);
  EXPECT_DOUBLE_EQ(quantile(one, 0.5), 7.5);
  EXPECT_DOUBLE_EQ(quantile(one, 1.0), 7.5);
}

TEST(Quantile, InterpolatesBetweenRanks) {
  const double four[] = {4, 1, 3, 2};  // unsorted on purpose
  EXPECT_DOUBLE_EQ(quantile(four, 0.0), 1.0);
  EXPECT_DOUBLE_EQ(quantile(four, 0.5), 2.5);
  EXPECT_DOUBLE_EQ(quantile(four, 1.0), 4.0);
  EXPECT_DOUBLE_EQ(quantile(four, 1.0 / 3.0), 2.0);
  EXPECT_NEAR(quantile(four, 0.99), 3.97, 1e-9);
}

TEST(Quantile, ClampsTheFraction) {
  const double two[] = {1, 2};
  EXPECT_DOUBLE_EQ(quantile(two, -1.0), 1.0);
  EXPECT_DOUBLE_EQ(quantile(two, 2.0), 2.0);
}

TEST(Quantile, LeavesTheInputAlone) {
  std::vector<double> v = {3, 1, 2};
  (void)quantile(v, 0.5);
  EXPECT_EQ(v, (std::vector<double>{3, 1, 2}));
}

TEST(Quantiles, AreEachTheOneQuantileWouldHaveGivenForOneSort) {
  const std::vector<double> values = {4.0, 1.0, 3.0, 2.0, 9.0, 7.0, 5.0};
  const std::vector<double> fractions = {0.0, 0.5, 0.9, 0.25, 1.0};
  const std::vector<double> answers = quantiles(values, fractions);
  ASSERT_EQ(answers.size(), fractions.size());
  for (size_t i = 0; i < fractions.size(); ++i)
    EXPECT_DOUBLE_EQ(answers[i], quantile(values, fractions[i]))
        << "fraction " << fractions[i];
  // The answers come back in the order they were asked for, not sorted.
  EXPECT_DOUBLE_EQ(answers[1], 4.0);
  EXPECT_GT(answers[2], answers[3]);

  const std::vector<double> none;
  EXPECT_TRUE(quantiles(none, fractions).size() == fractions.size());
  EXPECT_DOUBLE_EQ(quantiles(none, fractions)[0], 0.0);
}

TEST(Quantiles, TheMedianIsTheMiddleAndInterpolatesAcrossAnEvenRun) {
  const std::vector<double> even = {1.0, 2.0, 3.0, 4.0};
  EXPECT_DOUBLE_EQ(median(even), 2.5);
  const std::vector<double> odd = {1.0, 2.0, 3.0};
  EXPECT_DOUBLE_EQ(median(odd), 2.0);
}

TEST(Quantile, ReadsAMemberThroughAProjection) {
  struct Reading {
    int index;
    double value;
  };
  const std::vector<Reading> readings = {{0, 4}, {1, 1}, {2, 3}, {3, 2}};
  EXPECT_DOUBLE_EQ(quantile(readings, 0.5, &Reading::value), 2.5);
}

TEST(Quantile, OfSpansOfTimeIsASpanOfTimeInTheirOwnUnit) {
  using namespace std::chrono_literals;
  const std::vector<std::chrono::milliseconds> spans = {1ms, 2ms, 3ms, 4ms};
  // Whole milliseconds are read as fractional ones, so the median of an
  // even run is the half it falls on rather than a rounded whole.
  const auto median = quantile(spans, 0.5);
  static_assert(std::is_same_v<decltype(median),
                               const std::chrono::duration<double, std::milli>>);
  EXPECT_DOUBLE_EQ(median.count(), 2.5);
}
