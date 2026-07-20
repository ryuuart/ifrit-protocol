#pragma once

/** @file
 * Frame-timing helpers shared by the gallery, demo, and bench consumers.
 */

#include <chrono>

namespace sigil::weave::kit {

/** Converts a steady-clock duration to fractional microseconds — the unit
 *  the HUDs and reports use. */
[[nodiscard]] inline double
toMicroseconds(std::chrono::steady_clock::duration duration) {
  return std::chrono::duration<double, std::micro>(duration).count();
}

/** Brackets a measured region: started on construction, read via
 *  microseconds(). Handy inside relayout lambdas, where the measured work
 *  only happens on cache misses. */
class Stopwatch {
public:
  using Clock = std::chrono::steady_clock;

  /** Returns the time elapsed since construction or restart(). */
  [[nodiscard]] double microseconds() const {
    return toMicroseconds(Clock::now() - m_start);
  }

  /** Restarts the measured region at now. */
  void restart() { m_start = Clock::now(); }

private:
  Clock::time_point m_start = Clock::now();
};

} // namespace sigil::weave::kit
