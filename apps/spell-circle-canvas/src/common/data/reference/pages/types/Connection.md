---
kind: type
library: SigilData
name: Connection
qualified: sigil::data::Connection
group: The connection
status: stable
---

# Connection

## Description

A CONNECTION: a feed read as values.

A feed is bytes that keep arriving. A connection is that same door one
floor up: the bytes are read by the scheme the URI names — an OSC packet
through an `osc://` door, JSON text through every other — and what a
reader sees is the `sigil::data::Json` value, never the bytes. The
newest message is `Connection::latest` and the newest of one name
`latest(name)`, the ones not taken yet come out of `Connection::receive`
in order once a first call has opened that queue, a handler registered
with `Connection::on` runs for the messages it names and one registered
with `Connection::otherwise` for the messages no name did.

```cpp
Connection sky(hub, "osc://:9000");
sky.on("/sky/gust", [&](const Json& message) {
  gust = message["arguments"][0].number();
  sky.reply("/sky/ack", Json::Array{gust});   // back to that sender
});
sky.on("*", [&](const Json&) { ++messages; });
sky.otherwise([&](const Json&) { ++strangers; });
...
hub.dispatch(seconds);                   // the frame: handlers run here
wind = sky.latest()["arguments"][0].number();
calm = sky.latest("/sky/calm")["arguments"][0].number();
sky.send("/sky/ack", Json::Array{1});
```

### Nothing drives it but the frame

A connection registers on the hub's dispatch as it opens, so the call a
host already makes once a frame is what drains the feed, reads what
arrived and runs the handlers. Between two dispatches a connection
answers exactly what the last one left it, so every reading a frame
takes agrees with every other.

### A schema is the other way a message is read

A connection opened with one — `Connection(hub, uri, schema<Sky>())` —
reads and writes every message through it, so what a reader sees is the
schema's own JSON form whichever form the sender wrote and a message
that does not FIT the schema is no message at all.

BOTH FORMS ARRIVE THROUGH IT. An arrival whose bytes are the schema's
JSON form — read from the door's name where it ends `.json`, and
otherwise from the arrival's first byte that is not a space, since the
JSON form opens with a brace or a bracket — is parsed through the schema
and rendered back out of it; an arrival that is a buffer is verified
against the schema's root and rendered out of it the same way. So
`Connection::latest`, `Connection::receive` and every handler see the
schema's own JSON form whichever form the sender wrote, and a message
that does not fit the schema — a field it does not declare, a value of
the wrong type, a buffer of another schema — is
`Connection::undecodable` rather than a value carrying whichever fields
it happened to have.

GOING BACK OUT, `Connection::send` and `Connection::reply` write the
buffer the schema makes of the message and are false where it does not
fit — which the address-and-arguments spelling does not, unless the
schema declares those two fields.

AN `osc://`, A `midi://` OR AN `artnet://` DOOR TAKES NO SCHEMA and is
refused as it is opened: no feed is bound, nothing arrives, and
`Connection::error` says so. Each of those is a wire with its own
spelling of every value, down to the width a number goes out at, and a
buffer is not one of those spellings.

### And a typed door over both

`latest<Sky>()` hands out the newest message as the value type the
schema's generated VALUE header declares, read through that header's own
reading:

```cpp
namespace sky = feed_sky::values;    // what that schema generated
Connection door(hub, "udp://:27022", schema<feed_sky::Sky>());
...
hub.dispatch(seconds);               // the frame: the door fills
if (const std::optional<sky::Sky> state = door.latest<sky::Sky>())
  for (const sky::Band& band : state->bands) draw(band);
```

so a scene draws from a field of a value rather than from a lookup by
name, and a message that is not that value is nothing rather than a
reading of whatever it was. It reads the bytes the same dispatch left,
so the typed reading and the Json one are readings of ONE frame.

Nothing before the first arrival, where the bytes are not that value,
and on a connection onto nothing.

BOTH FORMS STILL READ. Where the door was opened with a schema and the
newest bytes are that schema's JSON form — read from the door's name
where it ends `.json`, and otherwise from the first byte that is not a
space — they go through the schema first, so a sender speaking JSON
hands out the same value as one speaking the buffer.

THE FRAME IS WHAT READS. What is decoded is the newest bytes the last
dispatch took off the feed — `Connection::latestBytes` — held there
whether or not they were a message in this door's scheme, since a buffer
arriving at a door read as JSON text is no Json message and is still the
value its sender wrote. So a delivery the dispatch has not taken yet is
nothing here exactly as it is nothing to `Connection::latest`, and every
reading a frame takes agrees with every other.

