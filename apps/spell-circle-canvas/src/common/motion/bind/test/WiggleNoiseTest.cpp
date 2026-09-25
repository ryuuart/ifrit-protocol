/** @file
 * The wiggle stage and the value-noise field behind it: bounded, smooth,
 * seeded, phased off the schedule rather than off a clock, and each piece
 * of the field inside the range its header states.
 */

#include "support/StandsAlone.h"

#include <gtest/gtest.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/bind/WiggleNoise.h>
#include <sigilmotion/ease/Ease.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace sigil::motion;

namespace {
/** Sample a binding across a phase sweep — the trace every wiggle pin
 *  below reasons about. */
std::vector<float> trace(const Binding& binding, float first, float last,
                         int count) {
  std::vector<float> samples;
  samples.reserve((size_t)count);
  for (int index = 0; index < count; ++index)
    samples.push_back(
        binding.apply(first + (last - first) * (float)index / (float)(count - 1)));
  return samples;
}
float spread(const std::vector<float>& samples) {
  return *std::max_element(samples.begin(), samples.end()) -
         *std::min_element(samples.begin(), samples.end());
}
/** How often a trace turns around — the metric that sees a fine tremble
 *  riding on a drift, which total travel does not. */
int reversals(const std::vector<float>& samples) {
  int count = 0;
  for (size_t index = 2; index < samples.size(); ++index)
    if ((samples[index] - samples[index - 1] > 0) !=
        (samples[index - 1] - samples[index - 2] > 0))
      ++count;
  return count;
}
/** Pure noise around rest: an output range that is one point, so only
 *  the wiggle moves the value while its phase still reads the schedule. */
Binding atRest(float amount, float frequency, uint32_t seed, int octaves = 1) {
  return Binding{.to = {0.0f, 0.0f},
                 .wiggle = {.amount = amount,
                            .frequency = frequency,
                            .seed = seed,
                            .octaves = octaves}};
}
}  // namespace

TEST(Wiggle, IsSmoothBoundedAndInOutputUnits) {
  // AMPLITUDE IS IN OUTPUT UNITS — the whole reason the stage sits after
  // the output range. ±12 around an output range of [-70, 170]: the value
  // never leaves the range widened by exactly 12, and it does leave the
  // un-widened one (otherwise "bounded" would be vacuous).
  const Binding pixels{.to = {-70.0f, 170.0f},
                       .wiggle = {.amount = 12.0f, .frequency = 9.0f, .seed = 4}};
  const std::vector<float> shaken = trace(pixels, 0.0f, 1.0f, 2001);
  const Binding plain{.to = {-70.0f, 170.0f}};
  bool leftTheRange = false;
  for (size_t index = 0; index < shaken.size(); ++index) {
    const float base = plain.apply((float)index / (float)(shaken.size() - 1));
    EXPECT_LE(std::fabs(shaken[index] - base), 12.0f + 1e-3f)
        << "at " << index;
    if (shaken[index] < -70.0f || shaken[index] > 170.0f) leftTheRange = true;
  }
  EXPECT_TRUE(leftTheRange) << "±12 px that never leaves the range is not a "
                               "wiggle — the bound claim would be vacuous";

  // SMOOTH, not white. Consecutive samples of a 9-cycle wiggle sampled
  // 2000× must step by far less than the peak-to-peak of the noise
  // itself; white noise would step by ~2·amount every sample.
  float largestStep = 0.0f;
  for (size_t index = 1; index < shaken.size(); ++index)
    largestStep =
        std::max(largestStep, std::fabs(shaken[index] - shaken[index - 1]));
  EXPECT_LT(largestStep, 1.0f)
      << "the trace teleports — this is not value noise";

  // …and the negative half of the same claim: at ONE sample per cycle the
  // very same field DOES jump, so the smoothness above is a property of
  // the noise and not of a wiggle too quiet to see.
  const std::vector<float> undersampled = trace(pixels, 0.0f, 20.0f, 181);
  float coarseStep = 0.0f;
  for (size_t index = 1; index < undersampled.size(); ++index)
    coarseStep = std::max(
        coarseStep, std::fabs(undersampled[index] - undersampled[index - 1]));
  EXPECT_GT(coarseStep, 4.0f);

  // An amount of zero is the whole stage disengaged, exactly.
  const Binding silent{.to = {-70.0f, 170.0f},
                       .wiggle = {.amount = 0.0f, .frequency = 9.0f}};
  for (float value : {0.0f, 0.3f, 0.77f, 1.0f})
    EXPECT_FLOAT_EQ(silent.apply(value), plain.apply(value));
}

