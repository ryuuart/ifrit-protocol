// Seer opens message connections, scene receivers and shared texture previews.

#include <WindowCapture.h>
#include <WindowChrome.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickWindow>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>

#include "Arguments.h"
#include "Inspection.h"
#include "SeerSession.h"
#include "TextureSources.h"
#include "texture/Grab.h"
#include "texture/Servers.h"

namespace {

/** How often the window's event loop answers the protocol: within a frame
 *  or two of a request. */
constexpr int kInspectionPumpMilliseconds = 30;

}  // namespace

int main(int argc, char* argv[]) {
  // Set before anything asks where this application's settings live: the
  // window remembers where it was left, and the location is named for
  // this application. Static setters, so no instance is needed yet.
  QCoreApplication::setOrganizationDomain(QStringLiteral("sigil.dev"));
  QCoreApplication::setApplicationName(QStringLiteral("Seer"));

  const auto parsed = seer::parseArguments(argc, argv);
  if (!parsed) return 2;
  const auto& args = *parsed;
  if (args.help) {
    std::printf(
        "usage: Seer [--schema <bfbs>] [--peer <uri>] [--say <message>]\n"
        "            [--receiver <uri>] [--shot <png>] [<uri>…]\n"
        "            [--inspect[=<port>]] [--state <directory>]\n"
        "       Seer --textures\n"
        "       Seer --texture <name> [--app <application>] [--shot <png>]\n"
        "       Seer --list-textures\n"
        "       Seer --texture <name> [--app <application>] --grab <png>\n"
        "            [--frames <count>] [--timeout <seconds>]\n");
    return 0;
  }
  if (args.listTextures) return seer::texture::listPublications();
  if (!args.grabPath.empty()) return seer::texture::runGrab(args);
  for (const auto& uri : args.wires)
    SeerSession::opensOn.append(QString::fromStdString(uri));
  SeerSession::receivesOn = QString::fromStdString(args.receiver);
  SeerSession::readsThrough = QString::fromStdString(args.schema);
  SeerSession::sendsTo = QString::fromStdString(args.peer);
  if (args.message) SeerSession::says = QString::fromStdString(*args.message);
  const QString shotPath = QString::fromStdString(args.shot);

  SeerSession::photographed = !shotPath.isEmpty();

  QGuiApplication application(argc, argv);

  // THE PROTOCOL, ON BY DEFAULT: an endpoint on loopback answering what
  // Seer is, its address under the state root, dispatched from this
  // thread's event loop; with no client attached a dispatch runs no
  // handler.
  std::filesystem::path stateRoot = args.state;
  if (stateRoot.empty())
    stateRoot = QStandardPaths::writableLocation(
                    QStandardPaths::AppLocalDataLocation)
                    .toStdString();
  std::error_code stateError;
  std::filesystem::create_directories(stateRoot, stateError);
  seer::Inspection inspection(args.inspectPort.value_or(0), stateRoot);
  QTimer inspectionPump;
  QObject::connect(&inspectionPump, &QTimer::timeout,
                   [&inspection] { inspection.dispatch(); });
  if (inspection.listening()) inspectionPump.start(kInspectionPumpMilliseconds);

  QQuickWindow::setDefaultAlphaBuffer(true);
  SeerSession session;
  TextureSources textures;
  QQmlApplicationEngine engine;
  engine.setInitialProperties(
      {{"session", QVariant::fromValue(&session)},
       {"textures", QVariant::fromValue(&textures)},
       {"textureName", QString::fromStdString(args.texture)},
       {"textureApplication", QString::fromStdString(args.application)},
       {"workspace", args.textures            ? 2
                     : !args.receiver.empty() ? 1
                                              : 0}});
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
