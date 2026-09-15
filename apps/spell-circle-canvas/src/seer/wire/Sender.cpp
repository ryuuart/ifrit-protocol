/** @file
 * The peer a message goes to, the one send, and the repeat the caller's
 * clock drives.
 */

#include "sigilseer/wire/Sender.h"

#include <utility>

#include "sigilseer/wire/Wires.h"

namespace sigil::seer {

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
