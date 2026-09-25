/** @file
 * Binding::apply — every stage against the arithmetic it stands in for,
 * the envelopes, the fixed declaration order the stages run in, and the
 * sawtooth wrap folds the output at.
 */

#include "support/StandsAlone.h"

#include <gtest/gtest.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/ease/Ease.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

using namespace sigil::motion;

namespace {

/** One stage beside the arithmetic a caller would write by hand instead.
 *  `exact` inputs are where both spellings are exact operations and the
 *  two must agree BIT for bit — a caller replacing one with the other
 *  sees identical numbers, not merely close ones; `approx` inputs are
 *  off that grid, where the two round differently and agree to float
 *  noise. */
struct Stage {
  const char* name;
  Binding binding;
  std::function<float(float)> hand;
  std::vector<float> exact;
  std::vector<float> approx;
};

std::string stageName(const testing::TestParamInfo<Stage>& info) {
  return info.param.name;
}

struct Stages : testing::TestWithParam<Stage> {};

/** Sixty-fourths from @p first to @p last — the grid on which a division
 *  by a power of two is exact in float. */
std::vector<float> sixtyFourths(int first, int last) {
  std::vector<float> values;
  for (int index = first; index <= last; ++index)
    values.push_back((float)index / 64.0f);
  return values;
}

}  // namespace

TEST_P(Stages, ReproduceTheArithmeticTheyStandInFor) {
  const Stage& stage = GetParam();
  for (float value : stage.exact)
    EXPECT_EQ(stage.binding.apply(value), stage.hand(value)) << "at " << value;
  for (float value : stage.approx)
    EXPECT_NEAR(stage.binding.apply(value), stage.hand(value), 1e-6f)
        << "at " << value;
}

INSTANTIATE_TEST_SUITE_P(
    Fields, Stages,
    testing::Values(
        // A trailing follower: the value, offset back and clamped. The
        // offset is an output range one unit wide whose width is rounded
        // from its two ends, so the two spellings agree to float noise.
        Stage{"OffsetThenClamp",
              Binding{.to = {-0.008f, 1.0f - 0.008f}, .clamp = {0.0f, 1.0f}},
              [](float value) {
                return std::clamp(value - 0.008f, 0.0f, 1.0f);
              },
              {0.0f, 0.004f},
              {0.008f, 0.31f, 0.7431f, 0.999f, 1.0f}},
        // The output range is the low end plus the phase times the width.
        Stage{"To",
              Binding{.to = {-70.0f, 170.0f}},
              [](float value) { return -70.0f + value * 240.0f; },
              {-1.0f, 0.0f, 0.5f, 1.0f, 2.5f},
              {0.31f, 0.99f}},
        // The looping phase, `fmod(t * k, 1)`: for a positive schedule
        // both are exact operations on the same product.
        Stage{"ToThenWrap",
              Binding{.to = {0.0f, 0.5f}, .wrap = 1.0f},
              [](float value) { return std::fmod(value * 0.5f, 1.0f); },
              {0.0f, 0.7f, 1.9f, 2.0f, 13.37f, 400.25f},
              {}},
        // The inverted sawtooth: reverse IS 1 − v.
        Stage{"Reverse",
              Binding{.reverse = true},
              [](float value) { return 1.0f - value; },
              {0.0f, 0.25f, 0.61f, 1.0f},
              {}},
        // Five levels across [0,1], nearest — so four steps.
        Stage{"Quantize",
              Binding{.quantize = 5},
              [](float value) { return std::round(value * 4.0f) / 4.0f; },
              sixtyFourths(0, 64),
              {}},
        Stage{"Clamp",
              Binding{.clamp = {0.0f, 1.0f}},
              [](float value) { return std::clamp(value, 0.0f, 1.0f); },
              {-4.0f, 0.0f, 0.5f, 1.0f, 4.0f},
              {}},
        // A clamped `from` is the `clamp((t−a)/(b−a), 0, 1)` idiom, stored
        // as one multiply-add.
        Stage{"ClampedFrom",
              Binding{.from = {0.25f, 0.75f}, .clampFrom = true},
              [](float value) {
                return std::clamp((value - 0.25f) / 0.5f, 0.0f, 1.0f);
              },
              sixtyFourths(-8, 72),
              {0.311f, 0.5002f, 0.7309f}}),
    stageName);

