#pragma once

/** @file
 * @ingroup measure-stats
 * The last few values of a live stream — the last so many, or the last
 * so long — and what they amount to: mean, ends, a quantile, the newest,
 * and the run itself in order for a sparkline.
 */

#include <sigilmeasure/stats/Quantile.h>
#include <sigilmeasure/time/Stopwatch.h>

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace sigil::measure {

/** HOW MUCH A `Window` HOLDS: a count, a span of time, or both — a value
 *  leaves when either says it is too old. */
struct WindowOptions {
  /** How many values the window holds, the oldest dropping first. Zero
   *  sets no count. */
  std::size_t count = 0;
  /** How far back the window reaches: a value stamped more than this
   *  before the newest time the window has seen is dropped. Zero sets no
   *  span. Time is whatever clock the caller stamps with — the scene's
   *  seconds, a feed's arrival times — so a window over scene time is
   *  deterministic. */
  Duration span{};
};

/** THE LAST FEW VALUES OF A STREAM, oldest first. `Window last{180}` holds
 *  the last 180; `Window recent{{.span = 4s}}` holds what arrived in the
 *  last four seconds, stamped by `add(value, at)`. Every reading is
 *  computed from the values held when it is asked — nothing is cached,
 *  so a value that left is gone from every number — and `values()` is the
 *  held run itself, contiguous and in order, which a chart's trace takes
 *  as it is. `Value` is a number (read as a double) or a `std::chrono`
 *  span (read in its own unit).
 *  @silent a window with neither a count nor a span holds everything
 *  added until `clear()`. */
template <Measurable Value = double>
class Window {
 public:
  /** What a mean or a quantile of the held values answers in. */
  using Reading = ReadingOf<Value>;

  /** The last @p count values. */
  explicit Window(std::size_t count = 120) : m_options{.count = count} {}
  /** A window bounded as @p options says. */
  explicit Window(WindowOptions options) : m_options(options) {}

  /** Adds @p value, stamped with the newest time the window has seen. */
  void add(Value value) { add(value, m_now); }
  /** Adds @p value stamped at @p at, and drops what the span leaves
   *  behind. Values are held in the order they are added; a stamp
   *  earlier than one already seen does not move the window's time back,
   *  and one already beyond the span's reach is not added at all. */
  void add(Value value, Duration at) {
    if (m_options.span > Duration::zero() && at < m_now - m_options.span)
      return;
    m_now = std::max(m_now, at);
    m_values.push_back(value);
    if (m_options.span > Duration::zero()) m_times.push_back(at);
    trim();
  }
  /** Moves the window's time to @p now with nothing added: what the span
   *  leaves behind is dropped, so a stream that fell silent empties. */
  void advance(Duration now) {
    m_now = std::max(m_now, now);
    trim();
  }
  /** Forgets every value and the window's time, keeping its bounds. */
  void clear() {
    m_values.clear();
    m_times.clear();
    m_head = 0;
    m_now = Duration::zero();
  }

  /** How many values are held. */
  [[nodiscard]] std::size_t size() const { return m_values.size() - m_head; }
  /** Whether no value is held. */
  [[nodiscard]] bool empty() const { return size() == 0; }
  /** The bounds the window was made with. */
  [[nodiscard]] const WindowOptions& options() const { return m_options; }

  /** The held values, oldest first, contiguous: a chart's trace or any
   *  loop reads them as they are. */
  [[nodiscard]] std::span<const Value> values() const {
    return std::span<const Value>(m_values).subspan(m_head);
  }
  /** When each held value was stamped, oldest first; empty for a window
   *  with no span, which keeps no stamps. */
  [[nodiscard]] std::span<const Duration> times() const {
    if (m_times.empty()) return {};
    return std::span<const Duration>(m_times).subspan(m_head);
  }

  /** The mean of the held values; zero when empty. */
  [[nodiscard]] Reading mean() const {
    if (empty()) return Reading{};
    Reading sum{};
    for (const Value& value : values()) sum += Reading(value);
    return sum / (double)size();
  }
  /** The smallest held value; zero when empty. */
  [[nodiscard]] Value min() const {
    return empty() ? Value{} : *std::min_element(values().begin(),
                                                  values().end());
  }
  /** The largest held value; zero when empty. */
  [[nodiscard]] Value max() const {
    return empty() ? Value{} : *std::max_element(values().begin(),
                                                  values().end());
  }
  /** `quantile()` over the held values: `quantile(0.99)` is the tail. */
  [[nodiscard]] Reading quantile(double fraction) const {
    return measure::quantile(values(), fraction);
  }
  /** The newest value; zero when empty. */
  [[nodiscard]] Value latest() const {
    return empty() ? Value{} : m_values.back();
  }

 private:
  void trim() {
    if (m_options.count > 0 && size() > m_options.count)
      m_head += size() - m_options.count;
    if (m_options.span > Duration::zero()) {
      const Duration oldest = m_now - m_options.span;
      while (!empty() && m_times[m_head] < oldest) ++m_head;
    }
    // The dropped front is released once it outweighs what is held, so a
    // window costs at most twice its contents and an add stays constant
    // time over a run.
    if (m_head >= 64 && m_head * 2 >= m_values.size()) {
      m_values.erase(m_values.begin(), m_values.begin() + (long)m_head);
      if (!m_times.empty())
        m_times.erase(m_times.begin(), m_times.begin() + (long)m_head);
      m_head = 0;
    }
  }

  WindowOptions m_options;
  std::vector<Value> m_values;
  std::vector<Duration> m_times;
  /** Where the held run starts in the two vectors. */
  std::size_t m_head = 0;
  Duration m_now{};
};

}  // namespace sigil::measure
