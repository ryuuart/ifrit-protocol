---
kind: function
library: SigilIO
name: registerTransports
qualified: sigil::io::registerTransports
group: Transports
status: stable
---

# registerTransports

## Description

Installs on the hub the transports that answer the schemes named — every
transport this feature carries when none is named — so a feed the hub is
then asked for on one of those schemes binds, calls, maps or opens what
its URI names. One call is the whole registration: a host that opens
anything a sketch asks for names nothing, and a program that speaks one
wire names that wire's scheme, `registerTransports(hub, {"udp"})`.

A transport answers several schemes and is installed under all of them
when any one is named: "udp", "osc" and "artnet" are one socket; "ws"
and "wss" are the listener with the client standing in front of it;
"shm", "midi", "serial", "grpc" and "quic" are one transport each; and
"webrtc" brings the websocket ends its introductions cross, since a
webrtc feed opens its signalling door through the same hub. Registering
a transport again replaces it. A scheme no transport here answers is
passed over, and a feed asked for on it is one whose `FeedState::error` says
no transport is registered for it.

The doors below are the transports, one section each: the URI grammar a
door answers to, what a message is on it, which way it sends, and what
thread it runs on.

### udp://, osc:// and artnet://

The UDP transport answers to the schemes "udp", "osc" and
"artnet". A URI of the form udp://:PORT listens on every interface, IPv4
and IPv6 alike, PORT 0 meaning any free port; udp://HOST:PORT (HOST a
name, an IPv4 address, or a bracketed IPv6 address) opens a socket that
sends to that peer and receives whatever comes back to it. Every socket
the transport opens runs on one thread of its own that lives as long as
the hub holds the transport.

A listening socket holds no one peer, so nothing goes out of it by
itself; what it can do is answer ONE sender, by the address that
sender's datagram arrived from — which is `Feed::send` with `SendOptions::to`.

#### osc:// and artnet:// are the same socket

osc:// is that same socket under another name: an osc://:9000 feed is a
UDP socket whose messages are OSC packets. artnet:// is that same socket
again, for the datagrams a lighting desk sends: an artnet://:6454 feed
listens for them and an artnet://HOST:6454 feed is a desk to send them
to. A feed keeps the scheme it was opened with — in its `Feed::uri`, in
the local `FeedState::localAddress` it reports and in the sender every arrival
names — so a reader picks the decoding off the URI rather than out of
the bytes.

### ws://, listening

The WebSocket transport answers to the scheme "ws". A URI of
the form ws://:PORT/PATH listens on every interface for peers reaching
that path, PORT 0 meaning any free port and an omitted PATH meaning "/".
Every text or binary message from any peer arrives in the feed naming the
peer it came from, and `Feed::send` goes out to every peer on that path
at once, while one named peer is answered alone through `Feed::send` with `SendOptions::to`.
Each listening feed runs on a thread of its own that stands until the
feed is closed.

#### A URI's query may name where its pages stand

ws://:PORT/PATH?pages=URI — and the same port then answers HTTP GET out
of that directory, so what a peer loads and the socket it opens back are
one address. "/" and "/index.html" are the directory's index.html; any
other path is the file of that name beneath the directory, typed by its
extension; a path naming no file there, one climbing out through "..",
and every request to a listener whose URI named no pages at all, are
answered 404. The URI is resolved through the hub's mount table as the
feed opens — a URI that resolves to no directory opens nothing and leaves
the reason on the feed — and what the listener keeps afterwards is the
directory itself, read again per request, so a page edited on disk is the
page the next reload is served. The path peers reach is the PATH alone: a
query is the listener's own arrangement and stands in neither the address
the feed reports nor the one an arrival names. A pages value that is an
absolute path is that directory itself, with no mount in between.

#### Where it stands and whom it admits

`?bind=ADDRESS` holds that one interface instead of every one, and the
address the feed reports is that interface's, so a listener bound to
127.0.0.1 cannot be reached from another machine. `?admit=ADDRESS`,
written once per peer, with `admit=loopback` for every loopback address,
lets in those peers alone: any other is answered 403 with a sentence
naming it, before the upgrade and before a page, and never becomes a
peer. A listener that names nobody admits everybody; an admission or a
bind that is no IP address opens nothing and leaves the reason on the
feed. `Feed::peers` names the peers attached now, so a door learns that
one has left when its name is gone.

#### What it sends

A send goes out as a binary frame, since a feed carries bytes;
`?frames=text` sends text frames instead, for peers that read every
message as a string, and what is sent must then be UTF-8. Any value but
`text` or `binary` opens nothing and leaves the reason on the feed.

#### This registration listens

