// SigilMotion: the frame clock's pause/scale/clamp behavior, the Ticker
// actually driving Choreograph motions to completion, and the animation
// VALUES (Transition / ease:: / animate() / bind()) standing up with no
// renderer under them.

#include <sigilmotion/Animation.h>
#include <sigilmotion/FrameClock.h>
#include <sigilmotion/Ticker.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

// POSITIVE CONTROL for the "SigilMotion alone" pin below. The whole point
// of moving the animation values out of <sigilcompose/Compose.h> is that a
// consumer can drive them without a drawing library, and a test asserting
// that is worthless if compose happens to be on the include path anyway —
// it would pass for the wrong reason. motion_test links SigilMotion and
// gtest only, so compose's headers must be UNREACHABLE from this TU. If
// SigilMotion ever grows a link edge that drags them in, this stops the
// build instead of quietly hollowing out the pin.
#if __has_include(<sigilcompose/Compose.h>)
#error "motion_test can see SigilCompose headers — the SigilMotion-alone \
pin is no longer proving anything (ROADMAP §37)."
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
  EXPECT_NEAR(clock.tick(5.0), 0.25, 1e-9); // suspended app: clamped

  clock.setTimeScale(0.5);
  EXPECT_NEAR(clock.tick(5.1), 0.05, 1e-9); // half speed
}

TEST(FrameClockTest, PauseFreezesElapsed) {
  FrameClock clock;
  clock.tick(0.0);
  clock.tick(0.1);
  clock.setPaused(true);
  EXPECT_EQ(clock.tick(0.2), 0.0);
  EXPECT_NEAR(clock.elapsed(), 0.1, 1e-9);
  clock.setPaused(false);
  // The paused span was consumed while paused — no catch-up jump.
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

  ticker.tick(0.6); // past the end
  EXPECT_NEAR(value.value(), 10.0f, 1e-4);
  EXPECT_FALSE(ticker.active()); // finished motions self-remove
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
  EXPECT_FALSE(ticker.tick(0.4)); // crossed 1.0 → retired
  EXPECT_FALSE(ticker.active());
}

// ---------------------------------------------------------------------------
// Animation values (<sigilmotion/Animation.h>) — moved out of SigilCompose
// 2026-07-29. These tests exist to prove the values are usable through
// SigilMotion ALONE: no Skia, no Yoga, no compose kernel is linked into
// motion_test, and the #error guard at the top of this file keeps it that
// way. Compose's own coverage of the same types (its reconciler compare,
// the Composer running them per frame) stays in compose_test.

TEST(AnimationValues, TransitionSurvivesAnEmptyEase) {
  // `{360ms, {}, 220ms}` — the obvious way to name a delay and keep the
  // house curve — leaves `ease` an EMPTY std::function. Reading it raw
  // throws bad_function_call on the first frame; easing() is the fix.
  const Transition named{360ms, {}, 220ms};
  EXPECT_EQ(named.duration, 360ms);
  EXPECT_EQ(named.delay, 220ms);
  EXPECT_FALSE((bool)named.ease);
  EXPECT_TRUE((bool)named.easing());
  EXPECT_NEAR(named.easing()(0.5f), choreograph::easeOutQuad(0.5f), 1e-6f);

  const Transition spec{200ms, ease::outBack()};
  EXPECT_GT(spec.easing()(0.8f), 1.0f); // overshoot, then settle
  EXPECT_NEAR(spec.easing()(1.0f), 1.0f, 1e-5f);
}

