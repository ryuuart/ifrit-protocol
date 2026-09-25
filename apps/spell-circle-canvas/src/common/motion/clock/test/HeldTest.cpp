/** @file
 * The held motion of an animatable: reading the value for the frame,
 * retargeting a ramp from where it is, blending a retarget so its
 * velocity carries, snapping when the next target is constant, taking a
 * caller's transition as the default for a constant change, playing an
 * entrance — from→to, keyframed with and without durations, delayed by a
 * stagger resolved for its place, looping and alternating — and the
 * synthesized 0→1 progress.
 */

#include "support/StandsAlone.h"

#include <gtest/gtest.h>
#include <sigilmotion/advanced/Held.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Transition.h>
#include <sigilmotion/values/Tween.h>

#include <array>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>

#include "support/Ramps.h"

using namespace sigil::motion;
using namespace std::chrono_literals;
using sigil::motion::test::ramped;

namespace {

/** Moves @p engine one stated frame of @p step past where it stands. */
void stepBy(Engine& engine, Duration step) {
  engine.advance(engine.elapsed() + step);
}

/** A linear entrance from @p from to @p to over @p length. */
Animatable<float> entrance(float from, float to, Duration length,
                           Staggered<Duration> delay = Duration{}) {
  return animate({.from = from,
                  .to = to,
                  .duration = length,
                  .delay = std::move(delay),
                  .ease = ease::linear});
}

}  // namespace

TEST(Held, ValueOfPrefersALiveValueThenARunningRampThenTheConstant) {
  const Animatable<float> constant = 3.0f;
  EXPECT_EQ(valueOf(nullptr, constant), 3.0f);
  HeldMotion held;
  held.live = 7.0f;
  EXPECT_EQ(valueOf(&held, constant), 3.0f);  // not started: ignored
  held.started = true;
  EXPECT_EQ(valueOf(&held, constant), 7.0f);
  const Animatable<float> live = animatable(9.0f);
  EXPECT_EQ(valueOf(&held, live), 9.0f);  // the live value wins
  // …shaped through its stages when it has any.
  EXPECT_FLOAT_EQ(valueOf(&held, bind(live, {.from = {0.0f, 10.0f}})), 0.9f);
  EXPECT_EQ(valueOf(nullptr, ramped(5.0f, 100ms)), 5.0f);
}

TEST(Held, ATransitionRampsToTheTargetAndRetargetsFromWhereItIs) {
  Engine engine;
  std::unique_ptr<HeldMotion> held;
  EXPECT_TRUE(retarget(engine, held, 0.0f, ramped(10.0f, 1s), {}));
  ASSERT_TRUE(held);
  EXPECT_TRUE(held->started);
  EXPECT_TRUE(held->isRunning());
  EXPECT_EQ(held->target, 10.0f);
  stepBy(engine, 500ms);
  EXPECT_NEAR(held->value(), 5.0f, 1e-4f);
  // A patch to the same target leaves the motion alone.
  EXPECT_TRUE(
      retarget(engine, held, ramped(10.0f, 1s), ramped(10.0f, 1s), {}));
  stepBy(engine, 250ms);
  EXPECT_NEAR(held->value(), 7.5f, 1e-4f);
  // A new target starts from the current value, not from the description.
  EXPECT_TRUE(retarget(engine, held, ramped(10.0f, 1s), ramped(0.0f, 1s), {}));
  EXPECT_NEAR(held->value(), 7.5f, 1e-4f);
  stepBy(engine, 500ms);
  EXPECT_NEAR(held->value(), 3.75f, 1e-4f);
  stepBy(engine, 1s);
  EXPECT_FLOAT_EQ(held->value(), 0.0f);
  EXPECT_FALSE(held->isRunning());
  EXPECT_FALSE(engine.isRunning());
}

TEST(Held, AConstantTargetSnapsAndStopsTheRamp) {
  Engine engine;
  std::unique_ptr<HeldMotion> held;
  retarget(engine, held, 0.0f, ramped(10.0f, 1s), {});
  stepBy(engine, 200ms);
  EXPECT_FALSE(retarget(engine, held, ramped(10.0f, 1s), 4.0f, {}));
  EXPECT_FALSE(held->started);
  EXPECT_FALSE(held->isRunning());
  EXPECT_EQ(valueOf(held.get(), 4.0f), 4.0f);
  // Nothing writes the value on later frames.
  const float stopped = held->value();
  stepBy(engine, 200ms);
  EXPECT_EQ(held->value(), stopped);
}

