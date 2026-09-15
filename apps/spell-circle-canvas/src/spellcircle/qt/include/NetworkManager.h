#pragma once
#include <QByteArray>
#include <QObject>
#include <chrono>
#include <cstdint>
#include <memory>

class QTimer;

/**
 * Listens on a UDP port for FlatBuffers-encoded SpellCircle Scene packets
 * and emits spellCircleReceived for each datagram.
 *
 * The port is a door on a resource hub. Opening it binds the socket
 * there and then, so the outcome is known as soon as start() returns and
 * listening/statusText report it immediately. Datagrams reach the door
 * on the transport's own thread and wait in it; a timer at the render
 * frame drains them onto this object's thread, in arrival order, so
 * nothing is delivered onto the user interface thread from outside it.
 *
 * The port can be reassigned while listening: setPort() reopens the door
 * on the new port and the listening/statusText properties report the
 * outcome, so a settings UI can drive the receiver directly. The chosen
 * port persists to a per-user JSON file via load()/save().
 */
class NetworkManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
  Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

 public:
  static constexpr uint16_t kDefaultPort = 27015;

  explicit NetworkManager(uint16_t port = kDefaultPort,
                          QObject* parent = nullptr);
  ~NetworkManager() override;

  /** Returns the configured UDP port. */
  int port() const { return m_port; }

  /** Sets the UDP port, reopening the door of a listening receiver. */
  void setPort(int port);

  /** Returns true while the socket is bound and receiving. */
  bool listening() const { return m_listening; }

  /** Returns a one-line human-readable receiver state for status displays. */
  QString statusText() const { return m_statusText; }

  /** Opens the door on the configured port. Listening and statusText
   *  carry the bind's outcome when this returns. */
  Q_INVOKABLE void start();

  /** Closes the door, which stops its socket and discards what it still
   *  holds. */
  Q_INVOKABLE void stop();

  /** Reads the persisted port from the JSON config file, if present.
   *  Returns false (leaving current values untouched) if the file is
   *  missing or malformed. A changed port rebinds an active receiver. */
  Q_INVOKABLE bool load();

  /** Writes the current network configuration to the JSON config file. */
  Q_INVOKABLE bool save() const;

 public slots:
  /** Emits one spellCircleReceived for each datagram the door has taken
   *  since the last call, oldest first. The frame timer calls this; a
   *  host that paces itself may call it instead. */
  void readArrivals();

 signals:
  /** Emitted on this object's thread with the transport's receipt time.
   *  The scene model verifies the payload before decoding. */
  void spellCircleReceived(const QString& source, const QByteArray& payload,
                           std::chrono::steady_clock::time_point receivedAt);

  void portChanged();
  void listeningChanged();
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
  void setStatusText(const QString& statusText);

  /** The hub and the door it opened. Declared here and defined in the
   *  implementation, so this header spells no resource-library type and
   *  a consumer of it links none. */
  struct Door;

  uint16_t m_port;
  bool m_listening = false;
  QString m_statusText;
  std::unique_ptr<Door> m_door;
  QTimer* m_drain;
};
