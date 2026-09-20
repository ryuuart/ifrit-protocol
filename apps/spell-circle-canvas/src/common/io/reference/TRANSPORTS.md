# SigilIO — feeds and the wires under them

The chapter on a resource that keeps ARRIVING: what a `Feed` is and
how a scheme opens one, the two ways an answer goes back out, then each
transport in turn — UDP with OSC and Art-Net over it, the WebSocket
listener and the client that calls one, shared memory, MIDI, serial,
gRPC, QUIC and WebRTC — and the recording a feed is written down as.
`README.md` beside the library is the front page; `SOURCES.md` is the
other half, the resource that is fetched once.

A **feed** is a resource that keeps arriving. `feed()` answers one `Feed`
per URI for as long as anyone holds it, and a later ask for the same URI
while it is held is the same object, so two readers of one port share
one socket. What arrives is a byte message, delivered by a transport from
whichever thread it runs on; the feed latches the newest as `latest()`
with a `generation()` that counts every arrival, and queues each arrival
for `receive()`, which hands them out in order and never waits.
`Feed::newest()` is that same latched message WHOLE — the generation it
came in as, the second it came in at, its bytes and the sender it named —
for a reader that wants more of the newest than its bytes and is not
draining the queue to get it. The queue is bounded by the feed's
`Policy`: when it is full the oldest arrival is dropped and `dropped()`
counts it, because a reader that fell behind a state feed wants the
newest, not the backlog. `close()` takes nothing more and keeps what was
received readable. A feed's `error()` says why a door could not be
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

A scheme opens through the `FeedTransport` registered for it, called
outside the hub's lock; the transport hands back an `OpenedFeed`: how the
feed closes it, how `send()` goes back through it when the way is two-way,
how `OpenedFeed::sendTo` answers one named sender when it can address one,
and the local `address()` it bound. Every arrival also names where it came
from: `sigil::io::Arrival::from` is the sender's address spelled the way a
URI of that scheme is, `udp://127.0.0.1:52341`, and is empty where the
transport has no way of knowing — and on a replayed recording, which holds
the messages and not who sent them.

`Feed::sendTo()` is the OTHER way back out, and the one a door that holds
no peer of its own has: it takes an address spelled the way an arrival's
`from` is and writes to that sender alone. It is false where the
transport cannot address one, where the feed is closed and where no
transport opened it — a replayed recording among them, which has no
sender to answer and no end to answer through. `send()` is unchanged by
it: a peer's feed still writes to its peer and a listening WebSocket
still broadcasts to every peer on its path.

`SigilIOTransport` registers nine transports under eleven schemes, two of
those transports sharing a scheme.
`registerUdp()` takes the UDP ones: `udp://:PORT` listens on every
interface, IPv4 and IPv6 alike, and `udp://HOST:PORT` is a peer that
`send()` reaches and whose replies arrive. A listener has no peer to
`send()` to and answers ONE sender instead, through `Feed::sendTo()`:
the address is read the way a URI of that scheme is written and
resolved as the literal it is, so answering waits on no name lookup and
a `from` that is not literal is nobody to answer. `osc://` is that same
socket under another name, for a port whose messages are OSC packets; a
feed keeps the scheme it was opened with, in its `uri()`, in its `address()`
and in every sender it names, so a reader picks the decoding off the URI
rather than out of the bytes. `artnet://` is that same socket once more,
for a port whose datagrams are a lighting desk's universes of dimmers —
`artnet://:6454` listens for them and `artnet://HOST:6454` is a desk to
send them to. Sockets in one UDP registration share a private IO thread.
Closing a feed releases its socket before returning, so its port can be
rebound immediately even while the closed feed object remains held.

`registerWebSocket()` takes `ws://`. `ws://:PORT/PATH` listens on every
interface for peers reaching that path — an omitted PATH being the root —
every text or binary message from any of them arrives naming that peer,
and one `send()` goes out to all of them at once while `Feed::sendTo()`
reaches the one peer it names and no other. Each listening feed holds a
loop on a thread of its own, and closing the feed gives the port back and
ends the peers still attached to it. A peer is named by the address its
own messages arrive under, and a peer that has left before the loop
reaches an answer is nobody to answer — the answer having been posted is
what `sendTo()` says, since the loop that holds the peers is not the
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
names.

