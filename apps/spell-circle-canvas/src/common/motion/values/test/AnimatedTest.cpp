/** @file
 * The held motion of an animatable: reading the value for the frame,
 * retargeting a ramp from where it is, snapping when the next target is
 * plain, taking a caller's transition as the default for a plain change,
 * and playing an entrance — both the from→to kind and the waypoint kind.
 */

#include <gtest/gtest.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilmotion/values/Animated.h>
#include <sigilmotion/values/Keyframes.h>

#include <chrono>
#include <memory>

using sigil::motion::Animatable;
using sigil::motion::AnimatedFloat;
using sigil::motion::mountEntrance;
using sigil::motion::resolveFloatAt;
using sigil::motion::Ticker;
using sigil::motion::Transition;
using sigil::motion::Transitioned;
using sigil::motion::transitionFloatAt;

namespace {

Animatable<float> ramped(float to, int ms) {
  Transitioned<float> t;
  t.value = to;
  t.spec.duration = std::chrono::milliseconds(ms);
  t.spec.ease = &choreograph::easeNone;
  return t;
}

Animatable<float> entrance(float from, float to, int ms) {
  Transitioned<float> t;
  t.value = to;
  t.from = from;
  t.spec.duration = std::chrono::milliseconds(ms);
  t.spec.ease = &choreograph::easeNone;
  return t;
}

}  // namespace

TEST(Animated, ResolveFloatAtPrefersABindingThenARunningRampThenThePlainValue) {
  const Animatable<float> plain = 3.0f;
  EXPECT_EQ(resolveFloatAt(nullptr, plain), 3.0f);
  AnimatedFloat anim;
  anim.value = 7.0f;
  EXPECT_EQ(resolveFloatAt(&anim, plain), 3.0f);  // not started: ignored
  anim.started = true;
  EXPECT_EQ(resolveFloatAt(&anim, plain), 7.0f);
  choreograph::Output<float> out{9.0f};
  const Animatable<float> bound = &out;
  EXPECT_EQ(resolveFloatAt(&anim, bound), 9.0f);  // the binding wins
  EXPECT_EQ(resolveFloatAt(nullptr, ramped(5.0f, 100)), 5.0f);
}

TEST(Animated, ATransitionRampsToTheTargetAndRetargetsFromWhereItIs) {
  Ticker ticker;
  std::unique_ptr<AnimatedFloat> anim;
  EXPECT_TRUE(transitionFloatAt(ticker, anim, 0.0f, ramped(10.0f, 1000), {}));
  ASSERT_TRUE(anim);
  EXPECT_TRUE(anim->started);
  EXPECT_EQ(anim->target, 10.0f);
  ticker.tick(0.5);
  EXPECT_NEAR(anim->value.value(), 5.0f, 1e-4f);
  // A patch to the same target leaves the motion alone.
  EXPECT_TRUE(transitionFloatAt(ticker, anim, ramped(10.0f, 1000),
                                ramped(10.0f, 1000), {}));
  ticker.tick(0.25);
  EXPECT_NEAR(anim->value.value(), 7.5f, 1e-4f);
  // A new target starts from the current value, not from the description.
  EXPECT_TRUE(transitionFloatAt(ticker, anim, ramped(10.0f, 1000),
                                ramped(0.0f, 1000), {}));
  EXPECT_NEAR(anim->value.value(), 7.5f, 1e-4f);
  ticker.tick(0.5);
  EXPECT_NEAR(anim->value.value(), 3.75f, 1e-4f);
}

TEST(Animated, APlainTargetSnapsAndDisconnectsTheRamp) {
  Ticker ticker;
  std::unique_ptr<AnimatedFloat> anim;
  transitionFloatAt(ticker, anim, 0.0f, ramped(10.0f, 1000), {});
  ticker.tick(0.2);
  EXPECT_FALSE(transitionFloatAt(ticker, anim, ramped(10.0f, 1000), 4.0f, {}));
  EXPECT_FALSE(anim->started);
  EXPECT_FALSE(anim->value.isConnected());
  EXPECT_EQ(resolveFloatAt(anim.get(), 4.0f), 4.0f);
}

TEST(Animated, ACallersDefaultTransitionRampsAPlainChange) {
  Ticker ticker;
  std::unique_ptr<AnimatedFloat> anim;
  Transition spec;
  spec.duration = std::chrono::milliseconds(1000);
  spec.ease = &choreograph::easeNone;
  EXPECT_TRUE(transitionFloatAt(ticker, anim, 0.0f, 8.0f, spec));
  ticker.tick(0.5);
  EXPECT_NEAR(anim->value.value(), 4.0f, 1e-4f);
}

TEST(Animated, AnEntrancePlaysFromToValueAfterTheExtraDelay) {
  Ticker ticker;
  std::unique_ptr<AnimatedFloat> anim;
  mountEntrance(ticker, anim, 5.0f, 0.0f);
  EXPECT_FALSE(anim);  // no entrance declared
  mountEntrance(ticker, anim, entrance(0.0f, 10.0f, 1000), 0.5f);
  ASSERT_TRUE(anim);
  EXPECT_EQ(anim->value.value(), 0.0f);
  ticker.tick(0.5);
  EXPECT_NEAR(anim->value.value(), 0.0f, 1e-4f);  // held for the carry
  ticker.tick(0.5);
  EXPECT_NEAR(anim->value.value(), 5.0f, 1e-4f);
}

TEST(Animated, AWaypointEntrancePlaysItsSegmentsInTurn) {
  Ticker ticker;
  std::unique_ptr<AnimatedFloat> anim;
  Transitioned<float> t;
  t.value = 0.0f;
  t.spec.ease = &choreograph::easeNone;
  t.waypoints = {{std::chrono::milliseconds(0), 0.0f},
                 {std::chrono::milliseconds(1000), 10.0f},
                 {std::chrono::milliseconds(2000), 0.0f}};
  mountEntrance(ticker, anim, t, 0.0f);
  ASSERT_TRUE(anim);
  EXPECT_EQ(anim->target, 0.0f);
  ticker.tick(1.0);
  EXPECT_NEAR(anim->value.value(), 10.0f, 1e-4f);
  ticker.tick(0.5);
  EXPECT_NEAR(anim->value.value(), 5.0f, 1e-4f);
}
