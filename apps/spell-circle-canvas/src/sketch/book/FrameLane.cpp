/** @file
 * The still and the frame-time gate, over one file the host has built.
 */

#include "FrameLane.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmeasure/stats/Samples.h>
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilsketch/core/Crash.h>
#include <sigilsketch/core/Session.h>
#include <sigilsketch/live/Host.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
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

/** Blocks until the first build lands (or fails); false = never got
 *  live. */
bool awaitFirstBuild(sketch::Host& host) {
  using namespace std::chrono_literals;
  for (int i = 0; i < 1200; ++i) {
    host.poll();
    if (host.live() || !host.errorLog().empty()) break;
    std::this_thread::sleep_for(50ms);
  }
  if (!host.live()) {
    std::fprintf(stderr, "sketch failed to build:\n%s\n",
                 host.errorLog().c_str());
    return false;
  }
  return true;
}

}  // namespace

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
             const std::filesystem::path& path) {
  if (!awaitFirstBuild(host)) return 1;
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
  const SkColor background = host.background().toSkColor();

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
    host.frame(sk, nextDt());
    sk.restore();
    flush();
  };

  // A warm-up, not a capture: what it has to reach is the state where
  // programs, bakes and atlases are hot, and any stretch of frames does
  // that. So an unstated --at takes the fallback here rather than the
  // sketch's declared moment, and the measured run stays the same run
  // whatever moment the author chose to photograph.
  const double warmSeconds = options.at >= 0.0 ? options.at : kFallbackMoment;
  const int warmup = std::max(1, (int)std::lround(warmSeconds / dt));
  for (int i = 0; i < warmup; ++i) step();

  // One profiled frame BEFORE the timed run, so a failure can name the
  // node instead of only the phase. Profiling costs a little, so it does
  // not ride along with the measured frames.
  std::vector<std::string> hot;
  if (sketch::Session* session = host.session()) {
    session->setProfiling(true);
    step();
    hot = session->costs(12);
    session->setProfiling(false);
  }

  std::vector<double> frames, updates, draws;
  std::vector<std::vector<double>> lanes;
  std::vector<std::string> laneNames;
  frames.reserve((size_t)options.benchFrames);
  for (int i = 0; i < options.benchFrames; ++i) {
    const sigil::measure::Stopwatch watch;
    step();
    frames.push_back(watch.elapsedMs());
    if (sketch::Session* session = host.session()) {
      const sketch::Timing timing = session->timing();
      updates.push_back(timing.updateMs);
      draws.push_back(timing.drawMs);
      const std::span<const sketch::Lane> frameLanes = session->lanes();
      lanes.resize(frameLanes.size());
      laneNames.resize(frameLanes.size());
      for (size_t l = 0; l < frameLanes.size(); ++l) {
        laneNames[l] = frameLanes[l].name;
        lanes[l].push_back(frameLanes[l].ms);
      }
    }
  }

  const auto mean = [](const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    double sum = 0;
    for (double x : v) sum += x;
    return sum / (double)v.size();
  };
  std::vector<double> sorted = frames;
  std::sort(sorted.begin(), sorted.end());
  const double p50 = sigil::measure::quantile(sorted, 0.50);
  const double p95 = sigil::measure::quantile(sorted, 0.95);
  const double p99 = sigil::measure::quantile(sorted, 0.99);

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
        captureMs = watch.elapsedMs();
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
      mean(frames), sorted.empty() ? 0.0 : sorted.back(),
      p50 > 0 ? 1000.0 / p50 : 0.0,
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

int runFrames(sketch::Host& host, const CaptureOptions& options) {
  if (!awaitFirstBuild(host)) return 1;
  // THE MOMENT THE SKETCH DECLARED wins over any number this program
  // could pick, because a still of an animation is a claim about that
  // animation and the author is the one who knows which frame makes it.
  // A stated --at overrides the declaration; a sketch declaring nothing
  // gets the fallback. The declaration is only readable once a body has
  // run its setup, which is why it is read here and not while parsing.
  const double declared = host.captureSeconds();
  const double at = options.at >= 0.0
                        ? options.at
                        : (declared > 0.0 ? declared : kFallbackMoment);
  // Step the clock to that moment with a fixed step, on a tiny scratch
  // surface: the real pixels come from the capture below.
  const double dt = 1.0 / options.fps;
  sk_sp<SkSurface> scratch =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(8, 8));
  const int warmup = std::max(1, (int)std::lround(at / dt));
  for (int i = 0; i < warmup; ++i) host.frame(*scratch->getCanvas(), dt);

  for (int index = 0; index < options.frames; ++index) {
    const std::string path =
        options.frames > 1 ? numberedPath(options.out, index + 1) : options.out;
    {
      sketch::PhaseMark mark(sketch::Phase::Capture);
      if (!host.capture(path, options.scale)) {
        std::fprintf(stderr, "failed to write %s\n", path.c_str());
        return 1;
      }
    }
    if (index + 1 < options.frames) host.frame(*scratch->getCanvas(), dt);
  }
  std::printf(
      "wrote %s (%d frame%s at %.3gx, t=%.3gs %s, build %d, work %.2f ms "
      "avg)\n",
      options.out.c_str(), options.frames, options.frames == 1 ? "" : "s",
      options.scale, at,
      options.at >= 0.0 ? "asked for"
                        : (declared > 0.0 ? "declared" : "by default"),
      host.generation(), host.workMsAverage());
  return 0;
}
