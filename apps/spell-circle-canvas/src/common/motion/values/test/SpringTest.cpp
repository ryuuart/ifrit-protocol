/** @file
 * The spring: that it is solved rather than integrated (one step of any
 * size lands where many small ones do), that damping decides whether it
 * crosses the target, that a moved target bends the flight instead of
 * restarting it, and what it answers at its edges.
 */

#include "support/StandsAlone.h"

#include <gtest/gtest.h>
#include <sigilmotion/values/Spring.h>

#include <chrono>
#include <cmath>
#include <vector>

using namespace sigil::motion;
using namespace std::chrono_literals;

namespace {

/** Run a spring for @p length in fixed steps of @p step, reporting where
 *  it ended. */
Spring run(Spring state, float target, Duration length, Duration step,
           SpringParameters parameters) {
  for (Duration time{}; time < length - Duration(1e-6); time += step)
    state = state.step(target, step, parameters);
  return state;
}

}  // namespace

TEST(Spring, OneBigStepLandsWhereManySmallOnesDo) {
  // The closed-form solution is what makes a spring safe on whatever
  // delta a frame clock hands over: a stalled frame is a big step, not
  // an explosion, and no substepping is needed to keep it stable.
  const SpringParameters parameters{.period = 350ms, .damping = 0.4f};
  const Spring start{.value = 120.0f, .velocity = -40.0f};

  const Spring stepped = run(start, 0.0f, 500ms, 1s / 240.0, parameters);
  const Spring once = start.step(0.0f, 500ms, parameters);
  EXPECT_NEAR(stepped.value, once.value, 1e-2f);
  EXPECT_NEAR(stepped.velocity, once.velocity, 1.0f);

  // A quarter-second frame — the clock's own worst case — stays finite
  // and keeps heading in, where an Euler step at this size would leave.
  const Spring stalled = start.step(0.0f, 250ms, parameters);
  EXPECT_LT(std::abs(stalled.value), std::abs(start.value));
  EXPECT_TRUE(std::isfinite(stalled.velocity));
}

TEST(Spring, DampingDecidesWhetherItCrossesTheTarget) {
  // Under 1 it overshoots — the reason to reach for a spring at all.
  bool crossed = false;
  Spring state{.value = 1.0f};
  for (int i = 0; i < 120; ++i) {
    state = state.step(0.0f, 1s / 120.0, {.period = 300ms, .damping = 0.3f});
    crossed = crossed || state.value < 0.0f;
  }
  EXPECT_TRUE(crossed);

  // At 1 and above it arrives from one side and never crosses.
  for (float damping : {1.0f, 1.4f, 3.0f}) {
    Spring settling{.value = 1.0f};
    for (int i = 0; i < 240; ++i) {
      settling = settling.step(0.0f, 1s / 120.0, {.period = 300ms, .damping = damping});
      ASSERT_GE(settling.value, -1e-5f) << "damping " << damping;
    }
    EXPECT_LT(settling.value, 0.05f) << "damping " << damping;
  }

  // Critical damping is a boundary in the solution, not in the motion:
  // the three branches agree where they meet.
  const Duration step = 50ms;
  const Spring under = (Spring{1.0f, 0.0f}).step(0.0f, step, {300ms, 0.9995f});
  const Spring critical = (Spring{1.0f, 0.0f}).step(0.0f, step, {300ms, 1.0f});
  const Spring over = (Spring{1.0f, 0.0f}).step(0.0f, step, {300ms, 1.0005f});
  EXPECT_NEAR(under.value, critical.value, 1e-3f);
  EXPECT_NEAR(over.value, critical.value, 1e-3f);
}

TEST(Spring, TheRingDecaysAtTheRateTheDampingNames) {
  // Successive extremes shrink by exp(-zeta*pi/sqrt(1-zeta^2)) — the
  // classic ratio, and the number a caller reasons in when it picks a
  // damping for a bounce it can SEE.
  const float zeta = 0.21545376f;
  const SpringParameters parameters{.period = Duration(0.39060562), .damping = zeta};
  const float expected =
      std::exp(-zeta * 3.14159265f / std::sqrt(1.0f - zeta * zeta));

  // A displacement of 40 released at rest, half a ring later: a hand-
  // authored ladder of +40 -> -20 -> +10 -> 0 is what this replaces, and
  // these params put the first extreme on it exactly.
  const Spring half = (Spring{40.0f, 0.0f}).step(0.0f, 200ms, parameters);
  EXPECT_NEAR(half.value, -20.0f, 1e-3f);
  EXPECT_NEAR(half.value / -40.0f, expected, 1e-3f);
  EXPECT_NEAR(half.velocity, 0.0f, 1e-2f);

  // …and it goes on ringing on its OWN period rather than on whatever
  // half-times the ladder's later segments were cut to.
  const Spring full = half.step(0.0f, 200ms, parameters);
  EXPECT_NEAR(full.value, 40.0f * expected * expected, 1e-3f);
}

