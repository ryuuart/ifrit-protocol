---
kind: type
library: SigilIO
name: Feed
qualified: sigil::io::Feed
group: The hub
status: stable
---

# Feed

## Description

A FEED: a resource that keeps arriving.

One door, keyed by URI, with a transport on one side and readers on the
other; this is the readers' side. The transport delivers byte messages,
through the feed's `sigil::io::Inlet`, from whatever thread it runs on; a reader on any thread either takes the newest message whole through
`Feed::latest`, with the revision that says how many have come, or
drains in order the ones it has not seen yet through `Feed::receive`. Neither ever waits for
a message: a reader that finds nothing is told so and gets on with its
frame.

A feed keeps the last `FeedPolicy::capacity` arrivals for
`Feed::receive`. When one more reaches a feed nobody has drained, the
oldest falls off the front and `Feed::dropped` counts it, so a reader
that cannot keep up loses the oldest messages rather than the newest and
can see that it happened. What `Feed::latest` answers is never dropped.

`Feed::latest` is THE NEWEST MESSAGE WHOLE — a `sigil::io::Message`: its
revision, when it arrived, its payload and the address it came from, read
out together so they are one message's. It is latched rather than
queued, so draining through `Feed::receive` leaves it standing.

### Sending back

`Feed::send` sends back through the opened end, and is false when the way
is one-way, when the feed is closed, and when no transport opened it.

`Feed::sendTo` sends to ONE sender: the address is spelled the way an
message's `Message::sender` is, `udp://127.0.0.1:52341`, which is how a
door that holds no peer of its own answers the one that wrote to it. It
is false when the way is one-way for that purpose, when the feed is
closed, and when no transport opened it.

`Feed::peers` names the peers attached NOW, spelled the way their
messages' `Message::sender` is, through the transport's `OpenedFeed::peers`.
A door that holds many — a websocket listener — learns that one has left
when its name is gone from here. It is empty on a closed feed, on one no
transport opened, and on a transport that holds senders rather than
attached peers, as a datagram socket does.

### The other side

What puts a message on a feed is not a reader's: the transport holds the
feed's `sigil::io::Inlet`, delivers through it, says through it what went
wrong — which `Feed::error` then answers — and hands back the end the
feed closes and sends through. A feed whose transport opened nothing
carries that reason and is opened again by the next ask for its URI. A
test puts messages on a feed it holds through
`sigil::io::testing::inletOf`.

### The same door plays a recording back

`Feed::record` appends every arrival from then on to a file and hands
back a `sigil::io::Recording`: the recording lasts exactly as long as that
handle does, or until `Recording::stop`, and `Recording::stopped` says
whether it still runs. A feed writes one recording at a time, so a second
`Feed::record` ends the first; a feed that closes ends its recording with
it. A feed `Hub::replay` put a recording in front of delivers what that
file holds as `Hub::dispatch` moves its time forward — so a scene that ran
against a live sender runs again against the file it wrote, arrival for
arrival.

`Hub::dispatch` moves a replayed recording's time to the seconds on the
caller's clock. The first dispatch after the feed is made fixes the
origin, so a recording starts when its feed is first advanced; every
recorded arrival due by then is delivered in order with its recorded
time, and the feed closes after the last one. A live feed is unaffected.

`Message::receivedAt` places a message from either half on one steady
clock, so a reader that timestamps what it draws does not have to know
which half it is reading. A live arrival keeps the time its transport
received it; a replayed one is measured from the first `Hub::dispatch`
that moved it, which preserves the recorded spacing however many arrivals a
single read drains at once. A negative, nonfinite or unrepresentable
time maps to that clock's origin rather than to an arbitrary instant.

## See also

`sigil::io::Message`, `sigil::io::Inlet`, `sigil::io::Transport`,
`sigil::io::Recording`, `sigil::io::RecordingWriter`.
