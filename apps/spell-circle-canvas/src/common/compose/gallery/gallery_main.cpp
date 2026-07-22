// ComposeGallery entry point: the Qt Quick app (WeaveGallery mold —
// QML sidebar over the scene registry, live metrics panel), or
// `--headless [outdir] [--gpu] [--scene <name|index>]` for the per-scene FPS
// table + PNG captures (--gpu sweeps on a Graphite Metal surface).
// `--scene` narrows the sweep to one entry, which is how a single scene gets
// looked at during visual work without paying for the other eighteen.

#include "GalleryScenes.h"

#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>

#include <cstdio>
#include <string>

int main(int argc, char *argv[]) {
  if (argc >= 2 && std::string(argv[1]) == "--headless") {
    std::string outDir = "compose_gallery_out";
    std::string only;
    bool gpu = false;
    for (int i = 2; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--gpu")
        gpu = true;
      else if (arg == "--scene" && i + 1 < argc)
        only = argv[++i];
      else
        outDir = arg;
    }
    int sceneIndex = -1;
    if (!only.empty()) {
      sceneIndex = compose_gallery::findScene(only);
      if (sceneIndex < 0) {
        std::fprintf(stderr, "no scene matches \"%s\"; known scenes:\n",
                     only.c_str());
        for (int i = 0; i < compose_gallery::kGallerySceneCount; ++i)
          std::fprintf(stderr, "  %2d  %s\n", i,
                       compose_gallery::kScenes[i].name);
        return 1;
      }
    }
    return compose_gallery::runHeadless(outDir, gpu, sceneIndex);
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
