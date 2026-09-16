/** @file
 * Seer: every wire a sketch listens on, what arrives there, and what is
 * sent back down it.
 *
 *   Seer [<uri>…]                  the window, with each URI already open
 *   Seer --schema <bfbs> [<uri>…]  …reading every message through it
 *   Seer --peer <uri> [<uri>…]     …with the send pane pointed at a peer
 *   Seer --say <message> [<uri>…]  …and that message said down it once
 *   Seer --shot <png> [<uri>…]     …photographed once it has run, and closed
 *
 * Every URI on the command line is opened before the window comes up and
 * the first of them is the one being read, so a run that always watches
 * the same wires is one command rather than fields typed again every
 * time. `--schema` names the binary schema the messages are read
 * through — what `flatc -b --schema` wrote beside a sender's generated
 * header — which the window also takes from a file picker; the picker
 * needs a person, so a run driven from a script names the file here.
 * Everything else a session does, it does in the window.
 *
 * `--peer` is the peer the send pane opens on, opened where the wires
 * are so the pane already stands on the form that peer's own dialect
 * asks for. `--say` is one message down it: the form comes up holding
 * it, and it goes out on a frame, once every wire this run opened is
 * standing — so a message said to a port the same run listens on crosses
 * onto a wire already being read, which is how a script proves a wire
 * carries anything at all. WHAT THE PEER SPEAKS DECIDES WHAT IS SAID: a
 * wire that carries anything takes the message as text, an OSC peer the
 * arguments under its address and a desk the dimmers of its universe,
 * each written the way a reader types it into the editor; an instrument
 * has no editor, so it takes the words of its form — "NoteOn 1 60 100",
 * the kind, the channel and the numbers that kind carries. A message the
 * form cannot spell is not sent and the note says so. Something to say
 * with nobody to say it to is a run that would do nothing, so it is
 * refused here rather than in the window.
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

#include <WindowCapture.h>
#include <WindowChrome.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickWindow>
#include <cstdio>
#include <cstring>

#include "SeerSession.h"

int main(int argc, char* argv[]) {
  // Set before anything asks where this application's settings live: the
  // window remembers where it was left, and the location is named for
  // this application. Static setters, so no instance is needed yet.
  QCoreApplication::setOrganizationDomain(QStringLiteral("sigil.dev"));
  QCoreApplication::setApplicationName(QStringLiteral("Seer"));

  QString shotPath;
  for (int at = 1; at != argc; ++at) {
    if (std::strcmp(argv[at], "--help") == 0) {
      std::printf(
          "usage: Seer [--schema <bfbs>] [--peer <uri>] [--say <message>] "
          "[--receiver <uri>] [--shot <png>] [<uri>…]\n");
      return 0;
    }
    if (std::strcmp(argv[at], "--receiver") == 0) {
      if (at + 1 == argc || argv[at + 1][0] == '-') {
        std::fprintf(stderr, "--receiver: name the scene source URI\n");
        return 2;
      }
      SeerSession::receivesOn = QString::fromLocal8Bit(argv[++at]);
      continue;
    }
    if (std::strcmp(argv[at], "--schema") == 0) {
      if (at + 1 == argc) {
        std::fprintf(stderr, "--schema: no binary schema to read through\n");
        return 2;
      }
      SeerSession::readsThrough = QString::fromLocal8Bit(argv[++at]);
      continue;
    }
    if (std::strcmp(argv[at], "--peer") == 0) {
      if (at + 1 == argc) {
        std::fprintf(stderr, "--peer: no URI to send to\n");
        return 2;
      }
      SeerSession::sendsTo = QString::fromLocal8Bit(argv[++at]);
      continue;
    }
    if (std::strcmp(argv[at], "--say") == 0) {
      if (at + 1 == argc) {
        std::fprintf(stderr, "--say: no message to send\n");
        return 2;
      }
      SeerSession::says = QString::fromLocal8Bit(argv[++at]);
      continue;
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
      std::fprintf(
          stderr,
          "%s: Seer takes the URIs to open, --schema, --peer, --say and "
          "--receiver and --shot\n",
          argv[at]);
      return 2;
    }
    SeerSession::opensOn.append(QString::fromLocal8Bit(argv[at]));
  }

  if (SeerSession::says && SeerSession::sendsTo.isEmpty()) {
    std::fprintf(stderr,
                 "--say: there is nobody to say it to; name the peer with "
                 "--peer <uri>\n");
    return 2;
  }

  SeerSession::photographed = !shotPath.isEmpty();

  QGuiApplication application(argc, argv);

  QQuickWindow::setDefaultAlphaBuffer(true);
  SeerSession session;
  QQmlApplicationEngine engine;
  engine.setInitialProperties({{"session", QVariant::fromValue(&session)}});
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
      [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
  engine.loadFromModule("Sigil.Seer", "Main");
  if (engine.rootObjects().isEmpty()) return 1;

  auto* receiverWindow =
      qobject_cast<QQuickWindow*>(engine.rootObjects().first());
  const auto keepReceiverRunning = [receiverWindow, &session] {
    if (!receiverWindow || !session.receiver()->opened()) return;
    WindowChrome::keepRendering(receiverWindow);
  };
  QObject::connect(session.receiver(), &Receiver::changed, &application,
                   keepReceiverRunning);
  keepReceiverRunning();

  if (!shotPath.isEmpty()) {
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) {
      std::fprintf(stderr, "--shot: no window to photograph\n");
      return 1;
    }
    ifrit::qt::captureWindow(*window, shotPath);
  }

  return QGuiApplication::exec();
}
