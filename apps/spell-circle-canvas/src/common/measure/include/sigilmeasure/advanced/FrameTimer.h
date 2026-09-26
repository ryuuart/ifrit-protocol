#pragma once

/** @file
 * @ingroup measure-time
 * Per-frame timing over three windows — the frame end to end, the
 * frame's own work, and the interval between presented frames — fed by
 * four marks laid in the render loop, and the plain sample a frame-budget
 * gate snapshots off them.
 */

#include <sigilmeasure/stats/Window.h>
#include <sigilmeasure/time/Stopwatch.h>

#include <chrono>
#include <cstddef>

namespace sigil::measure {

/** THE TIMING A FRAME-BUDGET GATE JUDGES A SCENE BY, as plain values, so
 *  a snapshot taken when a sample window closes survives the windows
 *  being cleared or refilled behind it. How a sample is written out is
 *  the writer's business, not this struct's. */
struct FrameSample {
  /** Mean end-to-end frame time, backend flush included: what the
   *  machine actually spent per frame. */
  Duration frame{};
  /** Mean of the frame's own work, the backend flush taken out — the
   *  same window as `frame` on a backend with no flush. */
  Duration work{};
  /** The tail of the end-to-end lane: its 0.99 quantile. */
  Duration frameTail{};
  /** The rate the frame's work alone would allow. A ceiling, not a frame
   *  rate: it says nothing about presenting the frame or flushing it,
   *  and it stays high exactly when a stutter comes from outside the
   *  measured work. */
  double headroomFps = 0;
};

/** HOW DEEP A `FrameTimer`'s LANES ARE, and what it reads as a pause. */
struct FrameTimerOptions {
  /** How many frames each lane holds, the oldest dropping first. */
  std::size_t frames = 120;
  /** A presented interval at least this long is a pause — a window drag,
   *  a sleep, a debugger stop — and not a frame: it seeds the cadence
   *  again and adds no sample. Zero reads every interval as a frame. */
  Duration pause{};
};

/** Four marks a render loop lays, three windows they feed. `begin()` at
 *  the top of the frame, `composed()` when the frame's OWN work is done
 *  with nothing yet flushed, `finished()` when the backend is done with
 *  it, `presented()` when it reaches the screen; `work()` holds
 *  begin→composed, `frame()` holds begin→finished, and `present()` the
 *  interval between consecutive presented marks. A loop that times its
 *  own spans adds them directly instead.
 *  @trap The two cost lanes are SEPARATE and neither is derived from
 *  the other: a synchronous backend drain is time the machine spent
 *  that is not work the frame did. */
class FrameTimer {
 public:
  /** The clock every mark is taken from. */
  using Clock = std::chrono::steady_clock;

  explicit FrameTimer(FrameTimerOptions options = {})
      : m_options(options),
        m_frame(options.frames),
        m_work(options.frames),
        m_present(options.frames) {}

  /** Opens a frame: the mark both cost lanes are measured from. */
  void begin() { m_begin = Clock::now(); }
  /** Closes the work lane for this frame. */
  void composed() { m_work.add(Clock::now() - m_begin); }
  /** Closes the end-to-end lane for this frame. */
  void finished() { m_frame.add(Clock::now() - m_begin); }
  /** The first mark after construction, a reset or a pause seeds the
   *  cadence and adds no sample: there is no previous frame to measure
   *  from. */
  void presented() {
    const Clock::time_point now = Clock::now();
    if (m_lastPresent != Clock::time_point{}) addPresent(now - m_lastPresent);
    m_lastPresent = now;
  }

  /** Adds an end-to-end span a caller timed itself. */
  void addFrame(Duration span) { m_frame.add(span); }
  /** Adds a work span a caller timed itself. */
  void addWork(Duration span) { m_work.add(span); }
  /** Adds a presentation interval a caller timed itself; one the options
   *  read as a pause is dropped. */
  void addPresent(Duration interval) {
    if (m_options.pause > Duration::zero() && interval >= m_options.pause)
      return;
    m_present.add(interval);
  }

  /** The end-to-end lane: begin to finished. */
  [[nodiscard]] const Window<Duration>& frame() const { return m_frame; }
  /** The work lane: begin to composed. */
  [[nodiscard]] const Window<Duration>& work() const { return m_work; }
  /** The presentation lane: the interval between presented frames. */
  [[nodiscard]] const Window<Duration>& present() const { return m_present; }

  /** The rate the frame's WORK alone would allow; 0 when no work sample
   *  has landed.
   *  @trap NOT a frame rate. A ceiling, which stays high exactly when a
   *  stutter comes from outside the measured work, so read it beside
   *  the end-to-end time and never instead of it. */
  [[nodiscard]] double headroomFps() const { return perSecond(m_work); }
  /** Frames per second as actually shown, from the mean presented
   *  interval; 0 before two frames have been presented. */
  [[nodiscard]] double presentedFps() const { return perSecond(m_present); }

  /** The lanes' steady-state numbers as plain values. */
  [[nodiscard]] FrameSample sample() const {
    return {.frame = m_frame.mean(),
            .work = m_work.mean(),
            .frameTail = m_frame.quantile(0.99),
            .headroomFps = headroomFps()};
  }

  /** Empties every lane and forgets the last presented mark, so the
   *  next `presented()` seeds rather than measures. */
  void reset() {
    m_frame.clear();
    m_work.clear();
    resetPresentation();
  }
  /** Empties the presentation lane and forgets its cadence. */
  void resetPresentation() {
    m_present.clear();
    m_lastPresent = {};
  }
  /** Forgets only the last presented mark, keeping every lane — for a
   *  stretch the loop knows something else held the screen, after which
   *  the interval across the gap is not a frame time. The next
   *  `presented()` seeds the cadence again. */
  void resume() { m_lastPresent = {}; }

 private:
  [[nodiscard]] static double perSecond(const Window<Duration>& lane) {
    const Duration mean = lane.mean();
    return mean > Duration::zero() ? 1.0 / mean.count() : 0.0;
  }

  FrameTimerOptions m_options;
  Window<Duration> m_frame, m_work, m_present;
  Clock::time_point m_begin;
  Clock::time_point m_lastPresent;
};

}  // namespace sigil::measure
