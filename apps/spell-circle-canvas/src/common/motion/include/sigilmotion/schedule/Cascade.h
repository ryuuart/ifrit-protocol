#pragma once

/** @file
 * A spread RESOLVED against the counts a frame actually has: the delay
 * ladder, the beat length, and the virtual span one master progress maps
 * onto. This is where a schedule becomes arithmetic.
 */

#include <sigilmotion/schedule/Spread.h>

#include <cstdint>
#include <vector>

namespace sigil::motion {

/** ONE BEAT OF A RESOLVED CASCADE, read back.
 *
 *  A cascade is otherwise an invisible remap: it numbers units, spreads
 *  them, and tells nobody. Anything that must travel WITH a cascade and is
 *  not one of its units — a playhead, a travelling underline, a caret, a
 *  per-unit meter — then has to restate `i · eachMs` in its own
 *  arithmetic, which stops agreeing with the engine the moment the cascade
 *  nests or takes a cue table. This is the schedule read back instead. */
struct Beat {
  /** The OUTER unit this beat belongs to. A nested cascade reports several
   *  beats sharing one `unitIndex`, one per inner unit inside it. */
  uint32_t unitIndex = 0;
  /** When this beat opens, in ms from the start of the master progress —
   *  the COMPOUNDED delay, outer plus inner, under a nested cascade. */
  float startMs = 0;
  /** This beat's own 0→1 at the master progress it was read at. Under a
   *  looping cascade it is the WRAPPED local time of the current cycle,
   *  and no cycle index rides beside it: the master is a phase mod 1 and
   *  carries no cycle count, so cycle identity lives with whoever steps
   *  the phase. */
  float localT = 0;
  /** The beat is running: it has begun and has not finished — under a
   *  looping cascade, mid-beat in its current cycle. */
  bool active = false;

  bool operator==(const Beat&) const = default;
};

/** The once-per-shape diagnostic behind a cue table that does not have one
 *  entry per unit: the tail either piles on the last cue or goes unread,
 *  and both are a table cut against the wrong run. */
void warnCueTableMismatch(size_t cueCount, size_t unitCount);

/** ONE CASCADE, resolved for a frame's unit counts: the delay ladder, the
 *  beat length, and the virtual span the master progress maps onto. Built
 *  per driven value per frame; localTime() is then a few adds per unit.
 *
 *  A pure function of a master float in [0,1] and two integer counts. It
 *  holds no clock: whoever owns the master decides what time is. */
struct Cascade {
  std::vector<float> outerOrder;  ///< outer unit → its place in the cascade
  std::vector<float> innerOrder;  ///< inner unit → the same, within a beat
  /** The author's start-time table at each level, in ms, or empty for the
   *  even ladder above. A table names delays outright, so the order, the
   *  spacing and the distribution curve have nothing left to say. */
  std::vector<float> outerCue, innerCue;
  choreograph::EaseFn outerDistribution, innerDistribution;
  float outerEach = 0;  ///< ms between outer starts
  float innerEach = 0;  ///< ms between inner starts
  float duration = 1;   ///< ms one unit's own motion lasts
  float beatMs = 1;     ///< ms one outer beat occupies
  /** Ms the master progress spans: the one-shot closing span, or the loop
   *  PERIOD when the cascade loops — either way, `master · totalMs` is the
   *  virtual time every local clock reads. */
  float totalMs = 1;
  /** The wrapping period (`Spread::loopMs`), or 0 for a one-shot cascade.
   *  When set, `totalMs` IS this period and localTime() folds each unit's
   *  elapsed time mod it, so every beat re-opens once per cycle. */
  float loopMs = 0;

  /** Resolves @p spec over @p outerCount units, each holding @p innerCount
   *  units of a nested spread. Reuses this object's vectors. */
  void build(const Spread& spec, uint32_t outerCount, uint32_t innerCount);
  /** When this unit's beat opens, in ms from the start of the master
   *  progress — the outer delay plus, under a nested cascade, the inner
   *  one. THE one place the schedule is arithmetic; everything that reports
   *  a start time reads it here. */
  [[nodiscard]] float startMs(uint32_t outerUnit, uint32_t innerUnit) const;
  /** The local 0→1 this unit sees at master progress @p master. Clamped at
   *  both ends for a one-shot cascade; a looping one folds the unit's
   *  elapsed time mod `loopMs` first, so the answer re-opens at 0 once per
   *  cycle and rests at 1 between its beat's close and its next opening. */
  [[nodiscard]] float localTime(float master, uint32_t outerUnit,
                                uint32_t innerUnit) const;
  /** The whole of one beat at @p master, for a host reading a schedule
   *  back rather than driving a value with it. `active` is the one fact
   *  the two accessors above do not state, and it is stated here so that
   *  every reader agrees what "running" means. */
  [[nodiscard]] Beat beat(float master, uint32_t outerUnit,
                          uint32_t innerUnit) const;
};

}  // namespace sigil::motion
