/** @file
 * The statistics a study is summarised with, against closed forms.
 *
 * Every claim here has an arithmetic answer that can be written down —
 * the variance of the first n whole numbers, the skewness of three zeros
 * and a one, the density of a flat run, a z-score's own mean and
 * deviation — so the cases assert those rather than a number this code
 * once produced. One claim is of a different kind and is the reason the
 * accumulation is shaped the way it is: a run of large, close values,
 * where the obvious formula loses every digit and can answer a negative
 * spread.
 */

#include <gtest/gtest.h>
#include <sigilmeasure/stats/Fit.h>
#include <sigilmeasure/stats/Histogram.h>
#include <sigilmeasure/stats/Moments.h>
#include <sigilmeasure/stats/Rescale.h>
#include <sigilmeasure/stats/Samples.h>

#include <cmath>
#include <numeric>
#include <vector>

namespace {

using namespace sigil::measure;

/** 1 to n. Its mean is (n + 1) / 2 and its variance over the whole
 *  population is (n² − 1) / 12, both of which are written down rather
 *  than computed a second way. */
std::vector<double> counting(int n) {
  std::vector<double> values((size_t)n);
  std::iota(values.begin(), values.end(), 1.0);
  return values;
}

}  // namespace

// ---- the running moments --------------------------------------------------

TEST(Moments, AreTheMeanAndSpreadTheClosedFormGivesForACountingRun) {
  const std::vector<double> values = counting(100);
  const Moments moments = Moments::of(values);
  EXPECT_EQ(moments.count(), 100u);
  EXPECT_DOUBLE_EQ(moments.mean(), 50.5);
  EXPECT_DOUBLE_EQ(moments.sum(), 5050.0);
  EXPECT_NEAR(moments.variance(), (100.0 * 100.0 - 1.0) / 12.0, 1e-9);
  EXPECT_NEAR(moments.sd(), std::sqrt((100.0 * 100.0 - 1.0) / 12.0), 1e-9);
  EXPECT_DOUBLE_EQ(moments.min(), 1.0);
  EXPECT_DOUBLE_EQ(moments.max(), 100.0);
  EXPECT_DOUBLE_EQ(moments.range(), 99.0);
}

TEST(Moments, TellApartTheRunInHandFromWhatItWasDrawnFrom) {
  const std::vector<double> values = counting(10);
  const Moments moments = Moments::of(values);
  // Bessel's correction is exactly the ratio n / (n − 1), and nothing
  // else about the two answers differs.
  EXPECT_NEAR(moments.sampleVariance(), moments.variance() * 10.0 / 9.0, 1e-12);
  EXPECT_NEAR(moments.sampleSd(), std::sqrt(moments.sampleVariance()), 1e-12);

  Moments one;
  one.add(7.0);
  // One value is a population with no spread and a sample that claims
  // nothing about the spread of what it came from.
  EXPECT_DOUBLE_EQ(one.variance(), 0.0);
  EXPECT_DOUBLE_EQ(one.sampleVariance(), 0.0);
}

TEST(Moments, KeepTheSpreadOfValuesThatAreLargeAndCloseTogether) {
  // The reason the deviation is accumulated rather than subtracted. The
  // values below differ from each other by ones and from zero by a
  // hundred million, so the mean of the squares and the square of the
  // mean agree to fifteen digits and their difference is nearly all
  // rounding — the answer that formula gives can even be negative.
  const double base = 1.0e8;
  std::vector<double> values, centred;
  for (int i = 0; i < 1000; ++i) {
    values.push_back(base + (double)i);
    centred.push_back((double)i);
  }
  const Moments moments = Moments::of(values);
  const Moments shifted = Moments::of(centred);
  EXPECT_DOUBLE_EQ(moments.mean(), base + shifted.mean());
  // Shifting a run does not change its spread, which is the property the
  // naive formula loses and this one keeps.
  EXPECT_NEAR(moments.variance(), shifted.variance(),
              shifted.variance() * 1e-9);
  EXPECT_GT(moments.variance(), 0.0);

  double sumSquares = 0.0, sum = 0.0;
  for (double value : values) {
    sum += value;
    sumSquares += value * value;
  }
  const double naive =
      sumSquares / (double)values.size() -
      (sum / (double)values.size()) * (sum / (double)values.size());
  EXPECT_GT(std::abs(naive - shifted.variance()), 1.0);
}