TEST(Held, ACallersDefaultTransitionRampsAConstantChange) {
  Engine engine;
  std::unique_ptr<HeldMotion> held;
  const Transition spec{.duration = 1s, .ease = ease::linear};
  EXPECT_TRUE(retarget(engine, held, 0.0f, 8.0f, spec));
  stepBy(engine, 500ms);
  EXPECT_NEAR(held->value(), 4.0f, 1e-4f);
}

TEST(Held, ABlendedRetargetCarriesTheVelocityWhereAReplacedOneStartsAtRest) {
  // The same flight — halfway up a linear climb to 100 over a second —
  // retargeted to 0 on a curve that starts at rest, blended and replaced.
  const auto retargeted = [](Composition composition) {
    Engine engine;
    std::unique_ptr<HeldMotion> held;
    const Animatable<float> climb = ramped(100.0f, 1s);
    retarget(engine, held, 0.0f, climb, {});
    stepBy(engine, 500ms);
    const Animatable<float> back = animate({.to = 0.0f,
                                            .duration = 1s,
                                            .ease = ease::inQuad,
                                            .composition = composition});
    EXPECT_TRUE(retarget(engine, held, climb, back, {}));
    const float before = held->value();
    stepBy(engine, 10ms);
    const float moved = held->value() - before;
    stepBy(engine, 3s);
    return std::array<float, 3>{before, moved, held->value()};
  };

  const std::array<float, 3> blended = retargeted(Composition::Blend);
  const std::array<float, 3> replaced = retargeted(Composition::Replace);
  EXPECT_NEAR(blended[0], 50.0f, 1e-3f);
  EXPECT_NEAR(replaced[0], 50.0f, 1e-3f);
  // Blended, the value keeps moving the way it was going right after the
  // retarget; replaced, it starts again from rest.
  EXPECT_GT(blended[1], 0.9f);
  EXPECT_LT(std::abs(replaced[1]), 0.01f);
  // Both land on the new target.
  EXPECT_NEAR(blended[2], 0.0f, 1e-3f);
  EXPECT_NEAR(replaced[2], 0.0f, 1e-3f);
}

TEST(Held, AnEntrancePlaysFromToValueAfterItsDelay) {
  Engine engine;
  std::unique_ptr<HeldMotion> held;
  enter(engine, held, 5.0f);
  EXPECT_FALSE(held);  // no entrance declared
  enter(engine, held, entrance(0.0f, 10.0f, 1s, 500ms));
  ASSERT_TRUE(held);
  EXPECT_EQ(held->value(), 0.0f);
  stepBy(engine, 500ms);
  EXPECT_NEAR(held->value(), 0.0f, 1e-4f);  // held for the delay
  stepBy(engine, 500ms);
  EXPECT_NEAR(held->value(), 5.0f, 1e-4f);
}

TEST(Held, AnEntranceThatGoesNowhereAndDoesNotRepeatIsNone) {
  Engine engine;
  std::unique_ptr<HeldMotion> held;
  enter(engine, held, entrance(5.0f, 5.0f, 1s));
  EXPECT_FALSE(held);
  // A tween that eases on change says nothing about mounting.
  enter(engine, held, ramped(5.0f, 1s));
  EXPECT_FALSE(held);
}

TEST(Held, AStaggeredEntranceResolvesItsDelayForItsPlace) {
  Engine engine;
  std::unique_ptr<HeldMotion> held;
  enter(engine, held, entrance(0.0f, 10.0f, 1s, stagger(100ms)), {3, 4});
  stepBy(engine, 300ms);
  EXPECT_NEAR(held->value(), 0.0f, 1e-4f);  // the fourth child waits 300ms
  stepBy(engine, 500ms);
  EXPECT_NEAR(held->value(), 5.0f, 1e-4f);
}

TEST(Held, AKeyframedEntrancePlaysItsStepsInTurn) {
  Engine engine;
  std::unique_ptr<HeldMotion> held;
  enter(engine, held,
        animate({.from = 0.0f,
                 .keyframes = {{.to = 10.0f, .duration = 1s},
                               {.to = 0.0f, .duration = 1s}},
                 .ease = ease::linear}));
  ASSERT_TRUE(held);
  EXPECT_EQ(held->target, 0.0f);
  stepBy(engine, 1s);
  EXPECT_NEAR(held->value(), 10.0f, 1e-4f);
  stepBy(engine, 500ms);
  EXPECT_NEAR(held->value(), 5.0f, 1e-4f);
}

