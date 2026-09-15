#include "NetworkManager.h"

#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/transport/Transport.h>
#include <spdlog/spdlog.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

/** The drain runs on the render frame, so a scene waits at most one frame
 *  in the door before it reaches the model. */
constexpr int kDrainIntervalMilliseconds = 16;

/** How many arrivals the door holds for the drain. A sender at animation
 *  frame rates leaves about one per drain, so this is a second of
 *  backlog: a frame the user interface spent elsewhere costs nothing,
 *  and a reader that falls further behind than that wants the newest
 *  scenes rather than the whole queue. */
constexpr size_t kArrivalCapacity = 64;

/** The address an arrival names, with the scheme taken off: a feed entry
 *  reads "127.0.0.1:52341" and the door it came through is the receiver's
 *  own concern. */
QString addressOf(const std::string& from) {
  const size_t scheme = from.find("://");
  if (scheme == std::string::npos) return QString::fromStdString(from);
  return QString::fromStdString(from.substr(scheme + 3));
}

}  // namespace

/** The resource hub this receiver opens its port on, the door itself, and
 *  the moment that door was made. */
struct NetworkManager::Door {
  Door() { sigil::io::registerUdp(hub); }

  sigil::io::Hub hub;
  std::shared_ptr<sigil::io::Feed> feed;
  /** An arrival carries the seconds since its feed was made, and the
   *  session measures its rate from receive times: the two are put back
   *  together here rather than at the drain, so the frame an arrival
   *  waited for is no part of the rate. */
  std::chrono::steady_clock::time_point openedAt;
};

NetworkManager::NetworkManager(uint16_t port, QObject* parent)
    : QObject(parent),
      m_port(port),
      m_statusText(tr("Stopped")),
      m_door(std::make_unique<Door>()),
      m_drain(new QTimer(this)) {
  m_drain->setInterval(kDrainIntervalMilliseconds);
  connect(m_drain, &QTimer::timeout, this, &NetworkManager::readArrivals);
}

NetworkManager::~NetworkManager() = default;

void NetworkManager::setPort(int port) {
  const auto boundedPort = static_cast<uint16_t>(qBound(1, port, 65535));
  if (m_port == boundedPort) return;
  m_port = boundedPort;
  emit portChanged();

  if (m_listening) start();
}

void NetworkManager::start() {
  const std::string uri = "udp://:" + std::to_string(m_port);
  // The hub answers one feed per URI for as long as anyone holds it, so
  // asking for the port already open keeps the door standing — and with
  // it the origin every arrival's time is counted from — rather than
  // closing a socket in order to bind the same one again.
  const auto openedAt = std::chrono::steady_clock::now();
  std::shared_ptr<sigil::io::Feed> opened =
      m_door->hub.feed(uri, {.capacity = kArrivalCapacity});
  if (opened != m_door->feed) {
    m_door->feed = std::move(opened);
    m_door->openedAt = openedAt;
  }

  const std::string error = m_door->feed->error();
  if (!error.empty()) {
    m_door->feed.reset();
    m_drain->stop();
    spdlog::error("UDP receiver failed on port {}: {}", m_port, error);
    setListening(false);
    setStatusText(tr("UDP failed on :%1 — %2")
                      .arg(m_port)
                      .arg(QString::fromStdString(error)));
    return;
  }

  m_drain->start();
  spdlog::info("UDP listening on :{}", m_port);
  setListening(true);
  setStatusText(tr("Listening on UDP :%1").arg(m_port));
}

void NetworkManager::stop() {
  m_drain->stop();
  m_door->feed.reset();
  spdlog::info("UDP socket closed");
  setListening(false);
  setStatusText(tr("Stopped"));
}

void NetworkManager::readArrivals() {
  // The door is re-read each turn: a scene reaching the model may take
  // the receiver down, and what a closed door still holds is nobody's to
  // emit.
  while (const std::shared_ptr<sigil::io::Feed> feed = m_door->feed) {
    const std::optional<sigil::io::Arrival> arrival = feed->receive();
    if (!arrival) return;
    const std::vector<std::byte>& bytes = arrival->bytes->bytes;
    const auto receivedAt =
        m_door->openedAt +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(arrival->at));
    emit spellCircleReceived(
        addressOf(arrival->from),
        QByteArray(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<qsizetype>(bytes.size())),
        receivedAt);
  }
}

void NetworkManager::setListening(bool listening) {
  if (m_listening == listening) return;
  m_listening = listening;
  emit listeningChanged();
}

void NetworkManager::setStatusText(const QString& statusText) {
  if (m_statusText == statusText) return;
  m_statusText = statusText;
  emit statusTextChanged();
}

QString NetworkManager::configFilePath() {
  return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
         "/network_config.json";
}

QString NetworkManager::legacyConfigFilePath() {
  return QCoreApplication::applicationDirPath() + "/network_config.json";
}

bool NetworkManager::load() {
  QString path = configFilePath();
  if (!QFile::exists(path)) {
    // A config that lives beside the executable keeps loading until the
    // first save() writes the per-user location, which wins from then on —
    // settings stored next to the binary migrate on the next save.
    const QString legacyPath = legacyConfigFilePath();
    if (QFile::exists(legacyPath)) path = legacyPath;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    spdlog::info("NetworkManager: no config file at {}, using defaults",
                 path.toStdString());
    return false;
  }

  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  if (!document.isObject()) {
    spdlog::warn("NetworkManager: malformed config file at {}",
                 path.toStdString());
    return false;
  }

  const QJsonObject rootObject = document.object();
  if (rootObject.contains("port")) setPort(rootObject["port"].toInt(m_port));
  return true;
}

bool NetworkManager::save() const {
  QJsonObject rootObject;
  rootObject["port"] = m_port;

  const QString path = configFilePath();
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    spdlog::error("NetworkManager: failed to write config file at {}",
                  path.toStdString());
    return false;
  }
  file.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
  return true;
}
