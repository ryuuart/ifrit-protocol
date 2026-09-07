#pragma once

/** @file
 * A SIGNAL THAT REPEATS, as a value: which wave, how fast, where in the
 * cycle it starts, how far it swings and what it swings about. It
 * answers a number for a time in seconds and reads the same everywhere —
 * in a describe, in a force's strength, in a sketch's draw.
 *
 * The bind chain already shapes a phase somebody else is stepping
 * (`pingPong`, `cosine`, `square`, `wave`); this is the same set of
 * shapes for the far more common case where there is no Output and no
 * ticker in reach — a clock reading and a number wanted from it. What it
 * removes at the call site is the FOLD: a bare `sin(t * k)` is one
 * expression, but a wave that starts somewhere, swings by something and
 * sits about something is four, and the modulus that keeps a triangle or
 * a pulse in its cycle is the one a hand-written phase gets wrong for
 * negative times.
 */

#include <cmath>
#include <cstdint>

namespace sigil::motion {

/** THE CLASSIC SHAPES, each read on a phase folded into [0, 1).
 *
 *  Each is stated over the unit phase and answers on [-1, 1], so the
 *  amplitude and the centre below mean the same thing whichever is
 *  chosen and a scene can swap one for another without re-tuning the
 *  numbers around it. */
enum class Wave : uint8_t {
  /** The swell: smooth everywhere, at the centre when the phase is 0.
   *  What a breath, a bob and a pulse of light are. */
  Sine,
  /** There and back at a constant rate — the shape a ping-pong is, and
   *  the one that reads as mechanical because its turns are corners. On
   *  the sine's phase: at the centre when the phase is 0, at the crest a
   *  quarter in, so swapping it for a sine keeps the timing and changes
   *  only the feel. */
  Triangle,
  /** Up and snap back: the scanline, the sweep, the marching offset.
   *  Its cycle starts at the BOTTOM and cuts at the end, which is where
   *  a ramp that stands for a sweep has to start. */
  Sawtooth,
  /** ON for the first `duty` of each cycle and OFF after — the blink,
   *  the strobe, the caret. Phase 0 is ON, so a thing born at the start
   *  of its cycle is born visible. */
  Square,
};

/** THE OSCILLATOR: one repeating signal, stated in the four numbers a
 *  designer picks and not in the arithmetic they turn into.
 *
 *  Comparable and free of state, so it is a token a look carries: one
 *  chosen for a scene's flicker is read by a colour, a scale and a
 *  force's strength alike, and the three cannot drift apart because
 *  there is one value rather than three spellings of a sine. */
struct Oscillator {
  Wave wave = Wave::Sine;
  /** Cycles per second. Zero holds the signal at its starting phase,
   *  which is the spelling of "not moving" — not a division by zero. */
  float hertz = 1.0f;
  /** Where in the cycle time zero is, in cycles: 0.25 is a quarter turn
   *  in. It wraps, so an offset per index (a row's stagger, a golden
   *  walk) needs no fold at the call site. */
  float phase = 0.0f;
  /** How far it swings either side of the centre. */
  float amplitude = 1.0f;
  /** What it swings about. */
  float centre = 0.0f;
  /** `Square`: the fraction of each cycle that is ON, held in [0, 1]. */
  float duty = 0.5f;

  bool operator==(const Oscillator&) const = default;

  /** THE SIGNAL AT @p seconds. */
  [[nodiscard]] float at(double seconds) const {
    return centre + amplitude * shape(fold(seconds));
  }

  /** The same call, so an oscillator IS an interpolator: anything that
   *  hands a number to a callable takes one. */
  float operator()(double seconds) const { return at(seconds); }

  /** WHERE IN THE CYCLE @p seconds is, in [0, 1). Exposed because
   *  anything travelling with the signal — a trail, a second wave a
   *  quarter turn behind, a stamp per cycle — needs the same phase this
   *  reads, and recovering it from the value cannot be done for a wave
   *  that visits a number twice. */
  [[nodiscard]] float fold(double seconds) const {
    const double turns = seconds * (double)hertz + (double)phase;
    const double folded = turns - std::floor(turns);
    return (float)folded;
  }

  /** THE WAVEFORM ITSELF on a phase already folded into [0, 1), in
   *  [-1, 1] and before the amplitude and the centre.
   *
   *  It is the shape a bound Output's own phase takes, so this is what
   *  `bind(&value).source(0, period).wave(...)` is handed: the binding
   *  folds the phase, and this says what the fold means. */
  [[nodiscard]] float shape(float unitPhase) const {
    constexpr float kTurn = 6.2831853071795864769f;
    switch (wave) {
      case Wave::Sine:
        return std::sin(unitPhase * kTurn);
      case Wave::Triangle: {
        const float shifted = unitPhase + 0.75f;
        return 4.0f * std::abs(shifted - std::floor(shifted) - 0.5f) - 1.0f;
      }
      case Wave::Sawtooth:
        return unitPhase * 2.0f - 1.0f;
      case Wave::Square:
        return unitPhase < (duty < 0.0f ? 0.0f : (duty > 1.0f ? 1.0f : duty))
                   ? 1.0f
                   : -1.0f;
    }
    return 0.0f;
  }
};

}  // namespace sigil::motion
