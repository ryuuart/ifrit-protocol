/** @file
 * The angle conversion: the two constants pinned to the float nearest
 * the exact ratio, the two verbs pinned to a multiplication by them, and
 * the two spellings a caller might reach for instead held up against
 * them — a quotient of a rounded pi, and a divide by the reciprocal —
 * and the angle that names its own measure: read from CSS text in each
 * of the four units, refused without one, and converted through the
 * same rounding.
 */

#include <gtest/gtest.h>
#include <sigilcore/compute/Angle.h>

#include <cmath>
#include <numbers>
#include <optional>
#include <string>

using namespace sigil::core;

TEST(Angle, TheConstantsAreTheNearestFloatToTheExactRatio) {
  EXPECT_EQ(angle::kDegToRad, (float)(std::numbers::pi / 180.0))
      << "degrees to radians is the exact ratio rounded once";
  EXPECT_EQ(angle::kRadiansToDegrees, (float)(180.0 / std::numbers::pi))
      << "radians to degrees is rounded independently, not inverted";
}

TEST(Angle, TheSpellingsACallerWritesByHandAnswerDifferently) {
  // Why the constants are written out and the verbs exist. A quotient of
  // a rounded pi lands an ulp below the exact ratio one way round, and
  // dividing by the reciprocal is a third answer again — an angle scaled
  // by either drifts, which is how one drawing ends up in two places.
  constexpr float kRoundedPi = std::numbers::pi_v<float>;
  EXPECT_NE(angle::kRadiansToDegrees, 180.0f / kRoundedPi);
  EXPECT_NE(angle::radians(22.5f), 22.5f / angle::kRadiansToDegrees);
  EXPECT_NE(angle::degrees(1.17f), 1.17f / angle::kDegToRad);
}

TEST(Angle, TheVerbsAreTheOneMultiplication) {
  static_assert(angle::radians(180.0f) == 180.0f * angle::kDegToRad);
  static_assert(angle::degrees(1.0f) == angle::kRadiansToDegrees);
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

TEST(Angle, CssTextIsReadInEachOfItsFourUnits) {
  EXPECT_EQ(angle::read("45deg"), angle::Angle::degrees(45));
  EXPECT_EQ(angle::read("1rad"), angle::Angle::radians(1));
  EXPECT_EQ(angle::read("50grad"), angle::Angle::gradians(50));
  EXPECT_EQ(angle::read("0.25turn"), angle::Angle::turns(0.25));
  EXPECT_EQ(angle::read("-1.5RAD"), angle::Angle::radians(-1.5))
      << "a unit is read in any case and a number carries its sign";
  EXPECT_EQ(angle::read("  +90Deg\t"), angle::Angle::degrees(90))
      << "blank space around the angle and a leading plus are CSS's";
  EXPECT_EQ(angle::read("1e1deg"), angle::Angle::degrees(10));
  EXPECT_EQ(angle::read(".5turn"), angle::Angle::turns(0.5));
}

TEST(Angle, TextWithoutAUnitOrWithAnotherIsRefused) {
  // A bare number in text would be read in whatever measure the slot
  // counts in, which is exactly the ambiguity the unit is there to end.
  for (const char* text : {"45", "", "deg", "45 deg", "45px", "45degs",
                           "+-45deg", "infdeg", "nanrad", "45turns"})
    EXPECT_EQ(angle::read(text), std::nullopt) << text;
}

TEST(Angle, AnAngleReachesTheMeasureItWasWrittenInUnchanged) {
  // A slot counting in the angle's own measure reads back the number as
  // written: no round trip through the other measure.
  EXPECT_EQ(angle::Angle::degrees(37.5).inDegrees(), 37.5f);
  EXPECT_EQ(angle::Angle::radians(0.6545).inRadians(), 0.6545f);
  // Between degrees and radians it is the one rounding.
  EXPECT_EQ(angle::Angle::degrees(37.5).inRadians(), angle::radians(37.5f));
  EXPECT_EQ(angle::Angle::radians(0.6545).inDegrees(),
            angle::degrees(0.6545f));
}

TEST(Angle, GradiansAndTurnsConvertByTheirShareOfAWholeTurn) {
  EXPECT_EQ(angle::Angle::turns(0.25).inDegrees(), 90.0f);
  EXPECT_EQ(angle::Angle::gradians(100).inDegrees(), 90.0f);
  EXPECT_FLOAT_EQ(angle::Angle::turns(0.5).inRadians(),
                  std::numbers::pi_v<float>);
  EXPECT_FLOAT_EQ(angle::Angle::gradians(200).inRadians(),
                  std::numbers::pi_v<float>);
  EXPECT_DOUBLE_EQ(angle::Angle::degrees(90).inTurns(), 0.25);
  EXPECT_DOUBLE_EQ(angle::Angle::gradians(100).inTurns(), 0.25);
  EXPECT_DOUBLE_EQ(angle::Angle::radians(std::numbers::pi).inTurns(), 0.5);
}

TEST(Angle, AnAngleWritesTheTextItIsReadFrom) {
  EXPECT_EQ(angle::text(angle::Angle::degrees(45)), "45deg");
  EXPECT_EQ(angle::text(angle::Angle::turns(0.25)), "0.25turn");
  EXPECT_EQ(angle::text(angle::Angle::gradians(-50)), "-50grad");
  for (const angle::Angle value :
       {angle::Angle::radians(0.785398), angle::Angle::degrees(-0.1)})
    EXPECT_EQ(angle::read(angle::text(value)), value);
}
