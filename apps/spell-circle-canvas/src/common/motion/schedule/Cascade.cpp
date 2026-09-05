/** @file
 * The cascade arithmetic: resolving a spread against a frame's counts,
 * and the two questions a resolved cascade answers per unit — when its
 * beat opens, and where inside it the master progress currently stands.
 */

#include <sigilmotion/schedule/Cascade.h>
#include <sigilmotion/schedule/Order.h>

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cmath>
#include <cstdio>

namespace sigil::motion {

namespace {
/** The per-unit spacing this cascade asks for, in ms. Amount-mode divides
 *  a fixed total across however many units there are; otherwise the
 *  spacing is fixed and the total grows. */
float spacingMs(const Spread& spec, uint32_t count) {
  if (spec.amountMs > 0 && count > 1) return spec.amountMs / (float)(count - 1);
  return std::max(spec.eachMs, 0.0f);
}

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
}  // namespace

void warnCueTableMismatch(size_t cueCount, size_t unitCount) {
  // Once per distinct shape: a cascade is rebuilt every frame, and one
  // mistyped table would otherwise scroll the same line past its author
  // forever. Distinct shapes still each get their say, because two
  // cascades can be wrong in two different ways.
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

void Cascade::build(const Spread& spec, uint32_t outerCount,
                    uint32_t innerCount) {
  duration = std::max(spec.durationMs, 1.0f);
  const uint32_t outer = std::max(outerCount, 1u);
  if (!spec.rankBy.empty() && spec.cueMs.empty())
    cascadeRanks(spec.rankBy, outer, outerOrder);
  else
    cascadeOrder(spec.from, outer, spec.seed, outerOrder);
  outerEach = spacingMs(spec, outer);
  outerCue = spec.cueMs;
  if (!outerCue.empty() && outerCue.size() != outer)
    warnCueTableMismatch(outerCue.size(), outer);

  if (spec.inner) {
    const uint32_t inner = std::max(innerCount, 1u);
    if (!spec.inner->rankBy.empty() && spec.inner->cueMs.empty())
      cascadeRanks(spec.inner->rankBy, inner, innerOrder);
    else
      cascadeOrder(spec.inner->from, inner, spec.inner->seed, innerOrder);
    innerEach = spacingMs(*spec.inner, inner);
    innerCue = spec.inner->cueMs;
    if (!innerCue.empty() && innerCue.size() != inner)
      warnCueTableMismatch(innerCue.size(), inner);
    // A NESTED cascade owns the beat: its own duration is what one unit's
    // motion lasts, and a beat is exactly as long as the inner ladder
    // needs. The outer durationMs would otherwise be a second, conflicting
    // statement about the same span.
    duration = std::max(spec.inner->durationMs, 1.0f);
    beatMs = duration + (innerCue.empty() ? innerEach * (float)(inner - 1)
                                          : lastCueMs(innerCue, inner));
    innerDistribution = spec.inner->distribution;
  } else {
    innerOrder.clear();
    innerCue.clear();
    innerEach = 0.0f;
    beatMs = duration;
    innerDistribution = nullptr;
  }
  outerDistribution = spec.distribution;
  totalMs = beatMs + (outerCue.empty() ? outerEach * (float)(outer - 1)
                                       : lastCueMs(outerCue, outer));
  // ONE loop for the whole cascade, read off the OUTER spread: a nested
  // loopMs would be a second, conflicting period over the same clock.
  // Looping, the master maps onto the PERIOD rather than the one-shot
  // closing span — one sweep 0→1 is one cycle — so totalMs IS the period
  // and localTime() folds each unit's elapsed time mod it.
  loopMs = std::max(spec.loopMs, 0.0f);
  if (loopMs > 0) totalMs = loopMs;
}

float Cascade::startMs(uint32_t outerUnit, uint32_t innerUnit) const {
  // Without a distribution curve the delay is the plain product the flat
  // cascade has always been — NOT the same product routed through a
  // normalise-and-rescale, which would differ in the last bit and move
  // every pixel of a settled reveal.
  const auto delayOf = [](const std::vector<float>& order, uint32_t index,
                          float each, const choreograph::EaseFn& shape) {
    if (order.empty()) return 0.0f;
    const uint32_t clamped = std::min<uint32_t>(index, order.size() - 1);
    if (!shape) return order[clamped] * each;
    const float last = order.size() > 1 ? (float)(order.size() - 1) : 1.0f;
    return shape(order[clamped] / last) * (each * last);
  };
  // A table states the delay; the ladder computes one. Nothing else about
  // the cascade changes between the two.
  return (outerCue.empty()
              ? delayOf(outerOrder, outerUnit, outerEach, outerDistribution)
              : cueAt(outerCue, outerUnit)) +
         (innerCue.empty()
              ? delayOf(innerOrder, innerUnit, innerEach, innerDistribution)
              : cueAt(innerCue, innerUnit));
}

float Cascade::localTime(float master, uint32_t outerUnit,
                         uint32_t innerUnit) const {
  if (loopMs > 0) {
    // The wrapping beat: elapsed time since this unit's start, folded into
    // [0, loopMs). The fold is what re-opens the beat once per cycle, keeps
    // master 0 and master 1 the same instant (so a wrapping bound phase
    // crosses its own seam with no jump), and puts every unit somewhere in
    // its cycle from the first frame — a start past the period lands at
    // start mod period rather than waiting. Past its duration a beat rests
    // at 1 until the fold brings it back to 0.
    float elapsed =
        std::fmod(master * totalMs - startMs(outerUnit, innerUnit), loopMs);
    if (elapsed < 0) elapsed += loopMs;
    return std::clamp(elapsed / duration, 0.0f, 1.0f);
  }
  return std::clamp(
      (master * totalMs - startMs(outerUnit, innerUnit)) / duration, 0.0f,
      1.0f);
}

Beat Cascade::beat(float master, uint32_t outerUnit, uint32_t innerUnit) const {
  Beat out;
  out.unitIndex = outerUnit;
  out.startMs = startMs(outerUnit, innerUnit);
  out.localT = localTime(master, outerUnit, innerUnit);
  // A beat that has begun and not finished. The clamped local time reads 0
  // both before the beat opens and exactly as it does, and 1 for the whole
  // of the rest of the cascade's life.
  out.active = out.localT > 0.0f && out.localT < 1.0f;
  return out;
}

}  // namespace sigil::motion