The sockets underneath carry no client and are built without TLS, so a
URI naming a host to call opens nothing here and leaves the reason on the
feed, and there is no "wss" it could hold a port for.
The client is what takes those URIs, and it stands in front of this.

### ws:// and wss://, calling

The WebSocket CLIENT answers to the schemes "ws" and "wss",
in front of whatever was registered for them: one scheme, and the shape
of the URI is what says which end of it a feed is. A URI of the form
ws://HOST:PORT/PATH (HOST a name, an IPv4 address, or a bracketed IPv6
address) calls that server, and wss://HOST:PORT/PATH calls it over TLS; a
URI naming no host is a port to hold and goes to the transport this one
was installed over, so the listener is installed first and a scheme
with no listener behind it refuses such a URI with the reason.

#### One session, one thread

Every message the server sends arrives whole, however many frames it was
split into, naming the server as its sender; `Feed::send` writes one
whole message back to it, a client having the one peer it dialled, which
is also the `FeedState::localAddress` it reports. Each feed runs one session on a
thread of its own, and the handshake runs there too: a feed is answered
before its server is reached, `FeedState::error` carries the reason when it
cannot be, and a send before then goes nowhere and says so. A server that
ends the session closes the feed.

### shm://

The SHARED MEMORY reader answers to the scheme "shm". A URI
of the form shm://NAME maps the shared memory object of that name and
delivers what the one writer of that region puts there, so a message
crosses from another process on this machine through memory both of them
have mapped, with no socket beneath it and no kernel on the way.

#### The reader never writes and never waits

The region is mapped read-only, and it carries a number that counts the
messages written into it: odd while one is being written and even once it
is whole. So a read that brackets its copy between two equal even numbers
took a message nobody was writing over, and one that does not is dropped
and read again at the next look.

#### Nothing pushes, so the feed looks

shm://NAME?rate=HERTZ reads the region a whole number of times a second,
120 times where the URI names no rate, and every message the writer
leaves standing between two looks is one arrival. Every arrival names the
region as its sender, spelled shm://NAME as the `FeedState::localAddress` the feed
reports is — the rate being the reader's own arrangement and no part of
what the region is called. A reader has no way back to a writer through
the region, so `Feed::send` is false on such a feed with or without `SendOptions::to`: a
scene that must answer holds another door for that. The looks of every
feed opened through one registration run on one thread, made when the
first of them opens.

#### What a look looks at is the name

So THE TWO ENDS MAY START IN EITHER ORDER. A feed opened on a name nobody
has made a region under is a door onto nothing rather than one that
failed — it reports that region as its `FeedState::localAddress`, nothing as its
`FeedState::error`, and delivers the moment a writer makes one. A door that
has a mapping maps whatever stands under the name afresh about once a
second — a region made again under a name is another object wearing that
word, which is what a writer started again leaves behind it — and lets
its mapping go where the name is gone. A shared memory object carries no
identity a reader could ask for, so what a door compares is the message:
one that is not the message it last delivered is one to deliver, whatever
count it stands under, which is what the first message of a region made
again is. A region that stands but is not one to read, one whose first
bytes are not this layout's among them, is left where it is with the
reason on the feed and the door still standing. Only a URI naming no
region at all opens nothing.

### midi://

The MIDI transport answers to the scheme "midi". A URI of
the form midi://in/NAME opens the first input port whose own name holds
NAME, letter for letter with case left out of it, and midi://out/NAME the
first output port that does; NAME left out altogether takes the first
port there is, which is the one controller on a desk that has one. A PORT
IS NAMED AND NEVER NUMBERED here, because which port a machine calls its
second depends on what else was plugged in this morning. A name nobody
answers to opens nothing and leaves on the feed both the name that was
looked for and the ports that do exist.

#### A port made rather than found

midi://in/virtual:NAME and midi://out/virtual:NAME MAKE a port of that
name instead of looking for one, so other software on this machine can
reach the scene as it reaches a controller. A system that does not offer
ports made rather than found opens nothing and says so.

#### An input delivers every message the wire carries

One arrival per message, status byte first and the bytes exactly as they
arrived — system exclusive included, since that is what a controller
answers a question with. The two a scene cannot use are left out: a clock
beats twenty-four times a quarter note and a sensing byte arrives several
times a second whether or not anybody played anything, so a feed taking
both would be a feed of heartbeat with the performance somewhere inside
it. Every arrival names the port as its sender, spelled midi://in/NAME
with the port's whole name, exactly as the `FeedState::localAddress` such a feed
reports is; an output reports midi://out/NAME the same way.

