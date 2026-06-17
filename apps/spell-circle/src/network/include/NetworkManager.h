#pragma once
#include <QObject>
#include <QUdpSocket>

class NetworkManager : public QObject {
  Q_OBJECT

public:
  static constexpr uint16_t kDefaultPort = 27015;

  explicit NetworkManager(uint16_t port = kDefaultPort,
                          QObject *parent = nullptr);

  bool start();
  void stop();

signals:
  void spellCircleReceived(const QString &source, const QByteArray &payload);

private slots:
  void onReadyRead();

private:
  uint16_t m_port;
  QUdpSocket *m_socket;
};
