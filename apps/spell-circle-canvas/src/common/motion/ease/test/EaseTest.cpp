/** @file
 * The named curves: each held to the animation runtime's own numbers so
 * that swapping one for its name moves no pixel, the families and the
 * steps read at their defining points, and the comparison rule every
 * curve slot shares.
 */

#include <choreograph/Choreograph.h>
#include <gtest/gtest.h>
#include <sigilmotion/ease/Ease.h>

#include <functional>
#include <utility>
#include <vector>

#include "support/StandsAlone.h"

using namespace sigil::motion;

namespace {

/** Every position a curve is read at: the unit range in 1/64 steps, the
 *  two ends, and the midpoint the in-out curves change halves at. */
std::vector<float> positions() {
  std::vector<float> values;
  for (int step = 0; step <= 64; ++step) values.push_back((float)step / 64.0f);
  return values;
}

void expectSameCurve(const Easing& ours, const Easing& theirs,
                     const char* name) {
  for (float position : positions())
    EXPECT_EQ(ours(position), theirs(position)) << name << " at " << position;
}

}  // namespace

TEST(Ease, EveryNamedCurveReadsTheRuntimesNumbers) {
  const std::vector<std::pair<const char*, std::pair<Easing, Easing>>> pairs{
      {"linear", {ease::linear, choreograph::easeNone}},
      {"inQuad", {ease::inQuad, choreograph::easeInQuad}},
      {"outQuad", {ease::outQuad, choreograph::easeOutQuad}},
      {"inOutQuad", {ease::inOutQuad, choreograph::easeInOutQuad}},
      {"inCubic", {ease::inCubic, choreograph::easeInCubic}},
      {"outCubic", {ease::outCubic, choreograph::easeOutCubic}},
      {"inOutCubic", {ease::inOutCubic, choreograph::easeInOutCubic}},
      {"inQuart", {ease::inQuart, choreograph::easeInQuart}},
      {"outQuart", {ease::outQuart, choreograph::easeOutQuart}},
      {"inOutQuart", {ease::inOutQuart, choreograph::easeInOutQuart}},
      {"inQuint", {ease::inQuint, choreograph::easeInQuint}},
      {"outQuint", {ease::outQuint, choreograph::easeOutQuint}},
      {"inOutQuint", {ease::inOutQuint, choreograph::easeInOutQuint}},
      {"inSine", {ease::inSine, choreograph::easeInSine}},
      {"outSine", {ease::outSine, choreograph::easeOutSine}},
      {"inOutSine", {ease::inOutSine, choreograph::easeInOutSine}},
      {"inExpo", {ease::inExpo, choreograph::easeInExpo}},
      {"outExpo", {ease::outExpo, choreograph::easeOutExpo}},
      {"inOutExpo", {ease::inOutExpo, choreograph::easeInOutExpo}},
      {"inCirc", {ease::inCirc, choreograph::easeInCirc}},
      {"outCirc", {ease::outCirc, choreograph::easeOutCirc}},
      {"inOutCirc", {ease::inOutCirc, choreograph::easeInOutCirc}},
      {"inBack", {ease::inBack(), [](float t) { return choreograph::easeInBack(t); }}},
      {"outBack", {ease::outBack(), [](float t) { return choreograph::easeOutBack(t); }}},
      {"inOutBack", {ease::inOutBack(), [](float t) { return choreograph::easeInOutBack(t); }}},
      {"inElastic", {ease::inElastic(1.0f, 0.4f), [](float t) { return choreograph::easeInElastic(t, 1.0f, 0.4f); }}},
      {"outElastic", {ease::outElastic(1.0f, 0.4f), [](float t) { return choreograph::easeOutElastic(t, 1.0f, 0.4f); }}},
      {"inOutElastic", {ease::inOutElastic(1.0f, 0.4f), [](float t) { return choreograph::easeInOutElastic(t, 1.0f, 0.4f); }}},
      {"inBounce", {ease::inBounce(), [](float t) { return choreograph::easeInBounce(t); }}},
      {"outBounce", {ease::outBounce(), [](float t) { return choreograph::easeOutBounce(t); }}},
      {"inOutBounce", {ease::inOutBounce(), [](float t) { return choreograph::easeInOutBounce(t); }}},
  };
  for (const auto& [name, curves] : pairs)
    expectSameCurve(curves.first, curves.second, name);
}

TEST(Ease, APowerFamilyAtAWholePowerIsTheNamedCurve) {
  for (float position : positions()) {
    EXPECT_NEAR(ease::in(2)(position), ease::inQuad(position), 1e-6f);
    EXPECT_NEAR(ease::out(3)(position), ease::outCubic(position), 1e-6f);
    EXPECT_NEAR(ease::inOut(2)(position), ease::inOutQuad(position), 1e-6f);
  }
}

TEST(Ease, StepsHoldThenJump) {
  const Easing late = ease::steps(4);
  EXPECT_EQ(late(0.0f), 0.0f);
  EXPECT_EQ(late(0.2f), 0.0f);
  EXPECT_EQ(late(0.3f), 0.25f);
  EXPECT_EQ(late(1.0f), 1.0f);
  const Easing early = ease::steps(4, true);
  EXPECT_EQ(early(0.1f), 0.25f);
  EXPECT_EQ(early(0.8f), 1.0f);
  // Held inside the unit range, so a caller past either end reads an end.
  EXPECT_EQ(late(1.5f), 1.0f);
  EXPECT_EQ(late(-0.5f), 0.0f);
}

TEST(Ease, ACurveComparesByItsShapeAndItsSettings) {
  // A plain function is its pointer, a shaped curve is its shape and its
  // numbers, and a capturing lambda is unequal to everything — including
  // to itself, because an Easing holding one cannot be read back.
  EXPECT_TRUE(easeEqual(ease::inQuad, ease::inQuad));
  EXPECT_FALSE(easeEqual(ease::inQuad, ease::outQuad));
  EXPECT_TRUE(easeEqual(ease::steps(3), ease::steps(3)));
  EXPECT_FALSE(easeEqual(ease::steps(3), ease::steps(3, true)));
  EXPECT_TRUE(easeEqual(ease::out(2.5f), ease::out(2.5f)));
  EXPECT_FALSE(easeEqual(ease::out(2.5f), ease::in(2.5f)));
  EXPECT_TRUE(easeEqual(ease::inOutBounce(), ease::inOutBounce()));
  const float scale = 2.0f;
  const Easing captured = [scale](float t) { return t * scale; };
  EXPECT_FALSE(easeEqual(captured, captured));
  EXPECT_TRUE(easeEqual({}, {}));
}