TEST(Moments, ReportWhichWayTheLongTailRuns) {
  // Anything symmetric about its mean is unskewed.
  EXPECT_NEAR(Moments::of(counting(9)).skewness(), 0.0, 1e-12);

  // Three zeros and a one: the mean is a quarter, the spread three
  // sixteenths, and the skewness two over the square root of three.
  const std::vector<double> lopsided = {0.0, 0.0, 0.0, 1.0};
  const Moments moments = Moments::of(lopsided);
  EXPECT_DOUBLE_EQ(moments.mean(), 0.25);
  EXPECT_DOUBLE_EQ(moments.variance(), 3.0 / 16.0);
  EXPECT_NEAR(moments.skewness(), 2.0 / std::sqrt(3.0), 1e-12);
  // The same run mirrored runs the other way by exactly as much.
  const std::vector<double> mirrored = {1.0, 1.0, 1.0, 0.0};
  EXPECT_NEAR(Moments::of(mirrored).skewness(), -2.0 / std::sqrt(3.0), 1e-12);

  // A run with no spread has no shape to report.
  const std::vector<double> flat = {5.0, 5.0, 5.0};
  EXPECT_DOUBLE_EQ(Moments::of(flat).skewness(), 0.0);
}

TEST(Moments, MergeToWhatOnePassOverTheWholeRunWouldHaveSaid) {
  const std::vector<double> whole = {3.0, 1.0, 4.0, 1.0, 5.0,
                                     9.0, 2.0, 6.0, 5.0, 3.5};
  const Moments once = Moments::of(whole);

  Moments front, back;
  for (size_t i = 0; i < 4; ++i) front.add(whole[i]);
  for (size_t i = 4; i < whole.size(); ++i) back.add(whole[i]);
  front.merge(back);

  EXPECT_EQ(front.count(), once.count());
  EXPECT_NEAR(front.mean(), once.mean(), 1e-12);
  EXPECT_NEAR(front.variance(), once.variance(), 1e-12);
  EXPECT_NEAR(front.skewness(), once.skewness(), 1e-10);
  EXPECT_DOUBLE_EQ(front.min(), once.min());
  EXPECT_DOUBLE_EQ(front.max(), once.max());

  // An empty half changes nothing, from either side.
  Moments empty;
  Moments copy = once;
  copy.merge(empty);
  EXPECT_EQ(copy.count(), once.count());
  empty.merge(once);
  EXPECT_NEAR(empty.mean(), once.mean(), 1e-12);
}

TEST(Moments, ReadZeroEverywhereBeforeAnythingIsAdded) {
  const Moments moments;
  EXPECT_TRUE(moments.empty());
  EXPECT_EQ(moments.count(), 0u);
  EXPECT_DOUBLE_EQ(moments.mean(), 0.0);
  EXPECT_DOUBLE_EQ(moments.sum(), 0.0);
  EXPECT_DOUBLE_EQ(moments.variance(), 0.0);
  EXPECT_DOUBLE_EQ(moments.skewness(), 0.0);
  // Not an infinite span: an empty summary reads as empty.
  EXPECT_DOUBLE_EQ(moments.min(), 0.0);
  EXPECT_DOUBLE_EQ(moments.max(), 0.0);
}

// ---- the histogram --------------------------------------------------------

TEST(Histogram, PutsEveryValueInTheBinItsEdgesName) {
  Histogram histogram(0.0, 10.0, 5);
  EXPECT_EQ(histogram.bins(), 5u);
  EXPECT_DOUBLE_EQ(histogram.binWidth(), 2.0);
  EXPECT_DOUBLE_EQ(histogram.edge(0), 0.0);
  EXPECT_DOUBLE_EQ(histogram.edge(5), 10.0);
  EXPECT_DOUBLE_EQ(histogram.centre(0), 1.0);

  // Half-open bins: a value on a boundary belongs to the bin above it.
  EXPECT_EQ(histogram.binOf(0.0), 0u);
  EXPECT_EQ(histogram.binOf(1.999), 0u);
  EXPECT_EQ(histogram.binOf(2.0), 1u);
  // Except the high edge itself, which belongs to the last bin rather
  // than to nothing.
  EXPECT_EQ(histogram.binOf(10.0), 4u);
}

