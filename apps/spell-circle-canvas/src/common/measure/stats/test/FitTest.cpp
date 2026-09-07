/** @file
 * The line a run of points is closest to: found exactly where there is
 * one, answered flat where there is not, and always with the residuals
 * that turn a slope into evidence.
 */

#include <gtest/gtest.h>
#include <sigilmeasure/stats/Fit.h>

#include <cmath>
#include <vector>

using namespace sigil::measure;

TEST(LineFit, APerfectLineIsFoundExactlyAndExplainsEverything) {
  const std::vector<double> xs{0, 1, 2, 3, 4};
  std::vector<double> ys;
  for (double x : xs) ys.push_back(3.5 + 2.25 * x);
  const LineFit<double> fit = lineFit<double>(xs, ys);
  EXPECT_NEAR(fit.slope, 2.25, 1e-12);
  EXPECT_NEAR(fit.intercept, 3.5, 1e-12);
  EXPECT_NEAR(fit.r2, 1.0, 1e-12);
  EXPECT_NEAR(fit.maxResidual, 0.0, 1e-12);
  EXPECT_NEAR(fit.rmsResidual, 0.0, 1e-12);
  EXPECT_EQ(fit.samples, 5u);
  EXPECT_NEAR(fit.at(10.0), 26.0, 1e-12);
}

TEST(LineFit, TheResidualIsWhatTurnsASlopeIntoEvidence) {
  // One point lifted off an otherwise exact line: the slope barely moves
  // and the residual is what says so.
  const std::vector<double> xs{0, 1, 2, 3, 4};
  std::vector<double> ys{0, 1, 2, 3, 4};
  ys[2] += 1.0;
  const LineFit<double> fit = lineFit<double>(xs, ys);
  EXPECT_NEAR(fit.slope, 1.0, 1e-12);
  EXPECT_GT(fit.maxResidual, 0.5);
  EXPECT_LT(fit.r2, 1.0);
  EXPECT_GT(fit.r2, 0.8) << "one lifted point still leaves a good line";
  EXPECT_NEAR(fit.residual(2.0, ys[2]), fit.maxResidual, 1e-12);
}

TEST(LineFit, WhatIsNotALineAnswersNoSlopeRatherThanADivideByZero) {
  const std::vector<double> flat{2, 2, 2, 2};
  const std::vector<double> ys{1, 5, -3, 9};
  const LineFit<double> vertical = lineFit<double>(flat, ys);
  EXPECT_EQ(vertical.slope, 0.0);
  EXPECT_NEAR(vertical.intercept, 3.0, 1e-12) << "the mean of the ordinates";
  EXPECT_EQ(vertical.r2, 0.0);
  EXPECT_NEAR(vertical.maxResidual, 6.0, 1e-12);
  // The residuals off that flat answer are the spread of the ordinates:
  // {1, 5, -3, 9} about 3 is {-2, 2, -6, 6}, so 80/4 under the root.
  EXPECT_NEAR(vertical.rmsResidual, std::sqrt(20.0), 1e-12);
  EXPECT_GT(vertical.rmsResidual, 0.0)
      << "a run with no slope still has ordinates that stand apart";

  const std::vector<double> one{7};
  const LineFit<double> single = lineFit<double>(one, one);
  EXPECT_EQ(single.slope, 0.0);
  EXPECT_NEAR(single.intercept, 7.0, 1e-12);
  EXPECT_DOUBLE_EQ(single.rmsResidual, 0.0)
      << "one point stands exactly on the answer through it";
  EXPECT_EQ(lineFit<double>({}, {}).samples, 0u);
}

TEST(LineFit, TheSumsAreAccumulatedInTheArgumentsOwnPrecision) {
  // A caller that has always fitted in float gets the float answer it
  // had, not a double one rounded back — which is the difference between
  // a drawing that holds and a drawing that moves by a sub-pixel.
  std::vector<float> xs, ys;
  for (int i = 0; i <= 80; ++i) {
    const float v = -27.0f + 70.0f * (float)i / 80.0f;
    xs.push_back(v);
    ys.push_back(std::log(std::tan((45.0f + v * 0.5f) * 0.017453293f)));
  }
  // The same accumulation the fit makes, written out: each point's
  // deviation from the means so far, folded in as it arrives, never the
  // sum of the squares less the square of the sum.
  float mx = 0, my = 0, sxx = 0, sxy = 0;
  for (size_t i = 0; i < xs.size(); ++i) {
    const float count = (float)(i + 1);
    const float dx = xs[i] - mx;
    const float dy = ys[i] - my;
    mx += dx / count;
    my += dy / count;
    sxx += dx * (xs[i] - mx);
    sxy += dx * (ys[i] - my);
  }
  const float b = sxy / sxx;
  const float a = my - b * mx;
  const LineFit<float> fit = lineFit<float>(xs, ys);
  EXPECT_EQ(fit.slope, b);
  EXPECT_EQ(fit.intercept, a);
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
