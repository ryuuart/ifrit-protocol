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
other. The transport delivers byte messages from whatever thread it runs
on; a reader on any thread either takes the newest message —
`Feed::latest` for its bytes and `Feed::newest` for the whole arrival,
with the generation that says how many have come — or drains in order the
ones it has not seen yet through `Feed::receive`. Neither ever waits for
a message: a reader that finds nothing is told so and gets on with its
frame.

A feed keeps the last `Feed::Policy::capacity` arrivals for
`Feed::receive`. When one more reaches a feed nobody has drained, the
oldest falls off the front and `Feed::dropped` counts it, so a reader
that cannot keep up loses the oldest messages rather than the newest and
can see that it happened. What `Feed::latest` and `Feed::newest` answer
is never dropped.

`Feed::newest` is THE NEWEST ARRIVAL WHOLE: the generation it came in as,
the second it came in at, its bytes and the address it came from. It is
latched rather than queued, so draining through `Feed::receive` leaves it
standing, as `Feed::latest` is left standing.

### Sending back

`Feed::send` sends back through the opened end, and is false when the way
is one-way, when the feed is closed, and when no transport opened it.

`Feed::sendTo` sends to ONE sender: the address is spelled the way an
arrival's `Arrival::from` is, `udp://127.0.0.1:52341`, which is how a
door that holds no peer of its own answers the one that wrote to it. It
is false when the way is one-way for that purpose, when the feed is
closed, and when no transport opened it.

### Failing without closing

`Feed::fail` says what went wrong, which `Feed::error` answers from then
on; an empty reason is nothing wrong, and takes off what stood there. The
feed stays open: a transport that lost one message still has a door.

A TRANSPORT THAT OPENED NOTHING SAYS SO HERE, before it hands back the
end it has none of, and the feed is then one that was never opened rather
than one with a door — which is what lets the next ask for its URI open
it again.

### Being handed the opened end

`Feed::opened` takes the end its transport opened, once: a second end,
and one handed to a feed that is already closed, is closed rather than
kept.

AN END WITH NOTHING IN IT, HANDED TO A FEED CARRYING A REASON, IS NO END
— a transport that could not open the URI says why through `Feed::fail`
and has nothing to give back. The feed stays unopened with that reason
standing, so the next ask for its URI opens it again into this same feed,
and every reader holding it is reading the door that opened. An end that
stands is kept with whatever the transport has said about it by then: the
reason an earlier ask left is taken off before the open that follows it,
not after.

### The same door plays a recording back

`Feed::record` appends every arrival from then on to a file, and a feed
the hub resolved to a recording file delivers what that file holds as
`Feed::advance` moves its time forward — so a scene that ran against a
live sender runs again against the file it wrote, arrival for arrival.

`Feed::advance` moves a replayed recording's time to the seconds on the
caller's clock. The first call fixes the origin, so a recording starts
when its feed is first advanced; every recorded arrival due by then is
delivered in order with its recorded time, and the feed closes after the
last one. A live feed ignores it.

`Feed::receivedAt` maps an arrival from either half onto one steady
clock, so a reader that timestamps what it draws does not have to know
which half it is reading. A live arrival keeps the time its transport
received it; a replayed one is measured from the first `Feed::advance`
call, which preserves the recorded spacing however many arrivals a
single read drains at once. A negative, nonfinite or unrepresentable
time maps to that clock's origin rather than to an arbitrary instant.

## See also

`sigil::io::Arrival`, `sigil::io::OpenedFeed`, `sigil::io::FeedTransport`,
`sigil::io::RecordingWriter`.