`registerWebSocketClient()` is the other end, and it takes `ws://` and
`wss://` both. The two ends share a scheme, and the SHAPE of the URI is
what says which one a feed is: `ws://HOST:PORT/PATH` and
`wss://HOST:PORT/PATH` name a server to call, `ws://:PORT/PATH` names a
port to hold. The client stands in front of whatever was registered for
those schemes and splits the two in one place — a URI with a host it
calls itself, a URI without one it hands to the listener behind it — so
`registerWebSocket()` is installed first, and a scheme with nothing
behind it refuses a hostless URI with the reason. The port is spelled
rather than taken from the scheme, so what a feed reaches is what its
URI says. A call is made over libcurl, which is where the TLS `wss://`
needs comes from: each feed holds one session on a thread of its own,
every message the server sends arrives whole however many frames it was
split into, `send()` writes one whole message back as bytes, and both
the `address()` the feed reports and the sender every arrival names are
the server it dialled — the one peer a client has, so `Feed::sendTo()`
is false on it. The handshake runs on that same thread: a feed is
answered before its server has been reached, `error()` carries the
reason when it cannot be, and a `send()` before then goes nowhere and
says so. A server that ends the session closes the feed.

`registerSharedMemory()` takes `shm://`, and it is the one transport
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
as its sender, spelled `shm://NAME` exactly as the `address()` the feed
reports is, the rate being the reader's own arrangement and no part of
what the region is called, as a listener's pages are no part of the path
its peers reach. The mapping is read-only and there is no way back
through it, so `send()` and `Feed::sendTo()` are both false on such a
feed: a scene that must answer holds another door for that.

WHAT A LOOK LOOKS AT IS THE NAME. A door with nothing mapped looks for
the name at every look, so THE TWO ENDS MAY START IN EITHER ORDER: a
feed opened on a name nobody has made a region under waits there,
carrying that region as its `address()` and nothing as its `error()`,
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

`registerMidi()` takes `midi://`, and what stands behind it is not a
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
as the `address()` such a feed reports is; an output reports
`midi://out/NAME` the same way. An input is ONE WAY — what comes back
down a cable is the other cable, which is a door of its own — so
`send()` and `Feed::sendTo()` are both false on one, while an output's
`send()` writes the bytes as one message and is false where the driver
refused it. No thread is started for any of this: the driver runs a
callback of its own on every arriving message, which is the thread an
arrival is delivered from, and an output is written on the thread that
asked.

`registerSerial()` takes `serial://`, and what stands behind it is the
oldest wire there is: a board on a cable, printing a line whenever it
has something to say. `serial://DEVICE?baud=RATE` opens the device file
of that path — whole and absolute, as in
`serial:///dev/tty.usbmodem1101?baud=115200` — at that rate. THE RATE IS
REQUIRED and there is none to fall back on, because two ends that
disagree about it read each other as noise, so a URI carrying none opens
nothing and says so. The rest of the wire stands in the same query and
has the defaults a board is wired for: `bits` is 5, 6, 7 or 8 and is 8,
`parity` is none, odd or even and is none, `stop` is 1 or 2 and is 1,
`flow` is none, software or hardware and is none. A port that will not
take one of those settings is a door that does not open, since a port
read at a setting nobody asked for answers bytes that are not the ones
on the wire.

A MESSAGE IS A LINE, AND SO IS A REPLY. What reaches a serial port is a
run of bytes with no message boundary anywhere in it, so the boundary is
the one the sender writes: an arrival is the bytes up to a newline, with
a carriage return before it left off and a blank line delivered to
nobody, and half a line is no arrival at all until the rest of it comes
— a reading with its numbers cut in two being worse than a reading that
has not arrived yet. A FEED THAT OPENS ONTO A WIRE ALREADY IN MID-LINE
takes the tail of that line as its first arrival: nothing in the bytes
says where a line began, so a reader that attaches to a board already
talking pays one fragment for it and reads whole lines from then on. A
run of 64 KiB with no newline among them is
handed over as the line it stands as and the reading begins again, so a
sender that frames nothing still reaches a reader instead of growing one
arrival without end. `send()` writes the bytes and a newline after them,
which is where the reader on the board stops.

A CABLE HOLDS ONE PEER. What is at the other end is the only thing
there, so `send()` reaches it and there is no sender to pick out by
name: `Feed::sendTo()` is false on such a feed. Every arrival names the
port as its sender, spelled `serial://DEVICE` with the settings left off
— what the board IS, and not how this end was told to read it — exactly
as the `address()` such a feed reports is. The ports every registration
opens share ONE thread, made when the first of them opens, so a hub
taught the scheme and never asked for a port starts nothing.

