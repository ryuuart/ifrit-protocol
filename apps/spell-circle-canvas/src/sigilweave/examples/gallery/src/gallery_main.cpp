#include <QCommandLineParser>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

int main(int argc, char *argv[]) {
  QGuiApplication application(argc, argv);
  QGuiApplication::setOrganizationDomain("sigil.dev");
  QGuiApplication::setApplicationName("WeaveGallery");

  // Windows request an alpha channel so the native vibrancy background
  // installed by Ui.WindowChrome shows through transparent QML windows.
  QQuickWindow::setDefaultAlphaBuffer(true);

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
  parser.process(application);

  QString initialText;
  if (parser.isSet(textOption)) {
    QFile file(parser.value(textOption));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
      initialText = QString::fromUtf8(file.readAll());
  }

  QQmlApplicationEngine engine;
  engine.setInitialProperties({
      {"initialScene", parser.value(sceneOption).toInt()},
      {"initialText", initialText},
  });
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
      []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
  engine.loadFromModule("SigilWeave.Gallery", "Main");

  return application.exec();
}
