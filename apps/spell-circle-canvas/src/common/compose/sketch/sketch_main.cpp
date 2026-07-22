// ComposeSketch — the p5-style live-coding host for SigilCompose.
//
//   ComposeSketch <sketch.cpp> [--assets <dir>] [--frame <out.png>]
//                              [--bench]
//
// Windowed (default): opens the live canvas, watches the sketch file,
// recompiles on save, hot-swaps the running sketch; compile errors
// overlay while the last good build keeps running.
//
// --frame: headless single-shot — compile, load, run 90 fixed steps,
// write a PNG, exit nonzero on compile/load failure (doubles as the CI
// smoke test for the whole reload pipeline).
//
// --bench: headless frame-time measurement against the 60 FPS gate. The
// distinction that matters — and the reason this is a separate mode
// rather than a number printed by --frame — is the SURFACE. The capture
// path steps the clock on an 8x8 scratch canvas, where every draw is
// clipped away and the reported "work ms" measures describe + reconcile
// with the pixels missing. --bench allocates the sketch's own declared
// canvas, warms it to --at so materials/atlases/picture caches are hot,
// then times --bench-frames real frames end to end.

#include "ComposeSketchView.h"
#include "SketchCrash.h"
#include "SketchHost.h"

#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkStream.h>
#include <include/core/SkSurface.h>
#include <include/encode/SkPngEncoder.h>

#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace {

