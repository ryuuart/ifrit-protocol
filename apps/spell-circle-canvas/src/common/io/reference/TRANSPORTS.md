# SigilIO — feeds and the wires under them

The chapter on a resource that keeps ARRIVING: what a `Feed` is and
how a scheme opens one, the two ways an answer goes back out, then each
transport in turn — UDP with OSC and Art-Net over it, the WebSocket
listener and the client that calls one, shared memory and MIDI — and
the recording a feed is written down as.
`README.md` beside the library is the front page; `SOURCES.md` is the
other half, the resource that is fetched once.

A **feed** is a resource that keeps arriving. `listen()` answers one `Feed`
per URI — a copyable handle — for as long as anyone holds it, and a later ask
for the same URI while it is held is a handle onto the same door, so two readers of one port share
one socket. What arrives is a byte message, delivered by a transport from
whichever thread it runs on; the feed latches the newest WHOLE as
`latest()` — a `sigil::io::Message`: its `Message::revision`, when it
arrived, its payload and the sender it named, read out together — with a
`state().revision` that counts every arrival, and queues each arrival for `receive()`, which
hands them out in order and never waits. The queue is bounded by the feed's
`ListenOptions`: when it is full the oldest arrival is dropped and `state().dropped`
counts it, because a reader that fell behind a state feed wants the
newest, not the backlog. `close()` takes nothing more and keeps what was
received readable. A feed's `state().error` says why a door could not be
opened — no scheme, no transport for it, a port already taken — and the
feed still exists, so a program that opened the wrong URI sees the
sentence rather than a null.

A feed whose door could NOT be opened is opened again by the next ask for
its URI, into that same feed: what stood in the way — a port another
program held, a device plugged in late, a transport registered after the
first ask — may be gone by the time somebody asks again, and every reader
holding the feed is then reading the door that opened, with the reason
taken off it. A feed that has a door is handed back as it stands, and so
is one that has closed, so asking twice for a URI that opened is one
socket and not two.

A scheme opens through the `Transport` registered for it, called
outside the hub's lock and handed the `Inlet` it delivers every message
through; the transport hands back an `TransportEnd`: how the
feed closes it, how `send()` goes back through it when the way is two-way,
how `TransportEnd::sendTo` answers one named sender when it can address one,
and the local `state().localAddress` it bound. Every arrival also names where it came
from: `sigil::io::Message::sender` is the sender's address spelled the way a
URI of that scheme is, `udp://127.0.0.1:52341`, and is empty where the
transport has no way of knowing — and on a replayed recording, which holds
the messages and not who sent them.

`Feed::send` with `SendOptions::to` is the OTHER way back out, and the one a door that holds
no peer of its own has: it takes an address spelled the way a message's
`sender()` is and writes to that sender alone. It is false where the
transport cannot address one, where the feed is closed and where no
transport opened it — a replayed recording among them, which has no
sender to answer and no end to answer through. `send()` is unchanged by
it: a peer's feed still writes to its peer and a listening WebSocket
still broadcasts to every peer on its path.

`SigilIOTransport` carries nine transports under eleven schemes, two of
those transports sharing a scheme, and `registerTransports(hub, schemes)`
installs the ones that answer the schemes it is handed — every one of them
when it is handed none; naming one scheme of a transport installs it under
all of its schemes, and naming it again replaces it.
The UDP transport takes the UDP ones: `udp://:PORT` listens on every
interface, IPv4 and IPv6 alike, and `udp://HOST:PORT` is a peer that
`send()` reaches and whose replies arrive. A listener has no peer to
`send()` to and answers ONE sender instead, through `Feed::send` with `SendOptions::to`:
the address is read the way a URI of that scheme is written and
resolved as the literal it is, so answering waits on no name lookup and
a `sender()` that is not literal is nobody to answer. `osc://` is that same
socket under another name, for a port whose messages are OSC packets; a
feed keeps the scheme it was opened with, in its `uri()`, in its `state().localAddress`
and in every sender it names, so a reader picks the decoding off the URI
rather than out of the bytes. `artnet://` is that same socket once more,
for a port whose datagrams are a lighting desk's universes of dimmers —
`artnet://:6454` listens for them and `artnet://HOST:6454` is a desk to
send them to. Sockets in one UDP registration share a private IO thread.
Closing a feed releases its socket before returning, so its port can be
rebound immediately even while the closed feed object remains held.

