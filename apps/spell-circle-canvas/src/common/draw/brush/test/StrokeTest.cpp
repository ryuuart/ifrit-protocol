/** @file
 * Strokes: segments and splines with their pressure.
 */

#include <gtest/gtest.h>
#include <sigildraw/brush/Stroke.h>

#include <vector>

namespace {

namespace brush = sigil::draw::brush;

TEST(Stroke, SamplesSegmentsAndSplinesWithPressure) {
  const brush::Stroke line = brush::segment({0, 0}, {10, 0}, 2.0f, 0.25f, 1.0f);
  ASSERT_EQ(line.size(), 6u);
  EXPECT_EQ(line.front().position, SkPoint::Make(0, 0));
  EXPECT_EQ(line.back().position, SkPoint::Make(10, 0));
  EXPECT_FLOAT_EQ(line.front().pressure, 0.25f);
  EXPECT_FLOAT_EQ(line.back().pressure, 1.0f);

  const std::vector<brush::Sample> controls = {
      {{0, 0}, 0.4f}, {{10, 10}, 1.0f}, {{20, 0}, 0.6f}};
  const brush::Stroke curve = brush::spline(controls, 2.0f, 0.8f);
  ASSERT_GT(curve.size(), controls.size());
  EXPECT_EQ(curve.front().position, controls.front().position);
  EXPECT_EQ(curve.back().position, controls.back().position);
  EXPECT_FLOAT_EQ(curve.front().pressure, 0.4f);
  EXPECT_FLOAT_EQ(curve.back().pressure, 0.6f);
}

TEST(Stroke, ZeroCurvatureIsTheChord) {
  const std::vector<brush::Sample> controls = {
      {{0, 0}, 1.0f}, {{10, 10}, 1.0f}, {{20, 0}, 1.0f}};
  const brush::Stroke straight = brush::spline(controls, 5.0f, 0.0f);
  for (const brush::Sample& sample : straight)
    EXPECT_NEAR(sample.position.fY, 10.0f - std::abs(sample.position.fX - 10.0f),
                1e-4f);
}

}  // namespace
