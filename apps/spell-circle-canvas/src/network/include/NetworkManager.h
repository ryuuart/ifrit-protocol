#pragma once
#include <QObject>
#include <QUdpSocket>

/**
 * Listens on a UDP port for FlatBuffers-encoded SpellCircle Scene packets
 * and emits spellCircleReceived for each datagram that passes verification.
 */
class NetworkManager : public QObject {
  Q_OBJECT

public:
  static constexpr uint16_t kDefaultPort = 27015;

  explicit NetworkManager(uint16_t port = kDefaultPort,
                          QObject *parent = nullptr);

  /** Binds the UDP socket and begins listening. Returns false on bind failure. */
  bool start();

  /** Closes the UDP socket. */
  void stop();

signals:
  /** Emitted for each verified SpellCircle datagram. @p source is "ip:port". */
  void spellCircleReceived(const QString &source, const QByteArray &payload);

private slots:
  void onReadyRead();

private:
  uint16_t m_port;
  QUdpSocket *m_socket;
};
