/** @file
 * The ticker's step: the timeline and the steppables advanced, the
 * derived Outputs re-evaluated in dependency order, and the activity
 * report.
 */

#include "sigilmotion/clock/Ticker.h"

#include <cmath>
#include <cstdio>

namespace sigil::motion {

Ticker::Ticker() {
  // Finished motions leave the timeline so active() settles to false
  // without bookkeeping.
  m_timeline.setDefaultRemoveOnFinish(true);
}

void Ticker::addStep(std::function<bool(double, double)> steppable) {
  m_steppables.push_back(std::move(steppable));
}

void Ticker::addFixedStep(double rate, std::function<bool()> steppable, int maxCatchUp,
                          choreograph::Output<float>* alphaOut,
                          FixedStatus* statusOut) {
  if (rate <= 0.0 || !steppable) return;
  add([rate, maxCatchUp, alphaOut, statusOut, steppable = std::move(steppable), total = 0.0,
       ran = 0.0](double deltaSeconds) mutable {
    total += deltaSeconds;
    // From TOTAL elapsed time, not a running accumulator: an accumulator
    // compared against a step slips one comparison over a long pre-roll,
    // so the same capture landed on either side of a step boundary
    // depending on the draw rate.
    // The epsilon absorbs accumulated float error — summing 1/144 a
    // hundred and forty-four times lands a hair under 1.0, and without it
    // the last step of a whole second goes missing.
    const double want = std::floor(total * rate + 1e-9);
    double due = want - ran;
    bool clamped = false;
    if (due > (double)maxCatchUp) {
      // Beyond the budget the backlog is DISCARDED rather than carried:
      // carrying it makes the next frame longer, which grows the backlog,
      // which is the spiral of death. Running slow for one frame is the
      // correct failure.
      due = (double)maxCatchUp;
      clamped = true;
    }
    bool alive = true;
    int steps = 0;
    for (; steps < (int)due; ++steps) {
      alive = steppable();
      if (!alive) break;
    }
    ran = want;  // discards anything the clamp skipped
    if (alphaOut) *alphaOut = (float)(total * rate - want);
    if (statusOut) {
      statusOut->stepsRun = steps;
      statusOut->clamped = clamped;
    }
    return alive;
  });
}

bool Ticker::derive(choreograph::Output<float>* output, const Bound& chain) {
  const BoundFloat& map = chain.value();
  const auto refuse = [](const char* why) {
    std::fprintf(stderr, "[sigilmotion] Ticker::derive refused: %s\n", why);
    return false;
  };
  if (!output || !map.source)
    return refuse(
        "a derivation needs both a destination Output and a "
        "bind(&source) chain");
  if (map.source == output)
    return refuse(
        "an Output cannot derive from itself — the chain would "
        "compound its own last answer every tick");
  for (const Derivation& d : m_derivations) {
    // ONE LEVEL ONLY (see the header): no Output may be both a
    // derivation's destination and a derivation's source, in either
    // registration order — phase two has no topological order, so a
    // chain of two would read one frame stale, silently.
    if (d.output == map.source)
      return refuse(
          "the source is itself a derived Output — derivations "
          "are one level only; derive from the original schedule "
          "instead");
    if (d.map.source == output)
      return refuse(
          "the destination already feeds another derivation — "
          "derivations are one level only; derive both from the "
          "original schedule instead");
    if (d.output == output)
      return refuse(
          "the destination is already written by a derivation — "
          "two writers of one cell would silently trade last-one-"
          "wins");
  }
  m_derivations.push_back({output, map});
  // Applied once at registration, so output is correct before the first tick.
  *output = map.apply(map.source->value());
  return true;
}

bool Ticker::tick(double deltaSeconds) {
  m_elapsed += deltaSeconds;
  // PHASE ONE — the sources: the timeline's Motions, then every
  // steppable, in registration order.
  m_timeline.step(deltaSeconds);
  for (auto it = m_steppables.begin(); it != m_steppables.end();) {
    if ((*it)(deltaSeconds, m_elapsed))
      ++it;
    else
      it = m_steppables.erase(it);
  }
  // PHASE TWO — the derivations. Every source has already been stepped
  // this frame, so a derivation NEVER reads a stale value, whatever the
  // registration order; and the one-level rule (enforced in derive())
  // means no derivation reads another's destination, so order within
  // this phase cannot matter either.
  for (const Derivation& d : m_derivations)
    *d.output = d.map.apply(d.map.source->value());
  return active();
}

bool Ticker::active() const {
  return !m_timeline.empty() || !m_steppables.empty();
}

}  // namespace sigil::motion
