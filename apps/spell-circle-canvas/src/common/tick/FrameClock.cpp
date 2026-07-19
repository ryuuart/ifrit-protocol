#include "ifrittick/FrameClock.h"

#include <algorithm>
#include <chrono>

namespace ifrit::tick {

double FrameClock::tick(double nowSeconds) {
  if (m_lastNow < 0.0) {
    m_lastNow = nowSeconds;
    return 0.0;
  }
  double delta = nowSeconds - m_lastNow;
  m_lastNow = nowSeconds;
  if (m_paused)
    return 0.0;
  delta = std::clamp(delta, 0.0, m_options.maxDelta) * m_timeScale;
  m_elapsed += delta;
  return delta;
}

double FrameClock::tick() {
  return tick(std::chrono::duration<double>(
                  std::chrono::steady_clock::now().time_since_epoch())
                  .count());
}

void FrameClock::setPaused(bool paused) { m_paused = paused; }

} // namespace ifrit::tick
