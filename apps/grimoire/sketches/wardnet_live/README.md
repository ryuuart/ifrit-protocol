# WardNet, live

The WardNet console with nothing scripted inside it: every row in the
scrollback, the alert chip and the marquee above, the cards and the load
meter come from small JSON messages arriving from outside, and every
message is answered to whoever sent it. A row mounts with an entrance
and types its body on behind a blinking cursor while the rows already
there keep their pictures, the alert chip breathes faster the worse its
level, the marquee carries the newest alert, and the meter glides to a
new load rather than jumping.
The canvas is 1280 × 720.

[The animated console guide](../../../spell-circle-canvas/docs/guides/animated-console-python.md)
walks through how the page moves, and [the driven-from-outside guide](../../../spell-circle-canvas/docs/guides/console-driven-from-outside-python.md)
through every file here step by step.

## The doors

| Door | Wire |
| --- | --- |
| `udp://:27030` | one JSON message a datagram |
| `ws://:27031/wardnet` | one JSON message a WebSocket message; the same port serves `pages/` over HTTP |

Both doors hand their messages to the same handlers, so a browser, a
terminal and a TouchDesigner patch can all drive the console at once.
Under the side column the busier door gives its own account of itself
through the kit's `connectionReadout`, set in the scrollback's terminal
voice, with the other door's count and where a browser finds the page
after it.

## The messages

Every message is a JSON object whose `"kind"` names what it changes:

```json
{"kind": "row", "tag": "WARN", "body": "relay/05 jitter above threshold"}
{"kind": "alert", "text": "BREACH · CHANNEL 9", "level": "breach"}
{"kind": "meter", "load": 0.7}
{"kind": "card", "title": "RELAY JITTER", "value": "41 ms", "note": "relay/05", "level": "warn"}
```

A `tag` is `TRACE`, `INFO`, `WARN` or `BREACH`; a `level` is `clear`,
`warn` or `breach`, and a card names one only if it wants a colour other
than the clear one. A card is found by its title, so a title sent again
changes that card. A message of any other kind is counted under the
doors as not understood.

Every message, understood or not, is answered through the door it came
by, to its sender:

```json
{"kind": "reply", "understood": "row", "rows": 12, "replies": 31}
```

`understood` is the kind handled, or `null` for a kind the console does
not read. A capture replays a recording, which names no sender, so it
answers nobody. `python -m sigil.examples.tools.send_live_signals` from
an installed `sigil` package sends a rising and falling load with a row
every second and prints each reply.

## The files

| File | What it is |
| --- | --- |
| `wardnet_live.py` | The sketch: two doors through `sigil.data.connect`, one handler per kind, and the slots each part of the page is drawn in. |
| `sender.py` | The scripted show, played over `--door udp` (the default, printing every reply) or `--door ws`, or `--record` to write the recordings under `data/`. The UDP door and the recordings need the standard library alone; the WebSocket door needs the `sigil` package. |
| `pages/index.html` | The page the WebSocket door serves: buttons for every kind of message. |
| `data/wardnet.feed` | One pass of the show, the recording a capture replays at the UDP door. |
| `data/silence.feed` | A recording with nothing in it, which a capture replays at the WebSocket door so a still opens no socket. |

`touchdesigner/scripts/wardnet_live.py` at the repository root is the
TouchDesigner side: an Execute DAT that sends the load from a CHOP and
every row appended to a Table DAT through a UDP Out DAT.

## Run it

From this directory:

```sh
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire wardnet_live.py
python3 sender.py                  # the show over UDP, over and over
python3 sender.py --door ws        # the same show over the WebSocket
```

Open `http://localhost:27031/` in a browser, or the machine's address
from a phone on the same network, and every button sends one message. A
still replays the recording instead of opening the ports, so it is the
same picture every run:

```sh
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire wardnet_live.py --frame wardnet_live.png
```

After editing the show in `sender.py`, write the recordings again with
`python3 sender.py --record`.

## What it puts under load

A Python sketch reading one message shape through two doors with one
set of handlers named by the message's kind, each answered to its
sender; a UDP listener and a
WebSocket listener that serves its own page; keyed rows appended into a
slot, each mounting with an entrance and typing its body on while the
rest keep their pictures; a measured marquee re-described when its alert changes; a
live value eased by the session's engine and followed through a binding,
so the meter moves with nothing described again; and a recording
replayed at a live URI for a deterministic capture.
