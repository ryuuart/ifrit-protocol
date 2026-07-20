// ComposeSketch — the p5-style live-coding host for SigilCompose.
//
//   ComposeSketch <sketch.cpp> [--assets <dir>] [--frame <out.png>]
//
// Windowed (default): opens the live canvas, watches the sketch file,
// recompiles on save, hot-swaps the running sketch; compile errors
// overlay while the last good build keeps running.
//
// --frame: headless single-shot — compile, load, run 90 fixed steps,
// write a PNG, exit nonzero on compile/load failure (doubles as the CI
// smoke test for the whole reload pipeline).

#include "ComposeSketchView.h"
#include "SketchHost.h"

#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkStream.h>
#include <include/core/SkSurface.h>
#include <include/encode/SkPngEncoder.h>

#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

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
};

std::string numberedPath(const std::string &path, int index) {
  const size_t dot = path.rfind('.');
  char suffix[16];
  std::snprintf(suffix, sizeof suffix, "_%04d", index);
  return dot == std::string::npos
             ? path + suffix
             : path.substr(0, dot) + suffix + path.substr(dot);
}

int runHeadless(sigil::compose::sketch::SketchHost &host,
                const CaptureOptions &options) {
  using namespace std::chrono_literals;
  // Wait for the first build to land (or fail).
  for (int i = 0; i < 1200; ++i) {
    host.poll();
    if (host.live() || !host.errorLog().empty())
      break;
    std::this_thread::sleep_for(50ms);
  }
  if (!host.live()) {
    std::fprintf(stderr, "sketch failed to build:\n%s\n",
                 host.errorLog().c_str());
    return 1;
  }
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
    if (!host.capture(path, options.scale)) {
      std::fprintf(stderr, "failed to write %s\n", path.c_str());
      return 1;
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
    else if (sketchPath.empty())
      sketchPath = arg;
  }
  if (sketchPath.empty() || !std::filesystem::exists(sketchPath)) {
    std::fprintf(stderr,
                 "usage: ComposeSketch <sketch.cpp> [--assets <dir>]\n"
                 "         [--frame <out.png>] [--at <sec>] [--scale <n>]\n"
                 "         [--frames <count>] [--fps <n>]\n"
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
  sigil::compose::sketch::SketchHost host(std::move(options), fonts());

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
