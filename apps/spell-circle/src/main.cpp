#include "SpellCircle.h"
#include "network/FeedModel.h"
#include "network/NetworkManager.h"
#include "spdlog/spdlog.h"
#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml/qqml.h>

// void qtMessageHandler(QtMsgType type, const QMessageLogContext &context,
//                       const QString &msg) {
//   const std::string text = msg.toStdString();

//   switch (type) {
//   case QtDebugMsg:
//     spdlog::debug(text);
//   case QtInfoMsg:
//     spdlog::info(text);
//   case QtWarningMsg:
//     spdlog::warn(text);
//   case QtCriticalMsg:
//     spdlog::error(text);
//   case QtFatalMsg:
//     spdlog::critical(text);
//   }
// }

int main(int argc, char *argv[]) {
  // qInstallMessageHandler(qtMessageHandler);
  QGuiApplication app(argc, argv);

  spdlog::info("App started");

  FeedModel feedModel;
  NetworkManager networkManager;

  QObject::connect(&networkManager, &NetworkManager::spellCircleReceived,
                   &feedModel, &FeedModel::onSpellCircleReceived);

  if (!networkManager.start()) {
    spdlog::error("NetworkManager failed to start");
    return 1;
  }

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("feedModel", &feedModel);

  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

  engine.loadFromModule("qtquick1", "Main");

  return app.exec();
}
