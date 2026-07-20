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