std::filesystem::path executableDir(const char *argv0) {
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

sigil::weave::FontContext &fonts() {
  static auto *context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

struct CaptureOptions {
  std::string out;
  double at = 1.5;    // seconds of fixed-step warmup before capture
  float scale = 1.0f; // multiplier over the sketch's canvas size
  int frames = 1;     // >1 captures a numbered sequence
  double fps = 60.0;  // fixed-step rate
  bool bench = false; // --bench: measure, don't write
  int benchFrames = 120;
};

/** The 60 FPS gate, in milliseconds per frame. */
constexpr double kFrameBudgetMs = 16.6;

std::string numberedPath(const std::string &path, int index) {
  const size_t dot = path.rfind('.');
  char suffix[16];
  std::snprintf(suffix, sizeof suffix, "_%04d", index);
  return dot == std::string::npos
             ? path + suffix
             : path.substr(0, dot) + suffix + path.substr(dot);
}

/** Blocks until the first build lands (or fails); false = never got live. */
bool awaitFirstBuild(sigil::compose::sketch::SketchHost &host) {
  using namespace std::chrono_literals;
  for (int i = 0; i < 1200; ++i) {
    host.poll();
    if (host.live() || !host.errorLog().empty())
      break;
    std::this_thread::sleep_for(50ms);
  }
  if (!host.live()) {
    std::fprintf(stderr, "sketch failed to build:\n%s\n",
                 host.errorLog().c_str());
    return false;
  }
  return true;
}

double percentile(std::vector<double> sorted, double q) {
  if (sorted.empty())
    return 0.0;
  const size_t i = (size_t)std::llround(q * (double)(sorted.size() - 1));
  return sorted[std::min(i, sorted.size() - 1)];
}

/** --bench: the 60 FPS gate, measured on the sketch's REAL canvas.
 *
 * Every frame runs the true per-frame path — clear, tick, Sketch::update
 * (describe + reconcile), Composer::draw (layout + volatile + paint), and
 * a readback that forces the raster to actually complete rather than
 * leaving work queued behind the timer. Warmup to --at first: the first
 * frames of any sketch pay for SkSL program compiles, Cache::Texture
 * bakes, brush snapshot()s, font atlases and glyph rasterization, and
 * folding those into the sample would measure the wrong thing.
 *
 * Always exits 0 — the verdict is data for the ledger, and a nonzero exit
 * breaks the agent loops that call this in a pipeline. */
int runBench(sigil::compose::sketch::SketchHost &host,
             const CaptureOptions &options,
             const std::filesystem::path &sketchPath) {
  if (!awaitFirstBuild(host))
    return 1;

  const SkSize canvas = host.canvasSize();
  const float scale = options.scale;
  const int width = std::max(1, (int)(canvas.width() * scale));
  const int height = std::max(1, (int)(canvas.height() * scale));
  // The same surface capture() would make — a real one, at the sketch's
  // own declared size, so nothing is clipped away.
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  if (!surface) {
    std::fprintf(stderr, "bench: could not allocate a %dx%d surface\n", width,
                 height);
    return 1;
  }
  SkCanvas &sk = *surface->getCanvas();
  const SkColor background = host.background().toSkColor();

  // Forces the pixels to exist. On raster this is already true when
  // draw() returns; the readback keeps the number honest if a GPU capture
  // backend is ever wired in behind the same call.
  SkBitmap probe;
  probe.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  const auto flush = [&] { (void)surface->readPixels(probe.pixmap(), 0, 0); };

  const double dt = 1.0 / options.fps;
  const auto step = [&] {
    sk.clear(background);
    sk.save();
    sk.scale(scale, scale);
    host.frame(sk, dt);
    sk.restore();
    flush();
  };

  const int warmup = std::max(1, (int)std::lround(options.at / dt));
  for (int i = 0; i < warmup; ++i)
    step();

  // One profiled frame BEFORE the timed run, so a FAIL can name the node
  // instead of only the phase. Profiling costs a little, so it does not
  // ride along with the measured frames.
  std::vector<sigil::compose::Composer::NodeCost> hotNodes;
  if (sigil::compose::Composer *composer = host.composer()) {
    composer->setProfiling(true);
    step();
    const auto &rows = composer->profile();
    hotNodes.assign(rows.begin(),
                 rows.begin() + (std::ptrdiff_t)std::min<size_t>(6, rows.size()));
    composer->setProfiling(false);
  }

  std::vector<double> frames, updates, draws, paints, layouts, reconciles;
  frames.reserve((size_t)options.benchFrames);
  for (int i = 0; i < options.benchFrames; ++i) {
    const auto begin = std::chrono::steady_clock::now();
    step();
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - begin)
                          .count();
    frames.push_back(ms);
    const auto &split = host.lastFrameTiming();
    updates.push_back(split.updateMs);
    draws.push_back(split.drawMs);
    if (const sigil::compose::Composer *composer = host.composer()) {
      const auto &stats = composer->stats();
      paints.push_back(stats.paintMs);
      layouts.push_back(stats.layoutMs);
      reconciles.push_back(stats.reconcileMs);
    }
  }

  const auto mean = [](const std::vector<double> &v) {
    if (v.empty())
      return 0.0;
    double sum = 0;
    for (double x : v)
      sum += x;
    return sum / (double)v.size();
  };
  std::vector<double> sorted = frames;
  std::sort(sorted.begin(), sorted.end());
  const double p50 = percentile(sorted, 0.50);
  const double p99 = percentile(sorted, 0.99);
  const double avg = mean(frames);
  const double worst = sorted.empty() ? 0.0 : sorted.back();
  const bool pass = p99 < kFrameBudgetMs;

  // One machine-readable line (the ledger greps for "BENCH ").
  std::printf("BENCH %s %dx%d frames=%d p50=%.2fms p99=%.2fms mean=%.2fms "
              "max=%.2fms fps50=%.1f VERDICT=%s\n",
              sketchPath.stem().string().c_str(), width, height,
              (int)frames.size(), p50, p99, avg, worst,
              p50 > 0 ? 1000.0 / p50 : 0.0, pass ? "PASS" : "FAIL");
  // …and the human one: what dominates is the ledger's first question.
  std::printf("  phases (mean ms): update %.2f [reconcile %.2f] · draw %.2f "
              "[layout %.2f · paint %.2f]\n",
              mean(updates), mean(reconciles), mean(draws), mean(layouts),
              mean(paints));
  if (const sigil::compose::Composer *composer = host.composer()) {
    const auto &stats = composer->stats();
    std::printf("  last frame: %zu nodes painted, %zu pictures recorded, "
                "%zu pictures live, %zu textures live, %zu instances\n",
                stats.nodesPainted, stats.picturesRecorded, stats.picturesLive,
                stats.texturesLive, stats.instances);
  }
  if (!hotNodes.empty()) {
    std::printf("  most expensive nodes (self ms, excluding children):\n");
    for (const auto &row : hotNodes) {
      const char *state = "live";
      switch (row.cacheState) {
      case sigil::compose::Composer::CacheState::Live:
        state = "live paint";
        break;
      case sigil::compose::Composer::CacheState::Picture:
        state = "[PICTURE — a replay still re-runs every shader, every pixel]";
        break;
      case sigil::compose::Composer::CacheState::Texture:
        state = "[texture — you asked for it]";
        break;
      case sigil::compose::Composer::CacheState::Promoted:
        state = "[TEXTURE, promoted by the library — not by you]";
        break;
      }
      std::printf("    %8.2f ms  %-40s %s\n", row.selfMs, row.label.c_str(),
                  state);
      // WHY it is not a bake. "live paint, 663 ms" with nothing beside it
      // is how sixteen studies shipped over the gate: every refusal is
      // individually correct and individually invisible.
      if (row.cacheState != sigil::compose::Composer::CacheState::Promoted &&
          row.cacheState != sigil::compose::Composer::CacheState::Texture &&
          row.selfMs >= 1.0)
        std::printf("              not baked: %s\n",
                    sigil::compose::Composer::promotionReason(row.promotion));
    }
  }
  if (pass) {
    std::printf("  PASS — p99 %.2f ms is inside the %.1f ms budget "
                "(%.0f FPS gate)\n",
                p99, kFrameBudgetMs, 1000.0 / kFrameBudgetMs);
  } else {
    const bool paintBound = mean(draws) >= mean(updates);
    std::printf("\n"
                "  ####################################################\n"
                "  ##  FAIL — p99 %.2f ms EXCEEDS the %.1f ms budget.\n"
                "  ##  This sketch does NOT hold 60 FPS at %dx%d.\n"
                "  ##  %s dominates.\n",
                p99, kFrameBudgetMs, width, height,
                paintBound ? "PAINT (draw)" : "DESCRIBE/RECONCILE (update)");
    if (paintBound) {
      // The distinction the corpus got wrong, stated where it is read.
      // `picturesRecorded == 0` looks like "fully cached" and is not: a
      // picture records the DRAW CALLS, so replaying it re-runs every SkSL
      // shader over every pixel, every frame. Only Cache::Texture keeps
      // the PIXELS. (Note: debug::coverage is a geometric path-tiling
      // check — it has nothing to do with paint cost, so do not send
      // people there for this.)
      std::printf(
          "  ##  Per-pixel cost, not tree cost. Note that\n"
          "  ##  'pictures recorded 0' above does NOT mean cached: a\n"
          "  ##  picture records the DRAW CALLS, so replaying it re-runs\n"
          "  ##  every shader over every pixel again. Only a BAKE keeps\n"
          "  ##  the PIXELS.\n"
          "  ##  First move: read the 'not baked' line under each node\n"
          "  ##  above. The library bakes what it can prove is safe; the\n"
          "  ##  common refusal is opacity/blend on a full-area material\n"
          "  ##  (the paper-grain idiom), and that one is yours to ask\n"
          "  ##  for with .cache(Cache::Texture). %zu nodes painted,\n"
          "  ##  %zu textures live.\n",
          host.composer() ? host.composer()->stats().nodesPainted : 0u,
          host.composer() ? host.composer()->stats().texturesLive : 0u);
    } else {
      std::printf("  ##  You are rebuilding the tree every frame. Describe\n"
                  "  ##  once in setup() and bind choreograph::Outputs, or\n"
                  "  ##  memo() the subtrees whose props did not change.\n");
    }
    std::printf("  ##  If it cannot be fixed, it goes in PERF_LEDGER.md\n"
                "  ##  with this number.\n"
                "  ####################################################\n");
  }
  std::fflush(stdout);
  return 0;
}

int runHeadless(sigil::compose::sketch::SketchHost &host,
                const CaptureOptions &options) {
  if (!awaitFirstBuild(host))
    return 1;
  // Step the clock to --at with fixed dt (a tiny scratch surface: the
  // real pixels come from capture()).
  const double dt = 1.0 / options.fps;
  sk_sp<SkSurface> scratch =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(8, 8));
  const int warmup = std::max(1, (int)std::lround(options.at / dt));
  for (int i = 0; i < warmup; ++i)
    host.frame(*scratch->getCanvas(), dt);

  for (int index = 0; index < options.frames; ++index) {
    const std::string path =
        options.frames > 1 ? numberedPath(options.out, index + 1)
                           : options.out;
    {
      sigil::compose::sketch::PhaseMark mark(
          sigil::compose::sketch::Phase::Capture);
      if (!host.capture(path, options.scale)) {
        std::fprintf(stderr, "failed to write %s\n", path.c_str());
        return 1;
      }
    }
    if (index + 1 < options.frames)
      host.frame(*scratch->getCanvas(), dt); // advance between frames
  }
  std::printf("wrote %s (%d frame%s at %.3gx, build %d, "
              "work %.2f ms avg / %.2f p99)\n",
              options.out.c_str(), options.frames,
              options.frames == 1 ? "" : "s", options.scale,
              host.generation(), host.workMsAverage(),
              host.workMsP99());
  return 0;
}

} // namespace

