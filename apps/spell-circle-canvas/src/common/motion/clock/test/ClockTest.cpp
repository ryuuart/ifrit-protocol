/** @file
 * The frame clock — what one reading after another means, and what pause,
 * time scale and the stall ceiling do to it — and the Ticker that drives
 * Choreograph motions, steppables and derivations to completion with no
 * renderer under it. No wall clock is read anywhere in this file: every
 * frame length is a number the test hands in.
 */

#include <gtest/gtest.h>
#include <sigilmotion/bind/Bound.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "support/StandsAlone.h"

using namespace sigil::motion;
namespace ch = choreograph;
using namespace std::chrono_literals;

TEST(FrameClock, TheFirstTickIsAZeroLengthFrameAndEveryOneAfterItIsADelta) {
  FrameClock clock;
  EXPECT_EQ(clock.tick(10.0), 0.0);
  EXPECT_NEAR(clock.tick(10.016), 0.016, 1e-9);
  EXPECT_NEAR(clock.elapsed(), 0.016, 1e-9);
}

TEST(FrameClock, AStallIsClampedToTheLongestFrameItWasGiven) {
  // A suspended app comes back to one enormous reading; the clock hands
  // out the ceiling rather than a delta every animation would jump on.
  FrameClock clock({.maxDelta = 0.25});
  clock.tick(0.0);
  EXPECT_NEAR(clock.tick(5.0), 0.25, 1e-9);
}

TEST(FrameClock, TheTimeScaleMultipliesEveryDeltaAfterItIsSet) {
  FrameClock clock;
  clock.tick(0.0);
  EXPECT_NEAR(clock.tick(0.1), 0.1, 1e-9);
  clock.setTimeScale(0.5);
  EXPECT_NEAR(clock.tick(0.2), 0.05, 1e-9);
}

TEST(FrameClock, ABackwardReadingReportsNoTimeRatherThanRewinding) {
  // Time in this library only goes forward, so a reading behind the last
  // one is a zero-length frame — never a negative delta an animation
  // would step backwards on. The clock still adopts the new reading, so
  // the frame after it measures from there.
  FrameClock clock;
  clock.tick(10.0);
  EXPECT_NEAR(clock.tick(10.1), 0.1, 1e-9);
  EXPECT_EQ(clock.tick(10.05), 0.0);
  EXPECT_NEAR(clock.elapsed(), 0.1, 1e-9);
  EXPECT_NEAR(clock.tick(10.15), 0.1, 1e-9);
}

