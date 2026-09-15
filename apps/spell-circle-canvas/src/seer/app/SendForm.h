#pragma once

/** @file
 * WHAT IS ABOUT TO BE SENT: the peer, the message, and how the message
 * is written.
 *
 * WHAT THE PEER SPEAKS IS WHAT A READER TYPES. A wire that carries
 * whatever a sender puts on it takes the editor: read as text it goes
 * out as its UTF-8 bytes, and read as hexadecimal it is the bytes those
 * digits spell, which is how a wire that carries no text at all is
 * answered by hand. A wire that names a format takes the message that
 * format is made of instead — an address with its arguments under it, a
 * note played on a channel, a universe of dimmers — because a packet, a
 * status byte or a header spelled by hand is a run of counts and padding
 * a reader gets wrong once and then distrusts. A message the form does
 * not spell is not sent, and the note says so rather than a wrong
 * message going out.
 */

#include <QtQml/qqmlregistration.h>
#include <sigilseer/wire/Sender.h>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
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
   *  as text. A peer that speaks a format of its own reads what is
   *  typed as that format whichever way this stands. */
  Q_PROPERTY(bool hexadecimal READ hexadecimal WRITE setHexadecimal NOTIFY
                 hexadecimalChanged)
  /** The word the peer's scheme speaks in — "osc", "midi", "dmx" and
   *  the rest — which is what says which form the pane offers. Empty
   *  before a peer is named and on a scheme there is no word for. */
  Q_PROPERTY(QString dialect READ dialect NOTIFY peerChanged)
  /** The address an OSC message is sent to, the one part of a packet
   *  that is not in the editor. */
  Q_PROPERTY(QString oscAddress READ oscAddress WRITE setOscAddress NOTIFY
                 oscAddressChanged)
  /** THE MIDI MESSAGE A FORM SPELLS: what kind it is, the channel it is
   *  played on, and the numbers that kind carries — under the names that
   *  kind calls them, an empty second name being a kind that takes one
   *  number. */
  Q_PROPERTY(QStringList midiKinds READ midiKinds CONSTANT)
  Q_PROPERTY(
      QString midiKind READ midiKind WRITE setMidiKind NOTIFY midiChanged)
  Q_PROPERTY(
      int midiChannel READ midiChannel WRITE setMidiChannel NOTIFY midiChanged)
  Q_PROPERTY(int midiFirst READ midiFirst WRITE setMidiFirst NOTIFY midiChanged)
  Q_PROPERTY(
      int midiSecond READ midiSecond WRITE setMidiSecond NOTIFY midiChanged)
  Q_PROPERTY(QString midiFirstName READ midiFirstName NOTIFY midiChanged)
  Q_PROPERTY(QString midiSecondName READ midiSecondName NOTIFY midiChanged)
  /** Which universe a packet of dimmers is for; the levels themselves
   *  are the editor, as a JSON list of numbers. */
  Q_PROPERTY(
      int dmxUniverse READ dmxUniverse WRITE setDmxUniverse NOTIFY dmxChanged)
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
  [[nodiscard]] QString dialect() const;
  [[nodiscard]] QString oscAddress() const { return m_oscAddress; }
  [[nodiscard]] QStringList midiKinds() const;
  [[nodiscard]] QString midiKind() const { return m_midiKind; }
  [[nodiscard]] int midiChannel() const { return m_midiChannel; }
  [[nodiscard]] int midiFirst() const { return m_midiFirst; }
  [[nodiscard]] int midiSecond() const { return m_midiSecond; }
  [[nodiscard]] QString midiFirstName() const;
  [[nodiscard]] QString midiSecondName() const;
  [[nodiscard]] int dmxUniverse() const { return m_dmxUniverse; }
  [[nodiscard]] bool repeating() const { return m_sender.repeating(); }
  [[nodiscard]] bool loopingBack() const { return m_loopingBack; }
  [[nodiscard]] qulonglong sent() const { return m_sender.sent(); }
  [[nodiscard]] QString note() const { return m_note; }

  void setPeerUri(const QString& uri);
  void setMessage(const QString& message);
  void setHexadecimal(bool hexadecimal);
  void setOscAddress(const QString& address);
  void setMidiKind(const QString& kind);
  void setMidiChannel(int channel);
  void setMidiFirst(int number);
  void setMidiSecond(int number);
  void setDmxUniverse(int universe);
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
  void oscAddressChanged();
  void midiChanged();
  void dmxChanged();
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

  /** Re-arms a repeat that is running, so an edit to what is being sent
   *  is what goes out from the next frame on rather than the message the
   *  form held when it was turned on. */
  void rearmRepeat();

  sigil::seer::Sender& m_sender;
  const double m_period;
  QString m_peerUri;
  QString m_message;
  QString m_note;
  /** Where a packet goes on the instrument at the other end. A wire
   *  carries one, so the pane opens on one rather than on nothing a
   *  reader would have to invent. */
  QString m_oscAddress = QStringLiteral("/sky/wind");
  /** A message somebody would play: middle C struck hard on the first
   *  channel. A form opening on nothing would have a reader inventing
   *  three numbers before they could send anything at all. */
  QString m_midiKind = QStringLiteral("NoteOn");
  int m_midiChannel = 1;
  int m_midiFirst = 60;
  int m_midiSecond = 100;
  /** The first universe, which is the one a desk plugged in and set to
   *  nothing is sending on. */
  int m_dmxUniverse = 0;
  bool m_hexadecimal = false;
  bool m_loopingBack = false;
  qulonglong m_sentSeen = 0;
};
