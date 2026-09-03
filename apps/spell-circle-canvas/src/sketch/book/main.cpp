/** @file
 * Sketchbook: the one live application, and the one headless renderer.
 *
 *   Sketchbook [--no-gpu]                      the app, on the last sketch
 *   Sketchbook --sketch <name>                 the app, on that one
 *   Sketchbook --list [--kind canvas|set]      the registry, one per line
 *   Sketchbook --headless <outdir> [--gpu] [--sketch <name>] [--kind <k>]
 *              [--ledger] [--no-promotion] [--capture-at <s>]
 *              [--timing-json <path>]          plates, and the timing table
 *   Sketchbook <file.cpp> [--frame <png>] [--bench] [--gpu]
 *                                              a file, live or measured
 *   Sketchbook <file.cpp>                      the app, on that file
 *   Sketchbook <stem>/<stem>.cpp               …either way, a sketch that
 *                                              is a directory, by its entry
 *   Sketchbook --window-bench [<sec>] [--window-size <WxH>]
 *              [--window-scale <n>] [--sketch <name>] [--kind <k>]
 *                                              the window's own frame rate
 *   … [--assets <dir>]                         where res:// mounts
 *   … [--plates <dir>]                        the stills the browser shows
 *
 * `--sketch` takes a case-insensitive substring and answers to a
 * sketch's filed name or its file stem, which is the loop for visual
 * iteration.
 *
 * A `.cpp` PATH IS TAKEN WHEREVER IT STANDS. The file joins the app's
 * list under its own stem and opens there, and it is compiled and
 * watched exactly as a sketch in this repository is. `--assets` names
 * the directory that mounts at `res://`; without it a sketch reads
 * `assets/` beside its own file.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmeasure/stats/Samples.h>
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Sources.h>
#include <sigilsketch/live/Crash.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/plate/Sweep.h>
#include <sigilsketch/set/Set.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

#ifndef SIGILSKETCH_NO_DEVICE
#include <sigilgeometry/device/Device.h>
#include <sigilworld/diligent/Painter.h>
#include <sigilworld/diligent/Runtime.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "SketchCatalog.h"
#include "SketchbookView.h"

namespace sketch = sigil::sketch;

namespace {

/** The 60 FPS gate, in milliseconds per frame. */
constexpr double kFrameBudgetMs = 16.6;

/** The default `--jitter-dt` amplitude: ±35% around the nominal
 *  interval, which is the spread a windowed host delivers when it is
 *  comfortably inside its budget and the compositor is merely uneven. */
constexpr double kDefaultJitter = 0.35;

/** How long `--window-bench` measures each sketch, and how long it lets
 *  one run before it starts. The measured stretch is longer than either
 *  rolling window the readout comes from, so what it reports is entirely
 *  frames from the measured stretch; the warm-up is what pays for the
 *  first frames' program compiles, texture bakes and glyph atlases. */
constexpr double kWindowBenchSeconds = 2.5;
constexpr double kWindowBenchWarmupSeconds = 1.2;

/** The compiler line the build captured, which lands beside the binaries
 *  rather than inside the bundle: a macOS application is a directory, and
 *  its executable sits three levels down inside it. */
std::filesystem::path flagsFileNear(const std::filesystem::path& exeDir) {
  std::filesystem::path beside = exeDir / "sketch_flags.rsp";
  if (std::filesystem::exists(beside)) return beside;
  return exeDir.parent_path().parent_path().parent_path() / "sketch_flags.rsp";
}

std::filesystem::path executableDir(const char* argv0) {
#ifdef __APPLE__
  char buffer[4096];
  uint32_t size = sizeof buffer;
  if (_NSGetExecutablePath(buffer, &size) == 0)
    return std::filesystem::canonical(buffer).parent_path();
#endif
  std::error_code ec;
  auto canonical = std::filesystem::canonical(argv0, ec);
  return ec ? std::filesystem::current_path() : canonical.parent_path();
}