TEST(FrameClock, APausedTickAddsNoElapsedTimeAndBanksNone) {
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

TEST(Ticker, DrivesAMotionToCompletionAndThenSettles) {
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

TEST(Ticker, ASteppableRunsUntilItReportsItIsDoneAndIsThenDropped) {
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
// derive() — a cell recomputed every tick from another cell through a
// bound chain, and the two-phase step that makes it current rather than
// one frame late.

TEST(Ticker, ADerivationNeverReadsAStaleSource) {
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
}

TEST(Ticker, ATimelineDrivenSourceIsSteppedBeforeItsDerivation) {
  // The timeline steps in phase one too, so a cell derived from a ramped
  // Output reads this frame's ramp rather than last frame's.
  Ticker ticker;
  ch::Output<float> ramped{0.0f}, shadow{0.0f};
  ASSERT_TRUE(ticker.derive(&shadow, bind(&ramped).scale(2.0f)));
  ticker.timeline().apply(&ramped).then<ch::RampTo>(1.0f, 1.0f);
  ticker.tick(0.5);
  EXPECT_NEAR(ramped.value(), 0.5f, 1e-4f);
  EXPECT_FLOAT_EQ(shadow.value(), ramped.value() * 2.0f);
}

TEST(Ticker, ADerivedCellIsCorrectBeforeTheFirstTick) {
  // The chain is applied once at registration, so a host that draws
  // before it ticks draws the right number.
  Ticker ticker;
  ch::Output<float> held{3.0f}, doubled{0.0f};
  ASSERT_TRUE(ticker.derive(&doubled, bind(&held).scale(2.0f)));
  EXPECT_FLOAT_EQ(doubled.value(), 6.0f);
}

TEST(Ticker, DeriveEnforcesTheOneLevelRuleLoudly) {
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

TEST(Ticker, DerivedOutputsComposeAndDoNotHoldTheTickerAwake) {
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

TEST(Ticker, AWiggleRigMovesOnlyWhenTheTickerAdvancesTheOutputItReads) {
  // A camera shake driven by the clock rather than by a phase the caller
  // steps: the seconds Output rides the ticker, and the rig reads it. The
  // rig sits at REST — `wiggle(&out, …)` is `bind(&out).scale(0)
  // .wiggle(…)` named — so what moves the property is only the noise, and
  // the only thing that moves the noise is the ticker advancing seconds.
  // What the shake is BOUNDED by is the binding's own claim and is asked
  // of it directly in the bind tests; what is asked here is that the
  // clock is what drives it.
  Ticker ticker;
  ch::Output<float> seconds = 0.0f;
  ticker.timeline().apply(&seconds).then<ch::RampTo>(2.0f, 2.0f);  // 1:1

  const BoundFloat shakeX = wiggle(&seconds, 12.f, 7.f, 1).value();
  const BoundFloat shakeY = wiggle(&seconds, 12.f, 7.f, 2).value();
  EXPECT_EQ(shakeX.source, &seconds);

  const float atRest = shakeX.apply(seconds.value());
  EXPECT_FLOAT_EQ(shakeX.apply(seconds.value()), atRest)
      << "the rig read time for itself rather than the Output it names";

  std::vector<float> xs, ys;
  for (int frame = 0; frame < 120; ++frame) {
    ticker.tick(1.0 / 60.0);
    xs.push_back(shakeX.apply(seconds.value()));
    ys.push_back(shakeY.apply(seconds.value()));
  }
  EXPECT_NE(std::count(xs.begin(), xs.end(), atRest), (long)xs.size())
      << "the rig never moved as the ticker ran";
  EXPECT_NE(xs, ys) << "two seeds on one clock moved together";

  // A tick of no length advances nothing, so the rig holds where it was.
  const float held = xs.back();
  ticker.tick(0.0);
  EXPECT_FLOAT_EQ(shakeX.apply(seconds.value()), held);
}

// ---------------------------------------------------------------------------
// addFixed — a steppable that advances at its own rate whatever the host
// draws at, which is what anything simulation-shaped needs before it will
// look the same on two machines.

namespace {

/** A fixed-rate steppable that only counts, and the count. */
struct FixedRun {
  Ticker ticker;
  int steps = 0;

  void at(double hz, int maxCatchUp = 8, ch::Output<float>* alpha = nullptr,
          Ticker::FixedStatus* status = nullptr) {
    ticker.addFixed(
        hz,
        [this] {
          ++steps;
          return true;
        },
        maxCatchUp, alpha, status);
  }

  /** Ticks @p seconds worth of frames at @p fps and answers the count. */
  int over(double seconds, double fps) {
    const int frames = (int)std::lround(seconds * fps);
    const double dt = seconds / (double)frames;
    for (int i = 0; i < frames; ++i) ticker.tick(dt);
    return steps;
  }
};

}  // namespace

TEST(Ticker, AFixedStepRunsAtItsOwnRateWhateverRateTheHostDrawsAt) {
  for (double fps : {24.0, 60.0, 144.0}) {
    FixedRun run;
    run.at(27.0);
    EXPECT_EQ(run.over(1.0, fps), 27) << "drawing at " << fps;
  }
}

TEST(Ticker, AFixedStepCountsFromTotalTimeSoOneMomentIsAlwaysTheSameStep) {
  // The count is `floor(total * hz)` and not a running sum, so the same
  // simulated instant lands on the same side of a step boundary at every
  // draw rate. A sum compared against a step size slips by one comparison
  // over a long pre-roll, and only at some rates.
  FixedRun reference;
  reference.at(60.0, 64);
  const int expected = reference.over(3.1, 60.0);
  for (double fps : {10.0, 15.0, 20.0, 30.0, 120.0}) {
    FixedRun run;
    run.at(60.0, 64);
    EXPECT_EQ(run.over(3.1, fps), expected) << "drawing at " << fps;
  }
}

TEST(Ticker, AFixedStepDropsItsBacklogRatherThanRunningItAndSaysSoWhenItDid) {
  // A hitch longer than one step would otherwise make the next frame run
  // the backlog, which takes longer still. Dropping simulated time is the
  // correct failure, and the flag is the only signal that anything
  // measured on that frame is meaningless.
  Ticker::FixedStatus status;
  FixedRun run;
  run.at(60.0, /*maxCatchUp=*/4, nullptr, &status);

  run.ticker.tick(10.0);  // ten seconds in one frame: 600 steps of backlog
  EXPECT_EQ(run.steps, 4);
  EXPECT_EQ(status.stepsRun, 4);
  EXPECT_TRUE(status.clamped);

  // The dropped time is gone rather than carried into the next frame.
  run.steps = 0;
  run.ticker.tick(1.0 / 60.0);
  EXPECT_EQ(run.steps, 1);
  EXPECT_FALSE(status.clamped);
}

TEST(Ticker, AFixedStepThatReportsItIsDoneIsDroppedLikeAnyOther) {
  Ticker ticker;
  int steps = 0;
  ticker.addFixed(60.0, [&steps] { return ++steps < 3; });
  ticker.tick(1.0);
  const int afterTheFirstSecond = steps;
  ticker.tick(1.0);
  EXPECT_EQ(steps, afterTheFirstSecond);
}

TEST(Ticker, AFixedStepPublishesHowFarThroughAStepTheFrameEnded) {
  // A fixed-rate simulation drawn straight from its own state judders
  // whenever the draw rate is not a multiple of its own; the leftover
  // fraction is what `lerp(previous, current, alpha)` needs.
  ch::Output<float> alpha{-1.0f};
  FixedRun run;
  run.at(10.0, 8, &alpha);

  run.ticker.tick(0.05);  // half a step in: none taken, and alpha says so
  EXPECT_EQ(run.steps, 0);
  EXPECT_NEAR(alpha.value(), 0.5f, 1e-4f);

  run.ticker.tick(0.07);  // across the boundary: one step, the rest left over
  EXPECT_EQ(run.steps, 1);
  EXPECT_NEAR(alpha.value(), 0.2f, 1e-4f);

  run.ticker.tick(0.08);  // landing on a boundary leaves nothing over
  EXPECT_EQ(run.steps, 2);
  EXPECT_NEAR(alpha.value(), 0.0f, 1e-4f);
}
