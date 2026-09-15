# SigilSeer — every wire, and what is going down it

A **wire** is one URI a message arrives on or leaves by. Seer is the tool
that watches all of them at once: it opens a wire, says what is coming
down it and what it speaks and who is at the other end, shows the newest
message seven ways, keeps the ones before it, sends a message back,
writes a wire down to a file, and opens that file again as if it were the
port.

It stands on SigilIO's feeds and on nothing else that draws. What
travels a wire is bytes, and what those bytes mean belongs to whoever is
at the other end — so a tool for looking at the wire itself can be
pointed at a wire whose format it has never heard of, which is the only
way it is useful while that format is still being decided.

```sh
cmake --build build --config Release --target Seer
open build/bin/Release/Seer.app                          # the window
build/bin/Release/Seer.app/Contents/MacOS/Seer udp://:27020
build/bin/Release/Seer.app/Contents/MacOS/Seer osc://:27050 ws://:27060/sky
build/bin/Release/Seer.app/Contents/MacOS/Seer midi://in/Launchpad artnet://:6454
build/bin/Release/Seer.app/Contents/MacOS/Seer --schema feed_sky.bfbs udp://:27020
build/bin/Release/Seer.app/Contents/MacOS/Seer --shot panes.png udp://:27020
```

Every scheme SigilIO carries is a wire here: `udp://:PORT` and
`udp://HOST:PORT`, `osc://` and `artnet://` — the same datagram socket
under the names that say its messages are packets and universes of
dimmers — `ws://:PORT/PATH`, which listens for peers on that path,
`shm://NAME`, `midi://in/NAME` and `midi://out/NAME`, and
`serial://DEVICE?baud=RATE`. NOTHING IS OPENED BY NAMING A SCHEME: a
wire is opened by typing its URI, so a scheme added underneath opens and
carries messages with nothing added here. What a scheme is named for is
the WORD it speaks in and the reading a wire of it opens on, and a
scheme with no word simply has none.

## Two targets

| target | what it is |
| --- | --- |
| `SigilSeerWire` | the archive: the wires, the log, the way out, the recorder, the schema they are read through, and the readings. Namespace `sigil::seer`, headers under `include/sigilseer/wire/`, no toolkit at all |
| `Seer` | the application: one window over that archive, a macOS bundle, its QML module `Sigil.Seer` |

Everything a wire DOES is in the archive, and it is asserted with no
application in the process. What is left in the application is which
pane shows what.

## The wires

```cpp
#include <sigilseer/wire/Log.h>
#include <sigilseer/wire/Rendering.h>
#include <sigilseer/wire/Sender.h>
#include <sigilseer/wire/Wires.h>

sigil::seer::Wires wires;                       // a hub, with the transports on it
auto scene = wires.open("udp://:27020");        // std::shared_ptr<sigil::io::Feed>

sigil::seer::Log log;
sigil::seer::Sender sender(wires);
sender.openPeer("udp://127.0.0.1:9001");        // the way back out

// …once a frame, on the caller's own clock:
wires.dispatch(seconds);                        // recordings move forward
wires.tick(seconds);                            // every wire is read
sender.tick(seconds);                           // a repeating message goes out
log.drain(*scene);                              // and the messages are taken

for (const sigil::seer::Vitals& wire : wires.vitals())
  show(wire.uri, wire.arrivalsPerSecond, wire.error);
```

`sigil::seer::Wires` owns one `sigil::io::Hub` with every transport
registered on it and keeps the feeds it opened, in opening order.
`sigil::seer::Wires::open()` answers a feed whether or not the URI could
be opened: a URI nothing can open is a wire carrying the sentence that
says why, because the reader has to see the URI they mistyped beside
what is wrong with it. `sigil::seer::Wires::close()` takes a wire off
the list; a wire that closed itself — a recording played out, a socket
the system ended — stays on it and says it is closed.

`sigil::seer::Wires::tick()` writes down one `sigil::seer::Vitals` per
wire: the rate over the last second of ticks, the newest message, how
many have arrived, how many were dropped, the local end, the error,
whether it is closed, and `sigil::seer::Vitals::lastFrom`, the address
that message came from. The rate is worked out from the generations
earlier ticks read, so it is a property of how often the host calls
this and needs no thread behind it. Nothing here has a clock:
`sigil::seer::Wires::dispatch()` and the tick both take the caller's
seconds.

`sigil::seer::Log` keeps what `sigil::seer::Log::drain()` takes off one
feed, newest last, up to the capacity it was made with;
`sigil::seer::Log::forgotten()` counts what fell off the front, which is
a different bound from the feed's own dropped count and is reported
separately. Every entry carries `sigil::seer::LogEntry::from`, the
sender the arrival named, since one wire carries messages from many
senders and which of them sent a message is a fact about the message.