TEST(Binding, FromAndToLandTheSourceRangeOnTheOutputRange) {
  const Binding binding{.from = {0.0f, 100.0f}, .to = {-70.0f, 170.0f}};
  for (float value : {0.0f, 25.0f, 50.0f, 100.0f})
    EXPECT_NEAR(binding.apply(value), -70.0f + value / 100.0f * 240.0f, 1e-4f);
  EXPECT_NEAR(binding.apply(0.0f), -70.0f, 1e-4f);
  EXPECT_NEAR(binding.apply(100.0f), 170.0f, 1e-4f);
}

TEST(Binding, AClampedFromHoldsTheDomainTheCurveSees) {
  // …so a curve downstream never sees a value outside its domain.
  const Binding clamped{.from = {0.2f, 0.4f}, .clampFrom = true};
  const Binding open{.from = {0.2f, 0.4f}};
  EXPECT_NEAR(clamped.apply(0.3f), open.apply(0.3f), 1e-4f);
  EXPECT_NEAR(clamped.apply(0.9f), 1.0f, 1e-4f);
  EXPECT_GT(open.apply(0.9f), 1.0f);

  // The clamp lands before the curve, and no curve here is total: an
  // overshooting curve evaluated far past the window returns exactly the
  // end of its range rather than being run outside its domain.
  const Binding overshoot{
      .from = {0.2f, 0.4f}, .clampFrom = true, .ease = ease::outBack()};
  EXPECT_NEAR(overshoot.apply(5.0f), 1.0f, 1e-4f);
}

TEST(Binding, TheStagesRunInDeclarationOrderWhateverOrderTheyAreThoughtOf) {
  // Levels, then the reversal, then the output range — the order the
  // fields are declared in — whichever of the three a reader thinks of
  // first. The three orders give three different answers, so the check
  // tells them apart.
  const Binding stepped{.quantize = 5, .reverse = true, .to = {0.0f, 80.0f}};
  for (int index = -32; index <= 96; ++index) {
    const float value = (float)index / 64.0f;
    const float declared =
        0.0f + (1.0f - std::round(value * 4.0f) / 4.0f) * 80.0f;
    EXPECT_EQ(stepped.apply(value), declared) << "at " << value;
  }
  // Reversing before the levels would snap the other way at a midpoint.
  EXPECT_EQ(stepped.apply(0.375f), 40.0f);
  EXPECT_NE(stepped.apply(0.375f),
            std::round((1.0f - 0.375f) * 4.0f) / 4.0f * 80.0f);
  // Putting the range on first would snap the pixels onto [0, 1] levels.
  EXPECT_NE(stepped.apply(0.25f),
            1.0f - std::round(0.25f * 80.0f * 4.0f) / 4.0f);

  // A binding filled field by field in another order is the same binding:
  // the order a caller writes the stages in is not an order they run in.
  Binding assigned;
  assigned.to = {0.0f, 80.0f};
  assigned.reverse = true;
  assigned.quantize = 5;
  EXPECT_TRUE(assigned == stepped);
  for (int index = -32; index <= 96; ++index) {
    const float value = (float)index / 64.0f;
    EXPECT_EQ(assigned.apply(value), stepped.apply(value)) << "at " << value;
  }
}

namespace {

/** One envelope in a binding that carries nothing else, and the name a
 *  failing parameter reports itself by. */
struct Shape {
  const char* name;
  Binding binding;
};

std::string shapeName(const testing::TestParamInfo<Shape>& info) {
  return info.param.name;
}

struct Envelopes : testing::TestWithParam<Shape> {};
struct PeriodicEnvelopes : testing::TestWithParam<Shape> {};

}  // namespace