TEST(Wiggle, IsDeterministicAndSeeded) {
  // PURE noise, no base contribution. A binding carrying an output range
  // of [-70, 170] would make every claim below a claim about the RAMP
  // rather than about the wiggle — in particular the correlation check in
  // part 4, where two traces sharing a ramp correlate almost perfectly
  // whatever their noise fields do.
  const auto rig = [](uint32_t seed) { return atRest(12.0f, 9.0f, seed); };

  // 1. SAME INPUT, SAME NUMBER — across independently built bindings and
  //    across repeated evaluation. No clock is read anywhere in apply().
  const std::vector<float> first = trace(rig(7), 0.0f, 3.0f, 601);
  const std::vector<float> second = trace(rig(7), 0.0f, 3.0f, 601);
  EXPECT_EQ(first, second);
  const float once = rig(7).apply(0.418f);
  for (int repeat = 0; repeat < 4; ++repeat)
    EXPECT_FLOAT_EQ(rig(7).apply(0.418f), once);

  // 2. …AND THE TRACE ACTUALLY MOVES. Without this, "identical across
  //    runs" is a claim about a constant. It must swing most of the way
  //    to its ±12 bound over three cycles.
  EXPECT_GT(spread(first), 12.0f);

  // 3. …AND A DIFFERENT INPUT SEQUENCE LANDS ELSEWHERE. Same seed, same
  //    shaping, a phase window shifted by half a cycle: the traces must
  //    NOT coincide.
  const std::vector<float> shifted = trace(rig(7), 0.055f, 3.055f, 601);
  EXPECT_NE(first, shifted);
  double drift = 0.0;
  for (size_t index = 0; index < first.size(); ++index)
    drift += std::fabs(first[index] - shifted[index]);
  EXPECT_GT(drift / (double)first.size(), 1.0);

  // 4. SEEDING. Same seed ⇒ identical, different seed ⇒ independent —
  //    the property that makes a two-axis shake possible at all. Two
  //    lanes sharing a seed move on a diagonal; these must not.
  EXPECT_EQ(trace(rig(1), 0.0f, 3.0f, 601), trace(rig(1), 0.0f, 3.0f, 601));
  const std::vector<float> horizontal = trace(rig(1), 0.0f, 3.0f, 601);
  const std::vector<float> vertical = trace(rig(2), 0.0f, 3.0f, 601);
  EXPECT_NE(horizontal, vertical);
  // Correlated axes are the real failure, not merely unequal ones: the
  // centred traces must be near-uncorrelated (adjacent SEEDS is the case
  // a weak hash gets wrong).
  double meanHorizontal = 0, meanVertical = 0;
  for (size_t index = 0; index < horizontal.size(); ++index) {
    meanHorizontal += horizontal[index];
    meanVertical += vertical[index];
  }
  meanHorizontal /= (double)horizontal.size();
  meanVertical /= (double)vertical.size();
  double covariance = 0, varianceHorizontal = 0, varianceVertical = 0;
  for (size_t index = 0; index < horizontal.size(); ++index) {
    const double centredHorizontal = horizontal[index] - meanHorizontal;
    const double centredVertical = vertical[index] - meanVertical;
    covariance += centredHorizontal * centredVertical;
    varianceHorizontal += centredHorizontal * centredHorizontal;
    varianceVertical += centredVertical * centredVertical;
  }
  EXPECT_GT(varianceHorizontal, 0.0);
  EXPECT_GT(varianceVertical, 0.0);
  EXPECT_LT(std::fabs(covariance /
                      std::sqrt(varianceHorizontal * varianceVertical)),
            0.35)
      << "seeds 1 and 2 wiggle together — a two-axis shake would slide "
         "along a diagonal";
}

TEST(Wiggle, AShakeAtRestParksWhereTheOutputRangeSays) {
  // An output range that is one point holds the property at REST there
  // and only the noise moves it, while the phase still comes from the
  // schedule. Moving the point parks the shake somewhere other than zero,
  // and the amount scales the same field rather than reshaping it.
  const Binding atZero = atRest(12.0f, 7.0f, 1);
  const Binding parked{
      .to = {100.0f, 100.0f},
      .wiggle = {.amount = 3.0f, .frequency = 7.0f, .seed = 1}};
  for (int index = 0; index <= 200; ++index) {
    const float value = (float)index / 100.0f;
    EXPECT_NEAR(parked.apply(value), 100.0f + atZero.apply(value) / 4.0f, 1e-3f)
        << "at " << value;
  }
}

