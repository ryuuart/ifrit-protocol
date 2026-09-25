#pragma once

/** @file
 * @ingroup motion-schedule
 *
 * A stagger RESOLVED against the counts a frame actually has: the delay
 * ladder, the beat length, and the span one master progress maps onto.
 * This is where a schedule becomes arithmetic — the timing a text track's
 * units and a host reading starts by hand run through.
 */

#include <sigilmotion/schedule/Stagger.h>
#include <sigilmotion/time/Duration.h>

#include <cstdint>
#include <vector>

namespace sigil::motion {

/** ONE BEAT OF A RESOLVED SCHEDULE, read back.
 *
 *  A schedule is otherwise an invisible remap: it numbers units, spreads
 *  them, and tells nobody. Anything that must travel WITH one and is not
 *  one of its units — a playhead, a travelling underline, a caret, a
 *  per-unit meter — would then restate the delay arithmetic, which stops
 *  agreeing with the engine the moment the schedule nests or takes a cue
 *  table. This is the schedule read back instead. */
struct Beat {
  /** The OUTER unit this beat belongs to. A nested schedule reports
   *  several beats sharing one `unitIndex`, one per inner unit inside it. */
  uint32_t unitIndex = 0;
  /** When this beat opens, from the start of the master progress — the
   *  COMPOUNDED delay, outer plus inner, under a nested schedule. */
  Duration start{};
  /** This beat's own 0→1 at the master progress it was read at. Under a
   *  looping schedule it is the WRAPPED progress of the current cycle. */
  float localProgress = 0;
  /** The beat is running: it has begun and has not finished. */
  bool running = false;

  bool operator==(const Beat&) const = default;
};

/** HOW A RUN OF UNITS SHARES ONE PROGRESS: each unit's delay, its own
 *  motion's length, whether the whole thing loops, and a second stagger
 *  inside every beat. What a text track carries. */
struct Timing {
  /** When each unit starts: `stagger(30ms)`, `stagger({0ms, 620ms})`,
   *  `cues({…})` or a plain duration every unit shares. */
  Staggered<Duration> delay = stagger(std::chrono::milliseconds(30));
  /** How long one unit's own motion lasts. Under `within`, the innermost
   *  unit's. */
  Duration duration = std::chrono::milliseconds(450);
  /** Above zero the schedule LOOPS: every beat re-opens once per period,
   *  offset by its start, and one sweep of the master 0→1 is one period.
   *  Zero is the one-shot schedule. */
  Duration loop{};
  /** A second stagger inside every beat — words, then the letters inside
   *  each word. A beat then lasts exactly as long as the inner run needs;
   *  nothing past one level of nesting is read. */
  std::optional<Staggered<Duration>> within;

  bool operator==(const Timing&) const = default;

  /** THE SPAN one master progress maps onto when this timing numbers
   *  @p count units, each holding @p innerCount inner units — the moment
   *  the last beat closes, or the period of a looping schedule. What a
   *  progress transition's duration should be for the schedule to run at
   *  its authored times. */
  [[nodiscard]] Duration span(uint32_t count, uint32_t innerCount = 1) const;
};

/** The once-per-shape diagnostic behind a cue table that does not have one
 *  entry per unit: the tail either piles on the last cue or goes unread,
 *  and both are a table cut against the wrong run. */
void warnCueTableMismatch(size_t cueCount, size_t unitCount);

/** ONE SCHEDULE, resolved for a frame's unit counts: the delay ladder, the
 *  beat length, and the span the master progress maps onto. Built per
 *  driven value per frame; `localProgress()` is then a few adds per unit.
 *
 *  A pure function of a master float in [0, 1] and two integer counts. It
 *  holds no clock: whoever owns the master decides what time is. Its
 *  arithmetic runs in float milliseconds. */
struct Schedule {
  std::vector<float> outerOrder;  ///< outer unit → its step in the run
  std::vector<float> innerOrder;  ///< inner unit → the same, within a beat
  /** The author's start-time table at each level, in ms, or empty for the
   *  even ladder above. */
  std::vector<float> outerCue, innerCue;
  Easing outerDistribution, innerDistribution;
  float outerEach = 0;   ///< ms between outer starts
  float innerEach = 0;   ///< ms between inner starts
  float outerStart = 0;  ///< ms added before every outer start
  float innerStart = 0;  ///< ms added before every inner start
  float duration = 1;    ///< ms one unit's own motion lasts
  float beatMs = 1;      ///< ms one outer beat occupies
  /** Ms the master progress spans: the one-shot closing span, or the loop
   *  PERIOD when the schedule loops. */
  float totalMs = 1;
  /** The wrapping period in ms, or 0 for a one-shot schedule. */
  float loopMs = 0;

  Schedule() = default;
  /** Resolves @p timing over @p outerCount units, each holding
   *  @p innerCount units of a nested stagger. */
  Schedule(const Timing& timing, uint32_t outerCount, uint32_t innerCount = 1) {
    build(timing, outerCount, innerCount);
  }
  /** The same, reusing this object's vectors. */
  void build(const Timing& timing, uint32_t outerCount, uint32_t innerCount);

  /** The span the master progress maps onto. */
  [[nodiscard]] Duration total() const;
  /** When this unit's beat opens, from the start of the master progress —
   *  the outer delay plus, under a nested schedule, the inner one. */
  [[nodiscard]] Duration start(uint32_t outerUnit, uint32_t innerUnit = 0) const;
  /** The local 0→1 this unit sees at master progress @p master. Clamped at
   *  both ends for a one-shot schedule; a looping one folds the unit's
   *  elapsed time by the period first, so the answer re-opens at 0 once per
   *  cycle and rests at 1 between its beat's close and its next opening. */
  [[nodiscard]] float localProgress(float master, uint32_t outerUnit,
                                    uint32_t innerUnit = 0) const;
  /** The whole of one beat at @p master, for a host reading a schedule
   *  back rather than driving a value with it. */
  [[nodiscard]] Beat beat(float master, uint32_t outerUnit,
                          uint32_t innerUnit = 0) const;

 private:
  /** The start in ms, the one place the schedule is arithmetic. */
  [[nodiscard]] float startMs(uint32_t outerUnit, uint32_t innerUnit) const;
};

}  // namespace sigil::motion