TEST(Spring, AMovedTargetBendsTheFlightRatherThanRestartingIt) {
  // The velocity is why this is a state and not a curve. Halfway to one
  // target, handed another, the value keeps the speed it had: it does
  // not stop, and it does not jump.
  const SpringParameters parameters{.period = 500ms, .damping = 1.0f};
  Spring state = run({.value = 0.0f}, 100.0f, 150ms, 1s / 120.0, parameters);
  ASSERT_GT(state.velocity, 1.0f);

  const float before = state.value;
  const Spring bent = state.step(-100.0f, 1s / 120.0, parameters);
  // No jump: the step carries it about as far as the speed it already
  // had, and not one frame further.
  EXPECT_NEAR(bent.value, before, state.velocity / 120.0f);
  EXPECT_LT(bent.velocity, state.velocity);  // it has begun turning
  EXPECT_GT(bent.velocity, 0.0f);        // and is still going up

  // It gets there, from wherever the turn left it.
  const Spring landed = run(bent, -100.0f, 3s, 1s / 120.0, parameters);
  EXPECT_NEAR(landed.value, -100.0f, 0.5f);
}

TEST(Spring, TheEdgesAnswerRatherThanDivide) {
  const SpringParameters parameters{.period = 300ms, .damping = 0.5f};
  const Spring state{.value = 7.0f, .velocity = 3.0f};

  // A step of no time is no step.
  EXPECT_EQ(state.step(0.0f, 0s, parameters).value, 7.0f);
  EXPECT_EQ(state.step(0.0f, -1s, parameters).velocity, 3.0f);

  // A period of zero is the spelling of "instant": at the target, at rest.
  const Spring snapped = state.step(42.0f, 16ms, {.period = 0s});
  EXPECT_EQ(snapped.value, 42.0f);
  EXPECT_EQ(snapped.velocity, 0.0f);

  // A negative damping reads as 0 — a bell that keeps its amplitude
  // rather than a solution that grows without bound.
  const SpringParameters bell{.period = 400ms, .damping = -2.0f};
  const Spring rung = run({.value = 5.0f}, 0.0f, 4s, 1s / 120.0, bell);
  EXPECT_LE(std::abs(rung.value), 5.0f + 1e-3f);
  EXPECT_TRUE(std::isfinite(rung.value));
}

TEST(Spring, SettledIsAskedOfTheDistanceAndTheRateTogether) {
  // Approaching exponentially, a spring never exactly arrives, so the
  // question is answered against the caller's own units.
  EXPECT_FALSE((Spring{.value = 10.0f, .velocity = 0.0f}).isSettled(0.0f, 0.5f));
  EXPECT_FALSE((Spring{.value = 0.0f, .velocity = 90.0f}).isSettled(0.0f, 0.5f));
  EXPECT_TRUE((Spring{.value = 0.1f, .velocity = 0.2f}).isSettled(0.0f, 0.5f));

  // Sitting exactly on a target it is heading through at speed is not
  // rest — which is the case a distance-only test gets wrong.
  EXPECT_FALSE((Spring{.value = 100.0f, .velocity = 400.0f}).isSettled(100.0f));

  const SpringParameters parameters{.period = 250ms, .damping = 0.6f};
  const Spring landed = run({.value = 60.0f}, 0.0f, 3s, 1s / 120.0, parameters);
  EXPECT_TRUE(landed.isSettled(0.0f));
}

TEST(Spring, TwoSpringsCompareByWhatTheyHold) {
  // A held spring is part of the description of the thing it moves, so
  // an owner asking whether anything changed compares the state rather
  // than stepping it again to see. Both numbers count: a value on its
  // target carrying speed is not the same state as one at rest there.
  EXPECT_EQ(Spring{}, (Spring{.value = 0.0f, .velocity = 0.0f}));
  EXPECT_EQ((Spring{.value = 3.0f, .velocity = -2.0f}),
            (Spring{.value = 3.0f, .velocity = -2.0f}));
  EXPECT_NE((Spring{.value = 3.0f}), (Spring{.value = 3.0f, .velocity = 0.1f}));
  EXPECT_NE((Spring{.value = 3.0f}), (Spring{.value = 3.5f}));

  // The parameters likewise, so a pair of settings can be told apart
  // without reading their fields one at a time.
  EXPECT_EQ(SpringParameters{},
            (SpringParameters{.period = 400ms, .damping = 0.5f}));
  EXPECT_NE((SpringParameters{.period = 400ms, .damping = 0.5f}),
            (SpringParameters{.period = 400ms, .damping = 0.55f}));
  EXPECT_NE((SpringParameters{.period = 400ms, .damping = 0.5f}),
            (SpringParameters{.period = 410ms, .damping = 0.5f}));

  // A step of no time answers the state it was handed, equal to it.
  const Spring moving{.value = 7.0f, .velocity = 3.0f};
  EXPECT_EQ(moving.step(0.0f, 0s, {.period = 300ms}), moving);
}
