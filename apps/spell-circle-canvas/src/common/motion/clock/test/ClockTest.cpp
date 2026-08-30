/** @file
 * The frame clock's pause/scale/clamp behavior and the Ticker driving
 * Choreograph motions, steppables and derivations to completion with no
 * renderer under it.
 */

#include <gtest/gtest.h>
#include <sigilmotion/bind/Bound.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>

#include <algorithm>
#include <cmath>
#include <vector>

// POSITIVE CONTROL for the "SigilMotion alone" tests below. Those claim a
// consumer can drive these values without linking a drawing library, and
// the claim would pass for the wrong reason if a drawing library happened
// to be on the include path anyway. This target links SigilMotion and
// gtest only, so a rendering library's headers must be UNREACHABLE here.
// If SigilMotion grows a link edge that drags them in, the build stops
// rather than quietly hollowing the tests out.
#if __has_include(<sigilcompose/Compose.h>)
#error \
    "motion_clock_test can see a drawing library's headers — the tests \
below no longer prove that SigilMotion stands alone."
#endif

using namespace sigil::motion;
namespace ch = choreograph;
using namespace std::chrono_literals;

TEST(FrameClockTest, FirstTickIsZeroThenDeltas) {
  FrameClock clock;
  EXPECT_EQ(clock.tick(10.0), 0.0);
  EXPECT_NEAR(clock.tick(10.016), 0.016, 1e-9);
  EXPECT_NEAR(clock.elapsed(), 0.016, 1e-9);
}

TEST(FrameClockTest, ClampsStallsAndScalesTime) {
  FrameClock clock({.maxDelta = 0.25});
  clock.tick(0.0);
  EXPECT_NEAR(clock.tick(5.0), 0.25, 1e-9);  // suspended app: clamped

  clock.setTimeScale(0.5);
  EXPECT_NEAR(clock.tick(5.1), 0.05, 1e-9);  // half speed
}

TEST(FrameClockTest, PauseFreezesElapsed) {
  FrameClock clock;
  clock.tick(0.0);
  clock.tick(0.1);
  clock.setPaused(true);
  EXPECT_EQ(clock.tick(0.2), 0.0);
  EXPECT_NEAR(clock.elapsed(), 0.1, 1e-9);
  clock.setPaused(false);
  // A paused tick still advances the clock's own timestamp, so the paused
  // span is consumed rather than banked: unpausing yields the delta since
  // the last tick, not a catch-up jump covering the whole pause.
  EXPECT_NEAR(clock.tick(0.3), 0.1, 1e-9);
}

TEST(TickerTest, DrivesMotionToCompletionAndSettles) {
  Ticker ticker;
  ch::Output<float> value = 0.0f;
  ticker.timeline().apply(&value).then<ch::RampTo>(10.0f, 1.0f);

  EXPECT_TRUE(ticker.active());
  ticker.tick(0.5);
  EXPECT_NEAR(value.value(), 5.0f, 1e-4);
  EXPECT_TRUE(ticker.active());

  ticker.tick(0.6);  // past the end
  EXPECT_NEAR(value.value(), 10.0f, 1e-4);
  EXPECT_FALSE(ticker.active());  // finished motions self-remove
}

TEST(TickerTest, SteppablesReportAndRetire) {
  Ticker ticker;
  double accumulated = 0.0;
  ticker.add([&accumulated](double dt) {
    accumulated += dt;
    return accumulated < 1.0;
  });

  EXPECT_TRUE(ticker.tick(0.4));
  EXPECT_TRUE(ticker.tick(0.4));
  EXPECT_FALSE(ticker.tick(0.4));  // crossed 1.0 → retired
  EXPECT_FALSE(ticker.active());
}

// ---------------------------------------------------------------------------
// Animation values (<sigilmotion/Animation.h>). These prove the values are
// usable through SigilMotion ALONE: no drawing library, no layout engine
// and no scene kernel is linked here, and the #error guard at the top of
// this file keeps it that way. A consumer's own coverage — how it stores
// these values and resolves them per frame — belongs in that consumer's
// tests.