The WebSocket listener takes `ws://`. `ws://:PORT/PATH` listens on every
interface for peers reaching that path — an omitted PATH being the root —
every text or binary message from any of them arrives naming that peer,
and one `send()` goes out to all of them at once while `Feed::send` with `SendOptions::to`
reaches the one peer it names and no other. Each listening feed holds a
loop on a thread of its own, and closing the feed gives the port back and
ends the peers still attached to it. A peer is named by the address its
own messages arrive under, and a peer that has left before the loop
reaches an answer is nobody to answer — the answer having been posted is
what `send()` with `.to` says, since the loop that holds the peers is not the
thread that asked. That registration LISTENS: the library underneath
carries no client and its sockets are built without TLS, so it opens a
port to hold and nothing else.

A URI'S QUERY MAY NAME WHERE ITS PAGES STAND —
`ws://:PORT/PATH?pages=URI` — and the same port then answers HTTP GET
out of that directory, so what a peer loads and the socket it opens back
are one address. `/` and `/index.html` are that directory's
`index.html`; any other path is the file of that name beneath it, typed
by its extension — html, css, js, json, png, jpg, svg and txt, and bytes
with no name otherwise. A path naming no file there, a path climbing out
through `..` exactly as a mount refuses one, a page larger than the
listener hands back, and every request to a listener whose URI named no
pages at all, are answered with a status and a plain sentence and never
with a page. The pages URI is resolved through the hub's mount table AS
THE FEED OPENS, and what the listener keeps from then on is the
directory: a feed may outlive the hub that opened it, and a request is
answered out of the filesystem and nothing else. So a pages URI that
resolves to no directory opens nothing and leaves the reason on the
feed, while a page edited on disk is the page the next reload is served,
the directory being read per request and cached nowhere. The query is
the listener's own arrangement and no part of the path peers reach: it
stands in neither the address the feed reports nor the sender an arrival
names. A pages value that is an ABSOLUTE PATH is that directory itself,
with no mount in between, for a program that writes its pages where it
keeps its own state.

The query may also say WHERE the listener stands and WHO may reach it.
`?bind=ADDRESS` holds that one interface instead of every one, and the
address the feed reports is then that interface's —
`ws://:0/sigil?bind=127.0.0.1` answers `ws://127.0.0.1:52341/sigil` — so
a listener bound to loopback cannot be reached from another machine
however it is asked. `?admit=ADDRESS`, written as often as there are
peers to admit, with `admit=loopback` standing for every loopback
address, lets in those peers and nobody else: any other is answered 403
with a sentence naming it, before the upgrade and before a page, so it
never becomes a peer, nothing it sends arrives and no send reaches it. A
listener that names nobody admits everybody. An admission or a bind that
is no IP address opens nothing and leaves the reason on the feed.

A feed carries bytes, so a send goes out as a binary frame. `?frames=text`
sends every message as a text frame instead, for peers that read each one
as a string — a browser page, a command-line client, a protocol whose
messages are JSON text — and what is sent must then be UTF-8.
`frames=binary` is the default spelled out; any other value opens nothing
and leaves the reason on the feed.

`Feed::peers()` names the peers attached NOW, each spelled as its own
arrivals' sender is: a peer is in it from the upgrade on and gone from it
once its socket closes, which is how a door that holds many learns that
one has left.

