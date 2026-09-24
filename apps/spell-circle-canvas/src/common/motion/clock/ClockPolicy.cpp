/** @file
 * A frame clock under a policy: which frames move it, by how much, and
 * when its budget runs out.
 */

#include "sigilmotion/clock/ClockPolicy.h"

#include <chrono>
#include <cmath>

namespace sigil::motion {

namespace {

/** How far short of its budget a clock may stand and have met it. */
constexpr double kBudgetTolerance = 1e-9;

}  // namespace

void PolicyClock::setPolicy(ClockPolicy policy,
                            std::optional<double> budgetSeconds) {
  m_policy = policy;
  m_expired = false;
  m_waiting = false;
  m_budgetEnds.reset();
  if (budgetSeconds && std::isfinite(*budgetSeconds) && *budgetSeconds >= 0.0)
    m_budgetEnds = m_elapsed + *budgetSeconds;
}

double PolicyClock::frame(double nowSeconds, bool arriving) {
  m_waiting = m_policy == ClockPolicy::PauseWhileLoading && arriving;
  const bool moves = m_policy == ClockPolicy::Wall ||
                     (m_policy == ClockPolicy::PauseWhileLoading && !arriving);
  if (moves) return account(m_wall.tick(nowSeconds));
  // A frame that moves nothing still takes its reading, so the wall's
  // next frame measures from here: the frame clock's own pause consumes
  // the reading without adding it.
  const bool held = m_wall.paused();
  m_wall.setPaused(true);
  (void)m_wall.tick(nowSeconds);
  m_wall.setPaused(held);
  return account(0.0);
}

double PolicyClock::frame(bool arriving) {
  return frame(std::chrono::duration<double>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count(),
               arriving);
}

double PolicyClock::step(double deltaSeconds) {
  m_waiting = false;
  const bool moves = m_policy == ClockPolicy::Advance && !m_wall.paused() &&
                     std::isfinite(deltaSeconds) && deltaSeconds > 0.0;
  return account(moves ? deltaSeconds : 0.0);
}

bool PolicyClock::still() const {
  return m_wall.paused() || m_policy == ClockPolicy::Pause || m_waiting;
}

void PolicyClock::restart() {
  if (m_budgetEnds) *m_budgetEnds -= m_elapsed;
  m_elapsed = 0.0;
  m_frames = 0;
  m_expired = false;
}

std::optional<double> PolicyClock::budgetRemaining() const {
  if (!m_budgetEnds) return std::nullopt;
  return *m_budgetEnds - m_elapsed;
}

double PolicyClock::account(double delta) {
  m_elapsed += delta;
  ++m_frames;
  m_expired = false;
  // A budget met by a sum of steps is met to within what the sum rounds
  // by: sixty steps of a sixtieth fall short of one by a few ulps.
  if (m_budgetEnds && m_elapsed >= *m_budgetEnds - kBudgetTolerance) {
    m_expired = true;
    m_budgetEnds.reset();
  }
  return delta;
}

}  // namespace sigil::motion
