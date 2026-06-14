#include "NetworkManager.h"
#include "SpellCircle_generated.h"
#include <QDebug>
#include <QHostAddress>
#include <flatbuffers/flatbuffers.h>
#include <spdlog/spdlog.h>

NetworkManager::NetworkManager(uint16_t port, QObject *parent)
    : QObject(parent), m_port(port) {
  connect(&m_socket, &QUdpSocket::readyRead, this,
          &NetworkManager::onReadyRead);
}

bool NetworkManager::start() {
  if (!m_socket.bind(QHostAddress::Any, m_port)) {
    spdlog::error("UDP bind failed on port {}: {}", m_port,
                  m_socket.errorString().toStdString());
    return false;
  }
  spdlog::info("UDP listening on :{}", m_port);
  return true;
}

void NetworkManager::stop() {
  m_socket.close();
  spdlog::info("UDP socket closed");
}

void NetworkManager::onReadyRead() {
  while (m_socket.hasPendingDatagrams()) {
    QByteArray payload(static_cast<int>(m_socket.pendingDatagramSize()),
                       Qt::Uninitialized);
    QHostAddress sender;
    quint16 senderPort = 0;
    m_socket.readDatagram(payload.data(), payload.size(), &sender, &senderPort);

    const QString source =
        QStringLiteral("%1:%2").arg(sender.toString()).arg(senderPort);

    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t *>(payload.constData()),
        static_cast<size_t>(payload.size()));
    if (!SpellCircle::VerifySceneBuffer(verifier)) {
      spdlog::warn("Dropped invalid SpellCircle buffer from {}",
                   source.toStdString());
      continue;
    }

    emit spellCircleReceived(source, payload);
  }
}