An input is ONE WAY — what comes back down a cable is the other cable,
which is a door of its own — so `Feed::send` with and without `SendOptions::to` are both
false on one, while an output's send writes the bytes as one message and
is false where the driver refused it.

#### No thread is started

The driver runs a callback of its own on every arriving message, which is
the thread a message is delivered from, and an output is written on the
thread that asked.

### serial://

The SERIAL transport answers to the scheme "serial". A URI
of the form serial://DEVICE?baud=RATE opens the device file of that path
— whole and absolute, as in serial:///dev/tty.usbmodem1101?baud=115200 —
at that rate. THE RATE IS REQUIRED and there is none to fall back on: two
ends that disagree about it read each other as noise, so a URI carrying
none opens nothing and says so. The rest of the wire's settings stand in
the same query and have the defaults a board is wired for —
bits=5|6|7|8 (8), parity=none|odd|even (none), stop=1|2 (1),
flow=none|software|hardware (none) — and a port that will not take one of
them is a door that does not open, since a port read at a setting nobody
asked for answers bytes that are not the ones on the wire.

#### A message is a line, both ways

What reaches a serial port is a run of bytes with no message boundary in
it, so the boundary is the one the sender writes: every arrival is the
bytes up to a newline, with a carriage return before it left off and a
blank line delivered to nobody, and half a line is no arrival at all
until the rest of it comes. A FEED THAT OPENS ONTO A WIRE ALREADY IN
MID-LINE takes the tail of that line as its first arrival, there being
nothing in the bytes that says where the line began. A run of 64 KiB with
no newline among them is handed over as the line it stands as and the
reading begins again, so a sender that frames nothing still reaches a
reader. `Feed::send` writes the bytes and a newline after them, which is
where the reader on the board stops.

#### A cable holds one peer

`Feed::send` reaches what is at the other end and there is no sender to
pick out by name, so `Feed::send` with `SendOptions::to` is false on such a feed. Every
arrival names the port as its sender, spelled serial://DEVICE with the
settings left off — what the board IS, and not how this end was told to
read it — exactly as the `FeedState::localAddress` the feed reports is. The ports
every registration opens share ONE thread, made when the first of them
opens, so a hub taught the scheme and never asked for a port starts
nothing.

### grpc://

The gRPC transport answers to the scheme "grpc". ONE SCHEME,
TWO SHAPES, and the shape of the URI is what says which end a feed is. A
URI of the form grpc://:PORT/Service/Method holds that port and serves
that one method, PORT 0 meaning any free port;
grpc://HOST:PORT/Service/Method (HOST a name, an IPv4 address, or a
bracketed IPv6 address) calls that method there. BOTH ENDS NAME THE
METHOD, so a URI carrying a service and nothing after it opens nothing
and leaves the reason on the feed.

#### The method is schema-agnostic

It carries bytes in both directions and nothing here parses one, so a
feed's buffers, its JSON and whatever else a sender writes cross it with
no generated stub in the transport. What a message MEANS is the business
of the library that owns the format, exactly as on every other door.

#### A server's peer is a call and not a caller

Every call that arrives is a stream of its own, one caller may hold
several at once, and each message written on one is an arrival naming
that call — `grpc://ADDRESS#NUMBER`, the number counting the calls that
feed has taken. `Feed::send` with `SendOptions::to` writes on the call it names, `Feed::send`
writes on every call standing and is false where none is, and a caller
that ends its half of the stream ends that peer. The `FeedState::localAddress` such
a feed reports is grpc://[::]:PORT/Service/Method, every interface of
both families being one dual-stack listener.

#### A client holds the one call it opened

`Feed::send` writes one message on it, every message the server writes is
an arrival naming the URI that was called — which is also the
`FeedState::localAddress` such a feed reports — and `Feed::send` with `SendOptions::to` is false on it,
a client having the one peer it called. The server ending the call closes
the feed, and a server that goes away mid-conversation fails it with the
sentence saying the call ENDED — which a call that never reached a server
at all does not say, the two being worth telling apart by reading the
reason. A channel that has not connected within ten seconds fails the
feed that way too, and a refused connection says it at once instead of
waiting the bound out.

#### No TLS at either end

In this cut, so both a server and a call stand on a machine or a network
somebody already trusts; a "grpcs" scheme carrying credentials is the
step after this one and is not registered.

#### No thread is started for a feed

gRPC runs threads of its own and calls back onto them, so a server's
callers and a client's stream are carried without this library holding a
loop, an executor or a completion queue of its own.

### quic://

The QUIC transport answers to the scheme "quic". ONE SCHEME,
TWO SHAPES, and the shape of the URI is what says which end a feed is. A
URI of the form quic://:PORT?cert=FILE&key=FILE holds that port, PORT 0
meaning any free port; quic://HOST:PORT (HOST a name, an IPv4 address, or
a bracketed IPv6 address) calls that end.