`registerGrpc()` takes `grpc://`, and it is one scheme at both ends: the
SHAPE of the URI is what says which end a feed is, as it is for the two
websocket ones, but here one registration takes both. `grpc://:PORT/
Service/Method` holds that port and serves that one method — PORT 0
meaning any free port — and `grpc://HOST:PORT/Service/Method` calls that
method there. BOTH ENDS NAME THE METHOD, a method being a leading slash,
a service and a method with nothing after, so a URI carrying a service
and nothing behind it is half an address and opens neither end.

THE METHOD CARRIES BYTES AND PARSES NOTHING. It is generic at both ends,
which is gRPC's own word for a method with no generated stub standing
over it, so a feed's buffers, its JSON and whatever else a sender writes
cross it exactly as they cross a datagram — and what a message MEANS is
the business of the library that owns the format, as it is on every
other door here. A scene that wants a wire format across gRPC writes
that format and this transport never learns it.

A SERVER'S PEER IS A CALL AND NOT A CALLER. Every call that arrives is a
bidirectional stream of its own and one caller may hold several at once,
so the address alone would not tell two of them apart: an arrival is
named `grpc://ADDRESS#NUMBER`, the number counting the calls that feed
has taken, and `Feed::sendTo()` writes on the call it names. `send()`
writes on every call standing and is false where none is — a door that
holds its own callers can say that a broadcast reached nobody, which a
door that posts to a loop cannot — and a caller that ends its half of
the stream ends that peer, its name answering nothing from then on. The
`address()` such a feed reports is
`grpc://[::]:PORT/Service/Method`, every interface of both families
being one dual-stack listener, and closing the feed gives the port back
and cancels the calls standing on it rather than waiting for a caller to
stop talking. A call asking for any other method is told nothing here
answers to it, a generic service being handed every method a caller
names.

A CLIENT HOLDS THE ONE CALL IT OPENED. `send()` writes one message on
it — and a message handed over before the server has been reached waits
on the stream rather than going nowhere, which is where this end differs
from the websocket client beside it — every message the server writes is
an arrival naming the URI that was called, which is also the `address()`
such a feed reports, and `Feed::sendTo()` is false on it, a client
having the one peer it called. The server ending the call closes the
feed, while a server that goes away mid-conversation fails it with the
sentence saying the call ENDED — which is not the sentence a call that
never reached a server gets, the two being worth telling apart by
reading the reason rather than by guessing which one happened.
A channel that has not connected within ten seconds fails the feed
with the sentence saying so, while a connection nobody takes is refused
at once and says that instead, so a host that swallows the connection
and a host that refuses it both answer rather than leaving a feed
waiting.

NO TLS AT EITHER END in this cut: a server takes callers without it and
a call is made without it, so both belong on a machine or a network
somebody already trusts. A `grpcs://` scheme carrying credentials is the
step after this one and nothing is registered for it. And NO THREAD IS
STARTED for any of this — gRPC runs threads of its own and calls back
onto them, so a server's callers and a client's stream are carried
without this library holding a loop, an executor or a completion queue.
The one thread this transport ever makes is the one a feed let go from
inside such a callback hands its shutdown to, a server being shut down
by waiting for every call's callbacks to end and a callback not being
able to wait for itself.

`registerQuic()` takes `quic://`, and it is one scheme at both ends, the
SHAPE of the URI saying which end a feed is, exactly as the gRPC one
beside it. `quic://:PORT?cert=FILE&key=FILE` holds that port — PORT 0
meaning any free port — and `quic://HOST:PORT` calls an end there.
Everything crosses ONE ENCRYPTED CONNECTION over ONE UDP PORT: the
handshake, the messages and the acknowledgements alike, which is what
lets a connection survive a laptop moving between networks and what
keeps a door down to one port to open on a stage.

