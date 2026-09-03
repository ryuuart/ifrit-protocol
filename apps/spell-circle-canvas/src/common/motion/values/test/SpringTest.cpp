/** @file
 * The spring: that it is solved rather than integrated (one step of any
 * size lands where many small ones do), that damping decides whether it
 * crosses the target, that a moved target bends the flight instead of
 * restarting it, and what it answers at its edges.
 */

#include <gtest/gtest.h>
#include <sigilmotion/values/Spring.h>

#include <cmath>
#include <vector>

#include "support/StandsAlone.h"

using namespace sigil::motion;

namespace {

/** Run a spring to `seconds` in fixed steps, reporting where it ended. */
Spring run(Spring s, float target, float seconds, float dt, SpringParams p) {
  for (float t = 0.0f; t < seconds - 1e-6f; t += dt)
    s = spring(s, target, dt, p);
  return s;
}

}  // namespace

TEST(Spring, OneBigStepLandsWhereManySmallOnesDo) {
  // The closed-form solution is what makes a spring safe on whatever
  // delta a frame clock hands over: a stalled frame is a big step, not
  // an explosion, and no substepping is needed to keep it stable.
  const SpringParams p{.periodSeconds = 0.35f, .damping = 0.4f};
  const Spring start{.value = 120.0f, .velocity = -40.0f};

  const Spring stepped = run(start, 0.0f, 0.5f, 1.0f / 240.0f, p);
  const Spring once = spring(start, 0.0f, 0.5f, p);
  EXPECT_NEAR(stepped.value, once.value, 1e-2f);
  EXPECT_NEAR(stepped.velocity, once.velocity, 1.0f);

  // A quarter-second frame — the clock's own worst case — stays finite
  // and keeps heading in, where an Euler step at this size would leave.
  const Spring stalled = spring(start, 0.0f, 0.25f, p);
  EXPECT_LT(std::abs(stalled.value), std::abs(start.value));
  EXPECT_TRUE(std::isfinite(stalled.velocity));
}

TEST(Spring, DampingDecidesWhetherItCrossesTheTarget) {
  // Under 1 it overshoots — the reason to reach for a spring at all.
  bool crossed = false;
  Spring s{.value = 1.0f};
  for (int i = 0; i < 120; ++i) {
    s = spring(s, 0.0f, 1.0f / 120.0f, {.periodSeconds = 0.3f, .damping = 0.3f});
    crossed = crossed || s.value < 0.0f;
  }
  EXPECT_TRUE(crossed);

  // At 1 and above it arrives from one side and never crosses.
  for (float damping : {1.0f, 1.4f, 3.0f}) {
    Spring q{.value = 1.0f};
    for (int i = 0; i < 240; ++i) {
      q = spring(q, 0.0f, 1.0f / 120.0f,
                 {.periodSeconds = 0.3f, .damping = damping});
      ASSERT_GE(q.value, -1e-5f) << "damping " << damping;
    }
    EXPECT_LT(q.value, 0.05f) << "damping " << damping;
  }

  // Critical damping is a boundary in the solution, not in the motion:
  // the three branches agree where they meet.
  const float dt = 0.05f;
  const Spring under = spring({1.0f, 0.0f}, 0.0f, dt, {0.3f, 0.9995f});
  const Spring crit = spring({1.0f, 0.0f}, 0.0f, dt, {0.3f, 1.0f});
  const Spring over = spring({1.0f, 0.0f}, 0.0f, dt, {0.3f, 1.0005f});
  EXPECT_NEAR(under.value, crit.value, 1e-3f);
  EXPECT_NEAR(over.value, crit.value, 1e-3f);
}

