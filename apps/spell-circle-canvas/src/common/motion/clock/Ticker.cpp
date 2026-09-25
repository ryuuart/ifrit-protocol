/** @file
 * The ticker's step: the motions and the steppables advanced, and the
 * activity report.
 */

#include "sigilmotion/clock/Ticker.h"

#include <cmath>
#include <cstdio>

namespace sigil::motion {

void Ticker::run(std::shared_ptr<detail::Stepped> motion) {
  if (motion) m_motions.push_back(std::move(motion));
}

void Ticker::addStep(std::function<bool(double, double)> steppable) {
  m_steppables.push_back(std::move(steppable));
}

void Ticker::addFixedStep(double rate, std::function<bool()> steppable, int maxCatchUp,
                          float* alphaOut,
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

bool Ticker::tick(double deltaSeconds) {
  m_elapsed += deltaSeconds;
  // The motions first, in the order they were started, then every
  // steppable in registration order: a steppable reading a moving value
  // reads this frame's number. A finished motion leaves the list.
  for (auto it = m_motions.begin(); it != m_motions.end();) {
    if ((*it)->advance(deltaSeconds))
      ++it;
    else
      it = m_motions.erase(it);
  }
  for (auto it = m_steppables.begin(); it != m_steppables.end();) {
    if ((*it)(deltaSeconds, m_elapsed))
      ++it;
    else
      it = m_steppables.erase(it);
  }
  return active();
}

bool Ticker::active() const {
  return !m_motions.empty() || !m_steppables.empty();
}

}  // namespace sigil::motion