sigil::weave::FontContext& fonts() {
  // Leaked deliberately: it owns Skia-backed state, and a static
  // destructor racing Skia teardown is a class of crash worth not having.
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

sketch::Assets& assets() {
  static auto* store = new sketch::Assets(SIGIL_SKETCH_ASSET_DIR);
  return *store;
}

#ifndef SIGILSKETCH_NO_DEVICE
/** Held for the run: the runtime borrows the device, and every texture
 *  and pipeline it made goes when the device does. */
std::unique_ptr<sigil::geometry::device::Device> g_device;

/** Puts every set sketch on the device, and says whether it could. The
 *  sweep treats a false answer as fatal because drawing the CPU's
 *  picture under a name that asked for the device's would put two
 *  different pictures under one name; the live host carries on, because
 *  a window can say which tier it is showing. */
bool useDevice() {
  std::string error;
  const sigil::geometry::device::DeviceConfig config;
  g_device = sigil::geometry::device::Device::create(config, &error);
  if (!g_device) {
    std::fprintf(stderr, "no device runtime (%s)\n", error.c_str());
    return false;
  }
  sketch::useRuntime(sigil::world::diligent::runtime(*g_device));
  // …and the 2D twin: a canvas sketch that stands a mesh up in space
  // reaches the same device through sketch::painterRuntime().
  sketch::usePainterRuntime(sigil::world::diligent::painterRuntime(*g_device));
  return true;
}

/** Lets the device go while the process is still running. It outlives
 *  every frame that used it and must go BEFORE the process does:
 *  released after its own queue, the textures and pipelines it made take
 *  their teardown into static destruction, where the locks they want no
 *  longer exist. */
void releaseDevice() {
  sketch::useRuntime({});
  sketch::usePainterRuntime({});
  g_device.reset();
}
#else
bool useDevice() {
  std::fprintf(stderr,
               "no device runtime (this binary was built without one)\n");
  return false;
}

void releaseDevice() {}
#endif

/** True when the selection holds anything drawn through the set
 *  runtime — the only reason to bring a device up. */
bool selectionNeedsDevice(int only, const std::string& kind) {
  const auto& entries = sketch::registry();
  const int first = only >= 0 ? only : 0;
  const int last = only >= 0 ? only + 1 : (int)entries.size();
  for (int i = first; i < last && i < (int)entries.size(); ++i) {
    if (!kind.empty() && kind != "set") continue;
    const sketch::Kind entry = entries[i].kind();
    if (entry && entry->runtime() == "set") return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Running one file, headless

/** Where a capture lands when neither the caller nor the sketch says. */
constexpr double kFallbackMoment = 1.5;

struct CaptureOptions {
  std::string out;
  /** Seconds of fixed-step stepping before the capture, as the CALLER
   *  stated it. Negative means unstated, which is not the same as 1.5:
   *  a still then lands at the moment the sketch itself declared, and
   *  only a sketch that declares none falls back to the number. */
  double at = -1.0;
  float scale = 1.0f;  // multiplier over the sketch's canvas size
  int frames = 1;      // >1 captures a numbered sequence
  double fps = 60.0;   // fixed-step rate
  bool bench = false;  // measure, do not write
  int benchFrames = 120;
  double jitterDt = 0.0;
};

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

/** The frame-time gate, measured on the sketch's REAL canvas.
 *
 *  Every frame runs the true per-frame path — clear, advance, draw, and
 *  a readback that forces the raster to complete rather than leaving
 *  work queued behind the timer. Warm to `--at` first: the first frames
 *  of any sketch pay for program compiles, texture bakes, snapshots and
 *  glyph atlases, and folding those into the sample measures the wrong
 *  thing.
 *
 *  Always exits 0. The verdict is the output, not the exit status, so a
 *  slow sketch does not abort a pipeline that is benching several. */
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
  const bool pass = p99 < kFrameBudgetMs;

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
      p50 > 0 ? 1000.0 / p50 : 0.0, pass ? "PASS" : "FAIL");
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
  if (pass) {
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

// ---------------------------------------------------------------------------
// The window's own frame-rate lane

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
 *  It drives the selection through the same property QML sets, so every
 *  switch takes the same path a reader's click does — the resident set
 *  included. False when there is nothing to present. */
bool startWindowBench(QGuiApplication& application, QQuickWindow& window,
                      QObject& view, const WindowBench& options,
                      std::vector<int> selection) {
  if (selection.empty()) {
    std::fprintf(stderr, "--window-bench: nothing selected\n");
    return false;
  }
  struct Run {
    std::vector<int> selection;
    size_t at = 0;
    bool measuring = false;
    std::chrono::steady_clock::time_point phaseStart;
  };
  auto run = std::make_shared<Run>();
  run->selection = std::move(selection);
  run->phaseStart = std::chrono::steady_clock::now();
  view.setProperty("sketchIndex", run->selection[0]);

  auto* timer = new QTimer(&application);
  timer->setInterval(8);
  QObject::connect(
      timer, &QTimer::timeout, &application,
      [&window, &view, options, run, timer] {
        // Each tick asks the item for a frame. The window drives itself
        // once it is presenting, and the ask costs nothing when it is
        // already going — but a window a compositor has stopped giving
        // frames to would otherwise be measured as a sketch that stopped
        // drawing, which is a different finding entirely.
        if (auto* item = qobject_cast<QQuickItem*>(&view)) item->update();
        const auto now = std::chrono::steady_clock::now();
        const double elapsed =
            std::chrono::duration<double>(now - run->phaseStart).count();
        if (!run->measuring) {
          if (elapsed < kWindowBenchWarmupSeconds) return;
          run->measuring = true;
          run->phaseStart = now;
          return;
        }
        if (elapsed < options.seconds) return;

        const sketch::Entry& entry =
            sketch::registry()[run->selection[run->at]];
        {
          QMutexLocker lock(&SketchbookView::hostMutex);
          const sketch::Host* host = SketchbookView::host;
          const QVariantMap metrics = view.property("metrics").toMap();
          const sketch::Kind kind = entry.kind();
          const std::string_view runtime = kind ? kind->runtime() : "?";
          const SkSize canvas = host ? host->canvasSize() : SkSize::Make(0, 0);
          const double work = host ? host->workMsAverage() : 0.0;
          // KEYED BY THE STEM, not by the filed name: the line is one
          // whitespace-separated record, and a filed name carries spaces
          // — "aero desktop" would be read as the name "aero" followed
          // by a field nobody wrote. The stem cannot contain a space and
          // is what --sketch already takes.
          std::printf(
              "WINDOW %s window=%dx%d@%g canvas=%dx%d kind=%.*s fps=%.1f "
              "work=%.2fms p99=%.2fms draw=%.2fms submit=%.2fms "
              "headroom=%.1f\n",
              entry.key, window.width(), window.height(),
              window.devicePixelRatio(), (int)canvas.width(),
              (int)canvas.height(), (int)runtime.size(), runtime.data(),
              host ? host->presentedFps() : 0.0, work,
              host ? host->workMsP99() : 0.0,
              host ? host->drawMsAverage() : 0.0,
              metrics.value(QStringLiteral("submitMs")).toDouble(),
              work > 0 ? 1000.0 / work : 0.0);
          std::fflush(stdout);
        }

        if (++run->at >= run->selection.size()) {
          timer->stop();
          QCoreApplication::quit();
          return;
        }
        view.setProperty("sketchIndex", run->selection[run->at]);
        run->measuring = false;
        run->phaseStart = now;
      });
  timer->start();
  return true;
}

/** The registry entries `--window-bench` will present: the whole table,
 *  or what `--sketch` and `--kind` narrow it to. A sketch this machine
 *  cannot run is named as stood down and left out, exactly as the sweep
 *  passes over it — a skip is not a failure and not a measurement. */
std::vector<int> windowBenchSelection(int only, const std::string& kind) {
  std::vector<int> selection;
  const auto& entries = sketch::registry();
  const int first = only >= 0 ? only : 0;
  const int last = only >= 0 ? only + 1 : (int)entries.size();
  for (int i = first; i < last && i < (int)entries.size(); ++i) {
    if (!kind.empty()) {
      const sketch::Kind entryKind = entries[i].kind();
      if (!entryKind || entryKind->runtime() != kind) continue;
    }
    std::string why;
    if (!entries[i].available(&why)) {
      std::printf("WINDOW %s SKIPPED %s\n", entries[i].key, why.c_str());
      continue;
    }
    selection.push_back(i);
  }
  std::fflush(stdout);
  return selection;
}

}  // namespace

// an uncaught exception ends the app with its message
// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[]) {
  std::filesystem::path sketchFile;
  std::filesystem::path assetsOverride;
  std::string selected, kind, shotPath;
  sketch::SweepOptions sweepOptions;
  CaptureOptions capture;
  WindowBench windowBench;
  bool headless = false, list = false, gpu = false, noGpu = false;
  std::optional<bool> deterministic;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--headless") {
      headless = true;
      if (i + 1 < argc && argv[i + 1][0] != '-')
        sweepOptions.outDir = argv[++i];
    } else if (arg == "--list") {
      list = true;
    } else if (arg == "--gpu") {
      gpu = true;
    } else if (arg == "--no-gpu") {
      noGpu = true;
    } else if (arg == "--kind" && i + 1 < argc) {
      kind = argv[++i];
    } else if (arg == "--sketch" && i + 1 < argc) {
      selected = argv[++i];
    } else if (arg == "--ledger") {
      sweepOptions.ledger = true;
    } else if (arg == "--no-promotion") {
      sweepOptions.noPromotion = true;
    } else if (arg == "--capture-at" && i + 1 < argc) {
      sweepOptions.captureAt = std::strtod(argv[++i], nullptr);
    } else if (arg == "--timing-json" && i + 1 < argc) {
      sweepOptions.timingJson = argv[++i];
    } else if (arg == "--shot" && i + 1 < argc) {
      shotPath = argv[++i];
    } else if (arg == "--assets" && i + 1 < argc) {
      assetsOverride = argv[++i];
    } else if (arg == "--plates" && i + 1 < argc) {
      SketchCatalog::platesDir = argv[++i];
    } else if (arg == "--window-bench") {
      // The stretch is optional: a bare flag takes the default, and only
      // a following token that reads as a number is consumed.
      windowBench.seconds = kWindowBenchSeconds;
      if (i + 1 < argc) {
        const std::string next = argv[i + 1];
        if (!next.empty() &&
            (std::isdigit((unsigned char)next[0]) || next[0] == '.')) {
          windowBench.seconds = std::stod(next);
          ++i;
        }
      }
    } else if (arg == "--window-size" && i + 1 < argc) {
      const std::string size = argv[++i];
      const size_t by = size.find('x');
      if (by == std::string::npos) {
        std::fprintf(stderr, "--window-size wants WIDTHxHEIGHT\n");
        return 2;
      }
      windowBench.width = std::max(1, std::stoi(size.substr(0, by)));
      windowBench.height = std::max(1, std::stoi(size.substr(by + 1)));
    } else if (arg == "--window-scale" && i + 1 < argc) {
      windowBench.scale = std::stod(argv[++i]);
    } else if (arg == "--frame" && i + 1 < argc) {
      capture.out = argv[++i];
    } else if (arg == "--at" && i + 1 < argc) {
      capture.at = std::stod(argv[++i]);
    } else if (arg == "--scale" && i + 1 < argc) {
      capture.scale = std::stof(argv[++i]);
    } else if (arg == "--frames" && i + 1 < argc) {
      capture.frames = std::max(1, std::stoi(argv[++i]));
    } else if (arg == "--fps" && i + 1 < argc) {
      capture.fps = std::stod(argv[++i]);
    } else if (arg == "--bench") {
      capture.bench = true;
    } else if (arg == "--bench-frames" && i + 1 < argc) {
      capture.benchFrames = std::max(1, std::stoi(argv[++i]));
    } else if (arg == "--deterministic") {
      deterministic = true;
    } else if (arg == "--no-deterministic") {
      deterministic = false;
    } else if (arg == "--jitter-dt") {
      // The amplitude is optional: a bare flag takes the default, and
      // only a following token that reads as a number is consumed.
      capture.jitterDt = kDefaultJitter;
      if (i + 1 < argc) {
        const std::string next = argv[i + 1];
        if (!next.empty() &&
            (std::isdigit((unsigned char)next[0]) || next[0] == '.')) {
          capture.jitterDt = std::stod(next);
          ++i;
        }
      }
    } else if (sketchFile.empty() && arg.size() > 4 &&
               arg.compare(arg.size() - 4, 4, ".cpp") == 0) {
      sketchFile = arg;
    } else {
      std::fprintf(stderr, "unknown argument \"%s\"\n", arg.c_str());
      return 2;
    }
  }

  const int chosen = selected.empty() ? -1 : sketch::find(selected);
  if (!selected.empty() && chosen < 0) {
    std::fprintf(stderr, "no sketch matches \"%s\"; known sketches:\n",
                 selected.c_str());
    const auto& entries = sketch::registry();
    for (int i = 0; i < (int)entries.size(); ++i)
      std::fprintf(stderr, "  %2d  %-24s %s\n", i, entries[i].name,
                   entries[i].category);
    return 1;
  }

  if (list) {
    // Spelled the way a plate names it, because a script that selects a
    // sketch here looks for its plate under the same name.
    //
    // A SKETCH THIS MACHINE CANNOT RUN IS STILL LISTED, with what it is
    // missing after a tab — one line, two consumers. A reader sees the
    // entry greyed and the reason beside it; a script splits on the tab
    // and knows not to ask for a plate it has just been told cannot
    // exist. Dropping it from the listing would say the same thing by
    // saying nothing, which reads as a sketch that was deleted.
    const bool toTerminal = isatty(fileno(stdout)) != 0;
    for (const sketch::Entry& entry : sketch::registry()) {
      if (!kind.empty()) {
        const sketch::Kind entryKind = entry.kind();
        if (!entryKind || entryKind->runtime() != kind) continue;
      }
      std::string why;
      if (entry.available(&why)) {
        std::printf("%s\n", entry.name);
        continue;
      }
      std::printf("%s%s\tunavailable: %s%s\n", toTerminal ? "\x1b[2m" : "",
                  entry.name, why.c_str(), toTerminal ? "\x1b[0m" : "");
    }
    return 0;
  }

  if (headless) {
    sweepOptions.only = chosen;
    sweepOptions.kind = kind;
    sweepOptions.gpu = gpu;
    // A device is brought up only when a SET in the selection draws
    // through one. A canvas sketch's device lane is the surface the
    // sweep allocates, and its mesh painter stays on the CPU executor
    // here whatever the flag says: a plate is hashed from that executor,
    // and the two rasterise the same picture but not the same bytes.
    if (gpu && selectionNeedsDevice(chosen, kind) && !useDevice()) return 1;
    const int result = sweep(sweepOptions, fonts(), assets());
    releaseDevice();
    return result;
  }

  // ---- one file, live or measured -------------------------------------
  const std::filesystem::path sketchDir = SIGIL_SKETCH_DIR;
  // Whether the file came from the command line or was derived from a
  // registry selection: only the first is a sketch this binary was not
  // built with, and only the first joins the app's list on its own.
  const bool fileGiven = !sketchFile.empty();
  if (!fileGiven && chosen >= 0)
    sketchFile = sketch::sourceOf(sketchDir, sketch::registry()[chosen].key);

  sketch::Host::Options options;
  // DETERMINISTIC BY DEFAULT WHEN CAPTURING. A capture exists to be
  // looked at or diffed, and a sketch that draws its own bake time into
  // its own plate differs from itself between two runs — so a pixel
  // sweep reports it as changed by a patch that changed nothing. The
  // live host keeps its real numbers, which is where they are wanted.
  options.deterministic =
      deterministic.value_or(!capture.out.empty() && !capture.bench);
  // ASSETS STAND BESIDE THE SKETCH unless `--assets` says otherwise.
  // Leaving this empty is what asks the host for that default, and it is
  // the same answer for a sketch in this repository — whose assets stand
  // beside it too — as for a file anywhere else on disk, which is what
  // makes a directory outside this checkout a place to work.
  options.assetsDir = assetsOverride;
  options.flagsFile = flagsFileNear(executableDir(argv[0]));
  // THE SHARED LAYER IS THIS REPOSITORY'S, for every sketch the host
  // builds: a file anywhere on disk compiles with the same flags, so it
  // may spell <shared/Name.h> too, and then needs the module behind it.
  options.sharedDir = sketchDir / "shared";

  if (!capture.out.empty() || capture.bench) {
    if (sketchFile.empty() || !std::filesystem::exists(sketchFile)) {
      std::fprintf(stderr,
                   "usage: Sketchbook <sketch.cpp> [--frame <out.png>] "
                   "[--at <sec>] [--scale <n>]\n"
                   "         [--frames <count>] [--fps <n>] [--bench] "
                   "[--bench-frames <n>]\n"
                   "         [--gpu] [--jitter-dt [amplitude]] "
                   "[--deterministic | --no-deterministic]\n");
      return 2;
    }
    if (!std::filesystem::exists(options.flagsFile)) {
      std::fprintf(stderr, "missing %s (rebuild Sketchbook)\n",
                   options.flagsFile.string().c_str());
      return 2;
    }
    options.sketchPath = std::filesystem::absolute(sketchFile);
    // Installed before the guest can ever run: without it, a fault
    // inside a sketch is a bare signal with nothing printed.
    sketch::installCrashReporter(options.sketchPath);
    // `--gpu` PUTS THIS RUN ON THE DEVICE, exactly as it does for a
    // sweep: a set draws its frame there, and a canvas sketch's mesh
    // painter rasterises there. Fatal when the device will not come up,
    // because a run that asked for the device and quietly gave the CPU's
    // picture puts two different pictures under one name — which is the
    // one thing a capture must never do.
    if (gpu && !useDevice()) return 1;
    int result = 0;
    {
      sketch::Host host(std::move(options), fonts());
      result = capture.bench ? runBench(host, capture, host.sketchPath())
                             : runFrames(host, capture);
    }
    // The session goes before the device does: it holds textures and
    // pipelines the device made, and releasing the device first takes
    // their teardown into static destruction.
    releaseDevice();
    return result;
  }

  // ---- the app ---------------------------------------------------------
  // THE LIVE HOST DRAWS SETS ON THE DEVICE. A device is what runs a
  // material's own body: the CPU mesh executor has no compiler, so every
  // surface reaches it as the colour the frame extracted, and a reader
  // looking at a lit set in this window would be looking at a picture no
  // recipe ever ran in. Unlike the sweep's flag, a device that will not
  // come up is not fatal here — there is no plate whose name would then
  // stand over two different pictures, only a window that says which
  // tier it is showing.
  // The window's own frame-rate lane opens the window at a stated size
  // and asks Qt for a stated scale, which it can only be told before the
  // application exists.
  if (windowBench.scale > 0.0) {
    char factor[32];
    std::snprintf(factor, sizeof factor, "%g", windowBench.scale);
    setenv("QT_SCALE_FACTOR", factor, 1);
  }
  if (noGpu || !useDevice())
    std::fprintf(stderr,
                 "[sketchbook] sets draw on the CPU mesh executor: a "
                 "surface reaches it as the colour extract read off it\n");
  SketchbookView::sketchDir = sketchDir;
  // WHERE THE BROWSER'S THUMBNAILS COME FROM: the quick tier's baseline,
  // unless the command line already named somewhere else.
