---
kind: type
library: SigilIO
name: Message
qualified: sigil::io::Message
group: The hub
status: stable
---

# Message

## Description

ONE MESSAGE OFF A FEED: the payload, who sent it, and when it came.

`Message::payload` is the bytes as they arrived, never null on a message
a feed took — an empty message carries an empty payload rather than
nothing. `Message::sender` is the address it came from, spelled the way
a URI of that scheme is, `udp://127.0.0.1:52341`; it is empty for a
replayed recording, which holds the messages and not who sent them, and
for a transport that has no way of knowing. It is also the address a
reply goes back to.

### When it came

`Message::arrivedAt` counts from when its feed opened; on a replayed
recording it is the time the recording carries, and it is what a
recording stores. `Message::receivedAt` places the same moment on the
steady clock: a live message keeps the time its transport received it,
and a replayed one is laid from the first advance that moved its
recording, which preserves the recorded spacing however many messages a
single read drains at once. A negative, nonfinite or unrepresentable time
maps to the origin of that clock rather than to an arbitrary instant.

`Message::revision` is 1 for the first message on a feed and counts up
from there — the feed's own count, which a recording does not carry, so
a recording read back is numbered as it is read.

A message made by hand carries what it was made with: the feed stamps
the revision and the receive time as it takes one.

## See also

`sigil::io::Feed`, whose `Feed::latest` and `Feed::receive` answer one,
and `sigil::io::RecordingWriter`, which writes them down.