The WebSocket client is the other end, and it takes `ws://` and
`wss://` both. The two ends share a scheme, and the SHAPE of the URI is
what says which one a feed is: `ws://HOST:PORT/PATH` and
`wss://HOST:PORT/PATH` name a server to call, `ws://:PORT/PATH` names a
port to hold. The client stands in front of whatever was registered for
those schemes and splits the two in one place — a URI with a host it
calls itself, a URI without one it hands to the listener behind it — so
the listener is installed first, and a scheme with nothing
behind it refuses a hostless URI with the reason. The port is spelled
rather than taken from the scheme, so what a feed reaches is what its
URI says. A call is made over libcurl, which is where the TLS `wss://`
needs comes from: each feed holds one session on a thread of its own,
every message the server sends arrives whole however many frames it was
split into, `send()` writes one whole message back as bytes, and both
the `state().localAddress` the feed reports and the sender every arrival names are
the server it dialled — the one peer a client has, so `Feed::send` with `SendOptions::to`
is false on it. The handshake runs on that same thread: a feed is
answered before its server has been reached and reads `Connecting` until
the server has switched protocols, then `Open`, so the first `send()`
after `Open` goes out; one whose server cannot be reached never reads
`Open`, `state().error` carries the reason, and the next ask for its URI
calls again. A server that ends the session closes the feed.

The shared memory reader takes `shm://`, and it is the one transport
here with no network under it. `shm://NAME` maps the shared memory
object of that name and answers what the ONE writer of that region put
there, so a scene crosses from another process on this machine through
memory both of them have mapped, with no socket beneath it and no kernel
on the way. A region opens with a fixed header of sixty-four bytes — the
sixteen bytes `sigil-shared-1` and its padding, then the capacity the
region was made for, a field left spare, a count of the messages written
into it, the size of the one standing now, and the wall-clock nanosecond
it was written at — and the payload begins where that header ends. Every
number stands at a fixed offset in the byte order of the machine both
ends run on, so a writer in another language is a structure definition
and not a library.

THE COUNT IS THE WHOLE PROTOCOL. The writer raises it to an odd number
before it touches the payload and to the next even one once the message
is whole; a reader copies the payload between two reads of that count
and keeps what it copied only when both reads are the same even number.
So a reader never takes half of one message and half of the next, and a
writer is never held up by a reader that is mid-copy: the two ends share
no lock and neither of them makes a system call to speak. What says a
message is NEW is the count and not what the message says, so the same
bytes written twice are two arrivals.

NOTHING PUSHES, SO THE FEED LOOKS. `shm://NAME?rate=HERTZ` reads the
region a whole number of times a second, 120 times where the URI names
no rate, and every message left standing between two looks is one
arrival — while a message written and written over between them is one
the reader missed rather than one it queued, a region holding the
message that stands NOW and no backlog. Every arrival names the region
as its sender, spelled `shm://NAME` exactly as the `state().localAddress` the feed
reports is, the rate being the reader's own arrangement and no part of
what the region is called, as a listener's pages are no part of the path
its peers reach. The mapping is read-only and there is no way back
through it, so `send()` and `Feed::send` with `SendOptions::to` are both false on such a
feed: a scene that must answer holds another door for that.

WHAT A LOOK LOOKS AT IS THE NAME. A door with nothing mapped looks for
the name at every look, so THE TWO ENDS MAY START IN EITHER ORDER: a
feed opened on a name nobody has made a region under waits there,
carrying that region as its `state().localAddress` and nothing as its `state().error`,
and delivers the moment a writer makes one. A door that has a mapping
maps whatever stands under the name afresh about once a second — a
region unlinked and made again under that name, which is what a writer
started again leaves behind it, is another object wearing the same word
— and lets its mapping go where the name is gone. A shared memory object
carries no identity a reader could ask for, so what a door compares is
the MESSAGE and not the object: it holds the count and the written-at
nanosecond of the one it last delivered, and a message in a mapping just
made that is not that one is a message to deliver, whatever count it
stands under — which is what the first message of a region made again
is. A region that stands under the name but is not one to read — one
smaller than its own header, one whose first bytes are not this
layout's, one claiming more payload than it has room for — is left where
it is with the reason on the feed, the door still standing, and is
looked at again when it changes. Only a URI naming no region at all,
which no writer could make one under, opens nothing.

