#include <cmath>
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

bool Ticker::tick(double deltaSeconds) {
  m_timeline.step(deltaSeconds);
  for (auto it = m_steppables.begin(); it != m_steppables.end();) {
    if ((*it)(deltaSeconds))
      ++it;
    else
      it = m_steppables.erase(it);
  }
  return active();
}

bool Ticker::active() const {
  return !m_timeline.empty() || !m_steppables.empty();
}

} // namespace sigil::motion