int main(int argc, char *argv[]) {
  std::filesystem::path sketchPath;
  std::filesystem::path assetsDir;
  CaptureOptions capture;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--assets" && i + 1 < argc)
      assetsDir = argv[++i];
    else if (arg == "--frame" && i + 1 < argc)
      capture.out = argv[++i];
    else if (arg == "--at" && i + 1 < argc)
      capture.at = std::stod(argv[++i]);
    else if (arg == "--scale" && i + 1 < argc)
      capture.scale = std::stof(argv[++i]);
    else if (arg == "--frames" && i + 1 < argc)
      capture.frames = std::max(1, std::stoi(argv[++i]));
    else if (arg == "--fps" && i + 1 < argc)
      capture.fps = std::stod(argv[++i]);
    else if (arg == "--bench")
      capture.bench = true;
    else if (arg == "--bench-frames" && i + 1 < argc)
      capture.benchFrames = std::max(1, std::stoi(argv[++i]));
    else if (sketchPath.empty())
      sketchPath = arg;
  }
  if (sketchPath.empty() || !std::filesystem::exists(sketchPath)) {
    std::fprintf(stderr,
                 "usage: ComposeSketch <sketch.cpp> [--assets <dir>]\n"
                 "         [--frame <out.png>] [--at <sec>] [--scale <n>]\n"
                 "         [--frames <count>] [--fps <n>]\n"
                 "         [--bench] [--bench-frames <n>]\n"
                 "starter: src/common/compose/sketch/sketches/hello.cpp\n");
    return 2;
  }

  sigil::compose::sketch::SketchHost::Options options;
  options.sketchPath = std::filesystem::absolute(sketchPath);
  options.assetsDir = assetsDir;
  options.flagsFile = executableDir(argv[0]) / "sketch_flags.rsp";
  if (!std::filesystem::exists(options.flagsFile)) {
    std::fprintf(stderr, "missing %s (rebuild ComposeSketch)\n",
                 options.flagsFile.string().c_str());
    return 2;
  }
  // Before the guest can ever run: a fault inside it used to be exit 139
  // and total silence.
  sigil::compose::sketch::installCrashReporter(options.sketchPath);
  sigil::compose::sketch::SketchHost host(std::move(options), fonts());

  if (capture.bench)
    return runBench(host, capture, host.sketchPath());
  if (!capture.out.empty())
    return runHeadless(host, capture);

  QGuiApplication application(argc, argv);
  QGuiApplication::setOrganizationDomain("sigil.dev");
  QGuiApplication::setApplicationName("ComposeSketch");
  ComposeSketchView::host = &host;

  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
      [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
  engine.loadFromModule("Compose.Sketch", "Main");
  return application.exec();
}