#### A QUIC connection is encrypted or it is not a connection

So there is no unencrypted form of this door. A port therefore NAMES THE
PAIR IT ANSWERS WITH: `cert` and `key` are the certificate and the
private key, each a path or a URI the hub resolves to one — resolved as
the feed opens, so a URI that names no file opens nothing and leaves the
reason on the feed. A call either trusts what it is shown or says plainly
that it will not look: quic://HOST:PORT?insecure=1 checks no certificate,
which is what reaches a SELF-SIGNED certificate — the ordinary case for a
machine on a stage, where nobody signs for anybody. What that gives up is
the certainty that the machine answering is the one the URI named; what
stays is that everything crossing the connection is encrypted all the
same. Both ends name the protocol "sigil-feed/1" in the handshake, so an
end speaking something else over that port is refused there rather than
left to send bytes nobody reads.

#### A message is one unidirectional stream

A send opens a stream, writes the bytes and ends it, and the arrival is
delivered when the end of that stream arrives — so a message keeps its
boundary, and no message waits behind another, a stream that lost a
packet holding up itself alone. The price is that two messages sent one
after the other may land in the other order. A stream that grows past 16
MiB is abandoned rather than held, a message being one whole thing.
`&datagrams=1` on either form sends every message as a QUIC DATAGRAM
instead — unreliable, unordered, and no larger than one packet on the
path carries, so a send of more than that is false, as is one made before
the path has said how large that is. Datagrams are always TAKEN, whatever
a door sends, so a peer that writes one reaches a door whose own URI
asked for nothing.

#### A port's peer is a connection

Every connection that reaches a listening feed is a peer named
`quic://ADDRESS#NUMBER`, the number counting the connections that feed
has taken; `Feed::send` with `SendOptions::to` writes on the connection it names, `Feed::send`
writes on every connection standing and is false where none is, and a
connection that ends ends that peer. The `FeedState::localAddress` such a feed
reports is quic://[::]:PORT, every interface of both families being one
dual-stack socket.

#### A call holds the one connection it opened

`Feed::send` writes on it, every message the other end writes is an
arrival naming quic://HOST:PORT — which is also the `FeedState::localAddress` such
a feed reports, the query being the door's own arrangement and no part of
what anybody reaches — and `Feed::send` with `SendOptions::to` is false on it. The other end
ending the connection closes the feed; a connection that fails after it
was reached fails the feed with the sentence saying it ENDED, which a
call that never reached anything does not say, the two being worth
telling apart by reading the reason. A call that has not been answered
within ten seconds fails the feed that way too, while a machine that says
at once that nothing is listening on that port ends the call there and
then — so a host that swallows the connection and one that turns it away
both answer, rather than either leaving a feed waiting.

#### No thread is started for a feed

The library underneath runs workers of its own and calls back onto them,
so the packets, the encryption, the streams and the timers are carried
without this library holding a loop, an executor or a socket.

### webrtc://

The WEBRTC transport answers to the scheme "webrtc". A URI
of the form webrtc://ROOM?signal=URI holds one conversation, named ROOM,
between this end and however many peers take it up, and every message
crosses STRAIGHT between them: two ends that have found each other speak
over no server, which is what lets a phone on a mobile network reach a
scene behind a router.

#### Neither end can dial the other, so they are introduced

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

#### The frame carries the introduction

`Hub::advance` is what reads the signalling door and answers it, so a
handshake takes a few frames and a host that never advances never
finishes one. What arrives on a channel is not frame-paced: it is
delivered the moment it lands.

#### One peer per connection, on a channel named "feed"

Every message arriving on one is an arrival naming that peer —
`webrtc://ROOM#NUMBER`, the number counting the peers this feed has taken
— `Feed::send` writes on every channel standing open, and `Feed::send` with `SendOptions::to`
writes on the one it names. A channel that closes ends its peer, and
closing the feed ends every connection and lets the signalling door go
with it. A feed that WAITS reports webrtc://ROOM?signal=URI as its
`FeedState::localAddress`, the signal spelled with the port that door bound — so
?signal=ws://:0/PATH is a way to wait, and what a caller has to dial is
read off the end holding it rather than agreed on beforehand. A feed that
TOOK A ROOM UP reports webrtc://ROOM, holding no door anybody dials.

#### No thread is started for a feed

The library underneath runs threads of its own and calls back onto them,
so the routes, the encryption and the stream are carried without this
library holding a loop or an executor of its own.

## See also

`sigil::io::SharedMemoryWriter`, the other end of a shm:// region.
