#include <WindowCapture.h>

#include <QCommandLineParser>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <cstdio>

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape): an
                                    // uncaught error ends the app
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
  const QCommandLineOption shotOption(
      "shot", "Capture the window to a PNG and exit.", "file");
  parser.addOption(shotOption);
  parser.process(application);
  if (parser.isSet(shotOption)) qputenv("IFRIT_NO_VIBRANCY", "1");

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
  if (engine.rootObjects().isEmpty()) return 1;

  if (parser.isSet(shotOption)) {
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) {
      std::fprintf(stderr, "--shot: no window to photograph\n");
      return 1;
    }
    ifrit::qt::captureWindow(*window, parser.value(shotOption));
  }

  return QGuiApplication::exec();
}
