#pragma once

/** @file
 * The frame clock: wall-clock time — or a delta a caller states outright
 * — turned into pausable, time-scalable, stall-clamped per-frame deltas.
 */

namespace sigil::motion {

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
 * drawing, or advance() where the caller already knows the step; feed the
 * returned delta to a Ticker (or anything steppable).
 */
struct FrameClockOptions {
  /** Largest delta a single tick may report, in seconds. */
  double maxDelta = 0.25;
};

/** Turns wall-clock readings into per-frame deltas an animation can
 *  trust. It clamps the delta so a stalled or suspended frame does not
 *  jump every animation forward by the length of the stall, and it can
 *  scale time so a whole scene runs slower or faster. */
class FrameClock {
 public:
  using Options = FrameClockOptions;

  explicit FrameClock(Options options = {}) : m_options(options) {}

  /** Advances to `nowSeconds` (monotonic, e.g. from steady_clock) and
   *  returns the scaled, clamped delta since the previous tick. */
  double tick(double nowSeconds);

  /** Convenience overload using std::chrono::steady_clock. */
  double tick();

  /** Steps by a delta the CALLER states, in seconds, and returns what the
   *  clock made of it — the same pause, time scale and stall clamp tick()
   *  applies, so a stepped clock and a wall clock are the same clock.
   *
   *  This is what a deterministic stepper needs: a plate sweep, a frame
   *  export, a fixed-rate capture, a scrub. Without it such a caller keeps
   *  its own accumulator beside the clock, and then the two disagree about
   *  what "now" is the first time anything pauses one of them.
   *
   *  A negative delta is clamped to zero rather than rewinding: time in
   *  this library only goes forward, and a caller that wants to go back
   *  builds a new clock. */
  double advance(double deltaSeconds);

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

}  // namespace sigil::motion
