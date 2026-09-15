/** @file
 * The send pane's state: the form the peer's own dialect asks for,
 * reading what it holds as bytes, filling it in from what a run says,
 * reaching the peer, and the one message or the repeat that goes out.
 */

#include "SendForm.h"

#include <sigilseer/wire/Rendering.h>

#include <QtCore/QByteArray>
#include <QtCore/QChar>
#include <QtCore/QLatin1String>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

sigil::io::Bytes bytesOf(const QByteArray& array) {
  const auto* const first =
      reinterpret_cast<const std::byte*>(array.constData());
  sigil::io::Bytes bytes;
  bytes.bytes.assign(first, first + array.size());
  return bytes;
}

/** The value of one hexadecimal digit, or -1 when it is not one. */
int digitOf(QChar character) {
  const char16_t code = character.unicode();
  if (code >= u'0' && code <= u'9') return code - u'0';
  if (code >= u'a' && code <= u'f') return code - u'a' + 10;
  if (code >= u'A' && code <= u'F') return code - u'A' + 10;
  return -1;
}

/** WHAT THE TWO NUMBERS OF @p kind ARE CALLED, the second being empty
 *  for a kind that carries one number. They are the names the message
 *  itself uses, so a reader typing into the field and a reader looking
 *  at the arrival on the other pane read one word for one thing. */
std::pair<QString, QString> numbersOf(const QString& kind) {
  if (kind == QLatin1String("NoteOn") || kind == QLatin1String("NoteOff"))
    return {QStringLiteral("Note"), QStringLiteral("Velocity")};
  if (kind == QLatin1String("PolyAftertouch"))
    return {QStringLiteral("Note"), QStringLiteral("Pressure")};
  if (kind == QLatin1String("ControlChange"))
    return {QStringLiteral("Controller"), QStringLiteral("Value")};
  if (kind == QLatin1String("ProgramChange"))
    return {QStringLiteral("Program"), QString()};
  if (kind == QLatin1String("Aftertouch"))
    return {QStringLiteral("Pressure"), QString()};
  if (kind == QLatin1String("PitchBend"))
    return {QStringLiteral("Bend"), QString()};
  return {QString(), QString()};
}

}  // namespace

SendForm::SendForm(sigil::seer::Sender& sender, double period, QObject* parent)
    : QObject(parent), m_sender(sender), m_period(period) {}

void SendForm::setPeerUri(const QString& uri) {
  if (uri == m_peerUri) return;
  m_peerUri = uri;
  emit peerChanged();
}

void SendForm::setMessage(const QString& message) {
  if (message == m_message) return;
  m_message = message;
  emit messageChanged();
  rearmRepeat();
}

void SendForm::setHexadecimal(bool hexadecimal) {
  if (hexadecimal == m_hexadecimal) return;
  m_hexadecimal = hexadecimal;
  emit hexadecimalChanged();
  rearmRepeat();
}

QString SendForm::dialect() const {
  // The scheme is where a reader said what the other end speaks, and it
  // is the only place that says it: one datagram socket carries packets
  // under one name, a universe of dimmers under another, and anything at
  // all under a third.
  const std::string_view word = sigil::seer::dialect(m_peerUri.toStdString());
  return QString::fromUtf8(word.data(), qsizetype(word.size()));
}

void SendForm::setOscAddress(const QString& address) {
  if (address == m_oscAddress) return;
  m_oscAddress = address;
  emit oscAddressChanged();
  rearmRepeat();
}

QStringList SendForm::midiKinds() const {
  // The kinds a person plays from a form. The two a keyboard leans out
  // are played by leaning on it rather than typed, and the messages
  // addressed to the room instead of to a channel are not a channel's
  // to send, so neither stands in a chooser of channel messages.
  return {QStringLiteral("NoteOn"), QStringLiteral("NoteOff"),
          QStringLiteral("ControlChange"), QStringLiteral("ProgramChange"),
          QStringLiteral("PitchBend")};
}

QString SendForm::midiFirstName() const { return numbersOf(m_midiKind).first; }

QString SendForm::midiSecondName() const {
  return numbersOf(m_midiKind).second;
}

void SendForm::setMidiKind(const QString& kind) {
  if (kind == m_midiKind) return;
  m_midiKind = kind;
  emit midiChanged();
  rearmRepeat();
}

void SendForm::setMidiChannel(int channel) {
  if (channel == m_midiChannel) return;
  m_midiChannel = channel;
  emit midiChanged();
  rearmRepeat();
}

void SendForm::setMidiFirst(int number) {
  if (number == m_midiFirst) return;
  m_midiFirst = number;
  emit midiChanged();
  rearmRepeat();
}

void SendForm::setMidiSecond(int number) {
  if (number == m_midiSecond) return;
  m_midiSecond = number;
  emit midiChanged();
  rearmRepeat();
}

void SendForm::setDmxUniverse(int universe) {
  if (universe == m_dmxUniverse) return;
  m_dmxUniverse = universe;
  emit dmxChanged();
  rearmRepeat();
}

