#pragma once

/** @file
 * The rolling frame-time lanes a running sketch is judged by.
 */

#include <sigilmeasure/stats/FrameSample.h>
#include <sigilmeasure/stats/Samples.h>

namespace sigil::sketch {

/** THE THREE ROLLING LANES, each over the last `kWindow` frames, read
 *  through the names a status bar and a headless table print. */
struct FrameStats {
  static constexpr size_t kWindow = 120;
  /// End to end: everything between the top of the frame and the point
  /// the backend has finished with it, the backend flush included.
  measure::Samples frameMs{kWindow};
  /// The frame's OWN work — the same window with the backend flush taken
  /// out. On a backend with no flush the two lanes are the same numbers.
  ///
  /// They are separate because they answer different questions and a
  /// headless sweep needs both. A synchronous GPU drain is not work the
  /// frame does; it is a serialization the harness imposes so a frame's
  /// cost cannot hide in queue depth. Charging it to "what the frame's
  /// work would allow" would understate the headroom of every GPU
  /// sketch, and leaving it out of the end-to-end number would
  /// understate what the machine actually spent. So neither number is
  /// derived from the other.
  measure::Samples workMs{kWindow};
  /// Wall deltas between presented frames.
  measure::Samples presentMs{kWindow};

  void add(double ms) { frameMs.add(ms); }
  void addWork(double ms) { workMs.add(ms); }
  void addPresent(double ms) { presentMs.add(ms); }

  [[nodiscard]] double presentedFps() const {
    const double avg = presentMs.mean();
    return avg > 0 ? 1000.0 / avg : 0;
  }
  /** The tail of the PRESENTED interval, which is the only lane that can
   *  see a stutter. An average present rate cannot: a window of frames
   *  that mostly hit the vsync and occasionally miss several in a row
   *  averages to very near the display rate, so the number a viewer
   *  would call "lagging" and the number a viewer would call "smooth"
   *  are the same number. What separates them is the worst interval, so
   *  it is reported beside the mean rather than folded into it. */
  [[nodiscard]] double presentPercentile(double p) const {
    return presentMs.percentile(p);
  }
  [[nodiscard]] double presentWorstMs() const { return presentMs.max(); }

  /** Mean END-TO-END frame time, backend flush included. */
  [[nodiscard]] double average() const { return frameMs.mean(); }
  [[nodiscard]] double percentile(double p) const {
    return frameMs.percentile(p);
  }
  /** Mean of the frame's own work, without the backend flush. */
  [[nodiscard]] double workAverage() const { return workMs.mean(); }
  [[nodiscard]] double workPercentile(double p) const {
    return workMs.percentile(p);
  }
  /** NOT A FRAME RATE: 1000 / mean(work ms) is the rate the frame's work
   *  alone would allow, with nothing said about presenting it and
   *  nothing said about the backend flush either. It is a ceiling, and
   *  it stays high exactly when a stutter is caused by something outside
   *  the measured work — which is why it is reported beside the
   *  end-to-end time rather than instead of it. */
  [[nodiscard]] double fps() const {
    const double avg = workAverage();
    return avg > 0 ? 1000.0 / avg : 0;
  }
  /** The steady-state numbers as plain values, for a snapshot taken the
   *  moment a sample window closes. */
  [[nodiscard]] measure::FrameSample sample() const {
    return {.frameMs = average(),
            .workMs = workAverage(),
            .p99Ms = percentile(0.99),
            .headroomFps = fps()};
  }
};

}  // namespace sigil::sketch
