#ifdef __APPLE__
#include "AppNap.h"
#endif
#include "spdlog/spdlog.h"
#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml/qqml.h>

int main(int argc, char *argv[]) {
#ifdef __APPLE__
  AppNap::disable();
#endif

  QGuiApplication app(argc, argv);

  spdlog::info("App started");

  QQmlApplicationEngine engine;

  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

  engine.loadFromModule("qtquick1", "Main");

  return app.exec();
}
