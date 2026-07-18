#include "NetworkManager.h"
#include "SpellCircle_generated.h"
#include "UdpReceiver.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <flatbuffers/flatbuffers.h>
#include <spdlog/spdlog.h>

NetworkManager::NetworkManager(uint16_t port, QObject *parent)
    : QObject(parent), m_port(port), m_statusText(tr("Stopped")),
      m_receiver(std::make_unique<spellcircle::UdpReceiver>()) {}

NetworkManager::~NetworkManager() {
  // Join the I/O thread before members go away — after stop() returns the
  // receiver invokes no more handlers.
  m_receiver->stop();
}

void NetworkManager::setPort(int port) {
  const auto boundedPort = static_cast<uint16_t>(qBound(1, port, 65535));
  if (m_port == boundedPort)
    return;
  m_port = boundedPort;
  emit portChanged();

  if (m_listening)
    start();
}

bool NetworkManager::start() {
  // The receiver rebinds in place (a port change while listening tears the
  // previous socket down first). The handler runs on the receiver's I/O
  // thread — hop onto this object's thread before touching any state; the
  // queued call is dropped automatically if this object is destroyed first.
  const std::string error = m_receiver->start(
      m_port,
      [this](std::vector<std::uint8_t> payload, std::string source) {
        QByteArray bytes(reinterpret_cast<const char *>(payload.data()),
                         static_cast<qsizetype>(payload.size()));
        QString sourceText = QString::fromStdString(source);
        QMetaObject::invokeMethod(
            this,
            [this, sourceText = std::move(sourceText),
             bytes = std::move(bytes)] { deliverDatagram(sourceText, bytes); },
            Qt::QueuedConnection);
      });

  if (!error.empty()) {
    spdlog::error("UDP bind failed on port {}: {}", m_port, error);
    setListening(false);
    setStatusText(tr("Bind failed on :%1 — %2")
                      .arg(m_port)
                      .arg(QString::fromStdString(error)));
    return false;
  }

  spdlog::info("UDP listening on :{}", m_port);
  setListening(true);
  setStatusText(tr("Listening on UDP :%1").arg(m_port));
  return true;
}

void NetworkManager::stop() {
  m_receiver->stop();
  spdlog::info("UDP socket closed");
  setListening(false);
  setStatusText(tr("Stopped"));
}

void NetworkManager::deliverDatagram(const QString &source,
                                     const QByteArray &payload) {
  flatbuffers::Verifier verifier(
      reinterpret_cast<const uint8_t *>(payload.constData()),
      static_cast<size_t>(payload.size()));
  if (!SpellCircle::VerifySceneBuffer(verifier)) {
    spdlog::warn("Dropped invalid SpellCircle buffer from {}",
                 source.toStdString());
    return;
  }

  emit spellCircleReceived(source, payload);
}

void NetworkManager::setListening(bool listening) {
  if (m_listening == listening)
    return;
  m_listening = listening;
  emit listeningChanged();
}

void NetworkManager::setStatusText(const QString &statusText) {
  if (m_statusText == statusText)
    return;
  m_statusText = statusText;
  emit statusTextChanged();
}

QString NetworkManager::configFilePath() {
  return QCoreApplication::applicationDirPath() + "/network_config.json";
}

bool NetworkManager::load() {
  QFile file(configFilePath());
  if (!file.open(QIODevice::ReadOnly)) {
    spdlog::info("NetworkManager: no config file at {}, using defaults",
                 configFilePath().toStdString());
    return false;
  }

  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  if (!document.isObject()) {
    spdlog::warn("NetworkManager: malformed config file at {}",
                 configFilePath().toStdString());
    return false;
  }

  const QJsonObject rootObject = document.object();
  if (rootObject.contains("port"))
    setPort(rootObject["port"].toInt(m_port));
  return true;
}

bool NetworkManager::save() const {
  QJsonObject rootObject;
  rootObject["port"] = m_port;

  QFile file(configFilePath());
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    spdlog::error("NetworkManager: failed to write config file at {}",
                  configFilePath().toStdString());
    return false;
  }
  file.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
  return true;
}
