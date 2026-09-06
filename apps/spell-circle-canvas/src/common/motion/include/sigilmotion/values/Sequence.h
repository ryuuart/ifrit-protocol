#pragma once

/** @file
 * A NUMBER GIVEN AT SEVERAL TIMES, and what it does between them: the
 * keyed steps as one value, read at any time in the caller's own units.
 *
 * `Transitioned` is a value together with how it MOVES when it changes,
 * and its waypoints play once, at a mount, on a ticker. This is the
 * other half: a track that is a function of a time and nothing else, so
 * a bake, a scrub, a shader's uniform and a value read three times in
 * one frame all get the same number. An envelope is one of these — an
 * attack that rises, a hold, a release — and so is a step sequencer, a
 * cue list, and a curve authored somewhere else and read here.
 */

#include <sigilcore/compute/Curve.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sigil::motion {

/** WHAT HAPPENS BETWEEN TWO KEYS. */
enum class Interpolation : uint8_t {
  /** Nothing: the value is the last key's until the next one, and the
   *  change is a cut. The step sequencer, the cue list, the frame
   *  index — anything whose values are states rather than positions. */
  Hold,
  /** A straight line, shaped by the leaving key's own curve. */
  Linear,
  /** THE SPLINE THROUGH THE KEYS: each segment bent so the value
   *  arrives and leaves at the average slope of its neighbours. What a
   *  hand-drawn motion path is, and the reason it is a prop rather than
   *  a second type — the keys are the same keys, and only what is drawn
   *  between them changed.
   *
   *  It OVERSHOOTS. A run of keys that turns sharply swings past the key
   *  it is turning at, which is the whole character of the curve and is
   *  wrong for anything bounded: a value that must stay inside a range
   *  is `Linear`, or is clamped by whatever reads it. Per-key curves are
   *  not read here — a spline's shape is its neighbours. */
  CatmullRom,
};

/** ONE KEY: when, what, and how the segment LEAVING it is shaped. */
struct Step {
  /** When, in the caller's own units — seconds for a track, a unit
   *  progress for an envelope, an index for a table. The sequence reads
   *  its keys in the order given and expects them ordered. */
  float at = 0.0f;
  float value = 0.0f;
  /** The shape of the segment from this key to the next, under
   *  `Linear`. A default-built curve is the straight line. It carries its
   *  own parameters, so a step eased by a named curve still compares. */
  core::curve::Curve curve{};

  bool operator==(const Step&) const = default;
};

/** THE TRACK: the keys, what happens between them, and whether it
 *  repeats.
 *
 *  Outside the keys it CLAMPS to the first or the last, unless it loops,
 *  because a track carries no answer for what lies beyond its ends — the
 *  same rule a colour ramp reads outside its stops. A sequence with no
 *  keys answers zero: a determinate number, so a caller cannot be handed
 *  whatever was on the stack. */
struct Sequence {
  std::vector<Step> steps;
  Interpolation interpolation = Interpolation::Linear;
  /** The track read as a cycle: a time past the last key folds back to
   *  the first, so the last key's time is the wrap point rather than a
   *  key that is ever reached. A loop is therefore authored with its
   *  last key repeating its first; ends that differ leave a jump at the
   *  wrap, which is a fact about the keys and not something to hide. */
  bool loop = false;

  bool operator==(const Sequence&) const = default;

  /** How long the track is, in the units its keys are in. */
  [[nodiscard]] float duration() const {
    return steps.empty() ? 0.0f : steps.back().at - steps.front().at;
  }

  /** THE VALUE AT @p time. */
  [[nodiscard]] float at(float time) const {
    if (steps.empty()) return 0.0f;
    if (steps.size() == 1) return steps.front().value;

    const float first = steps.front().at;
    const float span = duration();
    float when = time;
    if (loop && span > 0.0f) {
      const float folded = std::fmod(when - first, span);
      when = first + (folded < 0.0f ? folded + span : folded);
    } else {
      when = std::clamp(when, first, steps.back().at);
    }

    size_t index = 0;
    while (index + 1 < steps.size() && steps[index + 1].at <= when) ++index;
    const size_t next = index + 1 < steps.size() ? index + 1 : index;
    const Step& from = steps[index];
    const Step& to = steps[next];
    if (interpolation == Interpolation::Hold || next == index)
      return from.value;

    const float segment = to.at - from.at;
    // Two keys at one time are a CUT: the later one wins, which is how a
    // track says a value jumps, and dividing by the zero between them
    // would not have said anything.
    if (!(segment > 0.0f)) return to.value;
    const float unit = (when - from.at) / segment;

    if (interpolation == Interpolation::CatmullRom)
      return spline(index, next, unit);
    return from.value + (to.value - from.value) * from.curve(unit);
  }

  /** The same call, so a sequence IS an interpolator: a bound value's
   *  `wave()`, a scale's `through()` and anything else that hands a
   *  number to a callable takes one. */
  float operator()(float time) const { return at(time); }

 private:
  /** The key before @p index and the one after @p next, which is what a
   *  spline segment needs beyond its own two ends. A track that does not
   *  loop repeats its end keys, so the first and last segments bend the
   *  way a straight line into them would.
   *
   *  A LOOP reaches across the seam for them, and skips the key that
   *  closes it: the last key repeats the first, so the key before the
   *  first is the one BEFORE the last, and the key after the last is the
   *  second. Reaching for the duplicate instead would flatten the curve
   *  at exactly the join a loop exists to hide. */
  [[nodiscard]] float spline(size_t index, size_t next, float unit) const {
    const size_t count = steps.size();
    const size_t beforeIndex =
        index > 0 ? index - 1 : (loop && count > 2 ? count - 2 : index);
    const size_t afterIndex =
        next + 1 < count ? next + 1 : (loop && count > 2 ? 1 : next);
    const float p0 = steps[beforeIndex].value;
    const float p1 = steps[index].value;
    const float p2 = steps[next].value;
    const float p3 = steps[afterIndex].value;
    const float t2 = unit * unit, t3 = t2 * unit;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * unit +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
  }
};

}  // namespace sigil::motion
