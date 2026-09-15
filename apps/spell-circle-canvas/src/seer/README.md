# SigilSeer — every wire, and what is going down it

A **wire** is one URI a message arrives on or leaves by. Seer is the tool
that watches all of them at once: it opens a wire, says what is coming
down it, shows the newest message three ways, keeps the ones before it,
sends a message back, writes a wire down to a file, and opens that file
again as if it were the port.

It stands on SigilIO's feeds and on nothing else that draws. What
travels a wire is bytes, and what those bytes mean belongs to whoever is
at the other end — so a tool for looking at the wire itself can be
pointed at a wire whose format it has never heard of, which is the only
way it is useful while that format is still being decided.

```sh
cmake --build build --config Release --target Seer
open build/bin/Release/Seer.app                          # the window
build/bin/Release/Seer.app/Contents/MacOS/Seer udp://:27020
build/bin/Release/Seer.app/Contents/MacOS/Seer --shot panes.png udp://:27020
```

## Two targets

| target | what it is |
| --- | --- |
| `SigilSeerWire` | the archive: the wires, the log, the way out, the recorder, and the readings. Namespace `sigil::seer`, headers under `include/sigilseer/wire/`, no toolkit at all |
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
many have arrived, how many were dropped, the local end, the error, and
whether it is closed. The rate is worked out from the generations
earlier ticks read, so it is a property of how often the host calls
this and needs no thread behind it. Nothing here has a clock:
`sigil::seer::Wires::dispatch()` and the tick both take the caller's
seconds.

`sigil::seer::Log` keeps what `sigil::seer::Log::drain()` takes off one
feed, newest last, up to the capacity it was made with;
`sigil::seer::Log::forgotten()` counts what fell off the front, which is
a different bound from the feed's own dropped count and is reported
separately.

`sigil::seer::Sender` opens a peer through the same wires — so the peer
stands in the same list and its replies arrive on it — and sends one
message or the same message every period, with
`sigil::seer::Sender::tick()` as the only thing that sends.

`sigil::seer::Recorder` writes a wire down with
`sigil::seer::Recorder::record()` and opens a URI back onto a file with
`sigil::seer::Recorder::replay()`, which closes whatever was on that URI
first: a feed answers for a URI as long as anyone holds it, so the door
has to be let go before the file can take its place.

`sigil::seer::hexadecimal()`, `sigil::seer::printableText()` and
`sigil::seer::indentedJson()` are the three readings of a message. Each
answers an empty string when the message is not that — a message half of
which is text is not text, because a reader shown the readable half
would take the whole thing for a broken string rather than for bytes.

## The window

Four panes, and one session behind all of them. Every pane reads what
the last frame wrote, so the list, the readings and the log are one
frame's answer rather than three asks a moment apart.

* **Connections** — a field to open a URI, and the wires that are open:
  a dot for whether anything is coming, the URI, the rate, how many have
  arrived, the local end or the sentence that says why there is none,
  and a button to close it. Choosing a row is what the other panes read.
* **Receive** — the newest message as hexadecimal pairs, as text, and as
  an indented JSON document, whichever of those it is; a reading the
  message does not have is offered disabled, which is how the pane says
  what the message is not. The log stands below it, one line per message.
* **Send** — the peer, an editor read either as text or as hexadecimal
  digits, and the three ways a message leaves: once, on every frame, or
  as the echo of everything arriving on the wire being read.
* **Record and replay** — writing the wire being read to a file, and
  opening a file back onto a URI. What is above then reads the file
  exactly as it read the port.

The controls are Ifrit.Ui's — the theme derived from the system palette,
the glass panel, the native window dressing — so the window follows the
machine's light and dark appearance and invents no look of its own.

A URI on the command line is opened before the window comes up and is
the one being read, so a run that always watches the same port is one
command rather than a field typed again every time. `--shot` writes the
window down as a picture once it has run for a moment and then closes
it, which is how the panes are looked at from a script; a photographed
run keeps its own opaque ground rather than the machine's glass, because
the glass lies behind the window and a picture read back off the frames
cannot reach it. Everything else a session does, it does in the window.

## Boundary

SigilIO's feeds and byte vocabulary are the public dependency. The
transports are private — no header here names one, a scheme being a
string — and so is the JSON parser, because a reading answers a string
and nobody who links this inherits a document type. Nothing of
SigilSketch is here, and nothing that draws.

## Building

One archive and one application, both always configured. The cases are
in `seer_test` under `build/bin/<config>/tests/`, and every one of them
runs with no application in the process:

```sh
cmake --build build --config Release --target Seer seer_test
ctest --test-dir build -C Release -R '^Seer' --output-on-failure
```