TEST(AnimationValues, AnimateBuildersDescribeEntranceChangeAndPath) {
  const Transitioned<float> entrance = animate(from(0.0f).to(1.0f), {400ms});
  EXPECT_FLOAT_EQ(entrance.value, 1.0f);
  ASSERT_TRUE(entrance.from.has_value());
  EXPECT_FLOAT_EQ(*entrance.from, 0.0f);
  EXPECT_EQ(entrance.spec.duration, 400ms);
  EXPECT_TRUE(entrance.waypoints.empty());

  const Transitioned<float> change = animate(to(0.4f), {180ms});
  EXPECT_FLOAT_EQ(change.value, 0.4f);
  EXPECT_FALSE(change.from.has_value()); // no entrance: ramp on change only

  const Transitioned<float> path = animate(
      through({{0ms, 40.f}, {200ms, -20.f}, {300ms, 10.f}, {400ms, 0.f}}));
  EXPECT_EQ(path.waypoints.size(), 4u);
  ASSERT_TRUE(path.from.has_value());
  EXPECT_FLOAT_EQ(*path.from, 40.f);   // first waypoint is the entrance
  EXPECT_FLOAT_EQ(path.value, 0.f);    // last is the settled value
  EXPECT_EQ(path.spec.duration, 400ms);

  // The degenerate ask must still be DETERMINATE — value-initialized, not
  // whatever was on the stack.
  const Transitioned<float> empty = animate(through({}));
  EXPECT_FLOAT_EQ(empty.value, 0.0f);
}

