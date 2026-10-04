/** @file
 * The still and the frame-time gate, over one file the host has built.
 */

#include "FrameLane.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmeasure/advanced/Quantiles.h>
#include <sigilmeasure/stats/Summary.h>
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilsketch/core/Crash.h>
#include <sigilsketch/core/Session.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/plate/Graphite.h>
#include <sigilsketch/plate/Sweep.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/PaintOrder.h>
#include <sigilskia/graphite/Readback.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace sketch = sigil::sketch;

namespace {

/** The 60 FPS gate, in milliseconds per frame. */
constexpr double kFrameBudgetMs = 16.6;

/** Where a capture lands when neither the caller nor the sketch says. */
constexpr double kFallbackMoment = 1.5;

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
bool awaitFirstBuild(sketch::Host& host, std::string_view runtime) {
  using namespace std::chrono_literals;
  for (int i = 0; !host.live() && i < 1200; ++i) {
    host.poll();
    if (host.live() || !host.errorLog().empty()) break;
    std::this_thread::sleep_for(50ms);
  }
  if (!host.live()) {
    std::fprintf(stderr, "sketch failed to build:\n%s\n",
                 host.errorLog().c_str());
    return false;
  }
  if (!runtime.empty() && runtime != host.kind()) {
    std::fprintf(stderr, "plugin runtime is %.*s, not the requested %.*s\n",
                 (int)host.kind().size(), host.kind().data(),
                 (int)runtime.size(), runtime.data());
    return false;
  }
  return true;
}

/** The frame-time gate, measured on the sketch's REAL canvas.
 *
 *  Every frame runs the true per-frame path — clear, advance, draw, and
 *  a readback that forces the raster to complete rather than leaving
 *  work queued behind the timer. Warm to `--at` first: the first frames
 *  of any sketch pay for program compiles, texture bakes, snapshots and
 *  glyph atlases, and folding those into the sample measures the wrong
 *  thing.
 *
 *  A SLOW SKETCH IS NOT A FAILURE: the verdict is the output, not the
 *  exit status, so benching several in a row does not stop at the first
 *  one over budget. It exits 0 whenever it measured; a sketch that never
 *  built, or a surface that could not be allocated, exits 1. */
int runBench(sketch::Host& host, const CaptureOptions& options,
             const std::filesystem::path& path, std::string_view runtime) {
  if (!awaitFirstBuild(host, runtime)) return 1;
  const SkSize canvas = host.canvasSize();
  const int width = std::max(1, (int)(canvas.width() * options.scale));
  const int height = std::max(1, (int)(canvas.height() * options.scale));
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  if (!surface) {
    std::fprintf(stderr, "bench: could not allocate a %dx%d surface\n", width,
                 height);
    return 1;
  }
  SkCanvas& sk = *surface->getCanvas();
  const SkColor background =
      sigil::material::skia::toSkColor(host.background()).toSkColor();

  // Forces the pixels to exist. On raster this is already true when the
  // draw returns, and this path is raster by construction; the readback
  // is cheap insurance.
  SkBitmap probe;
  probe.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  const auto flush = [&] { (void)surface->readPixels(probe.pixmap(), 0, 0); };

  const double dt = 1.0 / options.fps;
  // A FIXED dt is not a neutral simplification for anything that
  // memoizes on a per-frame value. Every animated value is a function of
  // elapsed time, so under a fixed step the values a scene visits repeat
  // on the scene's own period — a memo keyed on one saturates within a
  // period, and a per-distinct-value cost that grows without bound reads
  // as free. A wall-clock host never revisits a value, so the regime a
  // fixed step measures is one the running application never enters.
  //
  // The sequence is a golden-ratio rotation: irrational, so it never
  // returns to a step it already took, and deterministic, so two runs
  // measure the same frames. It is off unless asked for, because every
  // other stepping path here depends on the fixed step.
  double jitterPhase = 0.0;
  const auto nextDt = [&] {
    if (options.jitterDt <= 0.0) return dt;
    constexpr double kGolden = 0.6180339887498949;
    jitterPhase = std::fmod(jitterPhase + kGolden, 1.0);
    return dt * (1.0 + options.jitterDt * (2.0 * jitterPhase - 1.0));
  };
  const auto step = [&] {
    sk.clear(background);
    sk.save();
    sk.scale(options.scale, options.scale);
    const bool ok = host.frame(sk, nextDt());
    sk.restore();
    if (ok) flush();
    return ok;
  };

  // A warm-up, not a capture: what it has to reach is the state where
  // programs, bakes and atlases are hot, and any stretch of frames does
  // that. So an unstated --at takes the fallback here rather than the
  // sketch's declared moment, and the measured run stays the same run
  // whatever moment the author chose to photograph.
  const double warmSeconds = options.at >= 0.0 ? options.at : kFallbackMoment;
  const int warmup = std::max(1, (int)std::lround(warmSeconds / dt));
  for (int i = 0; i < warmup; ++i)
    if (!step()) return 1;

  // One profiled frame BEFORE the timed run, so a failure can name the
  // node instead of only the phase. Profiling costs a little, so it does
  // not ride along with the measured frames.
  std::vector<std::string> hot;
  if (sketch::Session* session = host.session()) {
    session->setProfiling(true);
    if (!step()) return 1;
    hot = session->costs(12);
    session->setProfiling(false);
  }

  std::vector<double> frames, updates, draws;
  std::vector<std::vector<double>> lanes;
  std::vector<std::string> laneNames;
  frames.reserve((size_t)options.benchFrames);
  for (int i = 0; i < options.benchFrames; ++i) {
    const sigil::measure::Stopwatch watch;
    if (!step()) return 1;
    frames.push_back(sigil::measure::Milliseconds(watch.elapsed()).count());
    if (sketch::Session* session = host.session()) {
      const sketch::Timing timing = session->timing();
      updates.push_back(timing.updateMs);
      draws.push_back(timing.drawMs);
      const std::span<const sketch::LaneCost> frameLanes = session->lanes();
      lanes.resize(frameLanes.size());
      laneNames.resize(frameLanes.size());
      for (size_t l = 0; l < frameLanes.size(); ++l) {
        laneNames[l] = frameLanes[l].name;
        lanes[l].push_back(frameLanes[l].ms);
      }
    }
  }

  // The tails off one sort; the mean and the worst frame off one pass.
  const auto mean = [](const std::vector<double>& values) {
    return sigil::measure::summary(values).mean();
  };
  const std::vector<double> tails =
      sigil::measure::quantiles(frames, std::vector<double>{0.50, 0.95, 0.99});
  const double p50 = tails[0];
  const double p95 = tails[1];
  const double p99 = tails[2];
  const sigil::measure::Summary frameSummary = sigil::measure::summary(frames);

  // A DECLARED PLATE IS JUDGED ON ITS CAPTURE COST, not on 60 FPS. Its
  // subject is the size of the sheet it draws, so the still it is
  // photographed as is what a reader waits on, and the frame-time gate a
  // live scene must pass does not bind it. The mark is read here, off the
  // running session's declared canvas — never a per-sketch timeout
  // override, and nothing about the plate sweep changes.
  const bool plateOnly = host.plateOnly();
  double captureMs = 0.0;
  if (plateOnly) {
    if (sketch::Session* session = host.session()) {
      sk_sp<SkSurface> plate =
          SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
      if (plate) {
        plate->getCanvas()->clear(background);
        const sigil::measure::Stopwatch watch;
        session->still(*plate->getCanvas());
        (void)plate->readPixels(probe.pixmap(), 0, 0);  // force completion
        captureMs = sigil::measure::Milliseconds(watch.elapsed()).count();
      }
    }
  }
  const bool pass = plateOnly || p99 < kFrameBudgetMs;

  // One machine-readable line, prefixed so collectors can find it. The
  // step regime is on the line rather than only in the invocation: a
  // jittered number and a fixed-step number answer different questions
  // and must not be compared as though they were one measurement.
  std::printf(
      "BENCH %s %dx%d frames=%d step=%s p50=%.2fms p95=%.2fms p99=%.2fms "
      "mean=%.2fms max=%.2fms fps50=%.1f VERDICT=%s\n",
      path.stem().string().c_str(), width, height, (int)frames.size(),
      options.jitterDt > 0.0 ? "jittered" : "fixed", p50, p95, p99,
      frameSummary.mean(), frameSummary.max(), p50 > 0 ? 1000.0 / p50 : 0.0,
      plateOnly ? "PLATE" : (pass ? "PASS" : "FAIL"));
  std::printf("  phases (mean ms): update %.2f · draw %.2f", mean(updates),
              mean(draws));
  for (size_t l = 0; l < lanes.size(); ++l)
    std::printf(" · %s %.2f", laneNames[l].c_str(), mean(lanes[l]));
  std::printf("\n");
  if (sketch::Session* session = host.session())
    std::printf("  last frame: %s\n", session->counters().c_str());
  if (!hot.empty()) {
    std::printf("  most expensive nodes (self ms, excluding children):\n");
    for (const std::string& line : hot) std::printf("    %s\n", line.c_str());
  }
  if (plateOnly) {
    std::printf(
        "  PLATE — declared plate(), so it is judged on its CAPTURE COST,\n"
        "  not on 60 FPS: the still it is photographed as took %.2f ms at\n"
        "  %dx%d. The frame-time gate does not bind a sheet whose subject\n"
        "  is its own size; the plate sweep steps and captures it as ever.\n",
        captureMs, width, height);
  } else if (pass) {
    std::printf(
        "  PASS — p99 %.2f ms is inside the %.1f ms budget (%.0f FPS "
        "gate)\n",
        p99, kFrameBudgetMs, 1000.0 / kFrameBudgetMs);
  } else {
    const bool paintBound = mean(draws) >= mean(updates);
    std::printf(
        "\n  FAIL — p99 %.2f ms EXCEEDS the %.1f ms budget.\n"
        "  This sketch does NOT hold 60 FPS at %dx%d, and %s dominates.\n",
        p99, kFrameBudgetMs, width, height,
        paintBound ? "DRAWING" : "DESCRIBING");
    if (paintBound)
      std::printf(
          "  Per-pixel cost, not tree cost. A recording is not a pixel\n"
          "  cache: it stores the draw CALLS, so replaying it re-runs\n"
          "  every shader over every pixel again. Only a bake keeps the\n"
          "  pixels. Read the 'not baked' line under each node above; the\n"
          "  library bakes what it can prove is safe, and the common\n"
          "  refusal is yours to override by asking for the bake.\n");
    else
      std::printf(
          "  You are rebuilding the tree every frame. Describe once in\n"
          "  setup() and bind outputs, or memo the subtrees whose props\n"
          "  did not change.\n");
  }
  std::fflush(stdout);
  return 0;
}

int runFrames(sketch::Host& host, const CaptureOptions& options, bool gpu,
              std::string_view runtime,
              sigil::skia::GraphiteContext* canvasGraphite) {
  if (!awaitFirstBuild(host, runtime)) return 1;
  struct CaptureScope {
    sketch::Host* host = nullptr;
    std::unique_ptr<sigil::skia::PaintOrderCanvas> canvas;
    ~CaptureScope() {
      if (host) host->setCaptureBackend({});
    }
  } capture;
  if (gpu) {
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
  // that moment. A runtime that re-renders its still moves the scene one
  // step of its own to take it, so the next frame of a sequence is that
  // much nearer already; a rate faster than that step spaces its frames
  // at the step.
  const double dt = 1.0 / options.fps;
  const double stillStep = host.session() ? host.session()->stillStep() : 0.0;

  for (int index = 0; index < options.frames; ++index) {
    const std::string path = options.frames > 1
                                 ? numberedPath(options.outputPath, index + 1)
                                 : options.outputPath;
    if (!host.writePhotograph(path, options.scale)) {
      std::fprintf(stderr, "failed to write %s\n", path.c_str());
      return 1;
    }
    const double between = dt - stillStep;
    if (index + 1 < options.frames && between > 1e-12) {
      try {
        host.prepareCapture(between, options.fps, options.scale);
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

int runPluginSweep(sketch::Host& host, const sketch::SweepOptions& sweep,
                   CaptureOptions capture,
                   sigil::skia::GraphiteContext* canvasGraphite) {
  if (!awaitFirstBuild(host, sweep.kind)) return 1;
  if (sweep.promotion && sweep.noPromotion) {
    std::fprintf(stderr,
                 "--promotion and --no-promotion ask for opposite runs\n");
    return 2;
  }
  if (sweep.countPlane) {
    std::fprintf(stderr,
                 "--composites is unavailable for a native plugin capture\n");
    return 2;
  }
  sketch::Session* session = host.session();
  session->setAutoPromotion(sweep.promotion ? sketch::Session::Promotion::Eager
                                            : sketch::Session::Promotion::Off);
  if (sweep.promotion && session->canvas().nonlinearPicture)
    std::printf("plugin: declared nonlinear\n");
  capture.scale =
      sweep.density > 0 ? sweep.density : sketch::plateDensity(*session);
  std::error_code error;
  std::filesystem::create_directories(sweep.outputDirectory, error);
  if (error) {
    std::fprintf(stderr, "could not create plate directory: %s\n",
                 error.message().c_str());
    return 1;
  }
  capture.outputPath = (std::filesystem::path(sweep.outputDirectory) /
                        (std::string(sketch::kPlatePrefix) +
                         host.sketchPath().stem().string() + ".png"))
                           .string();
  return runFrames(host, capture, sweep.gpu, {}, canvasGraphite);
}
