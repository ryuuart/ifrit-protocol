/** @file
 * WHAT ONE FEED'S CALLBACKS SHARE: the feed everything is delivered
 * into, the arrangement its connections stand on, the connections
 * standing now, and the two small things one message on its way out or
 * in is made of.
 */

#pragma once

#include <msquic.h>

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "sigilio/hub/Feed.h"
#include "sigilio/source/Source.h"

namespace sigil::io::quic {

class Peer;

/** WHAT A FEED'S CALLBACKS SHARE: the feed everything is delivered into,
 *  the arrangement its connections stand on, what a send goes out as,
 *  and the connections standing now.
 *
 *  Held by the door and by every connection alike, so a callback still
 *  running when the feed is let go keeps everything it reads. The feed
 *  itself is held weakly: where it cannot be locked there is nobody left
 *  to deliver to. */
struct Session : std::enable_shared_from_this<Session> {
  ~Session() { giveBack(); }

  /** Gives the arrangement back, once. The door does it as it closes,
   *  which is never from inside one of the library's callbacks, and the
   *  destructor is only what catches a session no door closed. A
   *  connection still ending does not lose what it stands on: the
   *  library holds a reference of its own for every connection that took
   *  this arrangement. */
  void giveBack();

  std::weak_ptr<Feed> feed;
  HQUIC configuration = nullptr;
  bool datagrams = false;
  /** What a CALL reports and what every arrival on one names; empty on a
   *  door that holds a port, whose arrivals are named for the connection
   *  they came in on. */
  std::string called;

  /** Guards the two below. It is taken for the moves that add a
   *  connection, find one and take one out, never across a write: a peer
   *  waits for another peer and never for a stream. */
  std::mutex gate;
  /** How many connections this feed has taken, which is what numbers
   *  them. */
  uint64_t taken = 0;
  /** EVERY CONNECTION STANDING, under the address its messages are named
   *  by, which is what an answer to one of them is found through. */
  std::unordered_map<std::string, std::shared_ptr<Peer>> peers;
};

/** Delivers one message into @p session's feed, naming @p from. */
void deliver(const Session& session, const std::string& from, Bytes message);

/** ONE MESSAGE ON ITS WAY OUT: the bytes, and the buffer that points at
 *  them. It outlives the call that handed it over — the library writes
 *  FROM these bytes rather than from a copy of its own — and is let go
 *  when the stream it went on has ended, or when the datagram it was has
 *  reached a state it will not leave. */
struct Sending {
  explicit Sending(const Bytes& message) : bytes(message.bytes) {
    buffer.Length = static_cast<uint32_t>(bytes.size());
    buffer.Buffer = reinterpret_cast<uint8_t*>(bytes.data());
  }

  std::vector<std::byte> bytes;
  QUIC_BUFFER buffer{};
};

/** ONE MESSAGE ON ITS WAY IN: the session it will be delivered into, the
 *  name it will arrive under, and the bytes gathered so far. A peer's
 *  stream is one message, so what arrives in however many pieces is one
 *  arrival when the end of the stream comes — and a stream that grows
 *  past what a message may be is abandoned, its bytes dropped and
 *  nothing delivered. */
struct Incoming {
  std::shared_ptr<Session> session;
  std::string named;
  Bytes message;
  bool abandoned = false;
  bool delivered = false;
};

}  // namespace sigil::io::quic