The bytes and the sender come out of one ask. The sender travels with
the arrival — it is `sigil::io::Arrival::from` — and `sigil::io::Feed`
latches its newest arrival whole beside the queue it hands out, so
`sigil::io::Feed::newest()` answers both at once to a tick that drains
nothing. Every row therefore names where its own messages are coming
from, not only the row being read, and no row can show one message's
bytes under another message's sender.

`sigil::seer::Wires::readThrough()` puts one `sigil::data::Schema` over
every wire, and `sigil::seer::Wires::schema()` is the one that is on.
It is the wires' and not one wire's because a reader who went and found
a schema found it for the MESSAGES, which cross whichever wire the
sender happened to open. A tool holds no generated header for what it
is watching, so the token is made from a schema file's own bytes with
`sigil::data::Schema::fromBinarySchema()` — the `.bfbs` the sender's
build wrote beside its header — which is the whole of what Seer needs to
know about somebody else's format.

`sigil::seer::Sender` opens a peer through the same wires — so the peer
stands in the same list and its replies arrive on it — and sends one
message or the same message every period, with
`sigil::seer::Sender::tick()` as the only thing that sends.

A WIRE THAT NAMES A FORMAT IS ANSWERED IN WHAT IT SPEAKS, because the
hexadecimal a reader would otherwise type out is an address with its
type tags, a status byte with its channel in the low half of it, or a
header with the count of what follows — which is a message nobody spells
twice without a mistake in it. `sigil::seer::oscMessage()` spells the
bytes of one OSC message from an address and its arguments written as a
JSON list; `sigil::seer::midiMessage()` spells one MIDI message from its
kind, the channel it is played on and the numbers that kind carries, a
kind that takes one number leaving the second off the wire; and
`sigil::seer::dmxMessage()` spells one universe of dimmers from its
address and the levels written as a JSON list of numbers. Each answers
no bytes at all for a message it cannot spell, so half a message never
goes out.

`sigil::seer::Recorder` writes a wire down with
`sigil::seer::Recorder::record()` and opens a URI back onto a file with
`sigil::seer::Recorder::replay()`, which closes whatever was on that URI
first: a feed answers for a URI as long as anyone holds it, so the door
has to be let go before the file can take its place.

`sigil::seer::hexadecimal()`, `sigil::seer::printableText()`,
`sigil::seer::indentedJson()`, `sigil::seer::oscReading()`,
`sigil::seer::midiReading()`, `sigil::seer::dmxReading()` and
`sigil::seer::schemaReading()` are the seven readings of a message. Each
answers an empty string when the message is not that — a message half of
which is text is not text, because a reader shown the readable half
would take the whole thing for a broken string rather than for bytes.
All of them are written out in one layout, so a reader turning from one
to another reads what differs rather than how each was printed.

The two dialect readings are `sigil::data::decodeMidi()` and
`sigil::data::decodeArtNet()` laid out that same way: a message off an
instrument is its kind, the channel it was played on and the numbers
that kind carries, and a packet off a lighting desk is a universe of
dimmers with the address it is for. A list whose every member is a
number is the one place the layout differs, and it differs because the
thing differs: such a list is a run along a wire rather than a list of
values, so it is written across. A run that fits one line stands whole
beside its key and a longer one is rows of sixteen — a universe holds
512 dimmers, and a column that long is a column nobody reads.

The last is the only one that needs something besides the bytes,
because a buffer read in place says nothing about itself: the names of
its fields are in the schema and nowhere in the message. A buffer that
verifies as the schema's root is shown as the schema's own form, and a
message that IS that form already is converted through the schema and
back, so what a reader sees is what a door reading the same wire through
the same schema would hold rather than the text that happened to arrive.
A message the schema cannot hold is no reading, and the sentence the
call is asked for says whether there was no schema or no fit.

`sigil::seer::dialect()` is the word a scheme speaks in — `osc`, `midi`,
`dmx`, `lines`, `json`, and `bytes` where the scheme carries whatever a
sender puts on it — and empty for a scheme there is no word for. It is
read off the URI and off nothing else, because a reader picks the wire
to watch before a message has come down any of them. It says what the
wire SPEAKS and not what one message turned out to be: a socket named
for documents still carries a message that is none, and the readings are
where that shows.

`sigil::seer::hostAndPort()` is an address without the scheme in front
of it, for the lines that stand beside a wire whose scheme is spelled
there already.

## The window

