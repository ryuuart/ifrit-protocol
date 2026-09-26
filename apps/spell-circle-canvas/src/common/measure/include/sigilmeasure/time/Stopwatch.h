#pragma once

/** @file
 * @ingroup measure-time
 * How long something took: the one duration every reading in the library
 * answers, a stopwatch on the steady clock, and a block timed in one call.
 */

#include <chrono>
#include <utility>

/** HOW LONG SOMETHING TOOK, WHAT A RUN OF NUMBERS AMOUNTS TO, AND
 *  WHETHER A CLAIM ABOUT IT HELD. A stopwatch and a timed block; the
 *  summary, quantile and histogram of a run in hand; the window, smoothed
 *  reading and rate of a live stream; and the check whose printed verdict
 *  is computed from the values it reports. Reach for it wherever code
 *  measures itself — no other Sigil library stands under this one, so
 *  anything may. */
namespace sigil::measure {

/** A SPAN OF TIME, in seconds held as a double: the one unit every
 *  reading in this library answers. A `std::chrono` literal (`380ms`,
 *  `4s`) converts into it, and it converts into any floating-point unit a
 *  printout wants — `Milliseconds(watch.elapsed()).count()`. */
using Duration = std::chrono::duration<double>;

/** A span in milliseconds held as a double, for a printout or a field
 *  that states its unit. */
using Milliseconds = std::chrono::duration<double, std::milli>;

/** A span in microseconds held as a double, for a printout where a
 *  millisecond is already the whole budget. */
using Microseconds = std::chrono::duration<double, std::micro>;

/** THE TIME SINCE IT STARTED, on the steady clock: construction starts
 *  it and `restart()` starts it again. Steady rather than system time, so
 *  a clock adjustment mid-measure cannot produce a negative or absurd
 *  reading. */
class Stopwatch {
 public:
  /** The clock every reading is taken from. */
  using Clock = std::chrono::steady_clock;

  Stopwatch() : m_start(Clock::now()) {}

  /** The span since construction or the last restart. */
  [[nodiscard]] Duration elapsed() const { return Clock::now() - m_start; }
  /** Starts the span again from now. */
  void restart() { m_start = Clock::now(); }

 private:
  Clock::time_point m_start;
};

/** HOW LONG @p block TOOK: called once, on the steady clock. What the
 *  block returns is discarded; a caller that needs it keeps it from
 *  inside the block. */
template <class Block>
[[nodiscard]] Duration timed(Block&& block) {
  const Stopwatch watch;
  std::forward<Block>(block)();
  return watch.elapsed();
}

}  // namespace sigil::measure