TEST_P(Envelopes, StayInsideZeroToOneWhateverThePhase) {
  // The stage cannot hand a curve or an output range a value outside
  // [0,1], at any phase a schedule can reach — including one that never
  // reached zero.
  const Binding& binding = GetParam().binding;
  for (int index = -400; index <= 400; ++index) {
    const float value = (float)index / 37.0f;
    EXPECT_GE(binding.apply(value), -1e-6f) << "at " << value;
    EXPECT_LE(binding.apply(value), 1.0f + 1e-6f) << "at " << value;
  }
}

INSTANTIATE_TEST_SUITE_P(
    Shapes, Envelopes,
    testing::Values(
        Shape{"alternate", Binding{.alternate = true}},
        Shape{"cosine", Binding{.envelope = envelope::cosine()}},
        Shape{"trapezoid",
              Binding{.envelope = envelope::trapezoid(0.1f, 0.3f, 0.7f, 0.9f)}},
        Shape{"square", Binding{.envelope = envelope::square(0.6f)}}),
    shapeName);

TEST_P(PeriodicEnvelopes, DrawTheSameShapeOnEveryPeriod) {
  // A monotonic seconds value keeps breathing, bouncing or pulsing, and a
  // descending one is drawn by the same repeat rather than running off.
  // The phases are sixty-fourths so that shifting by a whole number of
  // periods is exact in float and the two answers are comparable at all.
  // The tolerance is for the one shape that repeats from the cosine
  // itself instead of from a fold: its argument grows with the phase, so
  // a shifted evaluation agrees to float noise on the angle rather than
  // bit for bit.
  const Binding& binding = GetParam().binding;
  for (int index = 0; index < 64; ++index) {
    const float phase = (float)index / 64.0f;
    EXPECT_NEAR(binding.apply(phase + 3.0f), binding.apply(phase), 2e-5f)
        << "at " << phase;
    EXPECT_NEAR(binding.apply(phase - 2.0f), binding.apply(phase), 2e-5f)
        << "at " << phase;
  }
}

// trapezoid is absent on purpose: it names positions inside ONE pass and
// stays dark past its last corner rather than drawing itself again.
INSTANTIATE_TEST_SUITE_P(
    Shapes, PeriodicEnvelopes,
    testing::Values(
        Shape{"alternate", Binding{.alternate = true}},
        Shape{"cosine", Binding{.envelope = envelope::cosine()}},
        Shape{"square", Binding{.envelope = envelope::square(0.6f)}},
        Shape{"shaped",
              Binding{.envelope = envelope::shaped(
                          [](float phase) { return phase * phase; })}}),
    shapeName);

