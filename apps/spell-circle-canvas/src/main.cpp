#ifdef __APPLE__
#include "AppNap.h"
#endif
#include "spdlog/spdlog.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

int main(int argc, char *argv[]) {
#ifdef __APPLE__
  AppNap::disable();
#endif

  QGuiApplication application(argc, argv);

  spdlog::info("App started");

  QQmlApplicationEngine engine;

  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
      []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

  engine.loadFromModule("SpellCircle.App", "Main");

#ifdef __APPLE__
  for (QObject *rootObject : engine.rootObjects()) {
    auto *window = qobject_cast<QQuickWindow *>(rootObject);
    if (!window)
      continue;

    // Keep the render-side texture and Syphon server alive across ordinary
    // visibility changes, and keep the render loop active when the native
    // window is merely covered by another application.
    window->setPersistentGraphics(true);
    window->setPersistentSceneGraph(true);
    if (!AppNap::keepRenderingWhileOccluded(window))
      spdlog::warn("Could not enable rendering while the window is occluded");
  }
#endif

  return application.exec();
}
