#ifdef __APPLE__
#include "AppNap.h"
#endif
#include "spdlog/spdlog.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>

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

  return application.exec();
}
