#pragma once

/** @file
 * The cadence a frame-rate sweep over a window keeps: when a selected
 * sketch's warm-up starts, when the stretch that is measured starts, and
 * when a sketch that never reached the screen is stood down.
 */

namespace sigil::sketch {

/** WHAT A PRESENTED RATE IS TAKEN OVER, as a state machine a window
 *  drives with wall time and one observation.
 *
 *  A rate read off a window is only the selected sketch's rate once the
 *  window is presenting THAT sketch. Selecting one and starting a clock
 *  are two different moments: the session is opened on the render
 *  thread, its first frame can cost seconds, and until that frame is on
 *  screen the numbers a reader would take belong to whatever was
 *  presented before. So nothing here advances on time alone — every
 *  step is told whether the window is presenting the selection, and the
 *  warm-up begins at the first frame of it rather than at the ask.
 *
 *  A sketch that never presents is not a slow sketch: it is a sketch
 *  this run cannot measure, and it is stood down by name at the ceiling
 *  rather than read as a rate of nothing. */
class BenchCadence {
 public:
  struct Times {
    /** Presented but not yet measured: the stretch a session spends
     *  reaching a steady frame before its rate counts. */
    double warmupSeconds = 1.2;
    /** The stretch the rate is taken over. */
    double measureSeconds = 2.5;
    /** How long a selection is given to reach the screen at all. */
    double ceilingSeconds = 30.0;
  };

  /** What the caller does with this step. */
  enum class Step {
    /** Nothing yet: keep asking for frames. */
    Wait,
    /** The measured stretch starts now — this is where a caller empties
     *  the rolling windows it will read, so what it reads afterwards
     *  describes the stretch and not the warm-up before it. */
    Begin,
    /** The measured stretch is over: read the numbers. */
    Read,
    /** The selection never reached the screen: stand it down. */
    Skip
  };

  explicit BenchCadence(Times times) noexcept : m_times(times) {}

  /** A sketch has been asked for, at @p now. */
  void select(double now) noexcept {
    m_phase = Phase::Asked;
    m_since = now;
  }

  /** One step of the cadence. @p presenting is whether the window is
   *  presenting the selection right now — the selected sketch's own
   *  session, with a frame of it already on screen. */
  Step advance(double now, bool presenting) noexcept {
    switch (m_phase) {
      case Phase::Asked:
        if (!presenting)
          return now - m_since >= m_times.ceilingSeconds ? Step::Skip
                                                         : Step::Wait;
        m_phase = Phase::WarmingUp;
        m_since = now;
        return Step::Wait;
      case Phase::WarmingUp:
        if (now - m_since < m_times.warmupSeconds) return Step::Wait;
        m_phase = Phase::Measuring;
        m_since = now;
        return Step::Begin;
      case Phase::Measuring:
        if (now - m_since < m_times.measureSeconds) return Step::Wait;
        m_phase = Phase::Done;
        return Step::Read;
      case Phase::Done:
        return Step::Wait;
    }
    return Step::Wait;
  }

  /** How long the current phase has been running at @p now. What a
   *  stand-down says it waited, and what a reader is told a measured
   *  stretch covered. */
  [[nodiscard]] double elapsed(double now) const noexcept {
    return now - m_since;
  }

 private:
  enum class Phase { Asked, WarmingUp, Measuring, Done };

  Times m_times;
  Phase m_phase = Phase::Done;
  double m_since = 0.0;
};

}  // namespace sigil::sketch
