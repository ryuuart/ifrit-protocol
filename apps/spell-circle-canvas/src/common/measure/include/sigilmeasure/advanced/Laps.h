#pragma once

/** @file
 * @ingroup measure-time
 * A lap timer: named marks laid through one span, each reading the time
 * since the mark before it; and a scope guard that writes the time a
 * block took into a value the caller keeps.
 */

#include <sigilmeasure/time/Stopwatch.h>

#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::measure {

/** Marks laid through one span of work, each named for the phase that
 *  just ended. `mark("layout")` returns the time since the previous mark,
 *  or since construction, and records it under that name, so consecutive
 *  laps tile the span exactly with no gap between them; `each()` reads
 *  them back in the order they were laid.
 *  @trap A NAME IS BORROWED, NOT COPIED, so the caller owns it and must
 *  outlive the timer. A `std::string` rvalue is refused at compile time
 *  rather than left dangling. */
class Laps {
 public:
  /** The clock every mark is taken from. */
  using Clock = std::chrono::steady_clock;

  Laps() : m_mark(Clock::now()) {}

  /** Ends the phase named @p name here; returns how long it took. */
  Duration mark(std::string_view name) {
    const Clock::time_point now = Clock::now();
    const Duration lap = now - m_mark;
    m_mark = now;
    m_laps.emplace_back(name, lap);
    return lap;
  }
  /** A literal names its phase with no conversion to choose between. */
  Duration mark(const char* name) { return mark(std::string_view(name)); }
  /** A name that dies at the end of the statement cannot be read back. */
  Duration mark(std::string&& name) = delete;

  /** Visits `(name, lap)` for every mark laid, oldest first. */
  void each(
      const std::function<void(std::string_view, Duration)>& visit) const {
    for (const auto& [name, lap] : m_laps) visit(name, lap);
  }
  /** How many marks have been laid. */
  [[nodiscard]] size_t size() const { return m_laps.size(); }
  /** The sum of every lap — the span from construction (or the last
   *  reset()) to the last mark. */
  [[nodiscard]] Duration total() const {
    Duration sum{};
    for (const auto& lap : m_laps) sum += lap.second;
    return sum;
  }

  /** Forgets the laps and starts the first phase now. */
  void reset() {
    m_laps.clear();
    m_mark = Clock::now();
  }

 private:
  Clock::time_point m_mark;
  std::vector<std::pair<std::string_view, Duration>> m_laps;
};

/** Writes the time a scope took into the value it was given, at scope
 *  exit — `{ ScopedDuration timing(layoutTime); layout(); }`.
 *  @trap The target is ASSIGNED, not accumulated, so a block entered
 *  twice reports its last run. */
class ScopedDuration {
 public:
  /** Starts timing; @p out receives the elapsed time at scope exit and
   *  must outlive this guard. */
  explicit ScopedDuration(Duration& out) : m_out(out) {}
  ScopedDuration(const ScopedDuration&) = delete;
  ScopedDuration& operator=(const ScopedDuration&) = delete;
  ~ScopedDuration() { m_out = m_watch.elapsed(); }

 private:
  Duration& m_out;
  Stopwatch m_watch;
};

}  // namespace sigil::measure
