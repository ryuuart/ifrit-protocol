/** @file
 * The repeating signal: each wave is the shape its name says on the
 * phase it is stated for, the fold survives a negative time and a
 * thousand cycles, the four numbers mean the same thing whichever wave
 * is chosen, and a still oscillator is not a division by zero.
 */

#include <gtest/gtest.h>
#include <sigilmotion/values/Oscillator.h>

#include <cmath>

using namespace sigil::motion;

TEST(Oscillator, EachWaveIsTheShapeItsNameSays) {
  const Oscillator sine{.wave = Wave::Sine};
  EXPECT_NEAR(sine.shape(0.0f), 0.0f, 1e-6f);
  EXPECT_NEAR(sine.shape(0.25f), 1.0f, 1e-6f);
  EXPECT_NEAR(sine.shape(0.75f), -1.0f, 1e-6f);

  // The triangle is on the sine's phase, so swapping one for the other
  // keeps the timing and changes only the feel.
  const Oscillator triangle{.wave = Wave::Triangle};
  EXPECT_NEAR(triangle.shape(0.0f), 0.0f, 1e-6f);
  EXPECT_NEAR(triangle.shape(0.25f), 1.0f, 1e-6f);
  EXPECT_NEAR(triangle.shape(0.5f), 0.0f, 1e-6f);
  EXPECT_NEAR(triangle.shape(0.75f), -1.0f, 1e-6f);
  // Straight between its corners, which is what makes it read
  // mechanical where the sine reads as a breath.
  EXPECT_NEAR(triangle.shape(0.125f), 0.5f, 1e-6f);

  // The ramp starts at the bottom and cuts at the end of its cycle.
  const Oscillator saw{.wave = Wave::Sawtooth};
  EXPECT_NEAR(saw.shape(0.0f), -1.0f, 1e-6f);
  EXPECT_NEAR(saw.shape(0.5f), 0.0f, 1e-6f);
  EXPECT_NEAR(saw.shape(0.999f), 1.0f, 1e-2f);

  // Phase 0 is ON, so a thing born at the start of its cycle is born
  // visible.
  const Oscillator blink{.wave = Wave::Square, .duty = 0.25f};
  EXPECT_FLOAT_EQ(blink.shape(0.0f), 1.0f);
  EXPECT_FLOAT_EQ(blink.shape(0.24f), 1.0f);
  EXPECT_FLOAT_EQ(blink.shape(0.26f), -1.0f);
}

TEST(Oscillator, TheFoldSurvivesANegativeTimeAndAThousandCycles) {
  const Oscillator wave{.wave = Wave::Sawtooth, .hertz = 3.0f};
  EXPECT_NEAR(wave.fold(0.0), 0.0f, 1e-6f);
  // The fold is the whole reason this is a value: a phase written by
  // hand is what gets a negative time wrong.
  EXPECT_NEAR(wave.fold(-1.0 / 6.0), 0.5f, 1e-5f);
  EXPECT_NEAR(wave.fold(1000.0 / 3.0 + 1.0 / 12.0), 0.25f, 1e-4f);
  EXPECT_GE(wave.fold(-12345.678), 0.0f);
  EXPECT_LT(wave.fold(-12345.678), 1.0f);
}

TEST(Oscillator, TheFourNumbersMeanTheSameThingWhicheverWaveItIs) {
  for (Wave shape : {Wave::Sine, Wave::Triangle, Wave::Sawtooth, Wave::Square}) {
    const Oscillator wave{
        .wave = shape, .hertz = 2.0f, .amplitude = 30.0f, .centre = 100.0f};
    for (int i = 0; i < 64; ++i) {
      const float value = wave.at((double)i * 0.013);
      EXPECT_GE(value, 70.0f - 1e-3f);
      EXPECT_LE(value, 130.0f + 1e-3f);
    }
  }
}

TEST(Oscillator, PhaseMovesTheCycleAndNoRateHoldsItStill) {
  const Oscillator plain{.wave = Wave::Sine, .hertz = 1.0f};
  const Oscillator quarterTurnIn{.wave = Wave::Sine, .hertz = 1.0f,
                                 .phase = 0.25f};
  EXPECT_NEAR(quarterTurnIn.at(0.0), plain.at(0.25), 1e-6f);
  // A phase past a whole turn wraps, so an offset per index needs no
  // fold at the call site.
  const Oscillator wrapped{.wave = Wave::Sine, .hertz = 1.0f, .phase = 4.25f};
  EXPECT_NEAR(wrapped.at(0.0), quarterTurnIn.at(0.0), 1e-5f);

  // No rate is the spelling of "not moving", not a division by zero.
  const Oscillator still{.wave = Wave::Sine, .hertz = 0.0f, .phase = 0.25f};
  EXPECT_FLOAT_EQ(still.at(0.0), still.at(9999.0));
  EXPECT_NEAR(still.at(9999.0), 1.0f, 1e-6f);
}

TEST(Oscillator, ItIsAValueACallerCanCarryAndCall) {
  const Oscillator flicker{.wave = Wave::Square, .hertz = 12.0f, .duty = 0.3f};
  EXPECT_EQ(flicker, (Oscillator{.wave = Wave::Square, .hertz = 12.0f,
                                 .duty = 0.3f}));
  EXPECT_NE(flicker, Oscillator{});
  // Callable, so anything that hands a number to an interpolator takes
  // one — including the wave stage of a shaped binding, which is handed
  // the folded phase this reads on.
  auto read = [](double t, const auto& signal) { return signal(t); };
  EXPECT_FLOAT_EQ(read(0.0, flicker), flicker.at(0.0));
}
