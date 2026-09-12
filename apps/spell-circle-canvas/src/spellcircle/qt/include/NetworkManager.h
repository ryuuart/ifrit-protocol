#pragma once
#include <QByteArray>
#include <QObject>
#include <boost/asio/any_io_executor.hpp>
#include <chrono>
#include <cstdint>
#include <memory>

namespace spellcircle {
class UdpReceiver;
}

/**
 * Listens on a UDP port for FlatBuffers-encoded SpellCircle Scene packets
 * and emits spellCircleReceived for each datagram.
 *
 * The transport runs on the application's supplied Boost.Asio executor.
 * Datagrams and binding results are marshalled onto this object's thread;
 * queued deliveries from a stopped or replaced binding are discarded.
 *
 * The port can be reassigned while listening: setPort() rebinds the socket
 * in place and the listening/statusText properties report the outcome, so a
 * settings UI can drive the receiver directly. The chosen port persists to a
 * per-user JSON file via load()/save().
 */
class NetworkManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
  Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)
  Q_PROPERTY(bool starting READ starting NOTIFY startingChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

 public:
  static constexpr uint16_t kDefaultPort = 27015;

  explicit NetworkManager(boost::asio::any_io_executor executor,
                          uint16_t port = kDefaultPort,
                          QObject* parent = nullptr);
  ~NetworkManager() override;

  /** Returns the configured UDP port. */
  int port() const { return m_port; }

  /** Sets the UDP port, replacing an active or pending binding. */
  void setPort(int port);

  /** Returns true while the socket is bound and receiving. */
  bool listening() const { return m_listening; }
  /** Whether a binding has been requested and is awaiting its result. */
  bool starting() const { return m_requestedListening && !m_listening; }

  /** Returns a one-line human-readable receiver state for status displays. */
  QString statusText() const { return m_statusText; }

  /** Requests a binding on the supplied executor. Listening and statusText
   *  report its asynchronous result and any later socket failure. */
  Q_INVOKABLE void start();

  /** Retires the binding and discards its queued deliveries. */
  Q_INVOKABLE void stop();

  /** Reads the persisted port from the JSON config file, if present.
   *  Returns false (leaving current values untouched) if the file is
   *  missing or malformed. A changed port rebinds an active receiver. */
  Q_INVOKABLE bool load();

  /** Writes the current network configuration to the JSON config file. */
  Q_INVOKABLE bool save() const;

 signals:
  /** Emitted on this object's thread with the transport's receipt time.
   *  The scene model verifies the payload before decoding. */
  void spellCircleReceived(const QString& source, const QByteArray& payload,
                           std::chrono::steady_clock::time_point receivedAt);

  void portChanged();
  void listeningChanged();
  void startingChanged();
  void statusTextChanged();

 private:
  /** The per-user config path. Writable and reinstall-safe, unlike a path
   *  beside the executable, which on macOS sits inside the .app bundle. */
  static QString configFilePath();
  /** The old beside-the-executable path, read by load() only when the
   *  per-user file does not exist yet — settings migrate on the next
   *  save(). */
  static QString legacyConfigFilePath();
  void setListening(bool listening);
  void setRequestedListening(bool requested);
  void setStatusText(const QString& statusText);

  uint16_t m_port;
  uint64_t m_generation = 0;
  bool m_requestedListening = false;
  bool m_listening = false;
  QString m_statusText;
  std::unique_ptr<spellcircle::UdpReceiver> m_receiver;
};