TEST(TickerTest, ADerivationNeverReadsAStaleSource) {
  // THE STEPPING ORDER. The derivation is registered FIRST and its
  // source's writer SECOND — the arrangement that would read one frame
  // stale under any single-phase step honouring registration order. The
  // two-phase contract (sources, then derivations) makes the answer
  // current whatever the order was.
  Ticker ticker;
  ch::Output<float> src{0.0f}, dst{0.0f};
  ASSERT_TRUE(ticker.derive(&dst, bind(&src).offset(-0.25f)));
  double t = 0.0;
  ticker.add([&](double dt) {
    t += dt;
    src = (float)t;
    return true;
  });

  ticker.tick(0.5);
  EXPECT_FLOAT_EQ(src.value(), 0.5f);
  EXPECT_FLOAT_EQ(dst.value(), 0.25f)
      << "the derivation read LAST frame's source — the two-phase step "
         "contract is broken";
  ticker.tick(0.25);
  EXPECT_FLOAT_EQ(dst.value(), 0.5f);

  // The same contract for a TIMELINE-driven source: the timeline steps in
  // phase one too.
  Ticker ticker2;
  ch::Output<float> ramped{0.0f}, shadow{0.0f};
  ASSERT_TRUE(ticker2.derive(&shadow, bind(&ramped).scale(2.0f)));
  ticker2.timeline().apply(&ramped).then<ch::RampTo>(1.0f, 1.0f);
  ticker2.tick(0.5);
  EXPECT_NEAR(ramped.value(), 0.5f, 1e-4f);
  EXPECT_FLOAT_EQ(shadow.value(), ramped.value() * 2.0f);

  // …and registration is applied immediately, so a derived cell is
  // correct BEFORE the first tick.
  Ticker ticker3;
  ch::Output<float> held{3.0f}, doubled{0.0f};
  ASSERT_TRUE(ticker3.derive(&doubled, bind(&held).scale(2.0f)));
  EXPECT_FLOAT_EQ(doubled.value(), 6.0f);
}

TEST(TickerTest, DeriveEnforcesTheOneLevelRuleLoudly) {
  Ticker ticker;
  ch::Output<float> a{1.0f}, b{0.0f}, c{0.0f}, x{0.0f};
  ASSERT_TRUE(ticker.derive(&b, bind(&a).scale(2.0f)));

  // A derivation of a derivation is REFUSED, in either registration
  // order, rather than accepted and silently one frame late.
  EXPECT_FALSE(ticker.derive(&c, bind(&b).offset(1.0f)))
      << "derive-of-derive must refuse: phase two has no topological order";
  EXPECT_FALSE(ticker.derive(&a, bind(&x).offset(1.0f)))
      << "writing an Output that FEEDS a derivation chains two levels the "
         "other way round";
  // Two writers of one destination, and self-derivation.
  EXPECT_FALSE(ticker.derive(&b, bind(&x).offset(1.0f)));
  EXPECT_FALSE(ticker.derive(&x, bind(&x).offset(1.0f)));
  // Degenerate registrations.
  EXPECT_FALSE(ticker.derive(nullptr, bind(&a)));
  EXPECT_FALSE(ticker.derive(&c, bind(nullptr)));

  // The refused chains never write: after a tick, c and x hold their own
  // values and the ACCEPTED derivation still runs.
  ticker.tick(0.1);
  EXPECT_FLOAT_EQ(b.value(), 2.0f);
  EXPECT_FLOAT_EQ(c.value(), 0.0f);
  EXPECT_FLOAT_EQ(x.value(), 0.0f);
}

