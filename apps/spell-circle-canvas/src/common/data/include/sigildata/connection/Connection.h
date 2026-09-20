#pragma once

/** @file
 * @ingroup data-connection
 * A CONNECTION: a feed read as values. The bytes are read by the scheme
 * the URI names — an OSC packet through an `osc://` door, JSON text
 * through every other, and the schema's own form on a door opened with
 * one — so what a reader sees is a `Json`, or the value type a
 * generated header declares, and never the bytes. NOTHING DRIVES IT BUT
 * THE FRAME: a connection registers on the hub's dispatch as it opens,
 * and between two dispatches it answers exactly what the last one left
 * it. ONE THREAD, the dispatching one, so a connection holds no lock of
 * its own.
 */

#include <sigildata/decode/FlatBuffer.h>
#include <sigildata/decode/Json.h>
#include <sigildata/values/Values.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/source/Source.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

  /** Opens @p uri on @p hub as the constructor above does, and reads
   *  and writes every message THROUGH @p schema — the one a sketch's
   *  own `.fbs` generates, `schema<Sky>()`. Both the buffer and the
   *  schema's JSON form arrive through it, and a message that does not
   *  FIT the schema is undecodable() rather than a value.
   *  @trap AN `osc://`, A `midi://` OR AN `artnet://` DOOR TAKES NO
   *  SCHEMA and is refused as it is opened: no feed is bound, nothing
   *  arrives, and error() says so. */
  Connection(io::Hub& hub, std::string_view uri, Schema schema,
             io::FeedPolicy policy = {});

  /** Takes over the moved-from door, leaving it closed. */
  Connection(Connection&&) noexcept = default;
  /** Takes over the moved-from door, closing this one. */
  Connection& operator=(Connection&&) noexcept = default;
  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  /** THE NEWEST MESSAGE; null until one has arrived and been read. A
   *  message that cannot be read leaves it standing, so a sender
   *  speaking the wrong language cannot blank a scene. Inside a handler
   *  it is the message that handler was given. */
  const Json& latest() const;

  /** THE NEWEST MESSAGE AS A VALUE OF ITS OWN: the bytes the last
   *  dispatch left, read through the reading the schema's generated
   *  value header wrote for Value. Nothing before the first arrival,
   *  where the bytes are not that value, and on a connection onto
   *  nothing. Where the door was opened with a schema and the bytes are
   *  that schema's JSON form, they go through the schema first.
   *  @trap IT IS A READING AND NOT A CACHE: those bytes are decoded
   *  every time this is asked and no value is held between two asks. */
  template <values::Readable Value>
  std::optional<Value> latest() const;

  /** THE NEWEST MESSAGE NAMED @p what; a null value until one of that
   *  name has arrived, which reads through as the default of whatever
   *  is asked of it. The name is the one on() registers under, so a
   *  reader takes one fader off the wire with no handler at all:
   *  `sky.latest("/sky/wind")["arguments"][0].number()`.
   *  @trap One latch per name, bounded by the policy's capacity: a
   *  message under one name too many drops the name written longest
   *  ago, which reads null again as if nothing had arrived under it. */
  const Json& latest(std::string_view what) const;

  /** How many messages have arrived on the feed, whether or not they
   *  could be read; 0 before the first. */
  uint64_t generation() const;

  /** The next message this reader has not taken, in order; nothing when
   *  none is waiting, and never a wait. What the handlers see is not
   *  taken from here — one message reaches both. The queue holds what
   *  the policy's capacity says and its oldest falls off the front.
   *  @trap THE FIRST CALL OPENS THE QUEUE: messages read before it are
   *  not held, and what the queue then loses to its own capacity is
   *  counted nowhere. */
  std::optional<Json> receive();

  /** Runs @p handler for every message named @p what, from now on. A
   *  MESSAGE'S NAME is its `address` where it carries one as text, and
   *  otherwise the first of `type`, `message_type` and `kind` it
   *  carries as text; `"*"` names every message. Handlers run on
   *  dispatch, on the dispatching thread, in the order the messages
   *  arrived and then in the order they were registered.
   *  @trap A handler registered after a message arrived does not see
   *  it, latest() being how a late reader catches up. */
  void on(std::string_view what, Handler handler);

  /** Runs @p handler for every message NO on() name matched, from now
   *  on, in registration order and after the handlers a name reached on
   *  that same message.
   *  @trap `"*"` is a handler's word for every message and not a name,
   *  so one standing does not make a message matched: a message no
   *  on() name reached arrives here whatever else ran for it. */
  void otherwise(Handler handler);

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

  /** Sends @p message back TO THE SENDER OF ONE, written exactly as
   *  send() writes it. Inside a handler that sender is the one that
   *  sent the message being handled, so a door that listens answers the
   *  desk that just spoke; outside one it is the sender of the newest
   *  message. False where there is nobody to answer, where the door
   *  cannot address one sender, and where the value has no spelling on
   *  that wire. */
  bool reply(const Json& message) const;

  /** THE OSC SPELLING of a reply: @p arguments under @p address, back
   *  to that same sender. */
  bool reply(std::string_view address, const Json& arguments) const;

  /** The URI this was opened on; empty for a connection onto nothing. */
  const std::string& uri() const;

  /** THE SCHEMA THIS DOOR READS AND WRITES THROUGH, as it was handed
   *  one; a schema that is none where it was opened without one, and on
   *  a connection onto nothing. */
  const Schema& schema() const;

  /** THE NEWEST ARRIVAL'S BYTES AS OF THE LAST DISPATCH, whole and
   *  unread: what latest<Value>() decodes, and what a reader that wants
   *  a wire this library has no reading for reads itself. Null before
   *  the first arrival, and on a connection onto nothing.
   *  @trap They are latched whether or not they were a message in this
   *  door's scheme, so a buffer at a door read as JSON text is here
   *  even though it reached no handler. */
  std::shared_ptr<const io::Bytes> latestBytes() const;

  /** The local end as the transport bound it, or empty. */
  std::string address() const;

  /** What went wrong at the door; empty when nothing did. A door that
   *  was REFUSED as it opened — an `osc://` one handed a schema — says
   *  so here and stays shut, nothing being bound for it. */
  std::string error() const;

  /** Arrivals the feed dropped before this connection drained them: a
   *  sender faster than the frame.
   *  @trap It is the FEED's count and nothing else: what receive()'s
   *  own queue loses to its own capacity is counted nowhere. */
  uint64_t dropped() const;

  /** Arrivals that were no message in this connection's scheme, and,
   *  where it has a schema, arrivals that did not fit it. They reach no
   *  reader, so a sender speaking the wrong language is seen here
   *  rather than in the drawing. */
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

/** The frame's newest bytes read as one value. It stands in the header
 *  because a template is instantiated where the value type is known,
 *  which is the consumer's own. */
template <values::Readable Value>
std::optional<Value> Connection::latest() const {
  const std::shared_ptr<const io::Bytes> newest = latestBytes();
  if (!newest) return std::nullopt;
  const Schema& through = schema();
  // The schema's JSON form is parsed to a buffer first, so a sender
  // that speaks it hands out the same value as one that sends the
  // buffer; text the schema cannot hold is no value at all.
  if (through && flatBufferLooksLikeJson(newest->asText(), uri())) {
    const std::optional<std::vector<std::byte>> buffer =
        through.binary(newest->asText());
    if (!buffer) return std::nullopt;
    return values::Read<Value>::from(*buffer);
  }
  return values::Read<Value>::from(newest->bytes);
}

}  // namespace sigil::data