IT IS A READING AND NOT A CACHE. Those bytes are decoded every time this
is asked and no value is held between two asks, so a scene that asks
once a frame pays that reading once a frame — which is the value it
draws from anyway.

THERE IS NO TYPED READING OF ONE NAME. A name is read off the value a
message decoded to and the latch under it holds that Json; a buffer
carries no name of that kind, so a door whose messages are values is
read whole.

The template reading stands in the header rather than in the
connection's one translation unit because a template is instantiated
where the value type is known, which is the consumer's own: everything
it reaches — the bytes the last dispatch left, the schema the door was
opened with, and the reading the generated header wrote — is named in a
header already.

### Reading one name

`latest(what)` is the newest message of that name; a null value until
one of that name has arrived, which reads through as the default of
whatever is asked of it. The name is the one `Connection::on` registers
under, so a reader takes one fader off the wire with no handler at all:
`sky.latest("/sky/wind")["arguments"][0].number()`.

One latch per name, and the names are bounded by the policy's capacity:
when a message arrives under one name too many, the name written longest
ago is dropped and reading it answers null again, as if nothing had ever
arrived under it. A message carrying no name of its own latches under
none, and `"*"` is a handler's word for every message rather than a name
a message can carry.

A MESSAGE'S NAME is its `address` where it carries one as text — which
is what an OSC message reads as, and what `Connection::send` writes for
one — and otherwise the first of `type`, `message_type` and `kind` it
carries as text.

### The queue and the handlers

`Connection::receive` answers the next message this reader has not
taken, in order. What the handlers see is not taken from here — one
message reaches both. THE FIRST CALL OPENS THE QUEUE: messages read
before it are not held, so a reader that registers handlers and never
calls it keeps no queue and loses nothing to one. From then on the queue
holds what the policy's capacity says and its oldest falls off the front
when it is full, which is what a reader that has fallen behind the
newest wants; what falls off there is counted nowhere.

`Connection::on` runs its handler for every message of that name, from
now on. `"*"` names every message, one with no name of its own included.
Several handlers may share a name, and each runs once per message in the
order they were registered. Handlers run on dispatch, on the dispatching
thread, in the order the messages arrived; one registered after a
message arrived does not see it, `Connection::latest` being how a late
reader catches up.

`Connection::otherwise` runs its handler for every message NO
`Connection::on` name matched. `"*"` is a handler's word for every
message and not a name, so one standing does not make a message matched:
a message no name reached arrives here whatever else ran for it. Several
may be registered and each runs once per such message, in the order they
were registered, after the handlers a name would have reached on that
same message.

### Sending and answering

`Connection::send` writes the message back through the same door, the
way that door is read: an `osc://` door takes the packet an `address`
and its `arguments` are written as, every other door the JSON text. On a
door that is not `osc://` the address-and-arguments spelling is the same
message as JSON, `{"address": …, "arguments": …}`, which is the form a
connection at the other end reads a name out of.

`Connection::reply` sends back TO THE SENDER OF ONE. Inside a handler
that sender is the one that sent the message being handled, so a door
that listens answers the desk that just spoke; outside one it is the
sender of the newest message. It is false when there is nobody to answer
— nothing has arrived, a recording holds the messages and not who sent
them, or the connection is onto nothing — and when the door cannot
address one sender or the value has no spelling on that wire.

### What a reader can ask about the door

`Connection::latestBytes` is THE NEWEST ARRIVAL'S BYTES AS OF THE LAST
DISPATCH, whole and unread: what the typed reading decodes, and what a
reader that wants a wire this library has no reading for reads itself.
They are latched whether or not they were a message in this door's
scheme, so a buffer at a door read as JSON text is here even though it
reached no handler.

`Connection::dropped` counts arrivals the feed dropped before this
connection drained them: a sender faster than the frame. It is the
FEED's count and nothing else: what the queue loses to its own capacity,
once a first `Connection::receive` has opened it, is counted neither
there nor anywhere.

`Connection::undecodable` counts arrivals that were no message in this
connection's scheme, and, where it has a schema, arrivals that did not
fit it. They reach no reader, so a sender speaking the wrong language is
seen there rather than in the drawing.

`Connection::feed` is THE FLOOR BELOW, for whoever wants the bytes: the
feed itself, which is what a recording is written from and what a reader
that wants no value reads.

### One thread

The value, the queue and the handlers are written and read on the
dispatching thread — the frame's — so a connection holds no lock of its
own; the feed underneath is the thread-safe part, and a transport
delivers into it from whatever thread it runs on. A connection DRAINS
the feed it is on, and draining is taking, so two connections on one URI
split the messages between them rather than each seeing all of them.

## See also

`sigil::data::Schema`, `sigil::data::Json`, `sigil::data::values::Read`.
