/** @file
 * Seer: every wire a sketch listens on, what arrives there, and what is
 * sent back down it.
 *
 *   Seer [<uri>…]                the window, with each URI already open
 *   Seer --shot <png> [<uri>…]   …photographed once it has run, and closed
 *
 * A URI on the command line is opened before the window comes up and the
 * last of them is the one being read, so a run that always watches the
 * same port is one command rather than a field typed again every time.
 * Everything else a session does, it does in the window.
 *
 * `--shot` IS THE WINDOW WITHOUT SOMEBODY IN FRONT OF IT: the frames the
 * window drew, read back and written down, which is how the panes are
 * looked at from a script. The run is driven for a stretch of frames
 * first, because a wire opened a moment ago has nothing on it yet and a
 * picture taken at once would be a picture of a tool that had not
 * started. It draws through the same renderer a session always draws
 * through — the panes are masked layers, and a renderer without them
 * would photograph a window with nothing in it.
 */

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickWindow>
#include <cstdio>
#include <cstring>
#include <memory>

#include "SeerSession.h"

namespace {

/** How many frames a photographed run is driven for before the picture
 *  is taken: long enough that a wire opened as the window came up has
 *  been read, drained and drawn. */
constexpr int kShotFrames = 90;

}  // namespace

int main(int argc, char* argv[]) {
  // Set before anything asks where this application's settings live: the
  // window remembers where it was left, and the location is named for
  // this application. Static setters, so no instance is needed yet.
  QCoreApplication::setOrganizationDomain(QStringLiteral("sigil.dev"));
  QCoreApplication::setApplicationName(QStringLiteral("Seer"));

  QString shotPath;
  for (int at = 1; at != argc; ++at) {
    if (std::strcmp(argv[at], "--help") == 0) {
      std::printf("usage: Seer [--shot <png>] [<uri>…]\n");
      return 0;
    }
    if (std::strcmp(argv[at], "--shot") == 0) {
      if (at + 1 == argc) {
        std::fprintf(stderr, "--shot: no file to write the picture to\n");
        return 2;
      }
      shotPath = QString::fromLocal8Bit(argv[++at]);
      continue;
    }
    if (argv[at][0] == '-') {
      std::fprintf(stderr, "%s: Seer takes the URIs to open and --shot\n",
                   argv[at]);
      return 2;
    }
    SeerSession::opensOn.append(QString::fromLocal8Bit(argv[at]));
  }

  SeerSession::photographed = !shotPath.isEmpty();

  QGuiApplication application(argc, argv);

  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
      [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
  engine.loadFromModule("Sigil.Seer", "Main");
  if (engine.rootObjects().isEmpty()) return 1;

  if (!shotPath.isEmpty()) {
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) {
      std::fprintf(stderr, "--shot: no window to photograph\n");
      return 1;
    }
    // Drive real frames rather than waiting for them. A window nobody is
    // looking at gets no render loop from the compositor, so the grab is
    // what makes the thing run.
    auto* warm = new QTimer(&application);
    auto framesLeft = std::make_shared<int>(kShotFrames);
    warm->setInterval(16);
    QObject::connect(warm, &QTimer::timeout, &application,
                     [window, shotPath, warm, framesLeft] {
                       if (--*framesLeft > 0) {
                         window->grabWindow();
                         return;
                       }
                       warm->stop();
                       const QImage picture = window->grabWindow();
                       if (picture.isNull() || !picture.save(shotPath, "PNG")) {
                         std::fprintf(stderr, "--shot: grab failed\n");
                         QCoreApplication::exit(1);
                         return;
                       }
                       std::printf("wrote %s (%dx%d)\n",
                                   shotPath.toLocal8Bit().constData(),
                                   picture.width(), picture.height());
                       QCoreApplication::quit();
                     });
    warm->start();
  }

  return QGuiApplication::exec();
}
