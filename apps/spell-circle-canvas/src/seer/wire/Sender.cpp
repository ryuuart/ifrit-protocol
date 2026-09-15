/** @file
 * The peer a message goes to, the one send, the repeat the caller's
 * clock drives, and the OSC message a reader spells.
 */

#include "sigilseer/wire/Sender.h"

#include <sigildata/decode/Json.h>
#include <sigildata/decode/Osc.h>

#include <optional>
#include <utility>
#include <vector>

#include "sigilseer/wire/Wires.h"

namespace sigil::seer {
namespace {

/** Whether @p text holds anything but blanks. An editor a reader has
 *  typed nothing into is a message with no arguments, not a document
 *  that would not parse. */
bool anythingIn(std::string_view text) {
  for (const char letter : text)
    if (letter != ' ' && letter != '\t' && letter != '\n' && letter != '\r')
      return true;
  return false;
}

}  // namespace

io::Bytes oscMessage(std::string_view address, std::string_view arguments) {
  data::Json carried{data::Json::Array{}};
  if (anythingIn(arguments)) {
    std::optional<data::Json> read = data::decodeJson(arguments);
    if (!read) return {};
    carried = std::move(*read);
  }
  io::Bytes message;
  message.bytes = data::encodeOsc(address, carried);
  return message;
}

Sender::Sender(Wires& wires) : m_wires(wires) {}

std::shared_ptr<io::Feed> Sender::openPeer(std::string_view uri) {
  m_peer = m_wires.open(uri);
  m_peerUri = std::string(uri);
  return m_peer;
}

bool Sender::send(const io::Bytes& bytes) {
  if (!m_peer || !m_peer->send(bytes)) return false;
  ++m_sent;
  return true;
}

void Sender::repeat(io::Bytes bytes, double period) {
  if (period <= 0) {
    stopRepeating();
    return;
  }
  m_message = std::move(bytes);
  m_period = period;
  m_repeating = true;
  m_due.reset();
}

void Sender::stopRepeating() {
  m_repeating = false;
  m_due.reset();
}

void Sender::tick(double seconds) {
  if (!m_repeating) return;
  if (m_due && seconds < *m_due) return;
  send(m_message);
  m_due = seconds + m_period;
}

}  // namespace sigil::seer
