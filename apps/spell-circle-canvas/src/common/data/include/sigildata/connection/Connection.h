#pragma once

/** @file
 * @ingroup data-connection
 * A CONNECTION: a feed read as values.
 *
 * A feed is bytes that keep arriving. A connection is that same door
 * one floor up: the bytes are read by the scheme the URI names — an OSC
 * packet through an `osc://` door, JSON text through every other — and
 * what a reader sees is the `Json` value, never the bytes. The newest
 * message is `latest()` and the newest of one name `latest(name)`, the
 * ones not taken yet come out of `receive()` in order once a first call
 * has opened that queue, a handler registered with `on()` runs for the
 * messages it names and one registered with `otherwise()` for the
 * messages no name did.
 *
 *     Connection sky(hub, "osc://:9000");
 *     sky.on("/sky/gust", [&](const Json& message) {
 *       gust = message["arguments"][0].number();
 *       sky.reply("/sky/ack", Json::Array{gust});   // back to that sender
 *     });
 *     sky.on("*", [&](const Json&) { ++messages; });
 *     sky.otherwise([&](const Json&) { ++strangers; });
 *     ...
 *     hub.dispatch(seconds);                   // the frame: handlers run here
 *     wind = sky.latest()["arguments"][0].number();
 *     calm = sky.latest("/sky/calm")["arguments"][0].number();
 *     sky.send("/sky/ack", Json::Array{1});
 *
 * NOTHING DRIVES IT BUT THE FRAME. A connection registers on the hub's
 * dispatch as it opens, so the call a host already makes once a frame
 * is what drains the feed, reads what arrived and runs the handlers.
 * Between two dispatches a connection answers exactly what the last one
 * left it, so every reading a frame takes agrees with every other.
 *
 * A SCHEMA IS THE OTHER WAY A MESSAGE IS READ. A connection opened with
 * one — `Connection(hub, uri, schema<Sky>())` — reads and writes every
 * message through it, so what a reader sees is the schema's own JSON
 * form whichever form the sender wrote and a message that does not FIT
 * the schema is no message at all.
 *
 * AND A TYPED DOOR OVER BOTH. `latest<Sky>()` hands out the newest
 * message as the value type the schema's generated VALUE header
 * declares, read through that header's own reading:
 *
 *     namespace sky = feed_sky::values;    // what that schema generated
 *     Connection door(hub, "udp://:27022", schema<feed_sky::Sky>());
 *     ...
 *     hub.dispatch(seconds);               // the frame: the door fills
 *     if (const std::optional<sky::Sky> state = door.latest<sky::Sky>())
 *       for (const sky::Band& band : state->bands) draw(band);
 *
 * so a scene draws from a field of a value rather than from a lookup by
 * name, and a message that is not that value is nothing rather than a
 * reading of whatever it was. It reads the bytes the same dispatch left,
 * so the typed reading and the Json one are readings of ONE frame.
 *
 * ONE THREAD. The value, the queue and the handlers are written and
 * read on the dispatching thread — the frame's — so a connection holds
 * no lock of its own; the feed underneath is the thread-safe part, and
 * a transport delivers into it from whatever thread it runs on. A
 * connection DRAINS the feed it is on, and draining is taking, so two
 * connections on one URI split the messages between them rather than
 * each seeing all of them.
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
   *  own `.fbs` generates, `schema<Sky>()`.
   *
   *  BOTH FORMS ARRIVE THROUGH IT. An arrival whose bytes are the
   *  schema's JSON form — read from the door's name where it ends
   *  `.json`, and otherwise from the arrival's first byte that is not a
   *  space, since the JSON form opens with a brace or a bracket — is
   *  parsed through the schema and rendered back out of it; an arrival
   *  that is a buffer is verified against the schema's root and
   *  rendered out of it the same way. So latest(), receive() and every
   *  handler see the schema's own JSON form whichever form the sender
   *  wrote, and a message that does not FIT the schema — a field it
   *  does not declare, a value of the wrong type, a buffer of another
   *  schema — is undecodable() rather than a value carrying whichever
   *  fields it happened to have.
   *
   *  GOING BACK OUT, send() and reply() write the buffer the schema
   *  makes of the message and are false where it does not fit — which
   *  the address-and-arguments spelling does not, unless the schema
   *  declares those two fields.
   *
   *  AN `osc://`, A `midi://` OR AN `artnet://` DOOR TAKES NO SCHEMA
   *  and is refused as
   *  it is opened: no feed is bound, nothing arrives, and error() says
   *  so. Each of those is a wire with its own spelling of every value,
   *  down to the width a number goes out at, and a buffer is not one of
   *  those spellings. */
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

  /** THE NEWEST MESSAGE AS A VALUE OF ITS OWN: the newest bytes that
   *  arrived, read through the reading the schema's generated value
   *  header wrote for Value — `door.latest<sky::Sky>()`, where that
   *  header put `Sky` in the schema's own `values` namespace. So a
   *  scene draws from a field of a value, `state->bands`, rather than
   *  from a lookup by name, and a message that is not that value is
   *  nothing rather than a reading of whatever it was.
   *
   *  Nothing before the first arrival, where the bytes are not that
   *  value, and on a connection onto nothing.
   *
   *  BOTH FORMS STILL READ. Where this door was opened with a schema
   *  and the newest bytes are that schema's JSON form — read from the
   *  door's name where it ends `.json`, and otherwise from the first
   *  byte that is not a space — they go through the schema first, so a
   *  sender speaking JSON hands out the same value as one speaking the
   *  buffer.
   *
   *  THE FRAME IS WHAT READS. What is decoded is the newest bytes the
   *  last dispatch took off the feed — latestBytes() below — held there
   *  whether or not they were a message in this door's scheme, since a
   *  buffer arriving at a door read as JSON text is no Json message and
   *  is still the value its sender wrote. So a delivery the dispatch
   *  has not taken yet is nothing here exactly as it is nothing to
   *  latest(), and every reading a frame takes agrees with every other.
   *
   *  IT IS A READING AND NOT A CACHE. Those bytes are decoded every
   *  time this is asked and no value is held between two asks, so a
   *  scene that asks once a frame pays that reading once a frame —
   *  which is the value it draws from anyway.
   *
   *  THERE IS NO TYPED READING OF ONE NAME. A name is read off the
   *  value a message decoded to and the latch under it holds that Json;
   *  a buffer carries no name of that kind, so a door whose messages
   *  are values is read whole. */
  template <values::Readable Value>
  std::optional<Value> latest() const;

  /** THE NEWEST MESSAGE NAMED @p what; a null value until one of that
   *  name has arrived, which reads through as the default of whatever
   *  is asked of it. The name is the one on() registers under, so a
   *  reader takes one fader off the wire with no handler at all:
   *  `sky.latest("/sky/wind")["arguments"][0].number()`.
   *
   *  One latch per name, and the names are bounded by the policy's
   *  capacity: when a message arrives under one name too many, the name
   *  written longest ago is dropped and reading it answers null again,
   *  as if nothing had ever arrived under it. A message carrying no
   *  name of its own latches under none, and `"*"` is a handler's word
   *  for every message rather than a name a message can carry. */
  const Json& latest(std::string_view what) const;

  /** How many messages have arrived on the feed, whether or not they
   *  could be read; 0 before the first. */
  uint64_t generation() const;

  /** The next message this reader has not taken, in order; nothing when
   *  none is waiting, and never a wait. What the handlers see is not
   *  taken from here — one message reaches both.
   *
   *  THE FIRST CALL OPENS THE QUEUE: messages read before it are not
   *  held, so a reader that registers handlers and never calls this
   *  keeps no queue and loses nothing to one. From then on the queue
   *  holds what the policy's capacity says and its oldest falls off the
   *  front when it is full, which is what a reader that has fallen
   *  behind the newest wants; what falls off there is counted nowhere.
   */
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

  /** Runs @p handler for every message NO on() name matched, from now
   *  on. `"*"` is a handler's word for every message and not a name, so
   *  one standing does not make a message matched: a message no on()
   *  name reached arrives here whatever else ran for it. Several may be
   *  registered and each runs once per such message, in the order they
   *  were registered, after the handlers a name would have reached on
   *  that same message. */
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
   *  message. False when there is nobody to answer — nothing has
   *  arrived, a recording holds the messages and not who sent them, or
   *  the connection is onto nothing — and when the door cannot address
   *  one sender or the value has no spelling on that wire. */
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
   *  a wire this library has no reading for reads itself. They are
   *  latched whether or not they were a message in this door's scheme,
   *  so a buffer at a door read as JSON text is here even though it
   *  reached no handler. Null before the first arrival, and on a
   *  connection onto nothing. */
  std::shared_ptr<const io::Bytes> latestBytes() const;

  /** The local end as the transport bound it, or empty. */
  std::string address() const;

  /** What went wrong at the door; empty when nothing did. A door that
   *  was REFUSED as it opened — an `osc://` one handed a schema — says
   *  so here and stays shut, nothing being bound for it. */
  std::string error() const;

  /** Arrivals the feed dropped before this connection drained them: a
   *  sender faster than the frame. It is the FEED's count and nothing
   *  else: what receive()'s own queue loses to its own capacity, once a
   *  first receive() has opened it, is counted neither here nor
   *  anywhere. */
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
 *  rather than in the connection's one translation unit because a
 *  template is instantiated where the value type is known, which is the
 *  consumer's own: everything it reaches — the bytes the last dispatch
 *  left, the schema the door was opened with, and the reading the
 *  generated header wrote — is named in a header already. */
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
