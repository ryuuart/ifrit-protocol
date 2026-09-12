#include "NetworkManager.h"

#include <spdlog/spdlog.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <utility>

#include "UdpReceiver.h"

NetworkManager::NetworkManager(boost::asio::any_io_executor executor,
                               uint16_t port, QObject* parent)
    : QObject(parent),
      m_port(port),
      m_statusText(tr("Stopped")),
      m_receiver(
          std::make_unique<spellcircle::UdpReceiver>(std::move(executor))) {}

NetworkManager::~NetworkManager() {
  // Retire callbacks before this QObject and its queued deliveries disappear.
  m_receiver->stop();
}

void NetworkManager::setPort(int port) {
  const auto boundedPort = static_cast<uint16_t>(qBound(1, port, 65535));
  if (m_port == boundedPort) return;
  m_port = boundedPort;
  emit portChanged();

  if (m_requestedListening) start();
}

void NetworkManager::start() {
  const uint64_t generation = ++m_generation;
  setRequestedListening(true);
  setListening(false);
  setStatusText(tr("Starting UDP :%1").arg(m_port));
  m_receiver->start(
      m_port,
      [this, generation](spellcircle::Datagram datagram) {
        QByteArray bytes(reinterpret_cast<const char*>(datagram.payload.data()),
                         static_cast<qsizetype>(datagram.payload.size()));
        QString sourceText = QString::fromStdString(datagram.source);
        QMetaObject::invokeMethod(
            this,
            [this, generation, sourceText = std::move(sourceText),
             bytes = std::move(bytes), receivedAt = datagram.receivedAt] {
              if (generation != m_generation || !m_requestedListening) return;
              emit spellCircleReceived(sourceText, bytes, receivedAt);
            },
            Qt::QueuedConnection);
      },
      [this, generation](spellcircle::UdpReceiver::Status status) {
        QMetaObject::invokeMethod(
            this,
            [this, generation, status = std::move(status)] {
              if (generation != m_generation || !m_requestedListening) return;
              if (status.error) {
                setRequestedListening(false);
                setListening(false);
                const std::string error = status.error.message();
                spdlog::error("UDP receiver failed on port {}: {}", status.port,
                              error);
                setStatusText(tr("UDP failed on :%1 — %2")
                                  .arg(status.port)
                                  .arg(QString::fromStdString(error)));
                return;
              }
              spdlog::info("UDP listening on :{}", status.port);
              setListening(true);
              setStatusText(tr("Listening on UDP :%1").arg(status.port));
            },
            Qt::QueuedConnection);
      });
}

void NetworkManager::stop() {
  ++m_generation;
  setRequestedListening(false);
  m_receiver->stop();
  spdlog::info("UDP socket closed");
  setListening(false);
  setStatusText(tr("Stopped"));
}

void NetworkManager::setListening(bool listening) {
  if (m_listening == listening) return;
  const bool wasStarting = starting();
  m_listening = listening;
  emit listeningChanged();
  if (wasStarting != starting()) emit startingChanged();
}

void NetworkManager::setRequestedListening(bool requested) {
  const bool wasStarting = starting();
  m_requestedListening = requested;
  if (wasStarting != starting()) emit startingChanged();
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
