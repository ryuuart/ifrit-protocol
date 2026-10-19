#pragma once

/** @file
 * One file, photographed: the still `--frame` writes at a stated moment,
 * and the numbered sequence after it.
 */

#include <filesystem>
#include <string>

namespace sigil::sketch {
class Host;
}  // namespace sigil::sketch
namespace sigil::skia {
class GraphiteContext;
}

/** Waits for the first build to land or fail; false when no session
 *  opened. */
bool awaitFirstBuild(sigil::sketch::Host& host);

/** What a run was asked to photograph. */
struct CaptureOptions {
  std::string outputPath;
  /** Seconds of fixed-step stepping before the capture, as the CALLER
   *  stated it. Negative means unstated, which is not the same as 1.5:
   *  a still then lands at the moment the sketch itself declared, and
   *  only a sketch that declares none falls back to the number. */
  double at = -1.0;
  float scale = 1.0f;  // multiplier over the sketch's canvas size
  int frames = 1;      // >1 captures a numbered sequence
  double fps = 60.0;   // fixed-step rate
};

/** THE STILL, AT THE MOMENT THE SKETCH DECLARED unless the caller named
 *  another, and the numbered sequence when more than one frame was
 *  asked for. Non-zero when the sketch never built or a frame could not
 *  be written. */
int runFrames(sigil::sketch::Host& host, const CaptureOptions& options,
              bool gpu = false,
              sigil::skia::GraphiteContext* canvasGraphite = nullptr);
