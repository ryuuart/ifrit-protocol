#pragma once

/** @file
 * @ingroup measure-stats
 * How often something happens: the events of the last span of time, and
 * how many that is a second.
 */

#include <sigilmeasure/stats/Window.h>
#include <sigilmeasure/time/Stopwatch.h>

#include <cstddef>
#include <span>

namespace sigil::measure {

/** HOW FAR BACK A `Rate` COUNTS. */
struct RateOptions {
  /** The events counted are the ones marked in the last `span`. */
  Duration span = std::chrono::seconds(1);
};

/** EVENTS PER SECOND, over the last span of time. `Rate arrivals{4s}`
 *  counts what was `mark()`ed in the last four seconds; `advance(now)`
 *  moves its time on with nothing marked, so a stream that fell silent
 *  reads zero. Time is the caller's clock — a feed's arrival times, the
 *  scene's seconds — so a rate over scene time is deterministic. The same
 *  instrument is a frame counter: mark every frame and read
 *  `perSecond()`. */
class Rate {
 public:
  /** Events over the last @p span. */
  explicit Rate(Duration span) : m_times({.span = span}), m_span(span) {}
  /** Events over the span @p options says. */
  explicit Rate(RateOptions options = {}) : Rate(options.span) {}

  /** One event, at @p at. */
  void mark(Duration at) { m_times.add(at, at); }
  /** Moves the rate's time to @p now; the events older than the span
   *  are dropped. */
  void advance(Duration now) { m_times.advance(now); }
  /** Forgets every event. */
  void clear() { m_times.clear(); }

  /** How many events are inside the span. */
  [[nodiscard]] std::size_t count() const { return m_times.size(); }
  /** The events inside the span, per second of the span. */
  [[nodiscard]] double perSecond() const {
    return m_span > Duration::zero() ? (double)count() / m_span.count() : 0.0;
  }
  /** When each event inside the span was marked, oldest first — a strip
   *  of ticks draws them. */
  [[nodiscard]] std::span<const Duration> times() const {
    return m_times.values();
  }

 private:
  Window<Duration> m_times;
  Duration m_span;
};

}  // namespace sigil::measure