TEST(AnimationValues, BoundChainComposesInCallOrder) {
  ch::Output<float> phase = 0.0f;

  const BoundFloat named = bind(&phase).source(0, 100).target(-70, 170).value();
  const BoundFloat manual =
      bind(&phase).source(0, 100).scale(240).offset(-70).value();
  for (float v : {0.f, 25.f, 50.f, 100.f})
    EXPECT_NEAR(named.apply(v), manual.apply(v), 1e-4f);
  EXPECT_NEAR(named.apply(0.f), -70.f, 1e-4f);
  EXPECT_NEAR(named.apply(100.f), 170.f, 1e-4f);

  // Order matters and reads the way it looks.
  EXPECT_NEAR(bind(&phase).scale(240).offset(-70).value().apply(0.5f),
              0.5f * 240.f - 70.f, 1e-4f);
  EXPECT_NEAR(bind(&phase).offset(-70).scale(240).value().apply(0.5f),
              (0.5f - 70.f) * 240.f, 1e-4f);

  // window() is source() that also clamps, so a curve downstream never
  // sees a value outside its domain.
  const BoundFloat w = bind(&phase).window(0.2f, 0.4f).value();
  const BoundFloat s = bind(&phase).source(0.2f, 0.4f).value();
  EXPECT_NEAR(w.apply(0.3f), s.apply(0.3f), 1e-4f);
  EXPECT_NEAR(w.apply(0.9f), 1.0f, 1e-4f);
  EXPECT_GT(s.apply(0.9f), 1.0f);

  EXPECT_NEAR(bind(&phase).quantize(5).value().apply(0.31f), 0.25f, 1e-4f);
  EXPECT_NEAR(bind(&phase).invert().value().apply(0.25f), 0.75f, 1e-4f);
  EXPECT_NEAR(bind(&phase).clamp(0, 1).value().apply(4.0f), 1.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// wiggle() — the procedural noise stage (2026-07-29). AE's most-used
// expression, phased off the NORMALISED INPUT rather than off a clock; see
// Bound::wiggle for both rulings.

namespace {
/** Sample a shaped binding across a phase sweep — the trace every wiggle
 *  pin below reasons about. */
std::vector<float> trace(const BoundFloat &b, float from, float to, int n) {
  std::vector<float> out;
  out.reserve((size_t)n);
  for (int i = 0; i < n; ++i)
    out.push_back(b.apply(from + (to - from) * (float)i / (float)(n - 1)));
  return out;
}
float spread(const std::vector<float> &v) {
  return *std::max_element(v.begin(), v.end()) -
         *std::min_element(v.begin(), v.end());
}
} // namespace

TEST(AnimationValues, WiggleIsSmoothBoundedAndInOutputUnits) {
  ch::Output<float> phase = 0.0f;

  // AMPLITUDE IS IN OUTPUT UNITS — the whole reason the stage sits after
  // the affine chain. ±12 around a target range of [-70, 170]: the value
  // never leaves the range widened by exactly 12, and it does leave the
  // un-widened one (otherwise "bounded" would be vacuous).
  const BoundFloat px =
      bind(&phase).target(-70.f, 170.f).wiggle(12.f, 9.f, 4).value();
  const std::vector<float> shaken = trace(px, 0.f, 1.f, 2001);
  const BoundFloat plain = bind(&phase).target(-70.f, 170.f).value();
  bool leftTheRange = false;
  for (size_t i = 0; i < shaken.size(); ++i) {
    const float base = plain.apply((float)i / (float)(shaken.size() - 1));
    EXPECT_LE(std::fabs(shaken[i] - base), 12.0f + 1e-3f) << "at " << i;
    if (shaken[i] < -70.f || shaken[i] > 170.f)
      leftTheRange = true;
  }
  EXPECT_TRUE(leftTheRange) << "±12 px that never leaves the range is not a "
                               "wiggle — the bound claim would be vacuous";

  // SMOOTH, not white. Consecutive samples of a 9-cycle wiggle sampled
  // 2000× must step by far less than the peak-to-peak of the noise
  // itself; white noise would step by ~2·amount every sample.
  float maxStep = 0.0f;
  for (size_t i = 1; i < shaken.size(); ++i)
    maxStep = std::max(maxStep, std::fabs(shaken[i] - shaken[i - 1]));
  EXPECT_LT(maxStep, 1.0f) << "the trace teleports — this is not value noise";

  // …and the negative half of the same claim: at ONE sample per cycle the
  // very same field DOES jump, so the smoothness above is a property of
  // the noise and not of a wiggle too quiet to see.
  const std::vector<float> undersampled = trace(px, 0.f, 20.f, 181);
  float coarseStep = 0.0f;
  for (size_t i = 1; i < undersampled.size(); ++i)
    coarseStep = std::max(coarseStep, std::fabs(undersampled[i] -
                                                undersampled[i - 1]));
  EXPECT_GT(coarseStep, 4.0f);

  // amount == 0 is the whole stage disengaged, exactly.
  for (float v : {0.f, 0.3f, 0.77f, 1.f})
    EXPECT_FLOAT_EQ(bind(&phase).target(-70.f, 170.f).wiggle(0.f, 9.f).value()
                        .apply(v),
                    plain.apply(v));
}

TEST(AnimationValues, WiggleIsDeterministicAndSeeded) {
  ch::Output<float> phase = 0.0f;
  // PURE noise, no base contribution: a rig carrying `.target(-70, 170)`
  // would make every claim below about the RAMP rather than about the
  // wiggle (the correlation check in 4 read 0.999 on such a rig — the
  // shared ramp, not a shared noise field).
  const auto rig = [](uint32_t seed) {
    return bind((const ch::Output<float> *)nullptr)
        .scale(0.f)
        .wiggle(12.f, 9.f, seed)
        .value();
  };

  // 1. SAME INPUT, SAME NUMBER — across independently built maps and
  //    across repeated evaluation. No clock is read anywhere in apply().
  const std::vector<float> a = trace(rig(7), 0.f, 3.f, 601);
  const std::vector<float> b = trace(rig(7), 0.f, 3.f, 601);
  EXPECT_EQ(a, b);
  const float once = rig(7).apply(0.418f);
  for (int repeat = 0; repeat < 4; ++repeat)
    EXPECT_FLOAT_EQ(rig(7).apply(0.418f), once);

  // 2. …AND THE TRACE ACTUALLY MOVES. Without this, "identical across
  //    runs" is a claim about a constant. It must swing most of the way
  //    to its ±12 bound over three cycles.
  EXPECT_GT(spread(a), 12.0f);

  // 3. …AND A DIFFERENT INPUT SEQUENCE LANDS ELSEWHERE. Same seed, same
  //    shaping, a phase window shifted by half a cycle: the traces must
  //    NOT coincide.
  const std::vector<float> shifted = trace(rig(7), 0.055f, 3.055f, 601);
  EXPECT_NE(a, shifted);
  double drift = 0.0;
  for (size_t i = 0; i < a.size(); ++i)
    drift += std::fabs(a[i] - shifted[i]);
  EXPECT_GT(drift / (double)a.size(), 1.0);

  // 4. SEEDING. Same seed ⇒ identical, different seed ⇒ independent —
  //    the property that makes a two-axis shake possible at all. Two
  //    lanes sharing a seed move on a diagonal; these must not.
  EXPECT_EQ(trace(rig(1), 0.f, 3.f, 601), trace(rig(1), 0.f, 3.f, 601));
  const std::vector<float> x = trace(rig(1), 0.f, 3.f, 601);
  const std::vector<float> y = trace(rig(2), 0.f, 3.f, 601);
  EXPECT_NE(x, y);
  // Correlated axes are the real failure, not merely unequal ones: the
  // centred traces must be near-uncorrelated (adjacent SEEDS is the case
  // a weak hash gets wrong).
  double mx = 0, my = 0;
  for (size_t i = 0; i < x.size(); ++i) { mx += x[i]; my += y[i]; }
  mx /= (double)x.size();
  my /= (double)y.size();
  double cov = 0, vx = 0, vy = 0;
  for (size_t i = 0; i < x.size(); ++i) {
    cov += (x[i] - mx) * (y[i] - my);
    vx += (x[i] - mx) * (x[i] - mx);
    vy += (y[i] - my) * (y[i] - my);
  }
  EXPECT_GT(vx, 0.0);
  EXPECT_GT(vy, 0.0);
  EXPECT_LT(std::fabs(cov / std::sqrt(vx * vy)), 0.35)
      << "seeds 1 and 2 wiggle together — a two-axis shake would slide "
         "along a diagonal";
}

TEST(AnimationValues, WigglePhaseComesFromTheScheduleNotTheOutput) {
  // RULING 2's other half: the noise PHASE is read off the normalised
  // input, so the affine chain moves the wiggle's SIZE and never its
  // TIMING. Two chains whose outputs differ only by a factor of 10 must
  // wiggle in step, not at ten times the rate.
  ch::Output<float> phase = 0.0f;
  const BoundFloat small = bind(&phase).scale(0.f).wiggle(1.f, 6.f, 3).value();
  const BoundFloat big = bind(&phase).scale(0.f).wiggle(10.f, 6.f, 3).value();
  for (int i = 0; i <= 200; ++i) {
    const float v = (float)i / 100.0f;
    EXPECT_NEAR(big.apply(v), small.apply(v) * 10.0f, 1e-3f) << "at " << v;
  }

  // The CURVE shapes the signal, not the schedule: sampling the phase
  // before map() means an eased chain's wiggle keeps its own rate.
  const BoundFloat eased =
      bind(&phase).scale(0.f).map(&choreograph::easeInQuint).wiggle(1.f, 6.f, 3)
          .value();
  for (int i = 0; i <= 200; ++i) {
    const float v = (float)i / 200.0f;
    EXPECT_NEAR(eased.apply(v), small.apply(v), 1e-4f) << "at " << v;
  }

  // quantize() likewise stair-steps the signal, never the wiggle.
  const BoundFloat stepped =
      bind(&phase).scale(0.f).quantize(4).wiggle(1.f, 6.f, 3).value();
  for (int i = 0; i <= 200; ++i) {
    const float v = (float)i / 200.0f;
    EXPECT_NEAR(stepped.apply(v), small.apply(v), 1e-4f);
  }

  // window() clamps the input, and the phase rides that clamp: a wiggle
  // scoped to a beat HOLDS outside it.
  const BoundFloat scoped =
      bind(&phase).window(0.2f, 0.4f).scale(0.f).wiggle(5.f, 6.f, 3).value();
  EXPECT_FLOAT_EQ(scoped.apply(0.9f), scoped.apply(2.0f));
  EXPECT_NE(scoped.apply(0.9f), scoped.apply(0.3f));

  // clamp() still applies LAST — a wiggled opacity lands in [0,1].
  const BoundFloat op =
      bind(&phase).target(0.9f, 1.0f).wiggle(0.4f, 12.f, 5).clamp(0.f, 1.f)
          .value();
  for (int i = 0; i <= 500; ++i) {
    const float v = op.apply((float)i / 500.0f);
    EXPECT_GE(v, 0.0f);
    EXPECT_LE(v, 1.0f);
  }

  // OCTAVES change the TEXTURE, not the SIZE — the normalisation ruling,
  // and the reason the two extra fields earn their place. The claim is
  // "a fine TREMBLE rides on the drift", so the honest metric is how
  // many times the trace turns around, not how far it travels: at
  // falloff 0.5 each octave carries the same slope, so total variation
  // only reads 1.26× (and a max single step reads 0.5× — the normaliser
  // hides it). Direction reversals see the tremble directly.
  const auto reversals = [](const std::vector<float> &v) {
    int n = 0;
    for (size_t i = 2; i < v.size(); ++i)
      if ((v[i] - v[i - 1] > 0) != (v[i - 1] - v[i - 2] > 0))
        ++n;
    return n;
  };
  const std::vector<float> one =
      trace(bind(&phase).scale(0.f).wiggle(10.f, 4.f, 8, 1).value(), 0, 4, 4001);
  const std::vector<float> three =
      trace(bind(&phase).scale(0.f).wiggle(10.f, 4.f, 8, 3).value(), 0, 4, 4001);
  // Measured 19 → 46 reversals (2.4×). Not the 4× the frequency ladder
  // suggests, because at falloff 0.5 the fine octaves carry the same
  // slope as the base and so only reverse the SUM about half the time —
  // which is also why `falloff` is worth exposing: 0.9 is turbulence,
  // 0.2 is a drift with a whisper on it.
  EXPECT_GT(reversals(three), reversals(one) * 2)
      << "octaves added no detail — they are not earning their two fields";
  EXPECT_LE(spread(three), 20.0f + 1e-3f); // still inside ±10: the SIZE
  EXPECT_LE(spread(one), 20.0f + 1e-3f);   // promise survives the octaves
}

TEST(AnimationValues, WiggleRigShakesTwoAxesAroundRest) {
  // The marquee case, and the one that proves seeding: a camera shake.
  // `wiggle(&out, …)` is `bind(&out).scale(0).wiggle(…)` named, so the
  // property sits at REST and only the noise moves it — the phase still
  // comes from the (contribution-zeroed) schedule.
  Ticker ticker;
  ch::Output<float> seconds = 0.0f;
  ticker.timeline().apply(&seconds).then<ch::RampTo>(2.0f, 2.0f); // 1:1

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
    if ((x > 0) == (y > 0))
      ++sameSign;
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
// derive() — the bind() chain reaching an OUTPUT (2026-07-29, ROADMAP §43).
// One new stage (wrap), one new verb (Ticker::derive), and a stepping-order
// contract with its own pin. Everything else is the existing chain, verbatim.

TEST(AnimationValues, WrapFoldsThePostAffineValueAtTheSeam) {
  ch::Output<float> phase = 0.0f;

  // The seam: a ramp through 1.0 folds back to 0, floor-convention.
  const BoundFloat looped = bind(&phase).wrap(1.0f).value();
  EXPECT_FLOAT_EQ(looped.apply(0.25f), 0.25f);
  EXPECT_FLOAT_EQ(looped.apply(1.25f), 0.25f);
  EXPECT_FLOAT_EQ(looped.apply(7.75f), 0.75f);
  EXPECT_FLOAT_EQ(looped.apply(1.0f), 0.0f); // the seam itself lands at 0

  // A DESCENDING schedule wraps UP into [0, period) — fmod alone would
  // answer a negative phase, which no consumer of a phase wants.
  EXPECT_FLOAT_EQ(looped.apply(-0.25f), 0.75f);

  // period == 0 is a NO-OP, not a division; so is a negative period.
  EXPECT_FLOAT_EQ(bind(&phase).wrap(0.0f).value().apply(3.5f), 3.5f);
  EXPECT_FLOAT_EQ(bind(&phase).wrap(-2.0f).value().apply(3.5f), 3.5f);

  // ORDER: after the affine chain (the wrap sees scaled units), before
  // clamp (a clamp still bounds the folded value).
  EXPECT_FLOAT_EQ(bind(&phase).scale(360.0f).wrap(360.0f).value().apply(1.5f),
                  180.0f);
  EXPECT_FLOAT_EQ(
      bind(&phase).wrap(1.0f).clamp(0.0f, 0.5f).value().apply(1.9f), 0.5f);

  // …and before wiggle, so a wrapped phase WIGGLES CONTINUOUSLY across
  // the seam: the noise phase reads the unwrapped schedule. The noise
  // contribution (wiggled minus base) must step smoothly across v = 1,
  // while the base itself jumps by a full period.
  const BoundFloat wig = bind(&phase).wrap(1.0f).wiggle(5.f, 3.f, 9).value();
  const float beforeSeam = wig.apply(0.9999f) - looped.apply(0.9999f);
  const float afterSeam = wig.apply(1.0001f) - looped.apply(1.0001f);
  EXPECT_NEAR(beforeSeam, afterSeam, 0.05f)
      << "the noise repeated with the wrap — its phase must read the "
         "unwrapped schedule";
}

TEST(AnimationValues, ChainStagesReproduceTheCorpusIdiomsBitExactly) {
  // The three hand-rolled retimes §43.3 names, spelled through the chain
  // and compared BIT-EXACTLY against their originals — the consolidation
  // claim is identity, not approximation.
  ch::Output<float> out = 0.0f;

  // vertigo_titles.cpp:105 — the pen tip trailing the growth schedule:
  // "a second, independently-owned Output the ticker re-copies from
  // growth − 0.008 every tick".
  const BoundFloat penTip = bind(&out).offset(-0.008f).clamp(0.f, 1.f).value();
  for (float g : {0.0f, 0.004f, 0.008f, 0.31f, 0.7431f, 0.999f, 1.0f})
    EXPECT_EQ(penTip.apply(g), std::clamp(g - 0.008f, 0.0f, 1.0f));

  // ScenesSkillTree.h:223 — pulseS = -0.12f + u * 1.12f.
  const BoundFloat pulse = bind(&out).scale(1.12f).offset(-0.12f).value();
  for (float u : {-1.0f, 0.0f, 0.31f, 0.5f, 0.99f, 1.0f, 2.5f})
    EXPECT_EQ(pulse.apply(u), -0.12f + u * 1.12f);

  // ScenesSkillTree.h:226 / ScenesVeloren.h — the looping phase,
  // fmod(t * k, 1): scale into cycles, wrap at 1. Positive schedules are
  // fmod bit-for-bit (both are exact operations on the same product).
  const BoundFloat ring = bind(&out).scale(0.5f).wrap(1.0f).value();
  for (float t : {0.0f, 0.7f, 1.9f, 2.0f, 13.37f, 400.25f})
    EXPECT_EQ(ring.apply(t), std::fmod(t * 0.5f, 1.0f));

  // ScenesVeloren.h's inverted sawtooth family: invert() IS 1 − v.
  const BoundFloat rev = bind(&out).invert().value();
  for (float v : {0.0f, 0.25f, 0.61f, 1.0f})
    EXPECT_EQ(rev.apply(v), 1.0f - v);

  // window(a, b) is the clamp((t−a)/(b−a), 0, 1) idiom (eva_magi's
  // frontAt, ScenesVeloren's beat windows). The normalisation is stored
  // as one multiply-add, so bit-identity holds on a dyadic grid where
  // both spellings are exact; off it they agree to float noise.
  const BoundFloat win = bind(&out).window(0.25f, 0.75f).value();
  for (int i = -8; i <= 72; ++i) {
    const float t = (float)i / 64.0f;
    EXPECT_EQ(win.apply(t), std::clamp((t - 0.25f) / 0.5f, 0.0f, 1.0f));
  }
  for (float t : {0.311f, 0.5002f, 0.7309f})
    EXPECT_NEAR(win.apply(t), std::clamp((t - 0.25f) / 0.5f, 0.0f, 1.0f),
                1e-6f);
}

TEST(AnimationValues, QuantizeTimeIsTheCanonicalFloorArithmetic) {
  // motion::quantizeTime — the ONE definition behind Material's uTime
  // stepping and the corpus's hand-rolled floor(t*N)/N (ScenesPersona's
  // 6 Hz caustics, ScenesAero's 8 Hz breathing, ksp's instrument
  // sampling). Bit-exact against the hand-rolled spelling in BOTH
  // precisions, because each call site keeps its own.
  for (double t : {0.0, 0.081, 1.0 / 6.0, 2.499999, 13.37, 1000.05}) {
    EXPECT_EQ(quantizeTime(t, 6.0), std::floor(t * 6.0) / 6.0);
    EXPECT_EQ(quantizeTime(t, 8.0), std::floor(t * 8.0) / 8.0);
    const float ft = (float)t;
    EXPECT_EQ(quantizeTime(ft, 8.0f), std::floor(ft * 8.0f) / 8.0f);
  }
  // hz <= 0 answers the input unchanged — "continuous", matching
  // Material::quantizeTime(0).
  EXPECT_EQ(quantizeTime(1.234, 0.0), 1.234);
  EXPECT_EQ(quantizeTime(1.234, -5.0), 1.234);
  // …and the value HOLDS between steps, which is the whole point.
  EXPECT_EQ(quantizeTime(0.10, 6.0), quantizeTime(0.16, 6.0));
  EXPECT_NE(quantizeTime(0.16, 6.0), quantizeTime(0.17, 6.0));
}

TEST(TickerTest, ADerivationNeverReadsAStaleSource) {
  // THE STEPPING-ORDER PIN (ROADMAP §43.3 ruling (a)). The derivation is
  // registered FIRST and its source's writer SECOND — the arrangement
  // that reads one frame stale under any single-phase step that honours
  // registration order. The two-phase contract (sources, then
  // derivations) makes the answer current regardless of order.
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

  // A derivation of a derivation is REFUSED (in either registration
  // order), not silently one frame late — §40/§42's standing lesson.
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
  ticker.tick(0.6); // the ramp finishes and self-removes
  EXPECT_FLOAT_EQ(trail.value(), 0.75f);
  EXPECT_FALSE(ticker.active())
      << "a derivation must not keep the host rendering forever";
  // …but a tick still refreshes it (a host may write the source by hand).
  phase = 0.5f;
  ticker.tick(0.0);
  EXPECT_FLOAT_EQ(trail.value(), 0.25f);
}

TEST(AnimationValues, TickerDrivesABoundChainWithNoRenderer) {
  // The pin: the clock half and the value half of SigilMotion, working
  // together, with nothing else linked. A choreograph Output rides the
  // Ticker; a shaped binding turns its [0,1] phase into pixels the way a
  // property slot downstream would read it.
  Ticker ticker;
  ch::Output<float> phase = 0.0f;
  const Transition spec{500ms, ease::outBack()};
  ticker.timeline().apply(&phase).then<ch::RampTo>(
      1.0f, (float)spec.duration.count() / 1000.0f, spec.easing());

  const BoundFloat toPixels = bind(&phase).target(-70.f, 170.f).value();
  EXPECT_NEAR(toPixels.apply(phase.value()), -70.f, 1e-3f);

  ASSERT_TRUE(ticker.active());
  ticker.tick(0.10); // t = 0.2 — still climbing
  const float early = toPixels.apply(phase.value());
  EXPECT_GT(early, -70.f);
  EXPECT_LT(early, 170.f);

  ticker.tick(0.15); // t = 0.5 — outBack is already past its target
  EXPECT_GT(toPixels.apply(phase.value()), 170.f);

  ticker.tick(0.40); // past the end
  EXPECT_NEAR(toPixels.apply(phase.value()), 170.f, 1e-3f);
  EXPECT_FALSE(ticker.active());
}

TEST(AnimationValues, AnimatableHoldsAllFourFormsWithNoKernel) {
  // Animatable<T> — the property SLOT — moved with the values it holds
  // (ROADMAP §37). Nothing here needs a reconciler, a canvas or a node.
  ch::Output<float> live = 3.0f;

  const Animatable<float> plain = 0.5f;
  const Animatable<float> ramped = animate(from(0.0f).to(1.0f), {400ms});
  const Animatable<float> bound = &live;
  const Animatable<float> shaped = bind(&live).source(0, 10).target(-70, 170);

  // The discriminant, in the order the pre-compaction std::variant had:
  // a shaped binding sorts AFTER a bare one rather than replacing it.
  EXPECT_EQ(plain.index(), 0);
  EXPECT_EQ(ramped.index(), 1);
  EXPECT_EQ(bound.index(), 2);
  EXPECT_EQ(shaped.index(), 3);

  ASSERT_NE(plain.plain(), nullptr);
  EXPECT_FLOAT_EQ(*plain.plain(), 0.5f);
  EXPECT_EQ(plain.transitioned(), nullptr);
  EXPECT_EQ(plain.binding(), nullptr);
  EXPECT_EQ(plain.boundMap(), nullptr);

  ASSERT_NE(ramped.transitioned(), nullptr);
  EXPECT_FLOAT_EQ(ramped.transitioned()->value, 1.0f);
  EXPECT_EQ(ramped.plain(), nullptr);

  // binding() answers for BOTH bound forms — the one accessor every
  // "bound ⇒ read it live" branch goes through — while boundMap() tells
  // them apart.
  EXPECT_EQ(bound.binding(), &live);
  EXPECT_EQ(shaped.binding(), &live);
  EXPECT_EQ(bound.boundMap(), nullptr);
  ASSERT_NE(shaped.boundMap(), nullptr);
  EXPECT_NEAR(shaped.boundMap()->apply(live.value()), -70.f + 0.3f * 240.f,
              1e-3f);

  // The out-of-line Extra block survives copies: copying a fat form must
  // deep-copy, not alias.
  Animatable<float> copy = shaped;
  ASSERT_NE(copy.boundMap(), nullptr);
  EXPECT_NE(copy.boundMap(), shaped.boundMap());
  EXPECT_EQ(copy.index(), 3);
  const Animatable<float> moved = std::move(copy);
  ASSERT_NE(moved.boundMap(), nullptr);
  EXPECT_EQ(moved.index(), 3);
}

TEST(AnimationValues, AnimatableDrivenByTheTickerWithNoKernel) {
  // A CONSUMER-SIDE resolve, written here in five lines, to show the
  // value type is enough on its own. Compose's real resolution is
  // context-aware (node transitions, stagger, mount entrances against a
  // PaintContext) and stays in compose — SigilMotion deliberately ships
  // no resolve surface.
  const auto readNow = [](const Animatable<float> &a, float fallback) {
    if (const float *p = a.plain())
      return *p;
    if (const choreograph::Output<float> *out = a.binding())
      return a.boundMap() ? a.boundMap()->apply(out->value()) : out->value();
    return fallback;
  };

  Ticker ticker;
  ch::Output<float> hp = 0.0f;
  ticker.timeline().apply(&hp).then<ch::RampTo>(100.0f, 1.0f);

  const Animatable<float> width = bind(&hp).source(0, 100).target(0, 240);
  EXPECT_NEAR(readNow(width, -1.f), 0.f, 1e-3f);

  ticker.tick(0.5);
  EXPECT_NEAR(readNow(width, -1.f), 120.f, 1e-2f);

  ticker.tick(0.6); // past the end
  EXPECT_NEAR(readNow(width, -1.f), 240.f, 1e-3f);
  EXPECT_FALSE(ticker.active());

  EXPECT_FLOAT_EQ(readNow(Animatable<float>{0.25f}, -1.f), 0.25f);
}
