#pragma once

/** @file
 * Photographing a live Qt Quick window: the loop that drives it to a
 * settled frame and writes that frame out as a PNG.
 */

#include <QString>
#include <functional>

class QQuickWindow;

/** THE QT QUICK CONTROLS THE DESKTOP TOOLS SHARE, and the native window
 *  dressing beside them. Most of the surface is QML; what is here in
 *  C++ is what QML cannot reach on its own — the platform's window
 *  material, the font database behind a family picker, a window
 *  photographed to a file, and a texture published out of the Qt
 *  renderer. Reach for it when building one of this repository's own
 *  desktop tools. */
namespace ifrit::qt {

/** How a window capture is driven. Left alone it ticks about once a
 *  display frame, waits for the window to settle, and photographs the
 *  frames after it. */
struct WindowCaptureOptions {
  /** How many frames are drawn after the window reports itself ready,
   *  before the photograph is taken. */
  int warmupFrames = 90;
  /** How long one tick of the capture loop waits, in milliseconds. */
  int intervalMs = 16;
  /** How many frames the loop waits for readiness before going on
   *  anyway, so a window stuck loading is still photographed. */
  int maxReadyFrames = 900;
  /** Run at the top of every tick, for a caller that has to advance
   *  something of its own. Empty does nothing. */
  std::function<void()> prepareFrame;
  /** Asked every tick until it answers true. Empty means ready at
   *  once. */
  std::function<bool()> ready;
};

/** Drives and captures a live Qt Quick window on its GUI thread.
 *  Each timer tick prepares and grabs a frame, including while waiting for
 *  ready(). Readiness waiting is bounded; warmup follows even on timeout so
 *  an application's loading or error state can be photographed. Writes a
 *  PNG, prints its path and dimensions, then exits the application with
 *  success or failure. Destroying the window cancels the pending capture. */
void captureWindow(QQuickWindow& window, QString output,
                   WindowCaptureOptions options = {});

}  // namespace ifrit::qt
