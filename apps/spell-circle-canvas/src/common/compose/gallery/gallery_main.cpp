// ComposeGallery entry point: the Qt Quick app (WeaveGallery mold —
// QML sidebar over the scene registry, live metrics panel), or
// `--headless [outdir] [--gpu]` for the per-scene FPS table + PNG captures
// (--gpu sweeps on a Graphite Metal surface; numbers only, no captures).

#include "GalleryScenes.h"

#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>

#include <string>

int main(int argc, char *argv[]) {
  if (argc >= 2 && std::string(argv[1]) == "--headless") {
    std::string outDir = "compose_gallery_out";
    bool gpu = false;
    for (int i = 2; i < argc; ++i) {
      if (std::string(argv[i]) == "--gpu")
        gpu = true;
      else
        outDir = argv[i];
    }
    return compose_gallery::runHeadless(outDir, gpu);
  }

  QGuiApplication application(argc, argv);
  QGuiApplication::setOrganizationDomain("sigil.dev");
  QGuiApplication::setApplicationName("ComposeGallery");

  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
      [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
  engine.loadFromModule("Compose.Gallery", "Main");
  return application.exec();
}
