#pragma once

/** @file
 * WHAT IS ABOUT TO BE SENT: the peer, the message, and how the message
 * is written.
 *
 * The editor holds text either way. Read as text it is sent as its UTF-8
 * bytes; read as hexadecimal it is the bytes those digits spell, which
 * is how a wire that carries no text at all is answered by hand. A
 * message the digits do not spell is not sent, and the note says so
 * rather than a wrong message going out.
 */

#include <QtQml/qqmlregistration.h>
#include <sigilseer/wire/Sender.h>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <optional>

/** The send pane's state, and the one sender behind it. */
class SendForm : public QObject {
  Q_OBJECT
  QML_ANONYMOUS

  /** The peer every message goes to, as a URI. */
  Q_PROPERTY(QString peerUri READ peerUri WRITE setPeerUri NOTIFY peerChanged)
  /** What the editor holds. */
  Q_PROPERTY(
      QString message READ message WRITE setMessage NOTIFY messageChanged)
  /** Whether the editor's text is read as hexadecimal digits rather than
   *  as text. */
  Q_PROPERTY(bool hexadecimal READ hexadecimal WRITE setHexadecimal NOTIFY
                 hexadecimalChanged)
  /** Whether the message goes out again on every frame. */
  Q_PROPERTY(
      bool repeating READ repeating WRITE setRepeating NOTIFY repeatingChanged)
  /** Whether every message arriving on the wire being read is sent
   *  straight back out to the peer. */
  Q_PROPERTY(bool loopingBack READ loopingBack WRITE setLoopingBack NOTIFY
                 loopingBackChanged)
  /** How many messages have left through the peer, echoes included. */
  Q_PROPERTY(qulonglong sent READ sent NOTIFY sentChanged)
  /** What happened last: why nothing was sent, or what the peer said.
   *  Empty when there is nothing to say. */
  Q_PROPERTY(QString note READ note NOTIFY noteChanged)

 public:
  /** @p period is how often a repeating message goes out — one frame of
   *  the host's own loop, since that loop is what drives the sender. */
  SendForm(sigil::seer::Sender& sender, double period,
           QObject* parent = nullptr);

  [[nodiscard]] QString peerUri() const { return m_peerUri; }
  [[nodiscard]] QString message() const { return m_message; }
  [[nodiscard]] bool hexadecimal() const { return m_hexadecimal; }
  [[nodiscard]] bool repeating() const { return m_sender.repeating(); }
  [[nodiscard]] bool loopingBack() const { return m_loopingBack; }
  [[nodiscard]] qulonglong sent() const { return m_sender.sent(); }
  [[nodiscard]] QString note() const { return m_note; }

  void setPeerUri(const QString& uri);
  void setMessage(const QString& message);
  void setHexadecimal(bool hexadecimal);
  void setRepeating(bool repeating);
  void setLoopingBack(bool looping);

  /** Sends what the editor holds, once. */
  Q_INVOKABLE void sendOnce();

  /** Sends @p bytes to the peer without touching the editor, which is
   *  how an arrival is echoed. */
  bool echo(const sigil::io::Bytes& bytes);

  /** Tells the pane that the sender's count moved, so a message sent by
   *  the repeat or by an echo shows up without the pane asking. */
  void refresh();

 signals:
  void peerChanged();
  void messageChanged();
  void hexadecimalChanged();
  void repeatingChanged();
  void loopingBackChanged();
  void sentChanged();
  void noteChanged();

 private:
  /** The bytes the editor spells, or nothing when it spells none — which
   *  leaves the note saying which character stopped it. */
  std::optional<sigil::io::Bytes> messageBytes();

  /** Opens the peer when the URI names one this is not already speaking
   *  to. False when there is no peer to send through, with the note
   *  saying why. */
  bool reachPeer();

  void setNote(const QString& note);

  sigil::seer::Sender& m_sender;
  const double m_period;
  QString m_peerUri;
  QString m_message;
  QString m_note;
  bool m_hexadecimal = false;
  bool m_loopingBack = false;
  qulonglong m_sentSeen = 0;
};
