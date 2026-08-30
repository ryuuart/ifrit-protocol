#pragma once

/** @file
 * A lap timer: named marks laid through one span, each reading the
 * milliseconds since the mark before it.
 */

#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::measure {

/** Marks laid through one span of work, each named for the phase that
 *  just ended. `mark("layout")` returns the milliseconds since the
 *  previous mark (or since construction) and records them under that
 *  name, so consecutive laps tile the span exactly: the end of one is
 *  the start of the next, with no gap between. The recorded laps read
 *  back through `each()` in the order they were laid. Names are kept as
 *  views: they are string literals in practice, and a lap timer that
 *  copied a string per phase would be a cost inside the span it times. */
class Laps {
 public:
  using Clock = std::chrono::steady_clock;

  Laps() : m_mark(Clock::now()) {}

  /** Ends the phase named @p name here; returns its milliseconds. */
  double mark(std::string_view name) {
    const Clock::time_point now = Clock::now();
    const double ms =
        std::chrono::duration<double, std::milli>(now - m_mark).count();
    m_mark = now;
    m_laps.emplace_back(name, ms);
    return ms;
  }

  /** Visits `(name, ms)` for every mark laid, oldest first. */
  void each(const std::function<void(std::string_view, double)>& fn) const {
    for (const auto& [name, ms] : m_laps) fn(name, ms);
  }
  size_t size() const { return m_laps.size(); }
  /** The sum of every lap — the span from construction (or the last
   *  reset()) to the last mark. */
  double totalMs() const {
    double sum = 0.0;
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
  std::vector<std::pair<std::string_view, double>> m_laps;
};

}  // namespace sigil::measure
