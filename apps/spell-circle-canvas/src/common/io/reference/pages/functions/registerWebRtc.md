---
kind: function
library: SigilIO
name: registerWebRtc
qualified: sigil::io::registerWebRtc
group: Transports
status: stable
---

# registerWebRtc

## Description

Installs the WEBRTC transport on the hub for the scheme "webrtc". A URI
of the form webrtc://ROOM?signal=URI holds one conversation, named ROOM,
between this end and however many peers take it up, and every message
crosses STRAIGHT between them: two ends that have found each other speak
over no server, which is what lets a phone on a mobile network reach a
scene behind a router.

### Neither end can dial the other, so they are introduced

`signal` names the ws:// or wss:// URI that introduction crosses, opened
on the hub as any other feed is, and THE SHAPE OF THAT URI SAYS WHICH END
THIS ONE IS: ws://:PORT/PATH is a port to hold, so this feed WAITS to be
taken up and answers whoever offers; ws://HOST:PORT/PATH is a server to
call, so this feed TAKES A ROOM UP and offers into it. What crosses that
door is JSON text — `{"kind":"offer","room":…,"sdp":…}`, the same with
"answer", and `{"kind":"candidate","room":…,"candidate":…,"mid":…}` —
each naming its room, so one socket carries as many conversations as
there are rooms on it and a door reads only its own. The signalling feed
is the transport's own to drain: a reader that drains it too takes
introductions away from the doors on it. `?ice=stun:HOST:PORT` names a
server asked what address this machine has to the world, may be given
more than once, and is needed for two ends on different networks and for
neither of two on one.

### The frame carries the introduction

`Hub::dispatch` is what reads the signalling door and answers it, so a
handshake takes a few frames and a host that never dispatches never
finishes one. What arrives on a channel is not frame-paced: it is
delivered the moment it lands.

### One peer per connection, on a channel named "feed"

Every message arriving on one is an arrival naming that peer —
`webrtc://ROOM#NUMBER`, the number counting the peers this feed has taken
— `Feed::send` writes on every channel standing open, and `Feed::sendTo`
writes on the one it names. A channel that closes ends its peer, and
closing the feed ends every connection and lets the signalling door go
with it. A feed that WAITS reports webrtc://ROOM?signal=URI as its
`Feed::address`, the signal spelled with the port that door bound — so
?signal=ws://:0/PATH is a way to wait, and what a caller has to dial is
read off the end holding it rather than agreed on beforehand. A feed that
TOOK A ROOM UP reports webrtc://ROOM, holding no door anybody dials.

### No thread is started for a feed

The library underneath runs threads of its own and calls back onto them,
so the routes, the encryption and the stream are carried without this
library holding a loop or an executor of its own.