#ifdef SIGILSKETCH_PLATES_DIR
  if (SketchCatalog::platesDir.empty())
    SketchCatalog::platesDir = SIGILSKETCH_PLATES_DIR;
#endif
  SketchbookView::assetsDir = options.assetsDir;
  SketchbookView::flagsFile = options.flagsFile;
  SketchbookView::sharedDir = options.sharedDir;
  // A FILE ON THE COMMAND LINE OPENS THE WINDOW ON THAT FILE. The
  // registry is the compiled-in table and settles the first time it is
  // read, so the file joins a session-local list the app's own listing
  // reads after it, under its own stem — the dylib a hot-loaded sketch
  // exports carries neither key nor name.
  int openAt = chosen;
  if (fileGiven) {
    SketchbookView::externals.push_back(std::filesystem::absolute(sketchFile));
    openAt = (int)sketch::registry().size();
  }
  sketch::installCrashReporter(sketchFile.empty() ? sketchDir : sketchFile);

  QGuiApplication application(argc, argv);
  QGuiApplication::setOrganizationDomain("sigil.dev");
  QGuiApplication::setApplicationName("Sketchbook");

  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
      [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
  engine.loadFromModule("Sigil.Sketchbook", "Main");

  const QObjectList& roots = engine.rootObjects();
  auto* window =
      roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(roots.first());
  QObject* view = nullptr;
  if (window)
    for (QObject* child : window->findChildren<QObject*>())
      if (child->property("sketchIndex").isValid() &&
          child->property("metrics").isValid()) {
        view = child;
        break;
      }
  if (view && openAt >= 0) view->setProperty("sketchIndex", openAt);

  if (windowBench.seconds > 0.0) {
    if (!window || !view) {
      std::fprintf(stderr, "--window-bench: no window to present in\n");
      return 1;
    }
    window->resize(windowBench.width, windowBench.height);
    window->raise();
    window->requestActivate();
    if (!startWindowBench(application, *window, *view, windowBench,
                          windowBenchSelection(chosen, kind)))
      return 1;
  }

  if (!shotPath.empty()) {
    if (!window || !view) {
      std::fprintf(stderr, "--shot: no window to grab\n");
      return 1;
    }
    // Drive real frames rather than waiting for them. An unfocused
    // window gets no render loop from the compositor, so a single
    // delayed grab catches a sketch that has not started — grabWindow()
    // is what makes the thing run. Marking the item dirty first is the
    // part that is easy to miss: without it the grab re-renders the
    // existing scene-graph node and never synchronizes, so the metrics
    // panel keeps showing what it had before anything was activated.
    auto* warm = new QTimer(&application);
    auto framesLeft = std::make_shared<int>(90);
    auto patience = std::make_shared<int>(900);
    warm->setInterval(16);
    QObject::connect(
        warm, &QTimer::timeout, &application,
        [window, view, shotPath, warm, framesLeft, patience] {
          if (auto* item = qobject_cast<QQuickItem*>(view)) item->update();
          // A SKETCH THIS BINARY DOES NOT CARRY HAS TO BE BUILT TO BE
          // SEEN, which takes longer than the warm-up does. So the
          // warm-up does not begin until something is live: otherwise a
          // grab of a file opened by path is always a picture of a
          // window compiling. The patience is bounded, because a file
          // that will never compile still has to be photographed —
          // the error overlay is what there is to look at.
          bool live = false;
          {
            QMutexLocker lock(&SketchbookView::hostMutex);
            live = SketchbookView::host && SketchbookView::host->live();
          }
          if (!live && --*patience > 0) {
            window->grabWindow();
            return;
          }
          if (--*framesLeft > 0) {
            window->grabWindow();
            return;
          }
          warm->stop();
          const QImage image = window->grabWindow();
          if (image.isNull() ||
              !image.save(QString::fromStdString(shotPath), "PNG")) {
            std::fprintf(stderr, "--shot: grab failed\n");
            QCoreApplication::exit(1);
            return;
          }
          std::printf("wrote %s (%dx%d)\n", shotPath.c_str(), image.width(),
                      image.height());
          QCoreApplication::quit();
        });
    warm->start();
  }
  const int status = QGuiApplication::exec();
  // The device outlives every frame that used it and must go before the
  // process does — and before it, whatever still holds textures it made:
  // released after its own queue, those take their teardown into static
  // destruction, where the locks they want no longer exist.
  {
    QMutexLocker lock(&SketchbookView::hostMutex);
    SketchbookView::sessions.clear();
    SketchbookView::host = nullptr;
  }
  releaseDevice();
  return status;
}
