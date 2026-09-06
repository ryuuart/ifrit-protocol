/** @file
 * The bind() chain builder: every stage against the arithmetic it stands
 * in for, the affine chain in call order, and the pairs of stages that
 * name one chain whichever way round they are written.
 */

#include <gtest/gtest.h>
#include <sigilmotion/bind/Bind.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "support/StandsAlone.h"

using namespace sigil::motion;
namespace ch = choreograph;

namespace {

/** A chain that reads no Output at all: `apply()` takes its input
 *  explicitly, so a row can hold one built at namespace scope. */
Bound unshaped() { return bind((const ch::Output<float>*)nullptr); }

/** One stage of the chain beside the arithmetic a caller would write by
 *  hand instead. `exact` inputs are where both spellings are exact
 *  operations and the two must agree BIT for bit — a caller replacing one
 *  with the other sees identical numbers, not merely close ones; `approx`
 *  inputs are off that grid, where the two divide in a different order
 *  and agree to float noise. */
struct Stage {
  const char* name;
  BoundFloat map;
  std::function<float(float)> hand;
  std::vector<float> exact;
  std::vector<float> approx;
};

std::string stageName(const testing::TestParamInfo<Stage>& info) {
  return info.param.name;
}

struct Stages : testing::TestWithParam<Stage> {};

/** Sixty-fourths from @p from to @p to — the grid on which a division by
 *  a power of two is exact in float. */
std::vector<float> sixtyFourths(int from, int to) {
  std::vector<float> out;
  for (int i = from; i <= to; ++i) out.push_back((float)i / 64.0f);
  return out;
}

}  // namespace

TEST_P(Stages, ReproduceTheArithmeticTheyStandInFor) {
  const Stage& stage = GetParam();
  for (float v : stage.exact)
    EXPECT_EQ(stage.map.apply(v), stage.hand(v)) << "at " << v;
  for (float v : stage.approx)
    EXPECT_NEAR(stage.map.apply(v), stage.hand(v), 1e-6f) << "at " << v;
}

INSTANTIATE_TEST_SUITE_P(
    Chain, Stages,
    testing::Values(
        // A trailing follower: the value, offset back and clamped.
        Stage{"OffsetThenClamp",
              unshaped().offset(-0.008f).clamp(0.f, 1.f).value(),
              [](float g) { return std::clamp(g - 0.008f, 0.0f, 1.0f); },
              {0.0f, 0.004f, 0.008f, 0.31f, 0.7431f, 0.999f, 1.0f},
              {}},
        // The affine chain in call order: scale, then offset.
        Stage{"ScaleThenOffset",
              unshaped().scale(1.12f).offset(-0.12f).value(),
              [](float u) { return -0.12f + u * 1.12f; },
              {-1.0f, 0.0f, 0.31f, 0.5f, 0.99f, 1.0f, 2.5f},
              {}},
        // The looping phase, `fmod(t * k, 1)`: for a positive schedule
        // both are exact operations on the same product.
        Stage{"ScaleThenWrap",
              unshaped().scale(0.5f).wrap(1.0f).value(),
              [](float t) { return std::fmod(t * 0.5f, 1.0f); },
              {0.0f, 0.7f, 1.9f, 2.0f, 13.37f, 400.25f},
              {}},
        // The inverted sawtooth: invert() IS 1 − v.
        Stage{"Invert",
              unshaped().invert().value(),
              [](float v) { return 1.0f - v; },
              {0.0f, 0.25f, 0.61f, 1.0f},
              {}},
        // Five levels across [0,1], nearest — so four steps.
        Stage{"Quantize",
              unshaped().quantize(5).value(),
              [](float v) { return std::round(v * 4.0f) / 4.0f; },
              sixtyFourths(0, 64),
              {}},
        Stage{"Clamp",
              unshaped().clamp(0.f, 1.f).value(),
              [](float v) { return std::clamp(v, 0.0f, 1.0f); },
              {-4.0f, 0.0f, 0.5f, 1.0f, 4.0f},
              {}},
        // `window(a, b)` is the `clamp((t−a)/(b−a), 0, 1)` idiom, stored
        // as one multiply-add.
        Stage{
            "Window",
            unshaped().window(0.25f, 0.75f).value(),
            [](float t) { return std::clamp((t - 0.25f) / 0.5f, 0.0f, 1.0f); },
            sixtyFourths(-8, 72),
            {0.311f, 0.5002f, 0.7309f}}),
    stageName);

