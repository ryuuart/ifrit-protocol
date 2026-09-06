/** @file
 * The curve value: what it answers, and — the reason it is a value at
 * all — when two of them are the same curve.
 */

#include <gtest/gtest.h>
#include <sigilcore/compute/Curve.h>

#include <cmath>
#include <functional>
#include <initializer_list>

using namespace sigil::core;

TEST(Curve, ADefaultCurveIsTheIdentityRamp) {
  const curve::Curve none;
  EXPECT_FLOAT_EQ(none.at(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(none.at(0.37f), 0.37f);
  EXPECT_FLOAT_EQ(none.at(1.0f), 1.0f);
  // …and it is callable, so it drops into anything taking a float→float.
  const std::function<float(float)> fn = none;
  EXPECT_FLOAT_EQ(fn(0.25f), 0.25f);
}

TEST(Curve, AShapedCurveComparesEqualAtTheSameSettings) {
  // A curve built by binding a shape parameter into a lambda compares
  // equal to nothing, so every value holding one re-patches forever. The
  // shape and the numbers are kept where they can be read back, so two
  // calls at the same argument are the same curve.
  EXPECT_EQ(curve::outBack(), curve::outBack());
  EXPECT_NE(curve::outBack(1.7f), curve::outBack(2.4f));
  EXPECT_EQ(curve::outElastic(1.0f, 0.3f), curve::outElastic(1.0f, 0.3f));
  EXPECT_NE(curve::outElastic(1.0f, 0.3f), curve::outElastic(1.0f, 0.5f));
  // A CSS curve is its four control numbers.
  EXPECT_EQ(curve::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f),
            curve::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f));
  EXPECT_NE(curve::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f),
            curve::cubicBezier(0.4f, 0.0f, 0.2f, 1.0f));
  // Two DIFFERENT shapes at the same numbers are different curves.
  EXPECT_NE(curve::outBack(1.7f), curve::inBack(1.7f));
}

TEST(Curve, ACallersOwnShapeIsComparableToo) {
  // The escape hatch: a captureless body over the parameter block reads
  // its own numbers and compares by the same rule the house shapes do.
  const auto power = [](float exponent) {
    return curve::Curve{
        [](float t, const float* p) { return std::pow(t, p[0]); }, {exponent}};
  };
  EXPECT_EQ(power(3.0f), power(3.0f));
  EXPECT_NE(power(3.0f), power(2.0f));
  EXPECT_FLOAT_EQ(power(2.0f).at(0.5f), 0.25f);
}

TEST(Curve, TheHouseShapesLandOnTheirEnds) {
  // Every shape is a reparameterisation of [0,1] onto itself: whatever it
  // does in the middle, it starts where it starts and ends where it ends.
  for (const curve::Curve& shape :
       {curve::outBack(), curve::inBack(), curve::inOutBack(),
        curve::outElastic(), curve::inElastic(), curve::outBounce(),
        curve::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f)}) {
    EXPECT_NEAR(shape.at(0.0f), 0.0f, 1e-6f);
    EXPECT_NEAR(shape.at(1.0f), 1.0f, 1e-6f);
  }
}

TEST(Curve, EachHouseShapeHasItsOwnCharacter) {
  // Back overshoots past its end before it settles.
  EXPECT_GT(curve::outBack().at(0.6f), 1.0f);
  // Its overshoot is the parameter: more of it swings further.
  EXPECT_GT(curve::outBack(3.0f).at(0.6f), curve::outBack(1.7f).at(0.6f));
  // In-back pulls the other way first.
  EXPECT_LT(curve::inBack().at(0.2f), 0.0f);
  // Elastic rings: it crosses its end more than once on the way to rest.
  int crossings = 0;
  bool above = false;
  for (int i = 1; i < 100; ++i) {
    const bool now = curve::outElastic().at(i / 100.0f) > 1.0f;
    if (now != above) ++crossings;
    above = now;
  }
  EXPECT_GT(crossings, 2);
  // Bounce never passes its end — it lands on it, repeatedly.
  for (int i = 0; i <= 100; ++i)
    EXPECT_LE(curve::outBounce().at(i / 100.0f), 1.0f + 1e-6f);
  // The CSS default is well past half way at the half, which a straight
  // line is not.
  EXPECT_GT(curve::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f).at(0.5f), 0.5f);
  // Smoothstep is symmetric about the middle and flat at both ends.
  EXPECT_FLOAT_EQ(curve::smoothstep(0.5f), 0.5f);
  EXPECT_FLOAT_EQ(curve::smoothstep(0.25f), 1.0f - curve::smoothstep(0.75f));
}
