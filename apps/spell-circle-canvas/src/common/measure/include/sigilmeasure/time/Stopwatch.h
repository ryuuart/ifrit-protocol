#pragma once

/** @file
 * A steady-clock stopwatch reading elapsed milliseconds, and a scope
 * guard that writes its elapsed time into a double when it goes out of
 * scope.
 */

#include <chrono>

namespace sigil::measure {

/** Milliseconds on the steady clock since construction or the last
 *  reset(). Steady rather than system time, so a clock adjustment
 *  mid-measure cannot produce a negative or absurd reading. For the time
 *  between consecutive phases of one span, see `Laps`. */
class Stopwatch {
 public:
  using Clock = std::chrono::steady_clock;

  Stopwatch() : m_start(Clock::now()) {}

  double elapsedMs() const {
    return std::chrono::duration<double, std::milli>(Clock::now() - m_start)
        .count();
  }
  void reset() { m_start = Clock::now(); }

 private:
  Clock::time_point m_start;
};

/** Writes the milliseconds a scope took into the double it was given,
 *  at scope exit — one line to time a block without laying marks by hand:
 *
 *      double layoutMs;
 *      { ScopedMs timed(layoutMs); layout(); }
 *
 *  The target is ASSIGNED, not accumulated, so a block entered twice
 *  reports its last run. */
class ScopedMs {
 public:
  explicit ScopedMs(double& out) : m_out(out) {}
  ScopedMs(const ScopedMs&) = delete;
  ScopedMs& operator=(const ScopedMs&) = delete;
  ~ScopedMs() { m_out = m_watch.elapsedMs(); }

 private:
  double& m_out;
  Stopwatch m_watch;
};

}  // namespace sigil::measure
