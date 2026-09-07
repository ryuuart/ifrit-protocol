/** @file
 * The wiggle stage and the value-noise field behind it: bounded, smooth,
 * seeded, phased off the schedule rather than off a clock, and each piece
 * of the field inside the range its header states.
 */

#include <gtest/gtest.h>
#include <sigilmotion/bind/Bind.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "support/StandsAlone.h"

using namespace sigil::motion;
namespace ch = choreograph;

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
/** How often a trace turns around — the metric that sees a fine tremble
 *  riding on a drift, which total travel does not. */
int reversals(const std::vector<float>& v) {
  int n = 0;
  for (size_t i = 2; i < v.size(); ++i)
    if ((v[i] - v[i - 1] > 0) != (v[i - 1] - v[i - 2] > 0)) ++n;
  return n;
}
}  // namespace

TEST(Bind, WiggleIsSmoothBoundedAndInOutputUnits) {
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

TEST(Bind, WiggleIsDeterministicAndSeeded) {
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

TEST(Bind, TheWiggleBuilderIsAChainAtRestAndComposesLikeAnyOther) {
  // `wiggle(&out, …)` is `bind(&out).scale(0).wiggle(…)` named: the
  // property sits at REST and only the noise moves it, while the phase
  // still comes from the schedule whose contribution was zeroed. It
  // returns an ordinary chain, so the affine stages still compose — an
  // offset parks the shake somewhere other than zero, and the amount
  // scales the same field rather than reshaping it.
  ch::Output<float> phase = 0.0f;
  const BoundFloat named = wiggle(&phase, 12.f, 7.f, 1).value();
  const BoundFloat spelled =
      bind(&phase).scale(0.f).wiggle(12.f, 7.f, 1).value();
  EXPECT_EQ(named.source, &phase);
  for (int i = 0; i <= 200; ++i) {
    const float v = (float)i / 100.0f;
    EXPECT_FLOAT_EQ(named.apply(v), spelled.apply(v)) << "at " << v;
  }

  const BoundFloat parked = wiggle(&phase, 3.f, 7.f, 1).offset(100.f).value();
  for (int i = 0; i <= 200; ++i) {
    const float v = (float)i / 100.0f;
    EXPECT_NEAR(parked.apply(v), 100.f + named.apply(v) / 4.0f, 1e-3f)
        << "at " << v;
  }
}

TEST(Bind, WigglePhaseComesFromTheScheduleNotTheOutput) {
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

  // OCTAVES change the TEXTURE, not the SIZE. At the default falloff each
  // added octave carries about the same slope as the base, so total
  // variation barely moves and the normaliser hides the change in step
  // size entirely; direction reversals see the tremble directly.
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

// The noise field is a testable seam this library states in its README:
// `WiggleNoise.h` documents a range for each piece — the lattice cell, one
// octave and the normalised sum — and those are promises to whoever reads
// the header, not incidental facts about the body. They are reachable
// piecewise so each can be held to its own range rather than only through
// the shake it adds up to.
TEST(BindNoise, EveryPieceOfTheFieldKeepsTheRangeItsHeaderStates) {
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
