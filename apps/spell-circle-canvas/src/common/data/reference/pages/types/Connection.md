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

A CONNECTION: a feed read as values — IO's `hub.listen`, one level up.

IO hands bytes; a connection hands values. `sigil::data::connect` opens
the door and answers a value handle whose verbs are the feed's own —
`Connection::latest`, `Connection::receive`, `Connection::send`,
`Connection::record`, `Connection::state`, `Connection::close` — and
what they answer is a `sigil::data::Message` whose `payload` is the
`sigil::data::Json` the bytes read as. The dialect is read off the
URI's scheme — an OSC packet through an `osc://` door, a MIDI message
through `midi://`, an Art-Net packet through `artnet://`, JSON text
through every other — or named in `ConnectOptions::dialect`. A handler
registered with `Connection::on` runs for the messages its OSC address
pattern names, and one registered with `Connection::otherwise` for the
messages no pattern did.

```cpp
auto sky = sigil::data::connect(hub, "osc://:9000");
sky.on("/sky/{gust,wind}", [&](const Message& message) {
  gust = message[0].number();                            // the first argument
  sky.reply(oscMessage("/sky/ack", gust));               // back to that sender
});
sky.on("*", [&](const Message&) { ++messages; });
sky.otherwise([&](const Message&) { ++strangers; });
...
sigil::io::advance(hub, time);                           // the frame: handlers run here
wind = sky.latest("/sky/wind").number();
if (sky.state().undecodable) warn("something speaks another language");
sky.send(oscMessage("/sky/ack", 1), {.to = sky.latest().sender()});
```

`sigil::data::replay` is the same door played from a recording — IO's
`hub.replay` read as values — so a capture reads a desk it recorded
without knowing it: `ctx.deterministic ? replay(hub, uri, file) :
connect(hub, uri)`.

A `Connection` is a copyable handle: every copy reads the same door, and
the door closes when `Connection::close` is called or the last handle
goes. A handle made empty is a connection onto nothing — no door, the
empty message, a closed state.

### Nothing drives it but the frame

A connection registers on the hub's advance as it opens, so the call a
host already makes once a frame is what drains the feed, reads what
arrived and runs the handlers. Between two advances a connection answers
exactly what the last one left it, so every reading a frame takes agrees
with every other.

### A message reads as its payload

`Message::payload` is the decoded value whole. `message["colors"]` is a
member of it, and on an OSC message — one carrying an `address` and an
`arguments` array — `message[0]` is the first ARGUMENT, and
`Message::number`, `Message::string` and `Message::boolean` read that
first argument as the message's value; on any other message they read
the payload itself. `Message::address` is the OSC path, `Message::name`
what a pattern matched, `Message::sender` who sent it, spelled as the
transport names a peer, `Message::arrivedAt` and `Message::receivedAt`
IO's two times, `Message::revision` its place in the feed's count and
`Message::bytes` the arrival unread. The empty message — before anything
arrived, or under a name nothing arrived under — has a null payload,
which reads through as the default of whatever is asked of it.

### A schema is the other way a message is read

A connection opened with one — `connect(hub, uri, {.schema =
schema<Sky>()})` — reads and writes every message through it, so what a
reader sees is the schema's own JSON form whichever form the sender
wrote, and a message that does not FIT the schema is no message at all.

BOTH FORMS ARRIVE THROUGH IT. An arrival whose first byte that is not a
space opens a brace or a bracket is the schema's JSON form, parsed
through the schema and rendered back out of it; an arrival that is a
buffer is verified against the schema's root and rendered out the same
way. A message that does not fit — a field it does not declare, a value
of the wrong type, a buffer of another schema — counts in
`ConnectionState::undecodable` rather than being a value carrying
whichever fields it happened to have.

GOING BACK OUT, `Connection::send` writes the buffer the schema makes of
the message and is false where it does not fit.

AN `osc://`, A `midi://` OR AN `artnet://` DOOR TAKES NO SCHEMA and is
refused as it is opened: no feed is bound, nothing arrives, and
`state().error` says so. Each of those is a wire with its own spelling
of every value, and a buffer is not one of those spellings.

### A typed reading over both

`Message::as` hands out a message as the value type the schema's
generated VALUE header declares, read through that header's own reading:

