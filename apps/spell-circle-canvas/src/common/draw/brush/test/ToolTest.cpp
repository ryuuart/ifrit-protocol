/** @file
 * Tools: the pressure envelope, the per-stroke roll, and the weighted
 * choice a sketch picks one with.
 */

#include <gtest/gtest.h>
#include <sigildraw/Pen.h>
#include <sigildraw/brush/Choice.h>
#include <sigildraw/brush/Pressure.h>
#include <sigildraw/brush/Tool.h>

namespace {

namespace brush = sigil::draw::brush;
using namespace sigil::draw;

TEST(Pressure, InterpolatesItsTwoHalves) {
  const brush::Pressure pressure{0.2f, 1.0f, 0.4f};
  EXPECT_FLOAT_EQ(pressure.at(0.0f), 0.2f);
  EXPECT_FLOAT_EQ(pressure.at(0.25f), 0.6f);
  EXPECT_FLOAT_EQ(pressure.at(0.5f), 1.0f);
  EXPECT_FLOAT_EQ(pressure.at(0.75f), 0.7f);
  EXPECT_FLOAT_EQ(pressure.at(1.0f), 0.4f);

  brush::Pressure custom;
  custom.curve = [](float progress) { return progress * progress; };
  EXPECT_FLOAT_EQ(custom.at(0.5f), 0.25f);

  const brush::Pressure gaussian =
      brush::Pressure::gaussianProfile(0.15f, 0.2f, 0.2f, 1.1f);
  EXPECT_NEAR(gaussian.at(0.5f), 1.1f, 1e-5f);
  EXPECT_LT(gaussian.at(0.0f), gaussian.at(0.5f));
}

TEST(Tool, PrepareStrokeRollsTheEnvelopeOnceFromThePensStream) {
  Pen pen;
  brush::Tool tool = brush::marker(SkColors::kBlack, 4.0f);
  tool.noise = 0.0f;
  pen.randomSeed(3);
  const brush::Tool first = brush::prepareStroke(pen, tool);
  pen.randomSeed(3);
  const brush::Tool again = brush::prepareStroke(pen, tool);
  ASSERT_TRUE(first.pressure.curve);
  EXPECT_FALSE(first.pressure.variation.has_value());
  EXPECT_FLOAT_EQ(first.pressure.at(0.3f), again.pressure.at(0.3f));
  EXPECT_FLOAT_EQ(first.opacity, tool.opacity);

  brush::Tool bell = tool;
  bell.pressure = brush::Pressure::gaussianProfile(0.2f, 0.0f, 0.5f, 1.0f);
  const brush::Tool rolled = brush::prepareStroke(pen, bell);
  ASSERT_TRUE(rolled.pressure.gaussian);
  EXPECT_NEAR(rolled.pressure.gaussian->center, 0.5f, 0.2f + 1e-6f);
}

TEST(Tool, WeightedChoiceUsesThePensDeterministicStream) {
  Pen pen;
  pen.randomSeed(74);
  EXPECT_EQ(brush::weightedChoice<int>(pen, {{3, 0.0f}, {7, 1.0f}}), 7);
  EXPECT_FALSE(brush::weightedChoice<int>(pen, {}).has_value());

  pen.randomSeed(74);
  const auto first = brush::weightedChoice<int>(pen, {{1, 1.0f}, {2, 3.0f}});
  pen.randomSeed(74);
  const auto repeated = brush::weightedChoice<int>(pen, {{1, 1.0f}, {2, 3.0f}});
  EXPECT_EQ(first, repeated);
}

}  // namespace
