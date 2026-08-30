#pragma once

/** @file
 * Per-frame timing over three sample rings — the frame end to end, the frame's
 * own work, and the interval between presented frames — fed by four
 * marks laid in the render loop.
 */

#include <sigilmeasure/stats/Samples.h>

#include <chrono>

namespace sigil::measure {

/** Four marks a render loop lays, three rings they feed.
 *
 *  - `begin()` at the top of the frame;
 *  - `composed()` when the frame's OWN work is done — every command
 *    recorded, nothing yet flushed to the backend;
 *  - `finished()` when the backend is done with the frame, flush included;
 *  - `presented()` when the frame reaches the screen.
 *
 *  `work()` holds begin→composed, `frame()` holds begin→finished, and
 *  `present()` holds the wall-clock delta between consecutive
 *  `presented()` marks. The two cost lanes are separate rather than one
 *  derived from the other because they answer different questions: a
 *  synchronous backend drain is not work the frame does, but it is time
 *  the machine spent, so charging it to the work lane understates
 *  headroom and leaving it out of the frame lane understates the cost.
 *  On a backend with no flush the two lanes hold the same numbers.
 *
 *  A loop that times its own spans may add samples directly instead of
 *  laying marks; the rings are the same either way. */
class FrameTimer {
 public:
  using Clock = std::chrono::steady_clock;

  explicit FrameTimer(size_t capacity = 120)
      : m_frame(capacity), m_work(capacity), m_present(capacity) {}

  void begin() { m_begin = Clock::now(); }
  void composed() { m_work.add(sinceBeginMs()); }
  void finished() { m_frame.add(sinceBeginMs()); }
  /** The first mark after construction or reset() seeds the cadence and
   *  adds no sample; there is no previous frame to measure from. */
  void presented() {
    const Clock::time_point now = Clock::now();
    if (m_lastPresent != Clock::time_point{})
      m_present.add(
          std::chrono::duration<double, std::milli>(now - m_lastPresent)
              .count());
    m_lastPresent = now;
  }

  void addFrame(double ms) { m_frame.add(ms); }
  void addWork(double ms) { m_work.add(ms); }
  void addPresent(double ms) { m_present.add(ms); }

  const Samples& frame() const { return m_frame; }
  const Samples& work() const { return m_work; }
  const Samples& present() const { return m_present; }

  /** NOT a frame rate: the rate the frame's work alone would allow, with
   *  nothing said about presenting it or flushing it. A ceiling, which
   *  stays high exactly when a stutter comes from outside the measured
   *  work — so read it beside the end-to-end time, never instead of it.
   *  0 when no work sample has landed. */
  double headroomFps() const {
    const double avg = m_work.mean();
    return avg > 0 ? 1000.0 / avg : 0.0;
  }
  /** Frames per second as actually shown, from the mean present interval;
   *  0 before two frames have been presented. */
  double presentedFps() const {
    const double avg = m_present.mean();
    return avg > 0 ? 1000.0 / avg : 0.0;
  }

  /** Empties every ring and forgets the last presented mark, so the
   *  next `presented()` seeds rather than measures. */
  void reset() {
    m_frame.clear();
    m_work.clear();
    m_present.clear();
    m_lastPresent = {};
  }
  /** Forgets only the presentation cadence — for a pause, after which
   *  the interval across the gap is not a frame time. */
  void resetPresentation() {
    m_present.clear();
    m_lastPresent = {};
  }

 private:
  double sinceBeginMs() const {
    return std::chrono::duration<double, std::milli>(Clock::now() - m_begin)
        .count();
  }
  Samples m_frame, m_work, m_present;
  Clock::time_point m_begin{};
  Clock::time_point m_lastPresent{};
};

}  // namespace sigil::measure
