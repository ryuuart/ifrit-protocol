/** @file
 * Shape interpolation: step counts, spacing modes and colour blending.
 */

#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>

#include "sigilgeometry/blend/Blend.h"

using namespace sigil::geometry;

TEST(Blend, EndpointsMatchKeysExactly) {
  blend::Key from{SkPath::Circle(100, 100, 50), {1, 0, 0, 1}};
  blend::Key to{SkPath::Circle(400, 100, 30), {0, 0, 1, 1}};
  blend::Options options;
  options.steps = 3;
  const std::vector<blend::Step> steps = blend::make(from, to, options);
  ASSERT_EQ(steps.size(), 5u);  // 2 keys + 3 intermediates
  EXPECT_EQ(steps.front().t, 0.0f);
  EXPECT_EQ(steps.back().t, 1.0f);
  // Endpoint colors are the key colors (OKLab is identity at t=0/1).
  EXPECT_NEAR(steps.front().fill.fR, 1.0f, 0.01f);
  EXPECT_NEAR(steps.back().fill.fB, 1.0f, 0.01f);
  // Midpoint centroid sits between the keys.
  const SkRect mid = steps[2].path.computeTightBounds();
  EXPECT_NEAR(mid.centerX(), 250.0f, 2.0f);
  EXPECT_NEAR(mid.centerY(), 100.0f, 2.0f);
}

TEST(Blend, SmoothColorScalesWithColorDistance) {
  blend::Key white{SkPath::Circle(0, 0, 10), {1, 1, 1, 1}};
  blend::Key black{SkPath::Circle(100, 0, 10), {0, 0, 0, 1}};
  blend::Key nearWhite{SkPath::Circle(100, 0, 10), {0.95f, 0.95f, 0.95f, 1}};
  blend::Options options;
  options.spacing = blend::Spacing::SmoothColor;
  const size_t far = blend::make(white, black, options).size();
  const size_t near = blend::make(white, nearWhite, options).size();
  // Spacing::SmoothColor picks the step count from the perceptual distance
  // between the two key colours, so that each step is a just-noticeable
  // change: black to white needs a step per 8-bit level, two nearly equal
  // greys need a handful.
  EXPECT_GT(far, 200u);
  EXPECT_LT(near, 40u);
}

TEST(Blend, DistanceSpacingCountsSpineLength) {
  blend::Key from{SkPath::Circle(0, 0, 10), {1, 0, 0, 1}};
  blend::Key to{SkPath::Circle(300, 0, 10), {0, 1, 0, 1}};
  blend::Options options;
  options.spacing = blend::Spacing::Distance;
  options.distance = 50;
  // 300px span / 50px = 6 slots -> 5 intermediates + 2 keys.
  const std::vector<blend::Step> steps = blend::make(from, to, options);
  EXPECT_EQ(steps.size(), 7u);
}

TEST(Blend, OklabMidGrayIsPerceptual) {
  const SkColor4f mid =
      blend::detail::lerpOklab({0, 0, 0, 1}, {1, 1, 1, 1}, 0.5f);
  // OKLab L is cube-root lightness: its black-white midpoint is linear
  // luminance 0.125 = sRGB ~0.389 — well below a naive sRGB lerp's 0.5
  // and far below a linear-light lerp's 0.735.
  EXPECT_NEAR(mid.fR, 0.389f, 0.03f);
  EXPECT_NEAR(mid.fR, mid.fG, 0.01f);
  EXPECT_NEAR(mid.fG, mid.fB, 0.01f);
}

TEST(Blend, SpinePlacesStepsAlongPath) {
  SkPathBuilder spine;
  spine.moveTo({0, 0});
  spine.lineTo({0, 400});  // vertical spine
  blend::Key from{SkPath::Circle(0, 0, 10), {1, 0, 0, 1}};
  blend::Key to{SkPath::Circle(0, 0, 10), {0, 1, 0, 1}};  // same spot
  blend::Options options;
  options.steps = 3;
  options.spine = spine.detach();
  const std::vector<blend::Step> steps = blend::make(from, to, options);
  ASSERT_EQ(steps.size(), 5u);
  // Steps should march down the vertical spine.
  float lastY = -1;
  for (const blend::Step& step : steps) {
    const float y = step.path.computeTightBounds().centerY();
    EXPECT_GT(y, lastY);
    lastY = y;
  }
  EXPECT_NEAR(steps.back().path.computeTightBounds().centerY(), 400.0f, 2.0f);
}
