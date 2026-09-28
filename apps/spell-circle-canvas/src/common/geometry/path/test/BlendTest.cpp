/** @file
 * Shape interpolation over resampled contours: how many steps a blend
 * makes, what decides the spacing, and where along a spine they land.
 */

#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>

#include <vector>

#include "sigilgeometry/advanced/Skia.h"
#include "sigilgeometry/path/blend/Blend.h"

using namespace sigil::geometry;
using namespace sigil::geometry::path;

TEST(Blend, EndpointsMatchKeysExactly) {
  blend::Key from{fromSk(SkPath::Circle(100, 100, 50)), {1, 0, 0, 1}};
  blend::Key to{fromSk(SkPath::Circle(400, 100, 30)), {0, 0, 1, 1}};
  blend::Options options;
  options.steps = 3;
  const std::vector<blend::Step> steps = blend::make(from, to, options);
  ASSERT_EQ(steps.size(), 5u);  // 2 keys + 3 intermediates
  EXPECT_EQ(steps.front().t, 0.0f);
  EXPECT_EQ(steps.back().t, 1.0f);
  // Endpoint colors are the key colors (OKLab is identity at t=0/1).
  EXPECT_NEAR(steps.front().fill.r, 1.0f, 0.01f);
  EXPECT_NEAR(steps.back().fill.b, 1.0f, 0.01f);
  // Midpoint centroid sits between the keys.
  const Rect mid = steps[2].path.bounds();
  EXPECT_NEAR((mid.min.x + mid.max.x) * 0.5f, 250.0f, 2.0f);
  EXPECT_NEAR((mid.min.y + mid.max.y) * 0.5f, 100.0f, 2.0f);
}

TEST(Blend, SmoothColorScalesWithColorDistance) {
  blend::Key white{fromSk(SkPath::Circle(0, 0, 10)), {1, 1, 1, 1}};
  blend::Key black{fromSk(SkPath::Circle(100, 0, 10)), {0, 0, 0, 1}};
  blend::Key nearWhite{fromSk(SkPath::Circle(100, 0, 10)), {0.95f, 0.95f, 0.95f, 1}};
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
  blend::Key from{fromSk(SkPath::Circle(0, 0, 10)), {1, 0, 0, 1}};
  blend::Key to{fromSk(SkPath::Circle(300, 0, 10)), {0, 1, 0, 1}};
  blend::Options options;
  options.spacing = blend::Spacing::Distance;
  options.distance = 50;
  // 300px span / 50px = 6 slots -> 5 intermediates + 2 keys.
  const std::vector<blend::Step> steps = blend::make(from, to, options);
  EXPECT_EQ(steps.size(), 7u);
}

TEST(Blend, SpinePlacesStepsAlongPath) {
  SkPathBuilder spine;
  spine.moveTo({0, 0});
  spine.lineTo({0, 400});  // vertical spine
  blend::Key from{fromSk(SkPath::Circle(0, 0, 10)), {1, 0, 0, 1}};
  blend::Key to{fromSk(SkPath::Circle(0, 0, 10)), {0, 1, 0, 1}};  // same spot
  blend::Options options;
  options.steps = 3;
  options.spine = fromSk(spine.detach());
  const std::vector<blend::Step> steps = blend::make(from, to, options);
  ASSERT_EQ(steps.size(), 5u);
  // Steps should march down the vertical spine.
  float lastY = -1;
  for (const blend::Step& step : steps) {
    const Rect bounds = step.path.bounds();
    const float y = (bounds.min.y + bounds.max.y) * 0.5f;
    EXPECT_GT(y, lastY);
    lastY = y;
  }
  const Rect last = steps.back().path.bounds();
  EXPECT_NEAR((last.min.y + last.max.y) * 0.5f, 400.0f, 2.0f);
}

// OKLab L is cube-root lightness: its black-white midpoint is linear
// luminance 0.125 = sRGB ~0.389 — well below a naive sRGB lerp's 0.5 and
// far below a linear-light lerp's 0.735. The blend reaches the colour
// library for the conversion; what this pins is the reach itself, since
// a channel dropped or transposed on the way through would show up
// nowhere else.
TEST(Blend, TheOklabMidpointReachedThroughTheBlendIsPerceptual) {
  const glm::vec4 mid =
      blend::detail::lerpOklab({0, 0, 0, 1}, {1, 1, 1, 1}, 0.5f);
  EXPECT_NEAR(mid.r, 0.389f, 0.03f);
  EXPECT_NEAR(mid.r, mid.g, 0.01f);
  EXPECT_NEAR(mid.g, mid.b, 0.01f);
  EXPECT_FLOAT_EQ(mid.a, 1.0f);
}

// A key, the dials between two of them and each step they expand into
// are VALUES, so a whole blend can be compared: make() is a function of
// the keys and the options, and expanding twice answers the same steps.
TEST(Blend, TheKeysTheOptionsAndTheStepsAreValues) {
  const blend::Key from{fromSk(SkPath::Circle(100, 100, 50)), {1, 0, 0, 1}};
  const blend::Key to{fromSk(SkPath::Circle(400, 100, 30)), {0, 0, 1, 1}};
  const blend::Key again{fromSk(SkPath::Circle(100, 100, 50)), {1, 0, 0, 1}};
  EXPECT_EQ(from, again);
  EXPECT_NE(from, to);

  blend::Key stroked = from;
  stroked.stroke = glm::vec4{0, 1, 0, 1};
  EXPECT_NE(from, stroked);

  blend::Options options;
  EXPECT_EQ(options, blend::Options{});
  options.steps = 3;
  EXPECT_NE(options, blend::Options{});

  const std::vector<blend::Step> steps = blend::make(from, to, options);
  EXPECT_EQ(steps, blend::make(from, to, options));
  ASSERT_GE(steps.size(), 2u);
  EXPECT_NE(steps.front(), steps.back());

  blend::Step later = steps.front();
  later.t = 1;
  EXPECT_NE(later, steps.front());
}

// The blended colours are pinned to the bit: the key colours changed their
// spelling to glm, never their arithmetic, so one in-between step of a
// stroked blend with translucent keys answers exactly what it always did.
TEST(Blend, AnInBetweenColourKeepsItsExactValue) {
  blend::Key from{fromSk(SkPath::Circle(100, 100, 50)), {0.9f, 0.2f, 0.1f, 1}};
  from.stroke = glm::vec4{0.1f, 0.3f, 0.8f, 0.5f};
  const blend::Key to{fromSk(SkPath::Circle(400, 100, 30)),
                      {0.1f, 0.4f, 0.95f, 0.7f}};
  blend::Options options;
  options.steps = 3;
  const std::vector<blend::Step> steps = blend::make(from, to, options);
  ASSERT_EQ(steps.size(), 5u);
  const blend::Step& middle = steps[2];
  EXPECT_EQ(middle.fill, glm::vec4(0x1.2cadcap-1f, 0x1.9a9568p-2f,
                                   0x1.326104p-1f, 0x1.b33334p-1f));
  ASSERT_TRUE(middle.stroke.has_value());
  EXPECT_EQ(*middle.stroke, glm::vec4(0x1.9d08fep-4f, 0x1.661d2ap-2f,
                                      0x1.bfb4c4p-1f, 0x1.333334p-1f));
}
