#pragma once
#include <QByteArray>
#include <QObject>

#include <memory>

namespace spellcircle {
class UdpReceiver;
}

/**
 * Listens on a UDP port for FlatBuffers-encoded SpellCircle Scene packets
 * and emits spellCircleReceived for each datagram that passes verification.
 *
 * The transport is the shared Qt-free spellcircle::UdpReceiver (ASIO), the
 * same backend the native macOS app uses; this class is only the Qt-facing
 * adapter — datagrams are marshalled from the receiver's I/O thread onto
 * this object's thread before verification and signal emission.
 *
 * The port can be reassigned while listening: setPort() rebinds the socket
 * in place and the listening/statusText properties report the outcome, so a
 * settings UI can drive the receiver directly. The chosen port persists to a
 * JSON file next to the executable via load()/save(), following the same
 * convention as GraphicsConfig.
 */
class NetworkManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
  Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
  static constexpr uint16_t kDefaultPort = 27015;

  explicit NetworkManager(uint16_t port = kDefaultPort,
                          QObject *parent = nullptr);
  ~NetworkManager() override;

  /** Returns the configured UDP port. */
  int port() const { return m_port; }

  /** Sets the UDP port, rebinding immediately when currently listening. */
  void setPort(int port);

  /** Returns true while the socket is bound and receiving. */
  bool listening() const { return m_listening; }

  /** Returns a one-line human-readable receiver state for status displays. */
  QString statusText() const { return m_statusText; }

  /** Binds the UDP socket and begins listening. Returns false on bind
   *  failure; statusText carries the socket error either way. */
  Q_INVOKABLE bool start();

  /** Closes the UDP socket. */
  Q_INVOKABLE void stop();

  /** Reads the persisted port from the JSON config file, if present.
   *  Returns false (leaving current values untouched) if the file is
   *  missing or malformed. Does not rebind by itself. */
  Q_INVOKABLE bool load();

  /** Writes the current network configuration to the JSON config file. */
  Q_INVOKABLE bool save() const;

signals:
  /** Emitted for each verified SpellCircle datagram. @p source is "ip:port". */
  void spellCircleReceived(const QString &source, const QByteArray &payload);

  void portChanged();
  void listeningChanged();
  void statusTextChanged();

private:
  /** Runs on this object's thread: verifies and emits one datagram. */
  void deliverDatagram(const QString &source, const QByteArray &payload);
  /** Returns the platform-specific persistent configuration path. */
  static QString configFilePath();
  void setListening(bool listening);
  void setStatusText(const QString &statusText);

  uint16_t m_port;
  bool m_listening = false;
  QString m_statusText;
  std::unique_ptr<spellcircle::UdpReceiver> m_receiver;
};
