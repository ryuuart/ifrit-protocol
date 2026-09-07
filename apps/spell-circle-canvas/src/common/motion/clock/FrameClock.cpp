/** @file
 * The frame clock's step, from a wall-clock reading or from a delta the
 * caller states: the first delta and any stall clamped, the pause
 * honoured, the time scale applied.
 */

#include "sigilmotion/clock/FrameClock.h"

#include <algorithm>
#include <chrono>

namespace sigil::motion {

double FrameClock::tick(double nowSeconds) {
  if (m_lastNow < 0.0) {
    m_lastNow = nowSeconds;
    return 0.0;
  }
  const double delta = nowSeconds - m_lastNow;
  m_lastNow = nowSeconds;
  return advance(delta);
}

double FrameClock::advance(double deltaSeconds) {
  if (m_paused) return 0.0;
  const double delta =
      std::clamp(deltaSeconds, 0.0, m_options.maxDelta) * m_timeScale;
  m_elapsed += delta;
  return delta;
}

double FrameClock::tick() {
  return tick(std::chrono::duration<double>(
                  std::chrono::steady_clock::now().time_since_epoch())
                  .count());
}

void FrameClock::setPaused(bool paused) { m_paused = paused; }

}  // namespace sigil::motion
