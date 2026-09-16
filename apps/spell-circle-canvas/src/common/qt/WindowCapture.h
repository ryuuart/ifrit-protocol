#pragma once

#include <QString>
#include <functional>

class QQuickWindow;

namespace ifrit::qt {

struct WindowCaptureOptions {
  int warmupFrames = 90;
  int intervalMs = 16;
  int maxReadyFrames = 900;
  std::function<void()> prepareFrame;
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
