#pragma once

/** @file
 * One steady-state frame timing sample — the numbers a frame-budget gate
 * judges a scene by.
 */

namespace sigil::measure {

/** The timing a headless sweep snapshots when its sample window closes:
 *  both numbers a frame-budget gate judges and the one it derives from.
 *  Plain numbers, so a snapshot survives the ring it was read from being
 *  cleared or refilled; how a sample is written out is the writer's
 *  business, not this struct's. */
struct FrameSample {
  /** Mean end-to-end frame time in milliseconds, backend flush included:
   *  what the machine actually spent per frame. */
  double frameMs = 0;
  /** Mean of the frame's own work in milliseconds, the backend flush taken
   *  out — the same window as `frameMs` on a backend with no flush. */
  double workMs = 0;
  /** The tail of the end-to-end lane: the 99th percentile frame time. */
  double p99Ms = 0;
  /** 1000 / `workMs` — the rate the frame's work alone would allow. A
   *  ceiling, not a frame rate: it says nothing about presenting the
   *  frame or flushing it, and it stays high exactly when a stutter comes
   *  from outside the measured work. */
  double headroomFps = 0;
};

}  // namespace sigil::measure