TEST(Binding, AlternateRunsThereAndBackAcrossTheSpan) {
  const Binding bounce{.alternate = true};

  // The characteristic points: dark at both ends of the span, peak at the
  // middle, linear in between.
  EXPECT_FLOAT_EQ(bounce.apply(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(bounce.apply(0.25f), 0.5f);
  EXPECT_FLOAT_EQ(bounce.apply(0.5f), 1.0f);
  EXPECT_FLOAT_EQ(bounce.apply(0.75f), 0.5f);
  EXPECT_FLOAT_EQ(bounce.apply(1.0f), 0.0f);

  // The RETURN is the point: the outbound and inbound halves mirror, so a
  // sweep comes back instead of jumping.
  for (int index = 0; index <= 64; ++index) {
    const float phase = (float)index / 128.0f;
    EXPECT_NEAR(bounce.apply(phase), bounce.apply(1.0f - phase), 1e-6f)
        << "at " << phase;
  }
}

TEST(Binding, CosineIsTheRaisedCosineBreath) {
  const Binding breath{.envelope = envelope::cosine()};

  // 0 at the extremes, 1 at the middle — the swell a one-way window
  // cannot give.
  EXPECT_NEAR(breath.apply(0.0f), 0.0f, 1e-6f);
  EXPECT_NEAR(breath.apply(0.5f), 1.0f, 1e-6f);
  EXPECT_NEAR(breath.apply(1.0f), 0.0f, 1e-6f);
  EXPECT_NEAR(breath.apply(0.25f), 0.5f, 1e-6f);
  EXPECT_NEAR(breath.apply(0.75f), 0.5f, 1e-6f);

  // It IS 0.5 − 0.5·cos(2πv), the arithmetic a hand-written breath spells
  // out.
  for (int index = 0; index <= 200; ++index) {
    const float value = (float)index / 100.0f;
    EXPECT_NEAR(
        breath.apply(value),
        (float)(0.5 - 0.5 * std::cos(6.283185307179586 * (double)value)),
        1e-6f)
        << "at " << value;
  }

  // EASED AT BOTH ENDS, where alternate turns on a corner: the same
  // journey, but the derivative vanishes at the extremes and at the peak.
  const Binding corner{.alternate = true};
  const float step = 1e-3f;
  EXPECT_LT(std::fabs(breath.apply(step) - breath.apply(0.0f)),
            std::fabs(corner.apply(step) - corner.apply(0.0f)));
  EXPECT_LT(std::fabs(breath.apply(0.5f + step) - breath.apply(0.5f - step)),
            1e-4f);
}

TEST(Binding, TrapezoidRampsToAHoldAtOneAndIsDarkOutsideItsCorners) {
  const Binding sheet{.envelope = envelope::trapezoid(0.1f, 0.3f, 0.7f, 0.9f)};

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
  for (int index = 1; index < 40; ++index)
    EXPECT_FLOAT_EQ(sheet.apply(0.3f + 0.4f * (float)index / 40.0f), 1.0f);
}

TEST(Binding, TrapezoidCornersOutOfOrderCollapseRatherThanDivide) {
  // A ZERO-LENGTH SHOULDER is an instant cut, not a division by zero.
  const Binding cut{.envelope = envelope::trapezoid(0.25f, 0.25f, 0.75f, 0.75f)};
  EXPECT_FLOAT_EQ(cut.apply(0.2f), 0.0f);
  EXPECT_FLOAT_EQ(cut.apply(0.5f), 1.0f);
  EXPECT_FLOAT_EQ(cut.apply(0.8f), 0.0f);
  for (int index = -100; index <= 300; ++index)
    EXPECT_TRUE(std::isfinite(cut.apply((float)index / 100.0f)));

  // Corners are held non-decreasing, so out-of-order ones cannot ask for
  // a negative ramp — they collapse onto the corner before them. A rise
  // that ends before it starts becomes an instant one.
  const Envelope instantRise = envelope::trapezoid(0.2f, 0.1f, 0.8f, 0.9f);
  EXPECT_LE(instantRise.riseStart, instantRise.holdStart);
  EXPECT_LE(instantRise.holdStart, instantRise.holdEnd);
  EXPECT_LE(instantRise.holdEnd, instantRise.fallEnd);
  const Binding instant{.envelope = instantRise};
  EXPECT_FLOAT_EQ(instant.apply(0.19f), 0.0f);
  EXPECT_FLOAT_EQ(instant.apply(0.21f), 1.0f);
  EXPECT_NEAR(instant.apply(0.85f), 0.5f, 1e-6f);

  // …and corners that collapse ONTO EACH OTHER ask for nothing and get
  // nothing: dark everywhere, rather than a spike or a division.
  const Binding nothing{.envelope = envelope::trapezoid(0.6f, 0.2f, 0.1f, 0.4f)};
  for (int index = -100; index <= 200; ++index)
    EXPECT_FLOAT_EQ(nothing.apply((float)index / 100.0f), 0.0f);
}

TEST(Binding, TrapezoidStaysDarkPastItsLastCornerRatherThanRepeating) {
  // The one envelope that is not periodic: it names positions inside ONE
  // pass, so a repeating sheet rides a phase that already wraps.
  const Binding sheet{.envelope = envelope::trapezoid(0.1f, 0.3f, 0.7f, 0.9f)};
  EXPECT_FLOAT_EQ(sheet.apply(1.3f), 0.0f);
  EXPECT_FLOAT_EQ(sheet.apply(2.5f), 0.0f);
}

TEST(Binding, SquarePulsesOnFirstAndPhaseZeroIsOn) {
  const Binding pulse{.envelope = envelope::square(0.6f)};

  // PHASE 0 IS ON — a caret born at the start of its cycle is born
  // visible — and the whole first `duty` of the period is on, exactly 1.
  EXPECT_FLOAT_EQ(pulse.apply(0.0f), 1.0f);
  EXPECT_FLOAT_EQ(pulse.apply(0.3f), 1.0f);
  EXPECT_FLOAT_EQ(pulse.apply(0.59f), 1.0f);
  // OFF from `duty` to the end of the period, exactly 0.
  EXPECT_FLOAT_EQ(pulse.apply(0.6f), 0.0f);
  EXPECT_FLOAT_EQ(pulse.apply(0.99f), 0.0f);

  // Phase 1 is phase 0 — ON, where a trapezoid is dark at the seam.
  EXPECT_FLOAT_EQ(pulse.apply(1.0f), 1.0f);

  // The default duty is half the period.
  const Binding half{.envelope = envelope::square()};
  EXPECT_FLOAT_EQ(half.apply(0.49f), 1.0f);
  EXPECT_FLOAT_EQ(half.apply(0.51f), 0.0f);

  // Duty is held inside [0, 1]: 0 is never on, 1 is always on (the fold
  // keeps the phase below 1).
  const Binding never{.envelope = envelope::square(-2.0f)};
  const Binding always{.envelope = envelope::square(5.0f)};
  for (int index = 0; index <= 20; ++index) {
    EXPECT_FLOAT_EQ(never.apply((float)index / 7.0f), 0.0f);
    EXPECT_FLOAT_EQ(always.apply((float)index / 7.0f), 1.0f);
  }

  // The two levels land wherever the output range puts them — the blink
  // that rests dim rather than vanishing.
  const Binding caret{.from = {0.0f, 1.06f},
                      .envelope = envelope::square(0.62f / 1.06f),
                      .to = {0.10f, 1.0f}};
  EXPECT_FLOAT_EQ(caret.apply(0.0f), 1.0f);
  EXPECT_FLOAT_EQ(caret.apply(0.61f), 1.0f);
  EXPECT_FLOAT_EQ(caret.apply(0.63f), 0.10f);
  EXPECT_FLOAT_EQ(caret.apply(1.07f), 1.0f);  // the next period is on again
}

TEST(Binding, ShapedEvaluatesTheCallersCurveOnTheFoldedPhase) {
  // The caller's function sees a phase in [0,1) and its drawing repeats
  // every period — the escape hatch behind every named envelope.
  const Binding saw{
      .envelope = envelope::shaped([](float phase) { return phase * phase; })};
  EXPECT_FLOAT_EQ(saw.apply(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(saw.apply(0.5f), 0.25f);
  EXPECT_FLOAT_EQ(saw.apply(1.5f), 0.25f);   // folded: 1.5 → 0.5
  EXPECT_FLOAT_EQ(saw.apply(-0.5f), 0.25f);  // …and up from below
  EXPECT_NEAR(saw.apply(3.9f), saw.apply(0.9f), 1e-5f);

  // The ease still shapes what the envelope produced, and the output
  // range still lands it in the property's units — the fixed stage order.
  const Binding staged{
      .envelope = envelope::shaped([](float phase) { return phase; }),
      .ease = ease::inQuad,
      .to = {0.0f, 100.0f}};
  EXPECT_FLOAT_EQ(staged.apply(0.5f), ease::inQuad(0.5f) * 100.0f);

  // An empty curve passes the folded phase through rather than calling
  // nothing.
  const Binding empty{.envelope = envelope::shaped(nullptr)};
  EXPECT_FLOAT_EQ(empty.apply(1.25f), 0.25f);
  for (int index = -20; index <= 40; ++index)
    EXPECT_TRUE(std::isfinite(empty.apply((float)index / 8.0f)));
}

TEST(Binding, TheEnvelopeOccupiesOneFixedPlaceAmongTheStages) {
  // The span it shapes is the one `from` named, so a beat on a longer
  // timeline swells inside its own window and rests outside it.
  const Binding beat{.from = {2.0f, 4.0f},
                     .clampFrom = true,
                     .envelope = envelope::cosine()};
  EXPECT_NEAR(beat.apply(3.0f), 1.0f, 1e-6f);
  EXPECT_NEAR(beat.apply(2.5f), 0.5f, 1e-6f);
  EXPECT_NEAR(beat.apply(0.0f), 0.0f, 1e-6f);  // clamped to the span's start
  EXPECT_NEAR(beat.apply(9.0f), 0.0f, 1e-6f);  // …and to its end

  // The curve then shapes what the envelope PRODUCED. Any curve through
  // (0,0) and (1,1) therefore rounds a trapezoid's shoulders and leaves
  // its hold at exactly 1 and its dark at exactly 0 — the property that
  // makes the shoulder shape a separate decision from the corners.
  const Binding sheet{.envelope = envelope::trapezoid(0.1f, 0.3f, 0.7f, 0.9f)};
  const Binding eased{.envelope = envelope::trapezoid(0.1f, 0.3f, 0.7f, 0.9f),
                      .ease = ease::inOutQuad};
  EXPECT_FLOAT_EQ(eased.apply(0.5f), 1.0f);
  EXPECT_FLOAT_EQ(eased.apply(0.05f), 0.0f);
  EXPECT_LT(eased.apply(0.15f), sheet.apply(0.15f));

  // The output range then lands the shape in the property's own units.
  const Binding gradient{.envelope = envelope::cosine(),
                         .to = {400.0f, 880.0f}};
  EXPECT_NEAR(gradient.apply(0.5f), 880.0f, 1e-3f);
  EXPECT_NEAR(gradient.apply(0.0f), 400.0f, 1e-3f);

  // wrap sits on the far side of the range: the envelope shapes the
  // phase, wrap folds the output.
  const Binding spun{.alternate = true, .to = {0.0f, 720.0f}, .wrap = 360.0f};
  EXPECT_FLOAT_EQ(spun.apply(0.25f), 0.0f);  // 0.5 · 720 = 360 → 0
  EXPECT_FLOAT_EQ(spun.apply(0.125f), 180.0f);

  // The wiggle phase is read before all of it, for the same reason wrap
  // runs after: the shake reads the SCHEDULE, so an alternating phase
  // does not retrace the identical shake on the way back.
  const Binding shaken{.alternate = true,
                       .to = {0.0f, 0.0f},
                       .wiggle = {.amount = 5.0f, .frequency = 4.0f, .seed = 3}};
  EXPECT_NE(shaken.apply(0.25f), shaken.apply(0.75f));
  const Binding bare{.to = {0.0f, 0.0f},
                     .wiggle = {.amount = 5.0f, .frequency = 4.0f, .seed = 3}};
  for (int index = 0; index <= 100; ++index) {
    const float value = (float)index / 50.0f;
    EXPECT_FLOAT_EQ(shaken.apply(value), bare.apply(value)) << "at " << value;
  }
}

TEST(Binding, TheBreathAndTheSheetMatchTheArithmeticTheyStandInFor) {
  // Against the arithmetic a caller writes by hand, at the precision
  // replacing one spelling with the other needs.

  // The BREATH: a raised cosine over a 7.2 s period, peaking at 3.6 s.
  const Binding swell{.from = {0.0f, 7.2f}, .envelope = envelope::cosine()};
  // The tolerance is float noise on the phase, not a shape difference:
  // the binding divides once in float where the hand-written line divides
  // in double, so the two agree to a few units in the last place of the
  // angle and to that much of the swell.
  for (double seconds : {0.0, 0.5, 1.8, 3.6, 5.0, 7.2, 9.9})
    EXPECT_NEAR(swell.apply((float)seconds),
                (float)(0.5 - 0.5 * std::cos(6.283185307 * seconds / 7.2)),
                1e-5f)
        << "at " << seconds;

  // The SHEET: up over [0.04, 0.42] s of a 15 s loop, held, and down over
  // [12.6, 14.2] — the trapezoid that lets the loop cut while dark.
  const Binding sheet{
      .from = {0.0f, 15.0f},
      .envelope = envelope::trapezoid(0.04f / 15.0f, 0.42f / 15.0f,
                                      12.6f / 15.0f, 14.2f / 15.0f)};
  EXPECT_FLOAT_EQ(sheet.apply(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(sheet.apply(2.0f), 1.0f);   // the quick capture moment
  EXPECT_FLOAT_EQ(sheet.apply(3.6f), 1.0f);   // …and the declared one
  EXPECT_FLOAT_EQ(sheet.apply(12.0f), 1.0f);  // still lit, late in the loop
  EXPECT_FLOAT_EQ(sheet.apply(14.6f), 0.0f);  // dark, and the loop can cut
  EXPECT_GT(sheet.apply(0.3f), 0.0f);
  EXPECT_LT(sheet.apply(0.3f), 1.0f);
}

TEST(Binding, WrapFoldsTheOutputAtTheSeam) {
  // The seam: a ramp through 1.0 folds back to 0, floor-convention.
  const Binding looped{.wrap = 1.0f};
  EXPECT_FLOAT_EQ(looped.apply(0.25f), 0.25f);
  EXPECT_FLOAT_EQ(looped.apply(1.25f), 0.25f);
  EXPECT_FLOAT_EQ(looped.apply(7.75f), 0.75f);
  EXPECT_FLOAT_EQ(looped.apply(1.0f), 0.0f);  // the seam itself lands at 0

  // A DESCENDING schedule wraps UP into [0, period) — fmod alone would
  // answer a negative phase, which no consumer of a phase wants.
  EXPECT_FLOAT_EQ(looped.apply(-0.25f), 0.75f);

  // A period of 0 is a NO-OP, not a division; so is a negative period.
  EXPECT_FLOAT_EQ(Binding{.wrap = 0.0f}.apply(3.5f), 3.5f);
  EXPECT_FLOAT_EQ(Binding{.wrap = -2.0f}.apply(3.5f), 3.5f);

  // ORDER: after the output range (the wrap sees output units), before
  // clamp (a clamp still bounds the folded value).
  EXPECT_FLOAT_EQ((Binding{.to = {0.0f, 360.0f}, .wrap = 360.0f}.apply(1.5f)),
                  180.0f);
  EXPECT_FLOAT_EQ(
      (Binding{.wrap = 1.0f, .clamp = {0.0f, 0.5f}}.apply(1.9f)), 0.5f);

  // …and before wiggle, so a wrapped phase WIGGLES CONTINUOUSLY across
  // the seam: the noise phase reads the unwrapped schedule. The noise
  // contribution (wiggled minus base) must step smoothly across v = 1,
  // while the base itself jumps by a full period.
  const Binding wiggled{
      .wrap = 1.0f,
      .wiggle = {.amount = 5.0f, .frequency = 3.0f, .seed = 9}};
  const float beforeSeam = wiggled.apply(0.9999f) - looped.apply(0.9999f);
  const float afterSeam = wiggled.apply(1.0001f) - looped.apply(1.0001f);
  EXPECT_NEAR(beforeSeam, afterSeam, 0.05f)
      << "the noise repeated with the wrap — its phase must read the "
         "unwrapped schedule";
}
