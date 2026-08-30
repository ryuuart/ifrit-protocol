// SigilMotionBind: the bind() chain — every stage in its fixed order,
// the envelopes, wrap, and the wiggle noise field — with no renderer
// under it.

#include <gtest/gtest.h>
#include <sigilmotion/Bind.h>

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
    "motion_bind_test can see a drawing library's headers — the tests \
below no longer prove that SigilMotion stands alone."
#endif

using namespace sigil::motion;
namespace ch = choreograph;
using namespace std::chrono_literals;

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
// wiggle() — the procedural noise stage, phased off the NORMALISED INPUT
// rather than off a clock; see Bound::wiggle for what that buys.

namespace {
/** Sample a shaped binding across a phase sweep — the trace every wiggle
 *  pin below reasons about. */
std::vector<float> trace(const BoundFloat& b, float from, float to, int n) {
  std::vector<float> out;
  out.reserve((size_t)n);
  for (int i = 0; i < n; ++i)
    out.push_back(b.apply(from + (to - from) * (float)i / (float)(n - 1)));
  return out;
}
float spread(const std::vector<float>& v) {
  return *std::max_element(v.begin(), v.end()) -
         *std::min_element(v.begin(), v.end());
}
}  // namespace

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
    if (shaken[i] < -70.f || shaken[i] > 170.f) leftTheRange = true;
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
    coarseStep =
        std::max(coarseStep, std::fabs(undersampled[i] - undersampled[i - 1]));
  EXPECT_GT(coarseStep, 4.0f);

  // amount == 0 is the whole stage disengaged, exactly.
  for (float v : {0.f, 0.3f, 0.77f, 1.f})
    EXPECT_FLOAT_EQ(
        bind(&phase).target(-70.f, 170.f).wiggle(0.f, 9.f).value().apply(v),
        plain.apply(v));
}