Four panes, and one session behind all of them. Every pane reads what
the last frame wrote, so the list, the readings and the log are one
frame's answer rather than three asks a moment apart.

* **Connections** — a field to open a URI, a picker for the schema every
  wire is read through with the root it declares beside it, and the
  wires that are open: a dot for whether anything is coming, the URI,
  the rate, how many have arrived, the local end or the sentence that
  says why there is none, the word the wire speaks in with the end its
  newest message came from beside it, and a button to close it. The word
  stands from the moment the wire is opened, so a reader looking down
  the column sees what each wire speaks before choosing one. Every row names its own sender, because the tick
  reads it off the arrival rather than off whoever drained one.
  Choosing a row is what the other panes read. A
  `ws://` wire's path is part of the URI it was opened on, so the row
  says which path it is listening for; how many peers have reached it is
  not something a feed can be asked, so no row claims it.
* **Receive** — the newest message as hexadecimal pairs, as text, as an
  indented JSON document, as an OSC packet, as an instrument's message,
  as a desk's universe of dimmers and as the schema's own form,
  whichever of those it is; a reading the message does not have is
  offered disabled, which is how the pane says what the message is not,
  and the sentence under the tabs says why the schema's reading is not
  there. Seven tabs stand in one compact row, each word as short as the
  format it names, so the row is one row at the narrowest the window
  goes. Choosing a wire opens the readings on the one its scheme makes
  natural — the instrument's message on a `midi://` wire, the universe
  on an `artnet://` one, the packet on `osc://`, the line off a
  `serial://` cable as text where it is text and as bytes where it is
  not, the document elsewhere, and the bytes where the message is none
  of those — except that a loaded schema that reads the message reads
  first, a reader having gone and found it for exactly that. A reader
  who picks a tab keeps it until they choose another wire. The log stands below it, one line per message: when it
  arrived, who sent it, how big it is, and what it says.
* **Send** — the peer, an editor read either as text or as hexadecimal
  digits, and the three ways a message leaves: once, on every frame, or
  as the echo of everything arriving on the wire being read. WHAT THE
  PEER SPEAKS DECIDES WHAT IS TYPED: on an `osc://` peer an address
  stands beside the peer and the editor is the arguments under it as a
  JSON list; on a `midi://` peer there is no editor at all but a small
  form — the kind, the channel from 1 to 16, and the one or two numbers
  that kind carries under the names it calls them; and on an `artnet://`
  peer a universe stands beside the peer and the editor is the dimmers
  under it, as a JSON list of numbers. What is echoed back is the bytes
  that arrived, whatever the wire is, because an echo is the message and
  not a reading of it.
* **Record and replay** — writing the wire being read to a file, and
  opening a file back onto a URI. What is above then reads the file
  exactly as it read the port.

The controls are Ifrit.Ui's — the theme derived from the system palette,
the glass panel, the native window dressing — so the window follows the
machine's light and dark appearance and invents no look of its own.

Every URI on the command line is opened before the window comes up and
the first of them is the one being read — a reader who names several
names the wire they are watching first and the rest to have them open —
so a run that always watches the same wires is one command rather than
fields typed again every time. `--schema <bfbs>` is the schema file the
messages are read through, loaded before the first wire is opened so the
first message to arrive is already read as fields; the window loads one
through a picker too, but a picker needs a person, so a run driven from
a script names the file. `--shot` writes the
window down as a picture once it has run for a moment and then closes
it, which is how the panes are looked at from a script; a photographed
run keeps its own opaque ground rather than the machine's glass, because
the glass lies behind the window and a picture read back off the frames
cannot reach it. Everything else a session does, it does in the window.

## Boundary

SigilIO's feeds and byte vocabulary are the public dependency, and so
are SigilData's decoders: the schema a wire is read through is one of
their values, and whoever hands one over has to be able to make one. The
transports stay private — no header here names one, a scheme being a
string. What every reading ANSWERS is still a string and what a spelled
message answers is still bytes, so nobody who links this is made to
speak in document types, even where the schema puts them within reach.
Nothing of SigilSketch is here, and nothing that draws.

## Building

One archive and one application, both always configured. The cases are
in `seer_test` under `build/bin/<config>/tests/`, and every one of them
runs with no application in the process. The codecs are linked there
beside the library, because what the dialect cases assert is that a
reading and a spelling agree with the bytes a sender writes; the build
also
compiles a schema of the cases' own and the schema cases read the
`.bfbs` file it writes off the disk, since what the schema reading
promises is a format no generated header in that binary was compiled
from:

```sh
cmake --build build --config Release --target Seer seer_test
ctest --test-dir build -C Release -R '^Seer' --output-on-failure
```
