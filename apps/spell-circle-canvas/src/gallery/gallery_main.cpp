#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[]) {
  QGuiApplication app(argc, argv);

  QCommandLineParser parser;
  parser.addHelpOption();
  const QCommandLineOption sceneOption(
      {"s", "scene"}, "Scene index to open with.", "index", "0");
  parser.addOption(sceneOption);
  parser.process(app);

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty(
      "initialScene", parser.value(sceneOption).toInt());
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
  engine.loadFromModule("TextFlow.Gallery", "Main");

  return app.exec();
}