TEST(AnimationValues, WiggleIsDeterministicAndSeeded) {
  ch::Output<float> phase = 0.0f;
  // PURE noise, no base contribution. A rig carrying `.target(-70, 170)`
  // would make every claim below a claim about the RAMP rather than about
  // the wiggle — in particular the correlation check in part 4, where two
  // traces sharing a ramp correlate almost perfectly whatever their noise
  // fields do.
  const auto rig = [](uint32_t seed) {
    return bind((const ch::Output<float>*)nullptr)
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
  for (size_t i = 0; i < a.size(); ++i) drift += std::fabs(a[i] - shifted[i]);
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
  for (size_t i = 0; i < x.size(); ++i) {
    mx += x[i];
    my += y[i];
  }
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
  // The noise PHASE is read off the normalised input, so the affine chain
  // moves the wiggle's SIZE and never its TIMING. Two chains whose
  // outputs differ only by a factor of 10 must wiggle in step, not at ten
  // times the rate.
  ch::Output<float> phase = 0.0f;
  const BoundFloat small = bind(&phase).scale(0.f).wiggle(1.f, 6.f, 3).value();
  const BoundFloat big = bind(&phase).scale(0.f).wiggle(10.f, 6.f, 3).value();
  for (int i = 0; i <= 200; ++i) {
    const float v = (float)i / 100.0f;
    EXPECT_NEAR(big.apply(v), small.apply(v) * 10.0f, 1e-3f) << "at " << v;
  }

  // The CURVE shapes the signal, not the schedule: sampling the phase
  // before map() means an eased chain's wiggle keeps its own rate.
  const BoundFloat eased = bind(&phase)
                               .scale(0.f)
                               .map(&choreograph::easeInQuint)
                               .wiggle(1.f, 6.f, 3)
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
  const BoundFloat op = bind(&phase)
                            .target(0.9f, 1.0f)
                            .wiggle(0.4f, 12.f, 5)
                            .clamp(0.f, 1.f)
                            .value();
  for (int i = 0; i <= 500; ++i) {
    const float v = op.apply((float)i / 500.0f);
    EXPECT_GE(v, 0.0f);
    EXPECT_LE(v, 1.0f);
  }

  // OCTAVES change the TEXTURE, not the SIZE. The claim is that a fine
  // tremble rides on the drift, so the honest metric is how often the
  // trace turns around rather than how far it travels: at the default
  // falloff each added octave carries about the same slope as the base,
  // so total variation barely moves and the normaliser hides the change
  // in step size entirely. Direction reversals see the tremble directly.
  const auto reversals = [](const std::vector<float>& v) {
    int n = 0;
    for (size_t i = 2; i < v.size(); ++i)
      if ((v[i] - v[i - 1] > 0) != (v[i - 1] - v[i - 2] > 0)) ++n;
    return n;
  };
  const std::vector<float> one = trace(
      bind(&phase).scale(0.f).wiggle(10.f, 4.f, 8, 1).value(), 0, 4, 4001);
  const std::vector<float> three = trace(
      bind(&phase).scale(0.f).wiggle(10.f, 4.f, 8, 3).value(), 0, 4, 4001);
  // Three octaves reverse direction well over twice as often as one, but
  // not the four times the frequency ladder alone would suggest: at
  // falloff 0.5 the fine octaves carry about the same slope as the base
  // and so only turn the SUM around some of the time. That is also why
  // `falloff` is worth exposing — near 1 it is turbulence, near 0 it is a
  // drift with a whisper on it.
  EXPECT_GT(reversals(three), reversals(one) * 2)
      << "octaves added no detail — they are not earning their two fields";
  EXPECT_LE(spread(three), 20.0f + 1e-3f);  // still inside ±10: the SIZE
  EXPECT_LE(spread(one), 20.0f + 1e-3f);    // promise survives the octaves
}

TEST(AnimationValues, PingPongRunsThereAndBackAcrossTheSpan) {
  ch::Output<float> phase = 0.0f;
  const BoundFloat pp = bind(&phase).pingPong().value();

  // The characteristic points: dark at both ends of the span, peak at the
  // middle, linear in between.
  EXPECT_FLOAT_EQ(pp.apply(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(pp.apply(0.25f), 0.5f);
  EXPECT_FLOAT_EQ(pp.apply(0.5f), 1.0f);
  EXPECT_FLOAT_EQ(pp.apply(0.75f), 0.5f);
  EXPECT_FLOAT_EQ(pp.apply(1.0f), 0.0f);

  // The RETURN is the point: the outbound and inbound halves mirror, so a
  // sweep comes back instead of jumping.
  for (int i = 0; i <= 64; ++i) {
    const float u = (float)i / 128.0f;
    EXPECT_NEAR(pp.apply(u), pp.apply(1.0f - u), 1e-6f) << "at " << u;
  }

  // Periodic, so a phase that keeps climbing keeps bouncing rather than
  // running off — and a descending one bounces too.
  EXPECT_FLOAT_EQ(pp.apply(2.5f), 1.0f);
  EXPECT_FLOAT_EQ(pp.apply(3.25f), 0.5f);
  EXPECT_FLOAT_EQ(pp.apply(-0.25f), 0.5f);
  EXPECT_FLOAT_EQ(pp.apply(-0.5f), 1.0f);

  // Bounded whatever the phase does: the stage cannot hand a curve or a
  // target range a value outside [0,1].
  for (int i = -400; i <= 400; ++i) {
    const float v = pp.apply((float)i / 37.0f);
    EXPECT_GE(v, 0.0f);
    EXPECT_LE(v, 1.0f);
  }
}

TEST(AnimationValues, CosineIsTheRaisedCosineBreath) {
  ch::Output<float> phase = 0.0f;
  const BoundFloat breath = bind(&phase).cosine().value();

  // 0 at the extremes, 1 at the middle — the swell a one-way window
  // cannot give.
  EXPECT_NEAR(breath.apply(0.0f), 0.0f, 1e-6f);
  EXPECT_NEAR(breath.apply(0.5f), 1.0f, 1e-6f);
  EXPECT_NEAR(breath.apply(1.0f), 0.0f, 1e-6f);
  EXPECT_NEAR(breath.apply(0.25f), 0.5f, 1e-6f);
  EXPECT_NEAR(breath.apply(0.75f), 0.5f, 1e-6f);

  // It IS 0.5 − 0.5·cos(2πv), which is the arithmetic every hand-rolled
  // breath in the corpus writes out.
  for (int i = 0; i <= 200; ++i) {
    const float v = (float)i / 100.0f;
    EXPECT_NEAR(breath.apply(v),
                (float)(0.5 - 0.5 * std::cos(6.283185307179586 * (double)v)),
                1e-6f)
        << "at " << v;
  }

  // EASED AT BOTH ENDS, where pingPong turns on a corner: the same
  // journey, but the derivative vanishes at the extremes and at the peak.
  const BoundFloat corner = bind(&phase).pingPong().value();
  const float d = 1e-3f;
  EXPECT_LT(std::fabs(breath.apply(d) - breath.apply(0.0f)),
            std::fabs(corner.apply(d) - corner.apply(0.0f)));
  EXPECT_LT(std::fabs(breath.apply(0.5f + d) - breath.apply(0.5f - d)), 1e-4f);

  // Periodic and bounded, for the same reason: a monotonic seconds Output
  // breathes for as long as it runs.
  EXPECT_NEAR(breath.apply(2.5f), 1.0f, 1e-6f);
  for (int i = -400; i <= 400; ++i) {
    const float v = breath.apply((float)i / 37.0f);
    EXPECT_GE(v, -1e-6f);
    EXPECT_LE(v, 1.0f + 1e-6f);
  }
}

TEST(AnimationValues, TrapezoidHoldsAtOneAndCutsWhileDark) {
  ch::Output<float> phase = 0.0f;
  const BoundFloat sheet =
      bind(&phase).trapezoid(0.1f, 0.3f, 0.7f, 0.9f).value();

  // THE FOUR CORNERS, each exactly on its number.
  EXPECT_FLOAT_EQ(sheet.apply(0.1f), 0.0f);  // riseStart: still dark
  EXPECT_FLOAT_EQ(sheet.apply(0.3f), 1.0f);  // holdStart: fully up
  EXPECT_FLOAT_EQ(sheet.apply(0.7f), 1.0f);  // holdEnd: still up
  EXPECT_FLOAT_EQ(sheet.apply(0.9f), 0.0f);  // fallEnd: dark again

  // Dark OUTSIDE the envelope — which is what lets a loop cut there.
  EXPECT_FLOAT_EQ(sheet.apply(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(sheet.apply(0.05f), 0.0f);
  EXPECT_FLOAT_EQ(sheet.apply(0.95f), 0.0f);
  EXPECT_FLOAT_EQ(sheet.apply(1.0f), 0.0f);

  // Linear shoulders, and a FLAT hold: every sample between the two inner
  // corners is exactly 1, not merely close.
  EXPECT_NEAR(sheet.apply(0.2f), 0.5f, 1e-6f);
  EXPECT_NEAR(sheet.apply(0.8f), 0.5f, 1e-6f);
  for (int i = 1; i < 40; ++i)
    EXPECT_FLOAT_EQ(sheet.apply(0.3f + 0.4f * (float)i / 40.0f), 1.0f);

  // A ZERO-LENGTH SHOULDER is an instant cut, not a division by zero.
  const BoundFloat cut =
      bind(&phase).trapezoid(0.25f, 0.25f, 0.75f, 0.75f).value();
  EXPECT_FLOAT_EQ(cut.apply(0.2f), 0.0f);
  EXPECT_FLOAT_EQ(cut.apply(0.5f), 1.0f);
  EXPECT_FLOAT_EQ(cut.apply(0.8f), 0.0f);
  for (int i = -100; i <= 300; ++i)
    EXPECT_TRUE(std::isfinite(cut.apply((float)i / 100.0f)));

  // Corners are held non-decreasing, so out-of-order ones cannot ask for
  // a negative ramp — they collapse onto the corner before them. A rise
  // that ends before it starts becomes an instant one.
  const BoundFloat instant =
      bind(&phase).trapezoid(0.2f, 0.1f, 0.8f, 0.9f).value();
  EXPECT_FLOAT_EQ(instant.apply(0.19f), 0.0f);
  EXPECT_FLOAT_EQ(instant.apply(0.21f), 1.0f);
  EXPECT_NEAR(instant.apply(0.85f), 0.5f, 1e-6f);

  // …and corners that collapse ONTO EACH OTHER ask for nothing and get
  // nothing: dark everywhere, rather than a spike or a division.
  const BoundFloat nothing =
      bind(&phase).trapezoid(0.6f, 0.2f, 0.1f, 0.4f).value();
  for (int i = -100; i <= 200; ++i)
    EXPECT_FLOAT_EQ(nothing.apply((float)i / 100.0f), 0.0f);

  // NOT periodic, unlike the other two envelopes: it names positions
  // inside ONE pass, and a phase past its last corner stays dark rather
  // than starting the sheet again.
  EXPECT_FLOAT_EQ(sheet.apply(1.3f), 0.0f);
  EXPECT_FLOAT_EQ(sheet.apply(2.5f), 0.0f);
}

TEST(AnimationValues, SquarePulsesOnFirstAndPhaseZeroIsOn) {
  ch::Output<float> phase = 0.0f;
  const BoundFloat pulse = bind(&phase).square(0.6f).value();

  // PHASE 0 IS ON — a caret born at the start of its cycle is born
  // visible — and the whole first `duty` of the period is on, exactly 1.
  EXPECT_FLOAT_EQ(pulse.apply(0.0f), 1.0f);
  EXPECT_FLOAT_EQ(pulse.apply(0.3f), 1.0f);
  EXPECT_FLOAT_EQ(pulse.apply(0.59f), 1.0f);
  // OFF from `duty` to the end of the period, exactly 0.
  EXPECT_FLOAT_EQ(pulse.apply(0.6f), 0.0f);
  EXPECT_FLOAT_EQ(pulse.apply(0.99f), 0.0f);

  // PERIODIC by the same fold pingPong uses: phase 1 is phase 0 — ON, not
  // the trapezoid's dark-at-the-seam — and the pattern repeats on every
  // period, negative phases included.
  EXPECT_FLOAT_EQ(pulse.apply(1.0f), 1.0f);
  EXPECT_FLOAT_EQ(pulse.apply(2.3f), 1.0f);
  EXPECT_FLOAT_EQ(pulse.apply(3.7f), 0.0f);
  EXPECT_FLOAT_EQ(pulse.apply(-0.5f), 1.0f);  // −0.5 folds to 0.5 < 0.6
  EXPECT_FLOAT_EQ(pulse.apply(-0.3f), 0.0f);  // −0.3 folds to 0.7

  // The default duty is half the period.
  const BoundFloat half = bind(&phase).square().value();
  EXPECT_FLOAT_EQ(half.apply(0.49f), 1.0f);
  EXPECT_FLOAT_EQ(half.apply(0.51f), 0.0f);

  // Duty is clamped: 0 is never on, 1 is always on (the fold keeps u < 1).
  const BoundFloat never = bind(&phase).square(-2.0f).value();
  const BoundFloat always = bind(&phase).square(5.0f).value();
  for (int i = 0; i <= 20; ++i) {
    EXPECT_FLOAT_EQ(never.apply((float)i / 7.0f), 0.0f);
    EXPECT_FLOAT_EQ(always.apply((float)i / 7.0f), 1.0f);
  }

  // The two levels land wherever the affine chain puts them — the blink
  // that rests dim rather than vanishing.
  const BoundFloat caret = bind(&phase)
                               .source(0.0f, 1.06f)
                               .square(0.62f / 1.06f)
                               .target(0.10f, 1.0f)
                               .value();
  EXPECT_FLOAT_EQ(caret.apply(0.0f), 1.0f);
  EXPECT_FLOAT_EQ(caret.apply(0.61f), 1.0f);
  EXPECT_FLOAT_EQ(caret.apply(0.63f), 0.10f);
  EXPECT_FLOAT_EQ(caret.apply(1.07f), 1.0f);  // the next period is on again
}

TEST(AnimationValues, WaveEvaluatesTheCallersShapeOnTheFoldedPhase) {
  ch::Output<float> phase = 0.0f;

  // The caller's function sees u in [0,1) and its drawing repeats every
  // period — the custom escape hatch behind every named envelope.
  const BoundFloat saw =
      bind(&phase).wave([](float u) { return u * u; }).value();
  EXPECT_FLOAT_EQ(saw.apply(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(saw.apply(0.5f), 0.25f);
  EXPECT_FLOAT_EQ(saw.apply(1.5f), 0.25f);   // folded: 1.5 → 0.5
  EXPECT_FLOAT_EQ(saw.apply(-0.5f), 0.25f);  // …and up from below
  EXPECT_NEAR(saw.apply(3.9f), saw.apply(0.9f), 1e-5f);

  // Same slot as the named shapes: naming wave replaces them, and naming
  // one of them replaces wave.
  const BoundFloat waved =
      bind(&phase).cosine().wave([](float u) { return u; }).value();
  EXPECT_FLOAT_EQ(waved.apply(0.25f), 0.25f);  // not the cosine's swell
  const BoundFloat named =
      bind(&phase).wave([](float) { return 9.0f; }).pingPong().value();
  EXPECT_FLOAT_EQ(named.apply(0.25f), 0.5f);  // the triangle, not the 9

  // map() still shapes what the wave produced, and the affine chain still
  // lands it in the property's units — the fixed stage order.
  const BoundFloat staged = bind(&phase)
                                .wave([](float u) { return u; })
                                .map(&ch::easeInQuad)
                                .scale(100.0f)
                                .value();
  EXPECT_FLOAT_EQ(staged.apply(0.5f), ch::easeInQuad(0.5f) * 100.0f);

  // An empty function passes the folded phase through rather than calling
  // nothing.
  const BoundFloat empty = bind(&phase).wave(nullptr).value();
  EXPECT_FLOAT_EQ(empty.apply(1.25f), 0.25f);
  for (int i = -20; i <= 40; ++i)
    EXPECT_TRUE(std::isfinite(empty.apply((float)i / 8.0f)));
}

TEST(AnimationValues, EnvelopesSitInTheFixedOrderWhateverTheCallOrder) {
  ch::Output<float> phase = 0.0f;

  // THE FIXED ORDER, stage by stage. Each pair below names the same
  // stages in a different order and must agree bit for bit.
  const auto same = [](const BoundFloat& a, const BoundFloat& b) {
    for (int i = -50; i <= 250; ++i) {
      const float v = (float)i / 100.0f;
      EXPECT_EQ(a.apply(v), b.apply(v)) << "at " << v;
    }
  };
  same(bind(&phase).cosine().target(400.f, 880.f).value(),
       bind(&phase).target(400.f, 880.f).cosine().value());
  same(bind(&phase).window(0.2f, 0.8f).pingPong().value(),
       bind(&phase).pingPong().window(0.2f, 0.8f).value());
  same(bind(&phase)
           .trapezoid(0.1f, 0.2f, 0.8f, 0.9f)
           .map(&ch::easeInQuad)
           .value(),
       bind(&phase)
           .map(&ch::easeInQuad)
           .trapezoid(0.1f, 0.2f, 0.8f, 0.9f)
           .value());
  same(bind(&phase).cosine().clamp(0.f, 0.5f).value(),
       bind(&phase).clamp(0.f, 0.5f).cosine().value());

  // ONE SHAPE PER BINDING: naming a second replaces the first, the way a
  // second map() replaces the first curve.
  same(bind(&phase).pingPong().cosine().value(), bind(&phase).cosine().value());
  same(bind(&phase).cosine().trapezoid(0.f, 0.25f, 0.75f, 1.f).value(),
       bind(&phase).trapezoid(0.f, 0.25f, 0.75f, 1.f).value());

  // AFTER source/window: the span the envelope shapes is the one source()
  // named, so a beat on a longer timeline swells inside its own window
  // and rests outside it.
  const BoundFloat beat = bind(&phase).window(2.0f, 4.0f).cosine().value();
  EXPECT_NEAR(beat.apply(3.0f), 1.0f, 1e-6f);
  EXPECT_NEAR(beat.apply(2.5f), 0.5f, 1e-6f);
  EXPECT_NEAR(beat.apply(0.0f), 0.0f, 1e-6f);  // clamped to the span's start
  EXPECT_NEAR(beat.apply(9.0f), 0.0f, 1e-6f);  // …and to its end

  // BEFORE map: the curve shapes what the envelope PRODUCED. Any curve
  // through (0,0) and (1,1) therefore rounds a trapezoid's shoulders and
  // leaves its hold at exactly 1 and its dark at exactly 0 — the property
  // that makes the shoulder shape a separate decision from the corners.
  const BoundFloat eased = bind(&phase)
                               .trapezoid(0.1f, 0.3f, 0.7f, 0.9f)
                               .map(&ch::easeInOutQuad)
                               .value();
  EXPECT_FLOAT_EQ(eased.apply(0.5f), 1.0f);
  EXPECT_FLOAT_EQ(eased.apply(0.05f), 0.0f);
  EXPECT_LT(
      eased.apply(0.15f),
      bind(&phase).trapezoid(0.1f, 0.3f, 0.7f, 0.9f).value().apply(0.15f));

  // BEFORE the affine chain: the shape lands in the property's own units.
  const BoundFloat grad = bind(&phase).cosine().target(400.f, 880.f).value();
  EXPECT_NEAR(grad.apply(0.5f), 880.f, 1e-3f);
  EXPECT_NEAR(grad.apply(0.0f), 400.f, 1e-3f);

  // COMPOSED WITH wrap, which is on the far side of the affine chain: the
  // envelope shapes the phase, wrap folds the output.
  const BoundFloat spun =
      bind(&phase).pingPong().scale(720.f).wrap(360.f).value();
  EXPECT_FLOAT_EQ(spun.apply(0.25f), 0.0f);  // 0.5 · 720 = 360 → 0
  EXPECT_FLOAT_EQ(spun.apply(0.125f), 180.f);

  // AFTER the wiggle phase is read, for the same reason wrap is: the
  // shake reads the SCHEDULE, so a ping-ponged phase does not retrace the
  // identical shake on the way back.
  const BoundFloat shaken =
      bind(&phase).pingPong().scale(0.f).wiggle(5.f, 4.f, 3).value();
  EXPECT_NE(shaken.apply(0.25f), shaken.apply(0.75f));
  const BoundFloat bare = bind(&phase).scale(0.f).wiggle(5.f, 4.f, 3).value();
  for (int i = 0; i <= 100; ++i) {
    const float v = (float)i / 50.0f;
    EXPECT_FLOAT_EQ(shaken.apply(v), bare.apply(v)) << "at " << v;
  }
}

TEST(AnimationValues, EnvelopeStagesReproduceTheHandRolledEnvelopes) {
  // Against the arithmetic the studies wrote by hand, at the precision a
  // migration needs.
  ch::Output<float> cycle = 0.0f;

  // The BREATH: a raised cosine over a 7.2 s period, peaking at 3.6 s.
  const BoundFloat swell = bind(&cycle).source(0.0f, 7.2f).cosine().value();
  // The tolerance is float noise on the phase, not a shape difference:
  // the chain divides once in float where the hand-written line divides
  // in double, so the two agree to a few units in the last place of the
  // angle and to that much of the swell.
  for (double t : {0.0, 0.5, 1.8, 3.6, 5.0, 7.2, 9.9})
    EXPECT_NEAR(swell.apply((float)t),
                (float)(0.5 - 0.5 * std::cos(6.283185307 * t / 7.2)), 1e-5f)
        << "at " << t;

  // The SHEET: up over [0.04, 0.42] s of a 15 s loop, held, and down over
  // [12.6, 14.2] — the trapezoid that lets the loop cut while dark.
  const BoundFloat sheet =
      bind(&cycle)
          .source(0.0f, 15.0f)
          .trapezoid(0.04f / 15.0f, 0.42f / 15.0f, 12.6f / 15.0f, 14.2f / 15.0f)
          .value();
  EXPECT_FLOAT_EQ(sheet.apply(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(sheet.apply(2.0f), 1.0f);   // the quick capture moment
  EXPECT_FLOAT_EQ(sheet.apply(3.6f), 1.0f);   // …and the declared one
  EXPECT_FLOAT_EQ(sheet.apply(12.0f), 1.0f);  // still lit, late in the loop
  EXPECT_FLOAT_EQ(sheet.apply(14.6f), 0.0f);  // dark, and the loop can cut
  EXPECT_GT(sheet.apply(0.3f), 0.0f);
  EXPECT_LT(sheet.apply(0.3f), 1.0f);
}

// ---------------------------------------------------------------------------
// derive() — the bind() chain reaching an OUTPUT instead of a property
// slot, plus the wrap stage it composes with and the stepping-order
// contract that makes a derived cell current rather than one frame late.

TEST(AnimationValues, WrapFoldsThePostAffineValueAtTheSeam) {
  ch::Output<float> phase = 0.0f;

  // The seam: a ramp through 1.0 folds back to 0, floor-convention.
  const BoundFloat looped = bind(&phase).wrap(1.0f).value();
  EXPECT_FLOAT_EQ(looped.apply(0.25f), 0.25f);
  EXPECT_FLOAT_EQ(looped.apply(1.25f), 0.25f);
  EXPECT_FLOAT_EQ(looped.apply(7.75f), 0.75f);
  EXPECT_FLOAT_EQ(looped.apply(1.0f), 0.0f);  // the seam itself lands at 0

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
  EXPECT_FLOAT_EQ(bind(&phase).wrap(1.0f).clamp(0.0f, 0.5f).value().apply(1.9f),
                  0.5f);

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
  // Each stage against the hand-written arithmetic it replaces, compared
  // BIT-EXACTLY: a caller replacing one with the other must see identical
  // numbers, not merely close ones.
  ch::Output<float> out = 0.0f;

  // A trailing follower: the source value, offset back and clamped.
  const BoundFloat penTip = bind(&out).offset(-0.008f).clamp(0.f, 1.f).value();
  for (float g : {0.0f, 0.004f, 0.008f, 0.31f, 0.7431f, 0.999f, 1.0f})
    EXPECT_EQ(penTip.apply(g), std::clamp(g - 0.008f, 0.0f, 1.0f));

  // The affine chain, in call order: scale then offset.
  const BoundFloat pulse = bind(&out).scale(1.12f).offset(-0.12f).value();
  for (float u : {-1.0f, 0.0f, 0.31f, 0.5f, 0.99f, 1.0f, 2.5f})
    EXPECT_EQ(pulse.apply(u), -0.12f + u * 1.12f);

  // The looping phase, fmod(t * k, 1): scale into cycles, wrap at 1. For
  // a positive schedule this is fmod bit for bit, both being exact
  // operations on the same product.
  const BoundFloat ring = bind(&out).scale(0.5f).wrap(1.0f).value();
  for (float t : {0.0f, 0.7f, 1.9f, 2.0f, 13.37f, 400.25f})
    EXPECT_EQ(ring.apply(t), std::fmod(t * 0.5f, 1.0f));

  // The inverted sawtooth: invert() IS 1 − v.
  const BoundFloat rev = bind(&out).invert().value();
  for (float v : {0.0f, 0.25f, 0.61f, 1.0f}) EXPECT_EQ(rev.apply(v), 1.0f - v);

  // window(a, b) is the clamp((t−a)/(b−a), 0, 1) idiom. The
  // normalisation is stored as one multiply-add, so bit-identity holds
  // on a dyadic grid where both spellings are exact; off it the two
  // agree to float noise.
  const BoundFloat win = bind(&out).window(0.25f, 0.75f).value();
  for (int i = -8; i <= 72; ++i) {
    const float t = (float)i / 64.0f;
    EXPECT_EQ(win.apply(t), std::clamp((t - 0.25f) / 0.5f, 0.0f, 1.0f));
  }
  for (float t : {0.311f, 0.5002f, 0.7309f})
    EXPECT_NEAR(win.apply(t), std::clamp((t - 0.25f) / 0.5f, 0.0f, 1.0f),
                1e-6f);
}

// The noise field is reachable piecewise: a caller can evaluate one lattice
// cell, one octave or the summed field directly, and each keeps its
// documented range.
TEST(BindNoise, PiecesAreLinkableAndBounded) {
  using namespace sigil::motion::detail;
  EXPECT_NE(wiggleHash(1u), wiggleHash(2u));
  for (int cell = -8; cell <= 8; ++cell) {
    const float l = wiggleLattice(cell, 7u);
    EXPECT_GE(l, -1.0f);
    EXPECT_LE(l, 1.0f);
  }
  float prev = wiggleOctave(0.0f, 7u);
  for (int i = 1; i <= 200; ++i) {
    const float x = i * 0.01f;
    const float o = wiggleOctave(x, 7u);
    EXPECT_GE(o, -1.0f);
    EXPECT_LE(o, 1.0f);
    EXPECT_LT(std::abs(o - prev), 0.2f);  // quintic smoothing: no jumps
    prev = o;
  }
  const float one = wiggleNoise(0.37f, 7u, 1, 0.5f);
  EXPECT_FLOAT_EQ(one, wiggleOctave(0.37f, 7u));
  EXPECT_LE(std::abs(wiggleNoise(0.37f, 7u, 8, 0.5f)), 1.0f);
}