TEST(Bind, TargetIsTheScaleAndOffsetThatLandTheRangeWhereItIsNamed) {
  ch::Output<float> phase = 0.0f;
  const BoundFloat named = bind(&phase).source(0, 100).target(-70, 170).value();
  const BoundFloat manual =
      bind(&phase).source(0, 100).scale(240).offset(-70).value();
  for (float v : {0.f, 25.f, 50.f, 100.f})
    EXPECT_NEAR(named.apply(v), manual.apply(v), 1e-4f);
  EXPECT_NEAR(named.apply(0.f), -70.f, 1e-4f);
  EXPECT_NEAR(named.apply(100.f), 170.f, 1e-4f);
}

TEST(Bind, TheAffineStagesApplyInTheOrderTheyAreWritten) {
  // Order matters here and reads the way it looks — unlike the stages
  // that own one fixed place in the chain.
  ch::Output<float> phase = 0.0f;
  EXPECT_NEAR(bind(&phase).scale(240).offset(-70).value().apply(0.5f),
              0.5f * 240.f - 70.f, 1e-4f);
  EXPECT_NEAR(bind(&phase).offset(-70).scale(240).value().apply(0.5f),
              (0.5f - 70.f) * 240.f, 1e-4f);
}

TEST(Bind, WindowIsSourceThatAlsoClampsItsDomain) {
  // …so a curve downstream never sees a value outside its domain.
  ch::Output<float> phase = 0.0f;
  const BoundFloat w = bind(&phase).window(0.2f, 0.4f).value();
  const BoundFloat s = bind(&phase).source(0.2f, 0.4f).value();
  EXPECT_NEAR(w.apply(0.3f), s.apply(0.3f), 1e-4f);
  EXPECT_NEAR(w.apply(0.9f), 1.0f, 1e-4f);
  EXPECT_GT(s.apply(0.9f), 1.0f);

  // The clamp lands before the curve, and no curve here is total: an
  // overshooting envelope evaluated far past the window returns exactly
  // the end of its range rather than being run outside its domain.
  const BoundFloat overshoot =
      bind(&phase)
          .window(0.2f, 0.4f)
          .map([](float t) { return ch::easeOutBack(t); })
          .value();
  EXPECT_NEAR(overshoot.apply(5.0f), 1.0f, 1e-4f);
}

namespace {

/** Two spellings of one chain: the same stages named the other way
 *  round, and the name a failing row reports itself by. */
struct Pair {
  const char* name;
  BoundFloat one;
  BoundFloat other;
};

std::string pairName(const testing::TestParamInfo<Pair>& info) {
  return info.param.name;
}

struct StagePairs : testing::TestWithParam<Pair> {};

}  // namespace

TEST_P(StagePairs, NameOneChainWhicheverOrderTheyAreWrittenIn) {
  // A stage with a fixed place in the chain takes it however the call
  // was written, so the two spellings must agree bit for bit.
  const Pair& pair = GetParam();
  for (int i = -50; i <= 250; ++i) {
    const float v = (float)i / 100.0f;
    EXPECT_EQ(pair.one.apply(v), pair.other.apply(v)) << "at " << v;
  }
}

INSTANTIATE_TEST_SUITE_P(
    CallOrder, StagePairs,
    testing::Values(
        Pair{"AnEnvelopeAndATargetRange",
             unshaped().cosine().target(400.f, 880.f).value(),
             unshaped().target(400.f, 880.f).cosine().value()},
        Pair{"AWindowAndAnEnvelope",
             unshaped().window(0.2f, 0.8f).pingPong().value(),
             unshaped().pingPong().window(0.2f, 0.8f).value()},
        Pair{"AnEnvelopeAndACurve",
             unshaped()
                 .trapezoid(0.1f, 0.2f, 0.8f, 0.9f)
                 .map(&ch::easeInQuad)
                 .value(),
             unshaped()
                 .map(&ch::easeInQuad)
                 .trapezoid(0.1f, 0.2f, 0.8f, 0.9f)
                 .value()},
        Pair{"AnEnvelopeAndAClamp",
             unshaped().cosine().clamp(0.f, 0.5f).value(),
             unshaped().clamp(0.f, 0.5f).cosine().value()},
        // The stepped readout: quantise takes its place before the affine
        // chain, so the steps land on round output units however the two
        // are written.
        Pair{"AQuantiseAndATargetRange",
             unshaped().quantize(5).target(0.f, 80.f).value(),
             unshaped().target(0.f, 80.f).quantize(5).value()},
        // …and after the curve, so an eased value still lands on a step.
        Pair{"AQuantiseAndACurve",
             unshaped().quantize(5).map(&ch::easeInQuad).value(),
             unshaped().map(&ch::easeInQuad).quantize(5).value()}),
    pairName);