TEST(Histogram, CountsWhatFellOutsideRatherThanDroppingOrClampingIt) {
  Histogram histogram(0.0, 1.0, 4);
  histogram.add(-0.5);
  histogram.add(-1.0);
  histogram.add(2.0);
  histogram.add(0.5);
  EXPECT_DOUBLE_EQ(histogram.below(), 2.0);
  EXPECT_DOUBLE_EQ(histogram.above(), 1.0);
  EXPECT_DOUBLE_EQ(histogram.total(), 1.0);
  // Clamping would have made a wall at the ends that is not in the data.
  EXPECT_DOUBLE_EQ(histogram.count(0), 0.0);
  EXPECT_DOUBLE_EQ(histogram.count(3), 0.0);
  EXPECT_DOUBLE_EQ(histogram.count(2), 1.0);
}

TEST(Histogram, BinsAFlatRunFlatAndItsDensityIntegratesToOne) {
  Histogram histogram(0.0, 1.0, 10);
  for (int i = 0; i < 1000; ++i) histogram.add(((double)i + 0.5) / 1000.0);
  for (size_t bin = 0; bin < histogram.bins(); ++bin) {
    EXPECT_DOUBLE_EQ(histogram.count(bin), 100.0) << "bin " << bin;
    EXPECT_NEAR(histogram.fraction(bin), 0.1, 1e-12);
    // A flat run over a unit interval has a density of one everywhere.
    EXPECT_NEAR(histogram.density(bin), 1.0, 1e-12);
  }
  double integral = 0.0;
  for (size_t bin = 0; bin < histogram.bins(); ++bin)
    integral += histogram.density(bin) * histogram.binWidth();
  EXPECT_NEAR(integral, 1.0, 1e-12);
  EXPECT_DOUBLE_EQ(histogram.total(), 1000.0);
}

TEST(Histogram, TakesWeightsSoAResampledRunNeedsNoSecondClass) {
  Histogram histogram(0.0, 2.0, 2);
  histogram.add(0.5, 3.0);
  histogram.add(1.5, 1.0);
  EXPECT_DOUBLE_EQ(histogram.count(0), 3.0);
  EXPECT_DOUBLE_EQ(histogram.count(1), 1.0);
  EXPECT_DOUBLE_EQ(histogram.total(), 4.0);
  EXPECT_NEAR(histogram.fraction(0), 0.75, 1e-12);
  EXPECT_EQ(histogram.mode(), 0u);
  EXPECT_DOUBLE_EQ(histogram.peak(), 3.0);
}

TEST(Histogram, OverARunTakesItsRangeFromTheRunSoNothingFallsOutside) {
  const std::vector<double> values = {2.0, 4.0, 6.0, 8.0};
  const Histogram histogram = Histogram::over(values, 4);
  EXPECT_DOUBLE_EQ(histogram.low(), 2.0);
  EXPECT_DOUBLE_EQ(histogram.high(), 8.0);
  EXPECT_DOUBLE_EQ(histogram.below(), 0.0);
  EXPECT_DOUBLE_EQ(histogram.above(), 0.0);
  EXPECT_DOUBLE_EQ(histogram.total(), 4.0);
}

TEST(Histogram, ARangeThatIsEmptyOrBackwardsIsOneBinAndNotADivideByZero) {
  Histogram backwards(5.0, 1.0, 4);
  EXPECT_DOUBLE_EQ(backwards.high(), 5.0);
  backwards.add(5.0);
  EXPECT_DOUBLE_EQ(backwards.total(), 1.0);
  EXPECT_EQ(backwards.binOf(5.0), 0u);

  Histogram none(0.0, 1.0, 0);
  EXPECT_EQ(none.bins(), 1u);
  none.add(0.5);
  EXPECT_DOUBLE_EQ(none.count(0), 1.0);
}

// ---- the derived rescaling ------------------------------------------------