TEST(Wiggle, PhaseComesFromTheScheduleNotTheOutput) {
  // The noise PHASE is read off the normalised input, so the output range
  // moves the wiggle's SIZE and never its TIMING. Two bindings whose
  // outputs differ only by a factor of 10 must wiggle in step, not at ten
  // times the rate.
  const Binding small = atRest(1.0f, 6.0f, 3);
  const Binding big = atRest(10.0f, 6.0f, 3);
  for (int index = 0; index <= 200; ++index) {
    const float value = (float)index / 100.0f;
    EXPECT_NEAR(big.apply(value), small.apply(value) * 10.0f, 1e-3f)
        << "at " << value;
  }

  // The CURVE shapes the signal, not the schedule: sampling the phase
  // before the ease means an eased binding's wiggle keeps its own rate.
  Binding eased = atRest(1.0f, 6.0f, 3);
  eased.ease = ease::inQuint;
  for (int index = 0; index <= 200; ++index) {
    const float value = (float)index / 200.0f;
    EXPECT_NEAR(eased.apply(value), small.apply(value), 1e-4f)
        << "at " << value;
  }

  // quantize likewise stair-steps the signal, never the wiggle.
  Binding stepped = atRest(1.0f, 6.0f, 3);
  stepped.quantize = 4;
  for (int index = 0; index <= 200; ++index) {
    const float value = (float)index / 200.0f;
    EXPECT_NEAR(stepped.apply(value), small.apply(value), 1e-4f);
  }

  // A clamped `from` holds the input, and the phase rides that clamp: a
  // wiggle scoped to a beat HOLDS outside it.
  Binding scoped = atRest(5.0f, 6.0f, 3);
  scoped.from = {0.2f, 0.4f};
  scoped.clampFrom = true;
  EXPECT_FLOAT_EQ(scoped.apply(0.9f), scoped.apply(2.0f));
  EXPECT_NE(scoped.apply(0.9f), scoped.apply(0.3f));

  // clamp still applies LAST — a wiggled opacity lands in [0,1].
  const Binding opacity{
      .to = {0.9f, 1.0f},
      .wiggle = {.amount = 0.4f, .frequency = 12.0f, .seed = 5},
      .clamp = {0.0f, 1.0f}};
  for (int index = 0; index <= 500; ++index) {
    const float value = opacity.apply((float)index / 500.0f);
    EXPECT_GE(value, 0.0f);
    EXPECT_LE(value, 1.0f);
  }

  // OCTAVES change the TEXTURE, not the SIZE. At the default falloff each
  // added octave carries about the same slope as the base, so total
  // variation barely moves and the normaliser hides the change in step
  // size entirely; direction reversals see the tremble directly.
  const std::vector<float> one = trace(atRest(10.0f, 4.0f, 8, 1), 0, 4, 4001);
  const std::vector<float> three = trace(atRest(10.0f, 4.0f, 8, 3), 0, 4, 4001);
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
TEST(WiggleNoise, EveryPieceOfTheFieldKeepsTheRangeItsHeaderStates) {
  using namespace sigil::motion::detail;
  EXPECT_NE(wiggleHash(1u), wiggleHash(2u));
  for (int cell = -8; cell <= 8; ++cell) {
    const float lattice = wiggleLattice(cell, 7u);
    EXPECT_GE(lattice, -1.0f);
    EXPECT_LE(lattice, 1.0f);
  }
  float previous = wiggleOctave(0.0f, 7u);
  for (int index = 1; index <= 200; ++index) {
    const float position = index * 0.01f;
    const float octave = wiggleOctave(position, 7u);
    EXPECT_GE(octave, -1.0f);
    EXPECT_LE(octave, 1.0f);
    EXPECT_LT(std::abs(octave - previous), 0.2f);  // quintic smoothing: no jumps
    previous = octave;
  }
  const float one = wiggleNoise(0.37f, 7u, 1, 0.5f);
  EXPECT_FLOAT_EQ(one, wiggleOctave(0.37f, 7u));
  EXPECT_LE(std::abs(wiggleNoise(0.37f, 7u, 8, 0.5f)), 1.0f);
}
