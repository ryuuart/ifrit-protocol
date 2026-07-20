// ComposeGallery entry point: the Qt Quick app (WeaveGallery mold —
// QML sidebar over the scene registry, live metrics panel), or
// `--headless <outdir>` for the per-scene FPS table + PNG captures.

#include "GalleryScenes.h"

#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>

#include <string>

int main(int argc, char *argv[]) {
  if (argc >= 2 && std::string(argv[1]) == "--headless")
    return compose_gallery::runHeadless(
        argc >= 3 ? argv[2] : "compose_gallery_out");

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
