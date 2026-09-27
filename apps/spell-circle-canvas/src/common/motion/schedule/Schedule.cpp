/** @file
 * The schedule arithmetic: resolving a timing against a frame's counts,
 * and the two questions a resolved schedule answers per unit — when its
 * beat opens, and where inside it the master progress currently stands.
 */

#include <sigilmotion/schedule/Schedule.h>

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace sigil::motion {

namespace {

/** A duration as the float milliseconds the schedule's arithmetic runs in. */
float milliseconds(Duration length) { return (float)(length.count() * 1000.0); }

/** The table entry unit `index` reads. Past the end it is the LAST entry:
 *  a short table piles its tail on one beat, which is visible, rather than
 *  extrapolating times its author never wrote. */
float cueAt(const std::vector<float>& table, uint32_t index) {
  return table[std::min<size_t>(index, table.size() - 1)];
}

/** The latest start any of `count` units reads out of `table` — what the
 *  master progress has to span for the last beat to open. A table is not
 *  required to ascend, so this is a max and not the final entry. */
float lastCueMs(const std::vector<float>& table, uint32_t count) {
  float latest = 0.0f;
  const size_t read = std::min<size_t>(count, table.size());
  for (size_t i = 0; i < read; ++i) latest = std::max(latest, table[i]);
  return latest;
}

/** ONE LEVEL of a run: its steps, its spacing, its table, its curve and
 *  what is added before every start. A step delay is the fixed spacing;
 *  a range divides its span across however many units there are; a table
 *  states every start outright. */
void level(const Staggered<Duration>& delay, uint32_t count,
           std::vector<float>& order, float& each, float& start,
           std::vector<float>& cue, Easing& distribution) {
  const StaggerOptions<Duration>& options = delay.options();
  cue.clear();
  distribution = nullptr;
  start = 0.0f;
  each = 0.0f;
  switch (delay.form()) {
    case Staggered<Duration>::Form::Value:
      order.assign(count, 0.0f);
      start = milliseconds(delay.firstValue());
      return;
    case Staggered<Duration>::Form::Table:
      order.clear();
      for (const Duration entry : delay.table()) cue.push_back(milliseconds(entry));
      if (!cue.empty() && cue.size() != count) warnCueTableMismatch(cue.size(), count);
      if (cue.empty()) cue.push_back(0.0f);
      return;
    case Staggered<Duration>::Form::Each:
      each = std::max(milliseconds(delay.firstValue()), 0.0f);
      start = milliseconds(options.start);
      break;
    case Staggered<Duration>::Form::Range: {
      const float span = milliseconds(delay.lastValue() - delay.firstValue());
      each = count > 1 ? span / (float)(count - 1) : 0.0f;
      start = milliseconds(options.start + delay.firstValue());
      break;
    }
  }
  staggerSteps(options.from, options.grid, options.axis, options.seed,
               options.rankBy, options.reverse, count, order);
  distribution = options.ease;
}

}  // namespace

void warnCueTableMismatch(size_t cueCount, size_t unitCount) {
  // Once per distinct shape: a schedule is rebuilt every frame, and one
  // mistyped table would otherwise scroll the same line past its author
  // forever. Distinct shapes still each get their say.
  static thread_local boost::unordered_flat_set<uint64_t> seen;
  const uint64_t key = ((uint64_t)cueCount << 32u) | (uint32_t)unitCount;
  if (!seen.insert(key).second) return;
  std::fprintf(stderr,
               "SigilMotion: a cue table of %zu times against %zu units — "
               "%s\n",
               cueCount, unitCount,
               cueCount < unitCount
                   ? "every unit past the table's end starts at its last time"
                   : "the times past the last unit are never read");
}

Duration Timing::span(uint32_t count, uint32_t innerCount) const {
  // The one arithmetic: the same resolved body a host runs per frame,
  // handed its counts directly instead of a laid-out run.
  return Schedule(*this, count, innerCount).total();
}

void Schedule::build(const Timing& timing, uint32_t outerCount,
                     uint32_t innerCount) {
  duration = std::max(milliseconds(timing.duration), 1.0f);
  const uint32_t outer = std::max(outerCount, 1u);
  level(timing.delay, outer, outerOrder, outerEach, outerStart, outerCue,
        outerDistribution);
  if (timing.within) {
    const uint32_t inner = std::max(innerCount, 1u);
    level(*timing.within, inner, innerOrder, innerEach, innerStart, innerCue,
          innerDistribution);
    // A NESTED schedule owns the beat: a beat is exactly as long as the
    // inner ladder needs.
    beatMs = duration + innerStart +
             (innerCue.empty() ? innerEach * (float)(inner - 1)
                               : lastCueMs(innerCue, inner));
  } else {
    innerOrder.clear();
    innerCue.clear();
    innerEach = 0.0f;
    innerStart = 0.0f;
    beatMs = duration;
    innerDistribution = nullptr;
  }
  totalMs = beatMs + outerStart +
            (outerCue.empty() ? outerEach * (float)(outer - 1)
                              : lastCueMs(outerCue, outer));
  // ONE loop for the whole schedule. Looping, the master maps onto the
  // PERIOD rather than the one-shot closing span — one sweep 0→1 is one
  // cycle — so totalMs IS the period and localProgress() folds each unit's
  // elapsed time by it.
  loopMs = timing.loop ? beatMs + std::max(milliseconds(timing.loopDelay), 0.0f)
                       : 0.0f;
  alternate = timing.loop && timing.alternate;
  if (loopMs > 0) totalMs = loopMs;
}

Duration Schedule::total() const {
  return std::chrono::duration<double, std::milli>(totalMs);
}

float Schedule::startMs(uint32_t outerUnit, uint32_t innerUnit) const {
  // Without a distribution curve the delay is the plain product — NOT the
  // same product routed through a normalise-and-rescale, which would
  // differ in the last bit and move every pixel of a settled reveal.
  const auto delayOf = [](const std::vector<float>& order, uint32_t index,
                          float each, const Easing& shape) {
    if (order.empty()) return 0.0f;
    const uint32_t clamped = std::min<uint32_t>(index, order.size() - 1);
    if (!shape) return order[clamped] * each;
    const float last = order.size() > 1 ? (float)(order.size() - 1) : 1.0f;
    return shape(order[clamped] / last) * (each * last);
  };
  // A table states the delay; the ladder computes one.
  const float outer =
      outerCue.empty()
          ? delayOf(outerOrder, outerUnit, outerEach, outerDistribution)
          : cueAt(outerCue, outerUnit);
  const float inner =
      innerCue.empty()
          ? delayOf(innerOrder, innerUnit, innerEach, innerDistribution)
          : cueAt(innerCue, innerUnit);
  // A start of zero adds nothing, so a plain ladder is the plain sum.
  return (outerStart != 0.0f ? outerStart + outer : outer) +
         (innerStart != 0.0f ? innerStart + inner : inner);
}

Duration Schedule::start(uint32_t outerUnit, uint32_t innerUnit) const {
  return std::chrono::duration<double, std::milli>(startMs(outerUnit, innerUnit));
}

float Schedule::localProgress(float master, uint32_t outerUnit,
                              uint32_t innerUnit) const {
  if (loopMs > 0) {
    // The wrapping beat: elapsed time since this unit's start, folded into
    // [0, loopMs). The fold re-opens the beat once per cycle, keeps master
    // 0 and master 1 the same instant, and puts every unit somewhere in
    // its cycle from the first frame. Past its duration a beat rests at 1
    // until the fold brings it back to 0.
    const float since = master * totalMs - startMs(outerUnit, innerUnit);
    float elapsed = std::fmod(since, loopMs);
    if (elapsed < 0) elapsed += loopMs;
    const float local = std::clamp(elapsed / duration, 0.0f, 1.0f);
    // Alternating, an odd cycle runs the beat backwards.
    if (alternate && ((int64_t)std::floor(since / loopMs) & 1) != 0)
      return 1.0f - local;
    return local;
  }
  return std::clamp(
      (master * totalMs - startMs(outerUnit, innerUnit)) / duration, 0.0f,
      1.0f);
}

Beat Schedule::beat(float master, uint32_t outerUnit, uint32_t innerUnit) const {
  Beat out;
  out.unitIndex = outerUnit;
  out.start = start(outerUnit, innerUnit);
  out.localProgress = localProgress(master, outerUnit, innerUnit);
  // A beat that has begun and not finished. The clamped progress reads 0
  // both before the beat opens and exactly as it does, and 1 for the whole
  // of the rest of the schedule's life.
  out.running = out.localProgress > 0.0f && out.localProgress < 1.0f;
  return out;
}

}  // namespace sigil::motion
