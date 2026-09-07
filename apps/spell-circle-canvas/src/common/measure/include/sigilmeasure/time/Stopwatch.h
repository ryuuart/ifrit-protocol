#pragma once

/** @file
 * A steady-clock stopwatch reading elapsed time, and a scope guard that
 * writes its elapsed milliseconds into a double when it goes out of
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
  /** The same span in microseconds — the unit a per-frame reading wants,
   *  where a millisecond is already the whole budget. */
  double elapsedUs() const {
    return std::chrono::duration<double, std::micro>(Clock::now() - m_start)
        .count();
  }
  void reset() { m_start = Clock::now(); }

 private:
  Clock::time_point m_start;
};

/** @p span as fractional microseconds, for a caller holding two clock
 *  readings rather than a stopwatch. */
inline double toMicroseconds(Stopwatch::Clock::duration span) {
  return std::chrono::duration<double, std::micro>(span).count();
}

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