TEST(Rescale, AZScoreLeavesTheRunWithNoMeanAndOneDeviation) {
  const std::vector<double> values = counting(50);
  const Rescale standardise = zScore(values);
  std::vector<double> mapped;
  mapped.reserve(values.size());
  for (double value : values) mapped.push_back(standardise(value));
  const Moments after = Moments::of(mapped);
  EXPECT_NEAR(after.mean(), 0.0, 1e-12);
  EXPECT_NEAR(after.sd(), 1.0, 1e-12);
  // Two runs in different units become comparable exactly because the
  // same value's standing in each is the same number.
  std::vector<double> scaled;
  for (double value : values) scaled.push_back(value * 1000.0 + 7.0);
  const Rescale other = zScore(scaled);
  for (size_t i = 0; i < values.size(); ++i)
    EXPECT_NEAR(standardise(values[i]), other(scaled[i]), 1e-9);
}

TEST(Rescale, AUnitRangePutsTheEndsOfTheRunAtTheEndsOfTheSpan) {
  const std::vector<double> values = {-4.0, 0.0, 6.0};
  const Rescale squeeze = unitRange(values);
  EXPECT_DOUBLE_EQ(squeeze(-4.0), 0.0);
  EXPECT_DOUBLE_EQ(squeeze(6.0), 1.0);
  EXPECT_DOUBLE_EQ(squeeze(1.0), 0.5);

  const Rescale chosen = unitRange(values, 100.0, 200.0);
  EXPECT_DOUBLE_EQ(chosen(-4.0), 100.0);
  EXPECT_DOUBLE_EQ(chosen(6.0), 200.0);
}

TEST(Rescale, IsInvertibleSoAReadingTakenOffADrawingGoesBack) {
  const std::vector<double> values = counting(20);
  for (const Rescale& map : {zScore(values), unitRange(values, -1.0, 1.0)})
    for (double value : values)
      EXPECT_NEAR(map.invert(map(value)), value, 1e-9);
}

TEST(Rescale, ARunWithNoWidthCollapsesRatherThanDividingByZero) {
  const std::vector<double> flat = {3.0, 3.0, 3.0};
  const Rescale standardise = zScore(flat);
  EXPECT_DOUBLE_EQ(standardise(3.0), 0.0);
  EXPECT_DOUBLE_EQ(standardise(9.0), 0.0);
  // A collapsed map cannot be undone; it answers where the run was.
  EXPECT_DOUBLE_EQ(standardise.invert(0.0), 3.0);

  const Rescale squeeze = unitRange(flat, 2.0, 8.0);
  EXPECT_DOUBLE_EQ(squeeze(3.0), 2.0);

  const Rescale nothing = zScore({});
  EXPECT_DOUBLE_EQ(nothing(4.0), 0.0);
}

// ---- the quantiles and the correlation ------------------------------------

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

TEST(LineFit, TheCorrelationSaysTheDirectionThatR2CannotSay) {
  const std::vector<double> xs = {0.0, 1.0, 2.0, 3.0};
  const std::vector<double> rising = {1.0, 3.0, 5.0, 7.0};
  const std::vector<double> falling = {7.0, 5.0, 3.0, 1.0};

  const LineFit<double> up = lineFit<double>(xs, rising);
  const LineFit<double> down = lineFit<double>(xs, falling);
  // Both explain everything, which is all r2 can say; the correlation
  // separates them.
  EXPECT_NEAR(up.r2, 1.0, 1e-12);
  EXPECT_NEAR(down.r2, 1.0, 1e-12);
  EXPECT_NEAR(up.correlation(), 1.0, 1e-12);
  EXPECT_NEAR(down.correlation(), -1.0, 1e-12);

  // And on a run that is not a line, it is the root of r2 with the
  // slope's sign and nothing else.
  const std::vector<double> scattered = {1.0, 4.0, 2.0, 8.0};
  const LineFit<double> loose = lineFit<double>(xs, scattered);
  EXPECT_NEAR(std::abs(loose.correlation()), std::sqrt(loose.r2), 1e-12);
  EXPECT_EQ(loose.correlation() < 0, loose.slope < 0);

  // Nothing to fit is no correlation rather than a divide by zero.
  const std::vector<double> flat = {2.0, 2.0, 2.0, 2.0};
  EXPECT_DOUBLE_EQ(lineFit<double>(flat, flat).correlation(), 0.0);
}
