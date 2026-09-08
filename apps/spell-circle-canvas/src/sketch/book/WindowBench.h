#pragma once

/** @file
 * The window's own frame-rate lane: each selected sketch presented in
 * the real window for a stated stretch, and the rate the compositor
 * answered with.
 */

#include <string>
#include <vector>

class QGuiApplication;
class QObject;
class QQuickWindow;

/** How long `--window-bench` measures each sketch, how long it lets one
 *  run before it starts, and how long it waits for a selection to reach
 *  the screen at all. The warm-up is what pays for the first frames'
 *  program compiles, texture bakes and glyph atlases, and it starts at
 *  the first presented frame rather than at the ask — a session whose
 *  first frame costs seconds would otherwise spend the whole of it
 *  before anything of that sketch was on screen. The rolling windows the
 *  readout comes from are emptied where the measured stretch begins, so
 *  what is reported is frames from that stretch whatever the sketch's
 *  rate. The ceiling is generous because a first frame legitimately
 *  can be seconds long; what it catches is a window that has stopped
 *  presenting altogether. */
constexpr double kWindowBenchSeconds = 2.5;
constexpr double kWindowBenchWarmupSeconds = 1.2;
constexpr double kWindowBenchCeilingSeconds = 30.0;

/** What `--window-bench` was asked for. */
struct WindowBench {
  double seconds = 0.0;  // above zero turns the lane on
  int width = 1440;
  int height = 900;
  double scale = 0.0;  // zero leaves the screen's own device pixel ratio
};

/** THE LANE OVER THE REAL WINDOW: each selected sketch presented for a
 *  stated stretch, one machine-readable line each.
 *
 *  What it measures that the frame-time gate cannot. `--bench` renders
 *  onto a raster surface at the sketch's declared size and presents
 *  nothing — it is the sketch's own cost and nothing else, which is what
 *  makes it a gate. Here the frame is drawn through the surface the
 *  window presents, at the window's pixels and its device pixel ratio,
 *  and the numbers carry the host's own overhead with them: the submit
 *  or texture upload that puts the frame on screen, and for a set the
 *  device readback and blit that its paint phase performs. The presented
 *  rate is the compositor's answer, so it is bounded by the display and
 *  a sketch inside its budget reads at the refresh rate.
 *
 *  WHAT IS MEASURED IS THE SKETCH THAT IS ON SCREEN. Selection goes
 *  through the same property QML sets, and setting it is an ask, not an
 *  arrival: the session opens on the render thread and its first frame
 *  can cost seconds. So each row waits for the window to be presenting
 *  the selection's own session before its warm-up starts, empties the
 *  rolling windows where the measured stretch begins, and reads them
 *  where it ends — one sketch measured over its own frames, whatever
 *  the one before it cost. A selection that never reaches the screen is
 *  stood down by name and the run says so in its exit status; it is not
 *  a rate of nothing.
 *
 *  False when there is nothing to present. */
bool startWindowBench(QGuiApplication& application, QQuickWindow& window,
                      QObject& view, const WindowBench& options,
                      std::vector<int> selection);

/** The registry entries `--window-bench` will present: the whole table,
 *  or what `--sketch` and `--kind` narrow it to. A sketch this machine
 *  cannot run is named as stood down and left out, exactly as the sweep
 *  passes over it — a skip is not a failure and not a measurement. */
std::vector<int> windowBenchSelection(int only, const std::string& kind);
