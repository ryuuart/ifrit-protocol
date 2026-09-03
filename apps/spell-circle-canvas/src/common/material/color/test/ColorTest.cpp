/** @file
 * The colour feature: the sRGB curve round-trips, so does OKLab, and the
 * hue wheel folds whatever it is handed.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/color/Color.h>

#include <algorithm>

using namespace sigil::material;

TEST(Color, SrgbRoundTrips) {
  for (float v : {0.0f, 0.02f, 0.25f, 0.5f, 0.75f, 1.0f})
    EXPECT_NEAR(linearToSrgb(srgbToLinear(v)), v, 1e-5f);
  const Color mid{0.5f, 0.5f, 0.5f, 1};
  const Color back = fromOklab(toOklab(mid));
  EXPECT_NEAR(back.r, 0.5f, 1e-4f);
  EXPECT_NEAR(back.g, 0.5f, 1e-4f);
}

TEST(Color, HsvWalksTheWheelAndFoldsWhateverItIsGiven) {
  // The six landmarks, exactly.
  const Color red = hsv(0, 1, 1);
  EXPECT_FLOAT_EQ(red.r, 1.0f);
  EXPECT_FLOAT_EQ(red.g, 0.0f);
  EXPECT_FLOAT_EQ(red.b, 0.0f);
  EXPECT_FLOAT_EQ(hsv(120, 1, 1).g, 1.0f);
  EXPECT_FLOAT_EQ(hsv(240, 1, 1).b, 1.0f);
  EXPECT_FLOAT_EQ(hsv(300, 1, 1).r, 1.0f);
  EXPECT_FLOAT_EQ(hsv(300, 1, 1).b, 1.0f);
  EXPECT_FLOAT_EQ(hsv(300, 1, 1).g, 0.0f);

  // The fold is the reason this is a library verb: the sextant ladder
  // answers magenta for anything it does not recognise, so an unwrapped
  // hue — a golden-angle walk, an angle in degrees — is silently wrong
  // everywhere past a full turn and everywhere below zero.
  for (float turn : {-720.0f, -360.0f, 360.0f, 1080.0f}) {
    const Color same = hsv(150.0f + turn, 0.7f, 0.6f);
    const Color once = hsv(150.0f, 0.7f, 0.6f);
    EXPECT_NEAR(same.r, once.r, 1e-5f) << "turn " << turn;
    EXPECT_NEAR(same.g, once.g, 1e-5f) << "turn " << turn;
    EXPECT_NEAR(same.b, once.b, 1e-5f) << "turn " << turn;
  }

  // Value is the largest channel, saturation the distance from grey, and
  // both are clamped — outside the unit range the ladder answers a colour
  // and it is the wrong one.
  const Color grey = hsv(200, 0, 0.4f);
  EXPECT_FLOAT_EQ(grey.r, 0.4f);
  EXPECT_FLOAT_EQ(grey.g, 0.4f);
  EXPECT_FLOAT_EQ(grey.b, 0.4f);
  EXPECT_FLOAT_EQ(hsv(200, 5.0f, 2.0f).b, 1.0f);
  EXPECT_FLOAT_EQ(hsv(200, -1.0f, -1.0f).r, 0.0f);
  const Color tone = hsv(30, 0.5f, 0.8f);
  EXPECT_FLOAT_EQ(std::max({tone.r, tone.g, tone.b}), 0.8f);
  EXPECT_FLOAT_EQ(std::min({tone.r, tone.g, tone.b}), 0.8f * 0.5f);

  // Alpha rides through, straight, like every other colour here.
  EXPECT_FLOAT_EQ(hsv(10, 1, 1, 0.25f).a, 0.25f);
}
