#include <cmath>
#include <cstdio>
#include "sigilmotion/Ticker.h"

namespace sigil::motion {

Ticker::Ticker() {
  // Finished motions leave the timeline so active() settles to false
  // without bookkeeping.
  m_timeline.setDefaultRemoveOnFinish(true);
}

void Ticker::add(std::function<bool(double)> steppable) {
  m_steppables.push_back(std::move(steppable));
}

void Ticker::addFixed(double hz, std::function<bool()> fn, int maxCatchUp,
                      choreograph::Output<float> *alphaOut,
                      FixedStatus *statusOut) {
  if (hz <= 0.0 || !fn)
    return;
  add([hz, maxCatchUp, alphaOut, statusOut, fn = std::move(fn), total = 0.0,
       ran = 0.0](double dt) mutable {
    total += dt;
    // From TOTAL elapsed time, not a running accumulator: an accumulator
    // compared against a step slips one comparison over a long pre-roll,
    // so the same capture landed on either side of a step boundary
    // depending on the draw rate.
    // The epsilon absorbs accumulated float error — summing 1/144 a
    // hundred and forty-four times lands a hair under 1.0, and without it
    // the last step of a whole second goes missing.
    const double want = std::floor(total * hz + 1e-9);
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
      alive = fn();
      if (!alive)
        break;
    }
    ran = want; // discards anything the clamp skipped
    if (alphaOut)
      *alphaOut = (float)(total * hz - want);
    if (statusOut) {
      statusOut->stepsRun = steps;
      statusOut->clamped = clamped;
    }
    return alive;
  });
}

bool Ticker::derive(choreograph::Output<float> *dst, const Bound &chain) {
  const BoundFloat &map = chain.value();
  const auto refuse = [](const char *why) {
    std::fprintf(stderr, "[sigilmotion] Ticker::derive refused: %s\n", why);
    return false;
  };
  if (!dst || !map.source)
    return refuse("a derivation needs both a destination Output and a "
                  "bind(&source) chain");
  if (map.source == dst)
    return refuse("an Output cannot derive from itself — the chain would "
                  "compound its own last answer every tick");
  for (const Derivation &d : m_derivations) {
    // ONE LEVEL ONLY (see the header): no Output may be both a
    // derivation's destination and a derivation's source, in either
    // registration order — phase two has no topological order, so a
    // chain of two would read one frame stale, silently.
    if (d.dst == map.source)
      return refuse("the source is itself a derived Output — derivations "
                    "are one level only; derive from the original schedule "
                    "instead");
    if (d.map.source == dst)
      return refuse("the destination already feeds another derivation — "
                    "derivations are one level only; derive both from the "
                    "original schedule instead");
    if (d.dst == dst)
      return refuse("the destination is already written by a derivation — "
                    "two writers of one cell would silently trade last-one-"
                    "wins");
  }
  m_derivations.push_back({dst, map});
  // Applied once at registration, so dst is correct before the first tick.
  *dst = map.apply(map.source->value());
  return true;
}

bool Ticker::tick(double deltaSeconds) {
  m_elapsed += deltaSeconds;
  // PHASE ONE — the sources: the timeline's Motions, then every
  // steppable, in registration order.
  m_timeline.step(deltaSeconds);
  for (auto it = m_steppables.begin(); it != m_steppables.end();) {
    if ((*it)(deltaSeconds))
      ++it;
    else
      it = m_steppables.erase(it);
  }
  // PHASE TWO — the derivations. Every source has already been stepped
  // this frame, so a derivation NEVER reads a stale value, whatever the
  // registration order; and the one-level rule (enforced in derive())
  // means no derivation reads another's destination, so order within
  // this phase cannot matter either.
  for (const Derivation &d : m_derivations)
    *d.dst = d.map.apply(d.map.source->value());
  return active();
}

bool Ticker::active() const {
  return !m_timeline.empty() || !m_steppables.empty();
}

} // namespace sigil::motion
