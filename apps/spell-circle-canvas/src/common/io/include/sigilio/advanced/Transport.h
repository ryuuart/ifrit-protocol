#pragma once

/** @file
 * @ingroup io-transport
 * WRITING A TRANSPORT: the producer's side of a feed. A transport is
 * handed the URI it is to open and an `Inlet` into the feed that URI
 * names; it delivers every message it receives through the inlet, says
 * through it what went wrong, and hands back the `TransportEnd` the feed
 * closes and sends through. A reader holds the `Feed` and none of this:
 * the verbs that put a message on a feed are the transport's alone.
 */

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "sigilio/source/Source.h"

namespace sigil::io {

class Feed;
class Hub;
class Inlet;

namespace detail {
class FeedDoor;

/** HOW A HUB FINDS THE TRANSPORTS LINKED INTO THIS PROGRAM: the transport
 *  feature hands its installer over here as the program starts, and a
 *  hub asked to listen on a scheme nothing is registered for runs it
 *  once — so linking the transports is what makes `Hub::listen()` open
 *  them. */
void setLinkedTransports(void (*install)(Hub& hub));
}  // namespace detail

namespace testing {
Inlet inletOf(const Feed& feed);
}  // namespace testing

/** WHAT A TRANSPORT HANDS BACK once it has opened a URI. */
struct TransportEnd {
  /** Closes the transport's end. The feed calls it once, from close()
   *  or from its destructor. */
  std::function<void()> close;
  /** Sends through the same door; empty when the way is one-way. */
  std::function<bool(const Bytes&)> send;
  /** Sends to ONE sender, named the way a message's `sender()` spells it;
   *  empty when the transport cannot address one. */
  std::function<bool(std::string_view to, const Bytes&)> sendTo;
  /** The peers attached to the door now, each named the way a
   *  message's `sender()` spells it; empty when the transport holds no peers
   *  of its own. */
  std::function<std::vector<std::string>()> peers;
  /** The local end as the transport bound it, "udp://[::]:52341";
   *  empty when it has none. */
  std::string localAddress;
};

/** THE PRODUCER'S SIDE OF ONE FEED: what a transport delivers through.
 *  It holds the feed weakly, so a feed nobody reads any more is gone and
 *  every verb here does nothing from then on — which is how a transport
 *  learns, through expired(), that its door can stop. Copyable, and
 *  callable from any thread: every copy is the same way in. */
class Inlet {
 public:
  /** An inlet into nothing: every verb does nothing, and expired() is
   *  true. */
  Inlet() = default;

  /** Puts one message on the feed, stamped with the seconds since the
   *  feed was made, naming @p sender the way a URI of the transport's
   *  scheme spells an address — `udp://127.0.0.1:52341` — or nobody
   *  when it is empty. */
  void deliver(Bytes payload, std::string sender = {}) const;

  /** Puts one message on the feed stamped with @p arrivedAt, the time
   *  since the feed opened as a recording carries it, and naming no
   *  sender: a recording holds the messages and not who sent them. */
  void deliver(Bytes payload, std::chrono::duration<double> arrivedAt) const;

  /** Says what went wrong, which the feed's `state().error` answers
   *  from then on; an empty reason takes off what stood there. The feed stays
   *  open: a transport that lost one message still has a door.
   *  @trap A TRANSPORT THAT OPENED NOTHING SAYS SO HERE, before it hands
   *  back the end it has none of: the feed is then one that was never
   *  opened, which is what lets the next ask for its URI open it again. */
  void fail(std::string why) const;

  /** Hands the feed an end opened after the transport answered — a
   *  conversation that stands only once its handshake is done. Once: a
   *  second end, and one handed to a feed that is already closed, is
   *  closed rather than kept. */
  void open(TransportEnd end) const;

  /** Shuts the door from the transport's side — the far end ended it —
   *  after which nothing more arrives; what was received stays
   *  readable. */
  void close() const;

  /** Whether the feed is gone: nobody holds it any more, so nothing
   *  delivered here would reach anyone. */
  bool expired() const;

 private:
  friend class Hub;
  friend Inlet testing::inletOf(const Feed& feed);
  explicit Inlet(std::weak_ptr<detail::FeedDoor> door);

  std::weak_ptr<detail::FeedDoor> m_feed;
};

/** HOW A SCHEME OPENS A DOOR: given the URI and the inlet into the feed
 *  it names, hands back the opened end. The transport keeps the inlet
 *  and delivers through it from whatever thread it runs on. */
using Transport =
    std::function<TransportEnd(std::string_view uri, Inlet inlet)>;

}  // namespace sigil::io
