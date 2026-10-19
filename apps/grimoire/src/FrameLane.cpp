/** @file
 * The still, over one file the host has built.
 */

#include "FrameLane.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilsketch/core/Crash.h>
#include <sigilsketch/core/Session.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/plate/Graphite.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/PaintOrder.h>
#include <sigilskia/graphite/Readback.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace sketch = sigil::sketch;

namespace {

std::string numberedPath(const std::string& path, int index) {
  const size_t dot = path.rfind('.');
  char suffix[16];
  std::snprintf(suffix, sizeof suffix, "_%04d", index);
  return dot == std::string::npos
             ? path + suffix
             : path.substr(0, dot) + suffix + path.substr(dot);
}

}  // namespace

/** Blocks until the first build lands (or fails); false = never got
 *  live. */
bool awaitFirstBuild(sketch::Host& host) {
  using namespace std::chrono_literals;
  for (int i = 0; !host.live() && i < 1200; ++i) {
    host.poll();
    if (host.live() || !host.errorLog().empty()) break;
    std::this_thread::sleep_for(50ms);
  }
  if (!host.live()) {
    std::fprintf(stderr, "sketch did not open:\n%s\n", host.errorLog().c_str());
    return false;
  }
  return true;
}

int runFrames(sketch::Host& host, const CaptureOptions& options, bool gpu,
              sigil::skia::GraphiteContext* canvasGraphite) {
  if (!awaitFirstBuild(host)) return 1;
  struct CaptureScope {
    sketch::Host* host = nullptr;
    std::unique_ptr<sigil::skia::PaintOrderCanvas> canvas;
    ~CaptureScope() {
      if (host) host->setCaptureBackend({});
    }
  } capture;
  if (gpu) {
    // THE CONTEXT THE CAPTURE PAINTS THROUGH: the device's own where the
    // sketch needs the device, which on Apple is a Metal context on the
    // Metal device the Vulkan driver runs over, so the still, the pages it
    // wears and the window's canvas are all Metal's; and on Apple a pure
    // scene2d capture's own Metal context, which starts no device at all.
    sigil::skia::GraphiteContext* graphite = nullptr;
#ifdef __APPLE__
    graphite = host.needsDevice() ? sketch::deviceGraphite() : canvasGraphite;
#else
    graphite = sketch::deviceGraphite();
#endif
    if (!graphite) {
      std::fprintf(stderr, "capture: the device has no Graphite context\n");
      return 1;
    }
    capture.host = &host;
    host.setCaptureBackend(
        {[graphite](const SkImageInfo& info) {
           return SkSurfaces::RenderTarget(graphite->recorder(), info);
         },
         [graphite](SkSurface& surface, const SkPixmap& pixels) {
           return sigil::skia::readbackPixels(*graphite, surface, pixels);
         },
         [graphite, &capture](SkSurface& surface) -> SkCanvas* {
           capture.canvas = std::make_unique<sigil::skia::PaintOrderCanvas>(
               *graphite, surface.getCanvas());
           return capture.canvas.get();
         }});
  }
  std::printf("backend: %s, runtime: %.*s\n",
              gpu ? "Graphite GPU" : "CPU raster", (int)host.kind().size(),
              host.kind().data());
  const double declared = host.captureSeconds();
  double at;
  try {
    at = host.prepareCapture(
        options.at >= 0.0 ? std::optional<double>{options.at} : std::nullopt,
        options.fps, options.scale);
  } catch (const std::exception& error) {
    std::fprintf(stderr, "capture: %s\n", error.what());
    return 1;
  }
  // THE SWEEP'S PHOTOGRAPH, so a file written at a moment is the plate of
  // that moment. A still does not move the scene, so the frames of a
  // sequence are one step of the rate apart, and each is reported with
  // the scene time its pixels show.
  const double dt = 1.0 / options.fps;
  double shown = at;

  for (int index = 0; index < options.frames; ++index) {
    const std::string path = options.frames > 1
                                 ? numberedPath(options.outputPath, index + 1)
                                 : options.outputPath;
    if (!host.writePhotograph(path, options.scale)) {
      std::fprintf(stderr, "failed to write %s\n", path.c_str());
      return 1;
    }
    if (options.frames > 1)
      std::printf("frame %d: t=%.9gs %s\n", index + 1, shown, path.c_str());
    if (index + 1 < options.frames) {
      try {
        shown += host.prepareCapture(dt, options.fps, options.scale);
      } catch (const std::exception& error) {
        std::fprintf(stderr, "capture: %s\n", error.what());
        return 1;
      }
    }
  }
  std::printf(
      "wrote %s (%d frame%s at %.3gx, t=%.3gs %s, build %d, work %.2f ms "
      "avg)\n",
      options.outputPath.c_str(), options.frames,
      options.frames == 1 ? "" : "s", options.scale, at,
      options.at >= 0.0 ? "asked for"
                        : (declared >= 0.0 ? "declared" : "by default"),
      host.generation(),
      sigil::measure::Milliseconds(host.frameTimes().work().mean()).count());
  return 0;
}
