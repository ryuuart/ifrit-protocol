#pragma once

/** @file
 * SENDING DOWN A WIRE: the peer a message goes to, one message or the
 * same message again and again, driven by the caller's own clock.
 *
 * The peer is a wire like any other — it is opened through Wires, it
 * stands in the same list, and what the peer sends back arrives on it —
 * so a reader watches the answer to what was just sent without opening
 * anything else. The repeat has no thread: `tick()` is what sends, so a
 * host that stops calling it stops sending, and nothing is in flight
 * once it returns.
 */

#include <sigilio/hub/Feed.h>
#include <sigilio/source/Source.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace sigil::seer {

class Wires;

/** THE WAY OUT: one peer, and the message that goes to it. */
class Sender {
 public:
  explicit Sender(Wires& wires);

  /** Opens @p uri as the peer every send reaches from now on, through
   *  the wires this was made over. A peer that cannot be opened is
   *  answered all the same, carrying the sentence that says why. */
  std::shared_ptr<io::Feed> openPeer(std::string_view uri);

  /** The peer, or null before one is opened. */
  const std::shared_ptr<io::Feed>& peer() const { return m_peer; }

  /** The URI the peer was opened on; empty before one is opened. */
  const std::string& peerUri() const { return m_peerUri; }

  /** Sends @p bytes to the peer once. False when there is no peer, when
   *  the wire is one-way, and when it is closed. */
  bool send(const io::Bytes& bytes);

  /** Sends @p bytes every @p period seconds for as long as tick() runs,
   *  beginning with the next tick. A period that is not positive stops
   *  the repeat instead. */
  void repeat(io::Bytes bytes, double period);

  /** Takes the repeat off; the peer and the message it was sending
   *  stay. */
  void stopRepeating();

  bool repeating() const { return m_repeating; }
  double period() const { return m_period; }

  /** Sends the repeated message when one is due at @p seconds on the
   *  caller's clock. A host that stalled sends one message and waits a
   *  whole period again: what was missed while nothing was running is
   *  not made up for in a burst. */
  void tick(double seconds);

  /** How many messages this sender has put on a wire. */
  uint64_t sent() const { return m_sent; }

 private:
  Wires& m_wires;
  std::shared_ptr<io::Feed> m_peer;
  std::string m_peerUri;
  io::Bytes m_message;
  bool m_repeating = false;
  double m_period = 0;
  /** When the next repeated message is due; nothing until the first
   *  tick after the repeat began, which is what makes that tick send. */
  std::optional<double> m_due;
  uint64_t m_sent = 0;
};

}  // namespace sigil::seer