TEST(Spring, TheRingDecaysAtTheRateTheDampingNames) {
  // Successive extremes shrink by exp(-zeta*pi/sqrt(1-zeta^2)) — the
  // classic ratio, and the number a caller reasons in when it picks a
  // damping for a bounce it can SEE.
  const float zeta = 0.21545376f;
  const SpringParams p{.periodSeconds = 0.39060562f, .damping = zeta};
  const float expected = std::exp(-zeta * 3.14159265f /
                                  std::sqrt(1.0f - zeta * zeta));

  // A displacement of 40 released at rest, half a ring later: a hand-
  // authored ladder of +40 -> -20 -> +10 -> 0 is what this replaces, and
  // these params put the first extreme on it exactly.
  const Spring half = spring({40.0f, 0.0f}, 0.0f, 0.2f, p);
  EXPECT_NEAR(half.value, -20.0f, 1e-3f);
  EXPECT_NEAR(half.value / -40.0f, expected, 1e-3f);
  EXPECT_NEAR(half.velocity, 0.0f, 1e-2f);

  // …and it goes on ringing on its OWN period rather than on whatever
  // half-times the ladder's later segments were cut to.
  const Spring full = spring(half, 0.0f, 0.2f, p);
  EXPECT_NEAR(full.value, 40.0f * expected * expected, 1e-3f);
}

TEST(Spring, AMovedTargetBendsTheFlightRatherThanRestartingIt) {
  // The velocity is why this is a state and not a curve. Halfway to one
  // target, handed another, the value keeps the speed it had: it does
  // not stop, and it does not jump.
  const SpringParams p{.periodSeconds = 0.5f, .damping = 1.0f};
  Spring s = run({.value = 0.0f}, 100.0f, 0.15f, 1.0f / 120.0f, p);
  ASSERT_GT(s.velocity, 1.0f);

  const float before = s.value;
  const Spring bent = spring(s, -100.0f, 1.0f / 120.0f, p);
  // No jump: the step carries it about as far as the speed it already
  // had, and not one frame further.
  EXPECT_NEAR(bent.value, before, s.velocity / 120.0f);
  EXPECT_LT(bent.velocity, s.velocity);        // it has begun turning
  EXPECT_GT(bent.velocity, 0.0f);              // and is still going up

  // It gets there, from wherever the turn left it.
  const Spring landed = run(bent, -100.0f, 3.0f, 1.0f / 120.0f, p);
  EXPECT_NEAR(landed.value, -100.0f, 0.5f);
}

TEST(Spring, TheEdgesAnswerRatherThanDivide) {
  const SpringParams p{.periodSeconds = 0.3f, .damping = 0.5f};
  const Spring s{.value = 7.0f, .velocity = 3.0f};

  // A step of no time is no step.
  EXPECT_EQ(spring(s, 0.0f, 0.0f, p).value, 7.0f);
  EXPECT_EQ(spring(s, 0.0f, -1.0f, p).velocity, 3.0f);

  // A period of zero is the spelling of "instant": at the target, at rest.
  const Spring snapped = spring(s, 42.0f, 0.016f, {.periodSeconds = 0.0f});
  EXPECT_EQ(snapped.value, 42.0f);
  EXPECT_EQ(snapped.velocity, 0.0f);

  // A negative damping reads as 0 — a bell that keeps its amplitude
  // rather than a solution that grows without bound.
  const SpringParams bell{.periodSeconds = 0.4f, .damping = -2.0f};
  const Spring rung = run({.value = 5.0f}, 0.0f, 4.0f, 1.0f / 120.0f, bell);
  EXPECT_LE(std::abs(rung.value), 5.0f + 1e-3f);
  EXPECT_TRUE(std::isfinite(rung.value));
}

TEST(Spring, MovingIsAskedOfTheDistanceAndTheRateTogether) {
  // Approaching exponentially, a spring never exactly arrives, so the
  // question is answered against the caller's own units.
  EXPECT_TRUE(springMoving({.value = 10.0f, .velocity = 0.0f}, 0.0f, 0.5f));
  EXPECT_TRUE(springMoving({.value = 0.0f, .velocity = 90.0f}, 0.0f, 0.5f));
  EXPECT_FALSE(springMoving({.value = 0.1f, .velocity = 0.2f}, 0.0f, 0.5f));

  // Sitting exactly on a target it is heading through at speed is not
  // rest — which is the case a distance-only test gets wrong.
  EXPECT_TRUE(springMoving({.value = 100.0f, .velocity = 400.0f}, 100.0f));

  const SpringParams p{.periodSeconds = 0.25f, .damping = 0.6f};
  const Spring landed = run({.value = 60.0f}, 0.0f, 3.0f, 1.0f / 120.0f, p);
  EXPECT_FALSE(springMoving(landed, 0.0f));
}