TEST(Held, AKeyframeWithNoDurationTakesAnEqualShareOfTheEntrance) {
  Engine engine;
  std::unique_ptr<HeldMotion> held;
  enter(engine, held,
        animate({.from = 0.0f,
                 .keyframes = {{.to = 10.0f}, {.to = 0.0f}},
                 .duration = 1s,
                 .ease = ease::linear}));
  ASSERT_TRUE(held);
  stepBy(engine, 250ms);
  EXPECT_NEAR(held->value(), 5.0f, 1e-4f);
  stepBy(engine, 250ms);
  EXPECT_NEAR(held->value(), 10.0f, 1e-4f);
  stepBy(engine, 250ms);
  EXPECT_NEAR(held->value(), 5.0f, 1e-4f);
  stepBy(engine, 500ms);
  EXPECT_FLOAT_EQ(held->value(), 0.0f);
  EXPECT_FALSE(held->isRunning());
}

TEST(Held, ALoopingEntranceRepeatsAndAnAlternatingOneComesBack) {
  Engine engine;
  std::unique_ptr<HeldMotion> held;
  enter(engine, held,
        animate({.from = 0.0f,
                 .to = 10.0f,
                 .duration = 1s,
                 .ease = ease::linear,
                 .loop = 1,
                 .alternate = true}));
  stepBy(engine, 500ms);
  EXPECT_NEAR(held->value(), 5.0f, 1e-4f);
  stepBy(engine, 750ms);
  EXPECT_NEAR(held->value(), 7.5f, 1e-4f);  // the second pass runs back
  stepBy(engine, 1s);
  EXPECT_FLOAT_EQ(held->value(), 0.0f);  // and ends where it began
  EXPECT_FALSE(held->isRunning());

  // For ever, it never stops.
  std::unique_ptr<HeldMotion> forever;
  enter(engine, forever,
        animate({.from = 0.0f,
                 .to = 10.0f,
                 .duration = 1s,
                 .ease = ease::linear,
                 .loop = -1}));
  stepBy(engine, 10250ms);
  EXPECT_NEAR(forever->value(), 2.5f, 1e-3f);
  EXPECT_TRUE(forever->isRunning());
  EXPECT_TRUE(engine.isRunning());
}

TEST(Held, AProgressHoldsForItsDelayThenRampsToOne) {
  Engine engine;
  std::unique_ptr<HeldMotion> held;
  progress(engine, held, {.duration = 1s, .delay = 500ms, .ease = ease::linear});
  ASSERT_TRUE(held);
  EXPECT_EQ(held->value(), 0.0f);
  stepBy(engine, 500ms);
  EXPECT_NEAR(held->value(), 0.0f, 1e-4f);
  stepBy(engine, 500ms);
  EXPECT_NEAR(held->value(), 0.5f, 1e-4f);
  stepBy(engine, 1s);
  EXPECT_FLOAT_EQ(held->value(), 1.0f);
  EXPECT_FALSE(held->isRunning());
}

TEST(Held, IsRunningIsALiveValueOrAHeldRampThatMoves) {
  Engine engine;
  const Animatable<float> live = animatable(1.0f);
  EXPECT_TRUE(isRunning(nullptr, live));
  EXPECT_FALSE(isRunning(nullptr, 1.0f));
  std::unique_ptr<HeldMotion> held;
  retarget(engine, held, 0.0f, ramped(1.0f, 1s), {});
  EXPECT_TRUE(isRunning(held.get(), ramped(1.0f, 1s)));
  held->stop();
  EXPECT_FALSE(isRunning(held.get(), ramped(1.0f, 1s)));
}

TEST(Held, ResolvePropertyTakesTheFallbackOnlyForAConstant) {
  const std::optional<Transition> fallback = Transition{.duration = 700ms};
  const ResolvedProperty<float> constant = resolveProperty<float>(2.0f, fallback);
  EXPECT_EQ(constant.target, 2.0f);
  ASSERT_TRUE(constant.transition);
  EXPECT_EQ(constant.transition->duration, 700ms);

  // A described motion keeps its own timing, resolved for its place.
  const Animatable<float> fan =
      animate({.to = stagger(10.0f), .duration = 300ms, .delay = stagger(40ms)});
  const ResolvedProperty<float> third = resolveProperty(fan, fallback, {2, 4});
  EXPECT_EQ(third.target, 20.0f);
  ASSERT_TRUE(third.transition);
  EXPECT_EQ(third.transition->duration, 300ms);
  EXPECT_NEAR(third.transition->delay.count(), 0.080, 1e-9);

  // A live value takes neither: it is already a running number.
  const Animatable<float> live = animatable(1.0f);
  const ResolvedProperty<float> running = resolveProperty(live, fallback);
  EXPECT_EQ(running.live, &live);
  EXPECT_FALSE(running.transition);
}