void SendForm::rearmRepeat() {
  // A repeat is sending the message the form held when it was turned on,
  // so an edit while it runs arms it again with what the form holds now.
  if (repeating()) setRepeating(true);
}

void SendForm::setRepeating(bool repeating) {
  if (!repeating) {
    m_sender.stopRepeating();
    emit repeatingChanged();
    return;
  }
  const std::optional<sigil::io::Bytes> bytes = messageBytes();
  if (!bytes || !reachPeer()) {
    m_sender.stopRepeating();
    emit repeatingChanged();
    return;
  }
  m_sender.repeat(*bytes, m_period);
  setNote({});
  emit repeatingChanged();
}

void SendForm::setLoopingBack(bool looping) {
  if (looping == m_loopingBack) return;
  m_loopingBack = looping;
  if (looping) reachPeer();
  emit loopingBackChanged();
}

void SendForm::sendOnce() {
  const std::optional<sigil::io::Bytes> bytes = messageBytes();
  if (!bytes) return;
  if (!reachPeer()) return;
  if (!m_sender.send(*bytes)) {
    setNote(QStringLiteral(
        "nothing went out: this wire has no way back through it"));
    return;
  }
  setNote({});
  refresh();
}

bool SendForm::fill(const QString& said) {
  // An instrument is the one peer with no editor in front of it, so
  // what is said fills that form's own fields. Every other peer holds
  // its message where a reader would have typed it.
  if (dialect() != QLatin1String("midi")) {
    setMessage(said);
    return true;
  }
  const std::optional<sigil::seer::MidiWords> words =
      sigil::seer::midiWords(said.toStdString());
  if (!words) {
    setNote(QStringLiteral(
        "no message: a kind, a channel and the numbers it carries, as in "
        "\"NoteOn 1 60 100\""));
    return false;
  }
  setMidiKind(QString::fromStdString(words->kind));
  setMidiChannel(words->channel);
  setMidiFirst(words->first);
  setMidiSecond(words->second);
  return true;
}

bool SendForm::echo(const sigil::io::Bytes& bytes) {
  if (!m_loopingBack || !reachPeer()) return false;
  return m_sender.send(bytes);
}

void SendForm::refresh() {
  if (m_sentSeen == m_sender.sent()) return;
  m_sentSeen = m_sender.sent();
  emit sentChanged();
}

std::optional<sigil::io::Bytes> SendForm::messageBytes() {
  const QString speaks = dialect();
  if (speaks == QLatin1String("osc")) {
    sigil::io::Bytes packet = sigil::seer::oscMessage(
        m_oscAddress.toStdString(), m_message.toStdString());
    if (packet.bytes.empty()) {
      setNote(QStringLiteral(
          "no packet: an OSC message is an address, and arguments spelled "
          "as a JSON list"));
      return std::nullopt;
    }
    return packet;
  }
  if (speaks == QLatin1String("midi")) {
    sigil::io::Bytes played = sigil::seer::midiMessage(
        m_midiKind.toStdString(), m_midiChannel, m_midiFirst, m_midiSecond);
    if (played.bytes.empty()) {
      setNote(QStringLiteral("no message: \"%1\" is no kind this wire carries")
                  .arg(m_midiKind));
      return std::nullopt;
    }
    return played;
  }
  if (speaks == QLatin1String("dmx")) {
    sigil::io::Bytes packet =
        sigil::seer::dmxMessage(m_dmxUniverse, m_message.toStdString());
    if (packet.bytes.empty()) {
      setNote(QStringLiteral(
          "no packet: the dimmers of a universe are a JSON list of "
          "numbers, as in [255, 128, 0]"));
      return std::nullopt;
    }
    return packet;
  }
  if (!m_hexadecimal) return bytesOf(m_message.toUtf8());

  sigil::io::Bytes bytes;
  int high = -1;
  for (const QChar character : m_message) {
    if (character.isSpace()) continue;
    const int digit = digitOf(character);
    if (digit < 0) {
      setNote(QStringLiteral("not hexadecimal: \"%1\"").arg(character));
      return std::nullopt;
    }
    if (high < 0) {
      high = digit;
      continue;
    }
    bytes.bytes.push_back(static_cast<std::byte>((high << 4) | digit));
    high = -1;
  }
  if (high >= 0) {
    setNote(QStringLiteral("a byte is two digits, and the last one is alone"));
    return std::nullopt;
  }
  return bytes;
}

bool SendForm::reachPeer() {
  if (m_peerUri.isEmpty()) {
    setNote(QStringLiteral(
        "no peer: a message goes to udp://host:port, osc://host:port, "
        "artnet://host:6454 or midi://out/NAME"));
    return false;
  }
  const std::string uri = m_peerUri.toStdString();
  if (!m_sender.peer() || m_sender.peerUri() != uri) {
    const std::shared_ptr<sigil::io::Feed> peer = m_sender.openPeer(uri);
    if (!peer->error().empty()) {
      setNote(QString::fromStdString(peer->error()));
      return false;
    }
    setNote({});
  }
  return true;
}

void SendForm::setNote(const QString& note) {
  if (note == m_note) return;
  m_note = note;
  emit noteChanged();
}
