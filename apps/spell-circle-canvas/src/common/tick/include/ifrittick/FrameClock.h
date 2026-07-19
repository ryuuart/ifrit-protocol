#pragma once

namespace ifrit::tick {

/**
 * Monotonic frame clock: turns wall-clock time into well-behaved
 * per-frame deltas for animation stepping.
 *
 * - Pausable: while paused, tick() returns 0 and elapsed() freezes.
 * - Time-scalable: slow-motion / overdrive without touching animations.
 * - Clamped: the first tick and any long stall (app suspended, debugger
 *   break) yield at most maxDelta instead of a giant catch-up step.
 *
 * Call tick() once per rendered frame from whatever loop or event drives
 * drawing; feed the returned delta to a Ticker (or anything steppable).
 */
struct FrameClockOptions {
  /** Largest delta a single tick may report, in seconds. */
  double maxDelta = 0.25;
};

class FrameClock {
public:
  using Options = FrameClockOptions;

  explicit FrameClock(Options options = {}) : m_options(options) {}

  /** Advances to `nowSeconds` (monotonic, e.g. from steady_clock) and
   *  returns the scaled, clamped delta since the previous tick. */
  double tick(double nowSeconds);

  /** Convenience overload using std::chrono::steady_clock. */
  double tick();

  void setPaused(bool paused);
  bool paused() const { return m_paused; }

  /** 1 = real time, 0.5 = half speed, 2 = double. Applied to deltas. */
  void setTimeScale(double scale) { m_timeScale = scale; }
  double timeScale() const { return m_timeScale; }

  /** Total scaled time accumulated across ticks, in seconds. */
  double elapsed() const { return m_elapsed; }

private:
  Options m_options;
  double m_lastNow = -1.0;
  double m_elapsed = 0.0;
  double m_timeScale = 1.0;
  bool m_paused = false;
};

} // namespace ifrit::tick
