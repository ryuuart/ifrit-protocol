/** @file
 * The send pane's state: reading the editor as bytes, reaching the peer,
 * and the one message or the repeat that goes out.
 */

#include "SendForm.h"

#include <QtCore/QByteArray>
#include <QtCore/QChar>
#include <QtCore/QLatin1String>
#include <cstddef>
#include <memory>
#include <string>

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
  // A repeat is sending what the editor held when it was turned on, so
  // an edit while it runs re-arms it with what the editor holds now.
  if (repeating()) setRepeating(true);
}

void SendForm::setHexadecimal(bool hexadecimal) {
  if (hexadecimal == m_hexadecimal) return;
  m_hexadecimal = hexadecimal;
  emit hexadecimalChanged();
  if (repeating()) setRepeating(true);
}

bool SendForm::oscPeer() const {
  // The scheme is where a reader said what the other end speaks, and it
  // is the only place that says it: the same datagram socket carries
  // packets under one name and anything at all under the other.
  return m_peerUri.startsWith(QLatin1String("osc://"));
}

void SendForm::setOscAddress(const QString& address) {
  if (address == m_oscAddress) return;
  m_oscAddress = address;
  emit oscAddressChanged();
  // A repeat is sending the packet the address held when it was turned
  // on, so an edit while it runs re-arms it with the address now.
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
  if (oscPeer()) {
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
        "no peer: a message goes to udp://host:port or osc://host:port"));
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