A PORT NAMES THE PAIR IT ANSWERS WITH, because a QUIC connection is
encrypted or it is not a connection: there is no unencrypted form of this
door to fall back to, so `cert` and `key` are required and a URI carrying
neither, or one of the two, opens nothing and leaves the reason on the
feed. Each is a path or a URI the hub resolves to one, resolved through
the mount table as the feed opens — so a scene's own certificate stands
beside it at `res://sky/cert.pem` and is named that way — and what the
listener keeps afterwards is what the library read out of the two files,
a feed being able to outlive the hub that opened it. A CALL EITHER TRUSTS
WHAT IT IS SHOWN OR SAYS PLAINLY THAT IT WILL NOT LOOK:
`quic://HOST:PORT?insecure=1` checks no certificate, which is what
reaches a SELF-SIGNED pair — the ordinary case for a machine on a stage,
where nobody signs for anybody. What that gives up is the certainty that
the machine answering is the one the URI named; what stays is that
everything crossing the connection is encrypted all the same. Both ends
name the protocol `sigil-feed/1` in the handshake, so software speaking
something else over that port is refused there rather than left to send
bytes nobody reads.

A MESSAGE IS ONE UNIDIRECTIONAL STREAM. A send opens a stream, writes the
bytes and ends it, and the arrival is delivered when the end of that
stream arrives — so a message keeps its boundary, which a byte stream has
none of, and no message waits behind another, a stream that lost a packet
holding up itself alone. The price is that two messages sent one after
the other may land in the other order, each stream being carried on its
own. A stream that grows past 16 MiB is abandoned rather than held, a
message being one whole thing. `?datagrams=1` on either form sends every
message as a QUIC DATAGRAM instead: unreliable, unordered, and no larger
than one packet on the path carries, so a send of more than that is
false, as is one made before the path has said how large that is.
Datagrams are always TAKEN whatever a door sends, so a peer that writes
one reaches a door whose own URI asked for nothing — which is what lets
one end send state it can afford to lose while the other answers in
streams.

A PORT'S PEER IS A CONNECTION. Every connection that reaches a listening
feed is a peer named `quic://ADDRESS#NUMBER`, the number counting the
connections that feed has taken; `Feed::sendTo()` writes on the
connection it names, `send()` writes on every connection standing and is
false where none is, and a connection that ends ends that peer, its name
answering nothing from then on. The `address()` such a feed reports is
`quic://[::]:PORT`, every interface of both families being one dual-stack
socket, and the query is no part of it: the pair a port answers with is
the door's own arrangement and nothing anybody reaches.

A CALL HOLDS THE ONE CONNECTION IT OPENED. `send()` writes on it, every
message the other end writes is an arrival naming `quic://HOST:PORT` —
which is also the `address()` such a feed reports, the query again being
the door's own arrangement — and `Feed::sendTo()` is false on it, a call
having the one end it reached. The other end ending the connection closes
the feed; a connection that fails after it was reached fails the feed
with the sentence saying it ENDED, which is not the sentence a call that
never reached anything gets, the two being worth telling apart by reading
the reason rather than by guessing which one happened. A call that has
not been answered within ten seconds fails the feed that way too, while a
machine that says at once that nothing is listening on that port ends the
call there and then and says THAT — so a host that swallows the
connection and one that turns it away both answer, rather than either
leaving a feed waiting.

NO THREAD IS STARTED for any of this: the library underneath runs workers
of its own and calls back onto them, so the packets, the encryption, the
streams and the timers are carried without this library holding a loop,
an executor or a socket. The one thread this transport ever makes is the
one a feed let go from inside such a callback hands its shutdown to, a
listener being given back by waiting for it to stop calling back and a
callback not being able to wait for itself. The library itself and the
registration its workers run under are made on the first `quic://` feed
and stand for the life of the process, giving either back being the same
wait.

`registerWebRtc()` takes `webrtc://`, and what it opens is the only door
here with NOTHING IN THE MIDDLE OF IT. A phone on a mobile network and a
scene behind a router have no address for each other, so neither can be
dialled; what they can do is say what addresses they might be reachable
at, hear the other's, and try every pair until one answers — and from
then on every message goes straight between them, over no server at all.
`webrtc://ROOM?signal=URI` is that conversation: ROOM is what it is
called, and the signal is the door the saying crosses.

