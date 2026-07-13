#include <QCommandLineParser>
#include <QFile>
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
  // Stress/testing affordance: preload the live-text editor from a file
  // (e.g. a 10k-word corpus) without pasting by hand.
  const QCommandLineOption textOption(
      {"t", "text"}, "File whose contents replace the scene's text.", "file");
  parser.addOption(textOption);
  parser.process(app);

  QString initialText;
  if (parser.isSet(textOption)) {
    QFile file(parser.value(textOption));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
      initialText = QString::fromUtf8(file.readAll());
  }

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty(
      "initialScene", parser.value(sceneOption).toInt());
  engine.rootContext()->setContextProperty("initialText", initialText);
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
  engine.loadFromModule("TextFlow.Gallery", "Main");

  return app.exec();
}
