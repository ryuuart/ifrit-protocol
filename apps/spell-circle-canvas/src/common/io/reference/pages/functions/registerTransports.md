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
"shm" and "midi" are one transport each. Registering a transport again
replaces it. A scheme no transport here answers is
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

## See also

`sigil::io::SharedMemoryWriter`, the other end of a shm:// region.