THE SIGNAL IS A FEED OF THIS SAME HUB, and the SHAPE of its URI is what
says which end this one is, exactly as it is for the two websocket ones.
`?signal=ws://:PORT/PATH` is a port to hold, so the feed WAITS to be
taken up and answers whoever offers — and that door may stand its own
pages, `?signal=ws://:PORT/PATH?pages=URI`, so the page a phone loads,
the socket it opens back and the introduction it makes are one address.
The PORT may be written `0`, which is the kernel's to fill, and the
address a feed reports is how the one it gave is read back.
`?signal=ws://HOST:PORT/PATH` is a server to call, so the feed TAKES A
ROOM UP and offers into it. What crosses that door is JSON text —
`{"kind":"offer","room":…,"sdp":…}`, the same with `"answer"`, and
`{"kind":"candidate","room":…,"candidate":…,"mid":…}` — every one of
them naming its room, so ONE SOCKET CARRIES AS MANY CONVERSATIONS AS
THERE ARE ROOMS ON IT and each door reads only its own. The doors of one
signalling URI share one reading of that feed, draining being taking;
what they do not share it with is anybody else, so a reader that drains
that same door takes introductions away from them.
`?ice=stun:HOST:PORT` names a server asked what address this machine has
to the world, may be written more than once, and is what two ends on
different networks need and two on one network do not.

THE FRAME CARRIES THE INTRODUCTION. `dispatch()` is what reads the
signalling door and answers it, so a handshake takes a few frames rather
than a few microseconds and a host that never dispatches never finishes
one — while what arrives on a channel is not frame-paced at all, being
delivered the moment it lands. An introduction written before its door
can take it waits for the next frame rather than being lost, which is
how a caller's offer stands until its socket has finished its own
handshake.

ONE PEER PER CONNECTION, on a channel named `feed`. An arrival is named
`webrtc://ROOM#NUMBER`, the number counting the peers that feed has
taken and never handed out twice; `Feed::sendTo()` writes on the one it
names, and `send()` writes on every channel standing open, which is what
makes an audience of phones see one sky rather than each its own. A
message larger than what a peer's channel takes is not written to that
peer, a channel carrying whole messages and cutting none in half.

A feed that WAITS reports `webrtc://ROOM?signal=URI` as its
`address()`, the signal spelled as that door BOUND it —
`webrtc://sky?signal=ws://[::]:52341/signal`, the room and the path
standing where the URI had them and the port between them being the one
taken. So `?signal=ws://:0/PATH` is a first-class way to wait: what a
phone has to be pointed at is read off the end holding the door rather
than agreed on beforehand. A feed that TOOK A ROOM UP reports
`webrtc://ROOM`, holding no door anybody dials. The ice servers stand in
neither, being what an end asks its own address of, as a listener's
pages are no part of the path its peers reach.

A CHANNEL THAT CLOSES ENDS ITS PEER, and a peer whose connection found
no route at all ends the same way — on the frame, a connection torn down
inside its own callback being one that waits for itself. AN END THAT
TOOK A ROOM UP TAKES IT UP AGAIN where the conversation it had is over
and it holds no other: it offers once when it opens, so a route that
never came good would otherwise leave that feed standing with nobody at
the other end of it, and the end waiting in the room answers the new
offer as it answers a first one, retiring what it held for that caller.
A SIGNAL THAT ENDS ENDS NO CONVERSATION, though: two ends that have found each other
speak through nothing else, so a door whose signalling socket closed
keeps every peer it has and only takes no new one. Closing the feed ends
every connection and lets the signalling door go with it, and the last
conversation crossing one is what gives that port back. NO THREAD IS
STARTED for any of this: the library underneath runs its own and calls
back onto them.

`Feed::receivedAt()` maps an arrival onto the steady clock (negative, nonfinite
or unrepresentable times use that clock's origin): live packets
keep their transport receive time and replayed packets keep their recorded
spacing relative to the first `advance()` call. Use it when elapsed-time
accounting must remain independent of when the host drains the queue.

A **recording** is a feed written down: `record(path)` appends every
arrival from then on, with the seconds since the feed was made, in the
format `RecordingWriter` writes and `readRecording()` reads. A URI that
resolves through the mount table to a regular file is not opened through
a transport at all: the feed replays that file, and `dispatch(seconds)`
advances every replay to that time on the caller's clock, delivering each
recorded arrival at its recorded second and closing the feed after the
last. That is how a deterministic run reads what a live one heard:
`mount()` the URI onto the recording, and the code that opened the port
opens the file.

Once every replay has been advanced, that same `dispatch()` runs each
callback registered through `onDispatch()`, in registration order, on
the dispatching thread and with the same seconds — so something that
reads feeds on the frame is driven by the call a host already makes,
sees what this very dispatch delivered, and is unregistered by letting
its `DispatchLease` go.
