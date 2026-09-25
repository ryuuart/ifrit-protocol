---
kind: type
library: SigilIO
name: Inlet
qualified: sigil::io::Inlet
group: Transports
status: stable
---

# Inlet

## Description

THE PRODUCER'S SIDE OF ONE FEED: what a transport delivers through.

A feed has two sides. A reader holds the `sigil::io::Feed` and takes
messages off it; a transport holds an `Inlet` and puts them on. The hub
hands the transport registered for a scheme the URI to open and the
inlet into the feed that URI names, and the transport keeps the inlet for
as long as its door stands:

```cpp
#include <sigilio/advanced/Transport.h>

sigil::io::registerTransport(hub, "pigeon", [](std::string_view uri, sigil::io::Inlet inlet) {
  auto door = openPigeonDoor(uri);                 // whatever the scheme is made of
  if (!door) {
    inlet.fail("no pigeon answers " + std::string(uri));
    return sigil::io::TransportEnd{};               // nothing opened: asked again later
  }
  door->onMessage([inlet](sigil::io::Bytes message, std::string from) {
    if (!inlet.expired()) inlet.deliver(std::move(message), std::move(from));
  });
  sigil::io::TransportEnd opened;
  opened.close = [door] { door->close(); };
  opened.send = [door](const sigil::io::Bytes& message) { return door->send(message); };
  opened.localAddress = door->boundAddress();
  return opened;
});
```

It holds the feed weakly. A feed nobody reads any more is gone, and from
then on every verb here does nothing and `Inlet::expired` is true — which
is how a transport running a loop of its own learns that it can stop.
Every copy of an inlet is the same way in, and every verb may be called
from any thread.

### Delivering

`Inlet::deliver` puts one message on the feed. Stamped now — the seconds
since the feed was made — it names the sender the way a URI of the
transport's scheme spells an address, `udp://127.0.0.1:52341`, or nobody
when the sender is left empty. Stamped with a time given, it names no
sender: that is a recording's frame, and a recording holds the messages
and not who sent them.

### Failing without closing

`Inlet::fail` says what went wrong, which `FeedState::error` answers from then
on; an empty reason is nothing wrong, and takes off what stood there. The
feed stays open: a transport that lost one message still has a door.

A TRANSPORT THAT OPENED NOTHING SAYS SO HERE, before it hands back the
end it has none of, and the feed is then one that was never opened rather
than one with a door — which is what lets the next ask for its URI open
it again. AN END WITH NOTHING IN IT, HANDED BACK TO A FEED CARRYING A
REASON, IS NO END: the feed stays unopened with that reason standing, so
the next ask for its URI opens it again into this same feed, and every
reader holding it is reading the door that opened. An end that stands is
kept with whatever the transport has said about it by then: the reason an
earlier ask left is taken off before the open that follows it, not after.

### Opening late, and closing from the far side

What the transport hands back is the end the feed closes and sends
through, `sigil::io::TransportEnd`. A conversation that stands only once a
handshake is done hands its end over later through `Inlet::open`
instead. Either way a feed takes one end: a second one, and one handed
to a feed that is already closed, is closed rather than kept.

`Inlet::close` shuts the door from the transport's side — the far end
ended the conversation — after which nothing more arrives; what was
received stays readable.

### In a test

A test that stands in for a transport does not open a socket to put a
message on a feed. `sigil::io::testing::inletOf` hands it the inlet into
a feed it holds, a hub's or its own:

```cpp
#include <sigilio/testing/Testing.h>

auto feed = std::make_shared<sigil::io::Feed>("fixture://scene");
sigil::io::testing::inletOf(feed).deliver(bytesOf("first"), "udp://127.0.0.1:52341");
```

## See also

`sigil::io::Feed`, `sigil::io::TransportEnd`, `sigil::io::Transport`,
`sigil::io::registerTransports`.
