#pragma once

/** @file
 * @ingroup measure-time
 * Per-frame timing over three sample rings — the frame end to end, the frame's
 * own work, and the interval between presented frames — fed by four
 * marks laid in the render loop.
 */

#include <sigilmeasure/stats/Samples.h>

#include <chrono>

namespace sigil::measure {

/** Four marks a render loop lays, three rings they feed. `begin()` at
 *  the top of the frame, `composed()` when the frame's OWN work is done
 *  with nothing yet flushed, `finished()` when the backend is done with
 *  it, `presented()` when it reaches the screen; `work()` holds
 *  begin→composed, `frame()` holds begin→finished, and `present()` the
 *  delta between consecutive presented marks. A loop that times its own
 *  spans may add samples directly instead.
 *  @trap The two cost lanes are SEPARATE and neither is derived from
 *  the other: a synchronous backend drain is time the machine spent
 *  that is not work the frame did. */
class FrameTimer {
 public:
  /** The clock every mark is taken from. */
  using Clock = std::chrono::steady_clock;

  /** Three rings each holding @p capacity samples, the oldest falling
   *  off as a newer one lands. */
  explicit FrameTimer(size_t capacity = 120)
      : m_frame(capacity), m_work(capacity), m_present(capacity) {}

  /** Opens a frame: the mark both cost lanes are measured from. */
  void begin() { m_begin = Clock::now(); }
  /** Closes the work lane for this frame. */
  void composed() { m_work.add(sinceBeginMs()); }
  /** Closes the end-to-end lane for this frame. */
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

  /** Adds an end-to-end sample a caller timed itself, in milliseconds. */
  void addFrame(double ms) { m_frame.add(ms); }
  /** Adds a work sample a caller timed itself, in milliseconds. */
  void addWork(double ms) { m_work.add(ms); }
  /** Adds a presentation interval a caller timed itself, in
   *  milliseconds. */
  void addPresent(double ms) { m_present.add(ms); }

  /** The end-to-end ring: begin to finished. */
  const Samples& frame() const { return m_frame; }
  /** The work ring: begin to composed. */
  const Samples& work() const { return m_work; }
  /** The presentation ring: the interval between presented frames. */
  const Samples& present() const { return m_present; }

  /** The rate the frame's WORK alone would allow; 0 when no work sample
   *  has landed.
   *  @trap NOT a frame rate. A ceiling, which stays high exactly when a
   *  stutter comes from outside the measured work, so read it beside
   *  the end-to-end time and never instead of it. */
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
  Clock::time_point m_begin;
  Clock::time_point m_lastPresent;
};

}  // namespace sigil::measure