`SharedMemoryWriter` is the other end, for a tool or a test written in
this language — a writer in any other needs the layout and nothing else.
Constructing one takes a region's name and a capacity and makes that
object, replacing whatever stood under the name, sized to hold that many
payload bytes;
`SharedMemoryWriter::write` puts one message in it and is false for a
message larger than that capacity, a message being written whole or not
at all; `SharedMemoryWriter::open` says whether the region stands; and
destruction unmaps the region and takes the name back, after which a
reader of that name has nothing to read until another region is made
under it. ONE WRITER PER REGION, a region carrying one count and not one
per writer. And EITHER END MAY START FIRST, a reader holding the name
rather than the memory: a region made after a feed was opened on its
name is one that feed reads, and so is the one a writer started again
makes under a name it took back.

The MIDI transport takes `midi://`, and what stands behind it is not a
network either: it is the controller on the desk beside the screen, its
pads and knobs coming in and its lights going out. `midi://in/NAME`
opens the first INPUT port whose own name holds NAME, letter for letter
with case left out of it, and `midi://out/NAME` the first output port
that does; NAME left out altogether takes the first port there is,
which is the one controller on a desk that has one. A PORT IS NAMED AND
NEVER NUMBERED, because which port a machine calls its second depends on
what else was plugged in this morning, and a name nobody answers to
opens nothing and leaves on the feed both the name that was looked for
and the ports that do exist — so a name typed from memory is corrected
by reading the sentence. `midi://in/virtual:NAME` and
`midi://out/virtual:NAME` MAKE a port of that name rather than looking
for one, which is how other software on this machine reaches a scene as
it reaches a controller, and a system that does not offer such a port
opens nothing and says so.

An input delivers EVERY MESSAGE THE WIRE CARRIES, one arrival per
message, the bytes exactly as they arrived with the status byte first —
system exclusive included, that being what a controller answers a
question with. The two a scene cannot use are left out: a clock beats
twenty-four times a quarter note and a sensing byte arrives several
times a second whether or not anybody played anything, so a feed taking
both would be a feed of heartbeat with the performance somewhere inside
it. Every arrival names the port as its sender, spelled `midi://in/NAME`
with the port's WHOLE name and not the piece the URI asked for, exactly
as the `state().localAddress` such a feed reports is; an output reports
`midi://out/NAME` the same way. An input is ONE WAY — what comes back
down a cable is the other cable, which is a door of its own — so
`send()` and `Feed::send` with `SendOptions::to` are both false on one, while an output's
`send()` writes the bytes as one message and is false where the driver
refused it. No thread is started for any of this: the driver runs a
callback of its own on every arriving message, which is the thread an
arrival is delivered from, and an output is written on the thread that
asked.

`Message::receivedAt()` places a message on the steady clock (negative, nonfinite
or unrepresentable times use that clock's origin): live packets
keep their transport receive time and replayed packets keep their recorded
spacing relative to the first `advance()` that moved the recording. Use it
when elapsed-time accounting must remain independent of when the host drains
the queue.

A **recording** is a feed written down: `record(path)` appends every
arrival from then on, with the time since the feed was made, in the
format `RecordingWriter` writes and `readRecording()` reads, until the
`Recording` it hands back is stopped or goes out of scope. A feed writes
one recording at a time; a second `record()` ends the first.
`Hub::replay(uri, recording)` puts a recording — a file, or a URI the
mount table resolves to one — in front of a URI: the feed standing there
is closed, and that feed and every later one asked for on the URI is not
opened through a transport at all but replays the file, and
`advance(time)` advances every replay to that time on the caller's
clock, delivering each recorded arrival at its recorded second and
closing the feed after the last. That is how a deterministic run reads
what a live one heard: `replay()` the URI from the recording, and the
code that opened the port opens the file.

Once every replay has been advanced, that same `advance()` runs each
callback registered through `onAdvance()`, in registration order, on
the advancing thread and with the same time — so something that
reads feeds on the frame is driven by the call a host already makes,
sees what this very advance delivered, and is unregistered by letting
its `Lease` go.
