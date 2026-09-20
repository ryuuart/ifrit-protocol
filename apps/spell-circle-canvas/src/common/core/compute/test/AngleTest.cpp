/** @file
 * The angle conversion: the two constants pinned to the float nearest
 * the exact ratio, the two verbs pinned to a multiplication by them, and
 * the two spellings a caller might reach for instead held up against
 * them — a quotient of a rounded pi, and a divide by the reciprocal.
 */

#include <gtest/gtest.h>
#include <sigilcore/compute/Angle.h>

#include <cmath>
#include <numbers>

using namespace sigil::core;

TEST(Angle, TheConstantsAreTheNearestFloatToTheExactRatio) {
  EXPECT_EQ(angle::kDegToRad, (float)(std::numbers::pi / 180.0))
      << "degrees to radians is the exact ratio rounded once";
  EXPECT_EQ(angle::kRadToDeg, (float)(180.0 / std::numbers::pi))
      << "radians to degrees is rounded independently, not inverted";
}

TEST(Angle, TheSpellingsACallerWritesByHandAnswerDifferently) {
  // Why the constants are written out and the verbs exist. A quotient of
  // a rounded pi lands an ulp below the exact ratio one way round, and
  // dividing by the reciprocal is a third answer again — an angle scaled
  // by either drifts, which is how one drawing ends up in two places.
  constexpr float kRoundedPi = std::numbers::pi_v<float>;
  EXPECT_NE(angle::kRadToDeg, 180.0f / kRoundedPi);
  EXPECT_NE(angle::radians(22.5f), 22.5f / angle::kRadToDeg);
  EXPECT_NE(angle::degrees(1.17f), 1.17f / angle::kDegToRad);
}

TEST(Angle, TheVerbsAreTheOneMultiplication) {
  static_assert(angle::radians(180.0f) == 180.0f * angle::kDegToRad);
  static_assert(angle::degrees(1.0f) == angle::kRadToDeg);
  EXPECT_EQ(angle::radians(0.0f), 0.0f);
  EXPECT_EQ(angle::degrees(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(angle::radians(180.0f), std::numbers::pi_v<float>);
  EXPECT_FLOAT_EQ(angle::degrees(std::numbers::pi_v<float>), 180.0f);
  EXPECT_FLOAT_EQ(std::sin(angle::radians(90.0f)), 1.0f);
}

TEST(Angle, ARoundTripIsWithinOneRoundingOfWhereItStarted) {
  // Two roundings of a value cannot be exact, so what is promised is the
  // size of the error and not its absence.
  for (const float degrees : {0.0f, 1.0f, 45.0f, 90.0f, 180.0f, 359.5f})
    EXPECT_NEAR(angle::degrees(angle::radians(degrees)), degrees, 1e-4f);
}
