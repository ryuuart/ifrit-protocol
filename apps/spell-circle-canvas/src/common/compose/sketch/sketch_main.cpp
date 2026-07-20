// ComposeSketch — the p5-style live-coding host for IfritCompose.
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

#include <textflow/FontContext.h>
#include <textflow/ports/SystemFontManager.h>

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

textflow::FontContext &fonts() {
  static auto *context =
      new textflow::FontContext(textflow::ports::systemFontManager());
  return *context;
}

int runHeadless(ifrit::compose::sketch::SketchHost &host,
                const std::string &outPath) {
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
  constexpr float kCapture = 2.0f;
  const SkSize size = ifrit::compose::sketch::SketchHost::kCanvasSize;
  sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
      (int)(size.width() * kCapture), (int)(size.height() * kCapture)));
  surface->getCanvas()->scale(kCapture, kCapture);
  for (int i = 0; i < 90; ++i) {
    surface->getCanvas()->clear(SK_ColorBLACK);
    host.frame(*surface->getCanvas(), 1.0 / 60.0);
  }
  SkBitmap bitmap;
  bitmap.allocPixels(surface->imageInfo());
  surface->readPixels(bitmap.pixmap(), 0, 0);
  SkFILEWStream stream(outPath.c_str());
  if (!stream.isValid() ||
      !SkPngEncoder::Encode(&stream, bitmap.pixmap(), {})) {
    std::fprintf(stderr, "failed to write %s\n", outPath.c_str());
    return 1;
  }
  std::printf("wrote %s (build %d)\n", outPath.c_str(),
              host.generation());
  return 0;
}

} // namespace

int main(int argc, char *argv[]) {
  std::filesystem::path sketchPath;
  std::filesystem::path assetsDir;
  std::string framePath;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--assets" && i + 1 < argc)
      assetsDir = argv[++i];
    else if (arg == "--frame" && i + 1 < argc)
      framePath = argv[++i];
    else if (sketchPath.empty())
      sketchPath = arg;
  }
  if (sketchPath.empty() || !std::filesystem::exists(sketchPath)) {
    std::fprintf(stderr,
                 "usage: ComposeSketch <sketch.cpp> [--assets <dir>] "
                 "[--frame <out.png>]\n"
                 "starter: src/common/compose/sketch/sketches/hello.cpp\n");
    return 2;
  }

  ifrit::compose::sketch::SketchHost::Options options;
  options.sketchPath = std::filesystem::absolute(sketchPath);
  options.assetsDir = assetsDir;
  options.flagsFile = executableDir(argv[0]) / "sketch_flags.rsp";
  if (!std::filesystem::exists(options.flagsFile)) {
    std::fprintf(stderr, "missing %s (rebuild ComposeSketch)\n",
                 options.flagsFile.string().c_str());
    return 2;
  }
  ifrit::compose::sketch::SketchHost host(std::move(options), fonts());

  if (!framePath.empty())
    return runHeadless(host, framePath);

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