```cpp
namespace sky = feed_sky::values;    // what that schema generated
auto door = connect(hub, "udp://:27022", {.schema = schema<feed_sky::Sky>()});
...
sigil::io::advance(hub, time);               // the frame: the door fills
if (const std::optional<sky::Sky> state = door.latest().as<sky::Sky>())
  for (const sky::Band& band : state->bands) draw(band);
```

so a scene draws from a field of a value rather than from a lookup by
name. It reads the message's own bytes, the schema's JSON form converted
through the door's schema first, so the typed reading and the `Json` one
are readings of ONE message. IT IS A READING AND NOT A CACHE: the bytes
are decoded every time it is asked. The template stands in the header
because it is instantiated where the value type is known, which is the
consumer's own.

### Reading one name

`latest(address)` is the newest message of that name, the empty message
until one has arrived: a reader takes one fader off the wire with no
handler at all, `sky.latest("/sky/wind").number()`. A pattern reads the
newest of every name it matches. One latch per name, bounded by the
options' capacity: when a message arrives under one name too many, the
name written longest ago is dropped and reads empty again.

A MESSAGE'S NAME is its `address` where it carries one as a string, and
otherwise the first of `type`, `message_type` and `kind` it carries as
one. A message carrying no name latches under none.

### Address patterns

`Connection::on` takes an OSC 1.0 address pattern, matched by
`sigil::data::matchesAddress`: `?` is one character, `*` any run of
them, `[a-c]` one of a set with `!` first negating it, `{wind,gust}` one
of a list — none of them crossing a `/`, so `/sky/*` names `/sky/wind`
and not `/sky/wind/gust`. A name with none of those characters matches
itself alone, which is how a JSON message's `kind` is named. `"*"` alone
names every message, one with no name included; it is a handler's word
and not a pattern a name must match, so a message only `"*"` reached
still reaches `Connection::otherwise`.

### The queue and the handlers

`Connection::receive` answers the next message this reader has not
taken, in order; the handlers see every message whether or not it is
taken there. THE FIRST CALL OPENS THE QUEUE unless the door was opened
with `ConnectOptions::queue`: messages read before it are not held, so a
reader that registers handlers and never calls it keeps no backlog. The
queue holds what the options' capacity says and loses its oldest first.

Handlers run on advance, on the advancing thread, in the order the
messages arrived and then in the order they were registered; one
registered after a message arrived does not see it, `Connection::latest`
being how a late reader catches up. `Connection::otherwise` handlers run
after the named ones, for a message no pattern other than `"*"` matched.

### Sending and answering

`Connection::send` writes a value back through the same door in its
dialect: an `osc://` door the packet `oscMessage(address, arguments)`
reads as, a `midi://` door the message its `kind` names, and JSON text on
every other. With `{.to = message.sender()}` it goes to ONE peer, which is
how a door that holds many answers one of them later. It is false when
the door is one-way, closed or never opened, and when the value has no
spelling in the dialect.

`Connection::reply` sends TO THE SENDER OF ONE: inside a handler, the
sender of the message being handled; outside one, of the newest. It is
false where there is nobody to answer — nothing has arrived, or a
recording, which holds the messages and not who sent them.

### Where the door stands

`Connection::state` is IO's `FeedState` — readiness, revision, dropped,
the local end, the error — and `ConnectionState::undecodable`, the
arrivals that were no message in the door's dialect or did not fit its
schema. It is one comparable value: a readout keeps the one it last
showed and describes again exactly when `state() != shown`, and
"anything new?" is `state().revision != seen`. `dropped` is the FEED's
count; what the queue loses to its own capacity is counted nowhere.

`Connection::record` writes every message from now on to a file in IO's
recording format until the handle it answers stops or goes, and
`Connection::feed` is THE FLOOR BELOW: the feed itself, for a reader
that wants the bytes.

### One thread

The messages, the queue and the handlers are written and read on the
advancing thread, so a connection holds no lock of its own; the feed
underneath is the thread-safe part. A connection DRAINS the feed it is
on, and draining is taking, so two connections on one URI split the
messages between them rather than each seeing all of them.

## See also

`sigil::data::Message`, `sigil::data::Schema`, `sigil::data::Json`,
`sigil::data::values::Read`.
