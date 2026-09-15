#pragma once

/** @file
 * A CONNECTION: a feed read as values.
 *
 * A feed is bytes that keep arriving. A connection is that same door
 * one floor up: the bytes are read by the scheme the URI names — an OSC
 * packet through an `osc://` door, JSON text through every other — and
 * what a reader sees is the `Json` value, never the bytes. The newest
 * message is `latest()`, the ones not taken yet come out of `receive()`
 * in order, and a handler registered with `on()` runs for the messages
 * it names.
 *
 *     Connection sky(hub, "osc://:9000");
 *     sky.on("/sky/gust", [&](const Json& message) {
 *       gust = message["arguments"][0].number();
 *     });
 *     sky.on("*", [&](const Json&) { ++messages; });
 *     ...
 *     hub.dispatch(seconds);                   // the frame: handlers run here
 *     wind = sky.latest()["arguments"][0].number();
 *     sky.send("/sky/ack", Json::Array{1});
 *
 * NOTHING DRIVES IT BUT THE FRAME. A connection registers on the hub's
 * dispatch as it opens, so the call a host already makes once a frame
 * is what drains the feed, reads what arrived and runs the handlers.
 * Between two dispatches a connection answers exactly what the last one
 * left it, so every reading a frame takes agrees with every other.
 *
 * ONE THREAD. The value, the queue and the handlers are written and
 * read on the dispatching thread — the frame's — so a connection holds
 * no lock of its own; the feed underneath is the thread-safe part, and
 * a transport delivers into it from whatever thread it runs on. A
 * connection DRAINS the feed it is on, and draining is taking, so two
 * connections on one URI split the messages between them rather than
 * each seeing all of them.
 */

#include <sigildata/decode/Json.h>
#include <sigilio/hub/Feed.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace sigil::io {
// The door is opened on a hub, which a consumer of this header names
// through its own; nothing here reads one.
class Hub;
}  // namespace sigil::io

namespace sigil::data {

/** ONE DOOR, ITS MESSAGES, AND THE HANDLERS OVER THEM. */
class Connection {
 public:
  /** What runs for one message. */
  using Handler = std::function<void(const Json& message)>;

  /** A connection onto nothing: no door, no message, and every reading
   *  below the answer that says so. It is what a scene's member stands
   *  at before there is a hub to open it on. */
  Connection() = default;

  /** Opens @p uri on @p hub — the one feed that URI names — and
   *  registers on the hub's dispatch. The URI's SCHEME says how a
   *  message is read: `osc` is an OSC packet and anything else is JSON
   *  text. @p policy is the feed's, and bounds what receive() holds as
   *  well. */
  Connection(io::Hub& hub, std::string_view uri, io::FeedPolicy policy = {});

  Connection(Connection&&) noexcept = default;
  Connection& operator=(Connection&&) noexcept = default;
  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  /** THE NEWEST MESSAGE; null until one has arrived and been read. A
   *  message that cannot be read leaves it standing, so a sender
   *  speaking the wrong language cannot blank a scene. Inside a handler
   *  it is the message that handler was given. */
  const Json& latest() const;

  /** How many messages have arrived on the feed, whether or not they
   *  could be read; 0 before the first. */
  uint64_t generation() const;

  /** The next message this reader has not taken, in order; nothing when
   *  none is waiting, and never a wait. What the handlers see is not
   *  taken from here — one message reaches both. The queue holds what
   *  the policy's capacity says and its oldest falls off the front when
   *  it is full, which is what a reader that registered handlers and
   *  never calls this wants. */
  std::optional<Json> receive();

  /** Runs @p handler for every message named @p what, from now on.
   *
   *  A MESSAGE'S NAME is its `address` where it carries one as text —
   *  which is what an OSC message reads as, and what send() writes for
   *  one — and otherwise the first of `type`, `message_type` and `kind`
   *  it carries as text. `"*"` names every message, one with no name of
   *  its own included. Several handlers may share a name, and each runs
   *  once per message in the order they were registered. Handlers run
   *  on dispatch, on the dispatching thread, in the order the messages
   *  arrived; one registered after a message arrived does not see it,
   *  latest() being how a late reader catches up. */
  void on(std::string_view what, Handler handler);

  /** Sends @p message back through the same door, written the way that
   *  door is read: an `osc://` door takes the packet an `address` and
   *  its `arguments` are written as, every other door the JSON text.
   *  False when the door is one-way, closed or never opened, and when
   *  the value has no spelling on that wire. */
  bool send(const Json& message) const;

  /** THE OSC SPELLING: @p arguments under @p address. On a door that is
   *  not `osc://` this is the same message as JSON,
   *  `{"address": …, "arguments": …}`, which is the form a connection
   *  at the other end reads a name out of. */
  bool send(std::string_view address, const Json& arguments) const;

  /** The URI this was opened on; empty for a connection onto nothing. */
  const std::string& uri() const;

  /** The local end as the transport bound it, or empty. */
  std::string address() const;

  /** What went wrong at the door; empty when nothing did. */
  std::string error() const;

  /** Arrivals the feed dropped before this connection drained them: a
   *  sender faster than the frame. */
  uint64_t dropped() const;

  /** Arrivals that were no message in this connection's scheme. They
   *  reach no reader, so a sender speaking the wrong language is seen
   *  here rather than in the drawing. */
  uint64_t undecodable() const;

  /** Whether nothing more is coming. A connection onto nothing is
   *  closed: there is no door for a message to arrive at. */
  bool closed() const;

  /** THE FLOOR BELOW, for whoever wants the bytes: the feed itself,
   *  which is what a recording is written from and what a reader that
   *  wants no value reads. Null for a connection onto nothing. */
  std::shared_ptr<io::Feed> feed() const;

 private:
  /** Everything a connection is, behind one pointer. What the hub
   *  dispatches reaches this rather than the connection, so moving a
   *  connection carries the handlers, the queue and the lease with it
   *  and what is registered goes on reading the same state. */
  struct State;
  std::shared_ptr<State> m_state;
};

}  // namespace sigil::data
