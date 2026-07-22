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

void Ticker::addFixed(double hz, std::function<bool()> fn, int maxCatchUp) {
  const double step = hz > 0.0 ? 1.0 / hz : 0.0;
  if (step <= 0.0 || !fn)
    return;
  add([step, maxCatchUp, fn = std::move(fn),
       accumulator = 0.0](double dt) mutable {
    accumulator += dt;
    // A frame may run at most maxCatchUp steps. Beyond that the backlog
    // is DISCARDED rather than carried: carrying it makes the next frame
    // longer, which grows the backlog, which is the spiral of death. The
    // sim running slow for one frame is the correct failure.
    int budget = maxCatchUp;
    bool alive = true;
    while (accumulator >= step && budget-- > 0) {
      accumulator -= step;
      alive = fn();
      if (!alive)
        return false;
    }
    if (accumulator >= step)
      accumulator = 0.0;
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