TEST(TickerTest, DerivedOutputsComposeAndDoNotHoldTheTickerAwake) {
  // A derived Output is an ORDINARY Output: the whole point. bind() it,
  // wiggle() off it, hand it to any Output*-typed consumer.
  Ticker ticker;
  ch::Output<float> phase{0.0f}, trail{0.0f};
  ticker.timeline().apply(&phase).then<ch::RampTo>(1.0f, 1.0f);
  ASSERT_TRUE(ticker.derive(&trail, bind(&phase).offset(-0.25f).clamp(0, 1)));

  const BoundFloat px = bind(&trail).target(0.0f, 240.0f).value();
  const BoundFloat shake = wiggle(&trail, 3.0f, 7.0f, 2).value();
  EXPECT_EQ(shake.source, &trail);

  ticker.tick(0.5);
  EXPECT_FLOAT_EQ(trail.value(), 0.25f);
  EXPECT_FLOAT_EQ(px.apply(trail.value()), 60.0f);
  EXPECT_LE(std::fabs(shake.apply(trail.value())), 3.0f + 1e-3f);

  // Derivations are pure in their sources, so they do NOT hold active()
  // true: when nothing else moves, nothing they read can move. A ticker
  // whose only tenant is a derivation settles, and hosts stay
  // event-driven.
  ticker.tick(0.6);  // the ramp finishes and self-removes
  EXPECT_FLOAT_EQ(trail.value(), 0.75f);
  EXPECT_FALSE(ticker.active())
      << "a derivation must not keep the host rendering forever";
  // …but a tick still refreshes it (a host may write the source by hand).
  phase = 0.5f;
  ticker.tick(0.0);
  EXPECT_FLOAT_EQ(trail.value(), 0.25f);
}

TEST(AnimationValues, WiggleRigShakesTwoAxesAroundRest) {
  // A camera shake, the case that seeding exists for.
  // `wiggle(&out, …)` is `bind(&out).scale(0).wiggle(…)` named, so the
  // property sits at REST and only the noise moves it — the phase still
  // comes from the schedule whose contribution was zeroed.
  Ticker ticker;
  ch::Output<float> seconds = 0.0f;
  ticker.timeline().apply(&seconds).then<ch::RampTo>(2.0f, 2.0f);  // 1:1

  const BoundFloat shakeX = wiggle(&seconds, 12.f, 7.f, 1).value();
  const BoundFloat shakeY = wiggle(&seconds, 12.f, 7.f, 2).value();
  EXPECT_EQ(wiggle(&seconds, 12.f, 7.f, 1).value().source, &seconds);

  float maxX = 0, maxY = 0, sameSign = 0;
  int samples = 0;
  for (int frame = 0; frame < 120; ++frame) {
    ticker.tick(1.0 / 60.0);
    const float x = shakeX.apply(seconds.value());
    const float y = shakeY.apply(seconds.value());
    EXPECT_LE(std::fabs(x), 12.0f + 1e-3f);
    EXPECT_LE(std::fabs(y), 12.0f + 1e-3f);
    maxX = std::max(maxX, std::fabs(x));
    maxY = std::max(maxY, std::fabs(y));
    if ((x > 0) == (y > 0)) ++sameSign;
    ++samples;
  }
  EXPECT_GT(maxX, 6.0f) << "the rig never moved";
  EXPECT_GT(maxY, 6.0f);
  // Not a diagonal: if the two axes shared a field they would agree in
  // sign on every single frame.
  EXPECT_LT(sameSign / (float)samples, 0.8f);

  // .offset() parks the shake somewhere other than zero, and it composes
  // in call order like any affine stage.
  const BoundFloat parked = wiggle(&seconds, 3.f, 7.f, 1).offset(100.f).value();
  EXPECT_NEAR(parked.apply(0.5f), 100.f + shakeX.apply(0.5f) / 4.0f, 1e-3f);
}

// ---------------------------------------------------------------------------
// The ENVELOPE stage — pingPong / cosine / trapezoid, the three SHAPES a
// one-way phase can take on its way through the chain.
