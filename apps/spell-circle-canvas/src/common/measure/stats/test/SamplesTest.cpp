/** @file
 * The one quantile every ring and every one-off sample list shares, the
 * several-fractions call that must agree with it off one sort, and the
 * rolling ring itself.
 */

#include <gtest/gtest.h>
#include <sigilmeasure/stats/Samples.h>

#include <vector>

using namespace sigil::measure;

TEST(Quantile, EmptyReadsZero) { EXPECT_DOUBLE_EQ(quantile({}, 0.5), 0.0); }

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

TEST(Samples, EmptyReadsZeroEverywhere) {
  Samples w(4);
  EXPECT_TRUE(w.empty());
  EXPECT_EQ(w.size(), 0u);
  EXPECT_DOUBLE_EQ(w.mean(), 0.0);
  EXPECT_DOUBLE_EQ(w.percentile(0.99), 0.0);
  EXPECT_DOUBLE_EQ(w.min(), 0.0);
  EXPECT_DOUBLE_EQ(w.max(), 0.0);
  EXPECT_DOUBLE_EQ(w.last(), 0.0);
}

TEST(Samples, WrapsAroundKeepingTheNewest) {
  Samples w(3);
  for (double v : {1.0, 2.0, 3.0, 4.0, 5.0}) w.add(v);
  EXPECT_EQ(w.size(), 3u);
  EXPECT_EQ(w.capacity(), 3u);
  EXPECT_EQ(w.samples(), (std::vector<double>{3, 4, 5}));
  EXPECT_DOUBLE_EQ(w.mean(), 4.0);
  EXPECT_DOUBLE_EQ(w.min(), 3.0);
  EXPECT_DOUBLE_EQ(w.max(), 5.0);
  EXPECT_DOUBLE_EQ(w.last(), 5.0);
  EXPECT_DOUBLE_EQ(w.percentile(0.5), 4.0);
}

TEST(Samples, OrderSurvivesExactlyOneLap) {
  Samples w(3);
  for (double v : {1.0, 2.0, 3.0}) w.add(v);
  EXPECT_EQ(w.samples(), (std::vector<double>{1, 2, 3}));
  w.add(4.0);
  EXPECT_EQ(w.samples(), (std::vector<double>{2, 3, 4}));
  EXPECT_DOUBLE_EQ(w.last(), 4.0);
}

TEST(Samples, ClearForgetsButKeepsCapacity) {
  Samples w(2);
  w.add(1.0);
  w.clear();
  EXPECT_TRUE(w.empty());
  EXPECT_EQ(w.capacity(), 2u);
  w.add(9.0);
  EXPECT_DOUBLE_EQ(w.last(), 9.0);
}

TEST(Samples, ZeroCapacityHoldsOne) {
  Samples w(0);
  w.add(1.0);
  w.add(2.0);
  EXPECT_EQ(w.size(), 1u);
  EXPECT_DOUBLE_EQ(w.last(), 2.0);
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

  EXPECT_TRUE(quantiles({}, fractions).size() == fractions.size());
  EXPECT_DOUBLE_EQ(quantiles({}, fractions)[0], 0.0);
}

TEST(Quantiles, TheMedianIsTheMiddleAndInterpolatesAcrossAnEvenRun) {
  const std::vector<double> even = {1.0, 2.0, 3.0, 4.0};
  EXPECT_DOUBLE_EQ(median(even), 2.5);
  const std::vector<double> odd = {1.0, 2.0, 3.0};
  EXPECT_DOUBLE_EQ(median(odd), 2.0);
}
