# A console driven from outside, in Python: JSON over UDP and a WebSocket

The WardNet console of [the animated console guide](guide:animated-console-python),
seen from its doors: nothing is scripted inside it. Every row in the scrollback, the alert
chip, the marquee, the cards and the load meter come from small JSON
messages that something else sends: a terminal, a browser page the
sketch serves itself, or a TouchDesigner patch. A new row mounts with an
entrance while the rows already on screen keep their pictures, the alert
chip breathes faster the worse its level, and the meter glides to a new
load instead of jumping.

The sketch is the directory `apps/grimoire/sketches/wardnet_live/`;
open it with `Grimoire wardnet_live.py` and edit it live. Every code
block below is copied from the files in that directory, from
`touchdesigner/scripts/wardnet_live.py`, or from the sketch's test.

## Who does what

| Piece | What it owns here |
| --- | --- |
| [SigilIO](doxygen:SigilIO) | The doors: datagrams on a UDP port, a WebSocket listener that also serves a directory of pages, a recording played back at a door's URI |
| [SigilData](doxygen:SigilData) | The connection: every arrival read as a `data.Json`, and a handler run for each message by the name its `"kind"` gives it |
| [SigilMotion](doxygen:SigilMotion) | The entrances, the loops, the eased load and the binding the meter's bar follows |
| The sketch, `wardnet_live.py` | Two doors, one handler per kind, a model the handlers write, and the slots each part of the page is drawn in |
| `sender.py` | A scripted show from a terminal, over either door, or as the recording a still replays |
| `pages/index.html` | A page of buttons the WebSocket door serves, one per kind of message |
| TouchDesigner | A CHOP's load and a Table DAT's rows, sent by a short Execute DAT |

## Step 1 — the message shape

A message is one JSON object, and its `"kind"` says which part of the
page it changes. Four kinds cover the whole console:

```json
{"kind": "row", "tag": "WARN", "body": "relay/05 jitter above threshold"}
{"kind": "alert", "text": "BREACH · CHANNEL 9", "level": "breach"}
{"kind": "meter", "load": 0.7}
{"kind": "card", "title": "RELAY JITTER", "value": "41 ms", "note": "relay/05", "level": "warn"}
```

A message carries only the one thing that changed. The sender writes
each kind with a small constructor, which is also the clearest
statement of the shape:

```python
def row(tag, body):
    """A row added to the scrollback: TRACE, INFO, WARN or BREACH."""
    return {"kind": "row", "tag": tag, "body": body}


def alert(text, level):
    """The alert chip and the marquee: clear, warn or breach."""
    return {"kind": "alert", "text": text, "level": level}


def meter(load):
    """The load, from zero to one."""
    return {"kind": "meter", "load": load}
```

The show is those messages at the seconds they land:

```python
    (3.0, row("BREACH", "unsigned packet on channel 9 · quarantined")),
    (3.05, alert("BREACH · CHANNEL 9", "breach")),
    (3.1, meter(0.91)),
    (3.3, card("QUARANTINE", "1 packet", "channel 9 held", "breach")),
```

## Step 2 — two doors, one set of handlers

The UDP door is a port. The WebSocket door is a port and a path, and its
query names a directory of pages: the same port then answers a browser's
HTTP request out of that directory, so the page and the socket it opens
back are one address. `ctx.local` names the directory beside the sketch.

```python
UDP_PORT = 27030
WEBSOCKET_PORT = 27031
UDP_DOOR = f"udp://:{UDP_PORT}"
WEBSOCKET_PATH = "/wardnet"  # where the page's socket connects
PAGES = "pages"  # the directory the WebSocket port serves
```

```python
def websocket_door(ctx: Scene2DContext) -> str:
    """The WebSocket door: the port and path a page's socket reaches, and
    the pages the same port serves, which stand beside this file."""
    return f"ws://:{WEBSOCKET_PORT}{WEBSOCKET_PATH}?pages={ctx.local(PAGES)}"
```

Both doors read JSON text, because neither scheme is `osc://`.
`data.connect` opens each one as a connection, and `on` names a handler
by the message's `"kind"`, so the same four handlers serve both doors.
A message no handler names reaches `otherwise`. Each handler is
registered wrapped in an answer, so every message is replied to after
it is handled:

```python
        # ONE SET OF HANDLERS, TWO DOORS. Both doors read JSON text, and a
        # handler is named by the message's "kind".
        self.doors = {
            "UDP": data.connect(hub, UDP_DOOR),
            "WEBSOCKET": data.connect(hub, websocket),
        }
        handlers = {
            "row": self.take_row,
            "alert": self.take_alert,
            "meter": self.take_meter,
            "card": self.take_card,
        }
        for door in self.doors.values():
            for kind, handler in handlers.items():
                door.on(kind, self.answered(door, kind, handler))
            door.otherwise(self.answered(door, "", self.take_stranger))
```

`door.reply` inside a handler sends to the sender of the message being
handled, through the door it came by: a datagram back to the port it
came from, a WebSocket message back to the page that sent it. A
recording names no sender, so a capture answers nobody and the readout
says so.

```python
    def answered(
        self, door: data.Connection, kind: str, handler: Callable[[data.Message], None]
    ) -> Callable[[data.Message], None]:
        """The handler for one kind, and then a reply to the message's
        sender through the door it came by. A recording names no sender, so
        in a capture nothing is answered."""

        def answer(message: data.Message) -> None:
            handler(message)
            # What a sender is told: the kind understood, or nothing where
            # the kind is not one the console reads, and the counts so far.
            answered = door.reply(
                {
                    "kind": "reply",
                    "understood": kind or None,
                    "rows": self.sequence,
                    "replies": self.replies + 1,
                }
            )
            if answered:
                self.replies += 1
                self.changed.add("doors")

        return answer
```

## Step 3 — handlers write the model; the frame describes the slots

A handler runs once per message, on the frame, before `update`. It
writes the model and notes which slot it changed, and draws nothing:

```python
    def take_row(self, message: data.Message) -> None:
        self.sequence += 1
        tag = message["tag"].string("INFO").upper()
        self.rows.append((self.sequence, tag, message["body"].string()))
        del self.rows[:-ROWS_SHOWN]
        self.changed.add("log")
```

Every reading names what to use when the field is absent, so a message
missing a field still lands. `update` then describes again only the
slots a message changed, once per frame however many messages arrived,
and the door readout whenever a door's state moved:

```python
    def update(self, elapsed: float, ctx: Scene2DContext) -> None:
        states = self.states()
        if states != self.shown:
            self.shown = states
            self.changed.add("doors")
        drawn = {
            "log": self.scrollback,
            "alert": self.chip,
            "marquee": self.ticker,
            "cards": self.card_column,
            "meter": self.load_meter,
            "doors": self.door_readout,
        }
        for name in sorted(self.changed):
            self.composer.renderSlot(name, drawn[name](ctx))
        self.changed.clear()
```

The page itself is described once, in `setup`, with a `compose.slot`
where each changing part goes.

## Step 4 — keyed rows and entrances

A row is keyed by its sequence number and carries its entrances: a fade
and a short slide, and its body typing itself on, a glyph at its turn,
all played once, when the row mounts. The newest row carries a blinking
cursor.

```python
def cursor() -> compose.Element:
    """A block cursor blinking on a loop on its opacity lane."""
    return (
        compose.box()
        .width(8)
        .height(14)
        .fill(ACCENT)
        .opacity(motion.animate(from_=1.0, to=0.0, duration=0.45, loop=-1, alternate=True))
    )


def log_row(sequence: int, tag: str, body: str, newest: bool = False) -> compose.Element:
    """One scrollback row, keyed by its sequence number. The body types
    itself on, a glyph at its turn, when the row mounts, and the newest row
    carries the cursor; a row already on screen keeps its picture when the
    slot is described again."""
    parts: list[compose.Element | compose.Text] = [
        label(f"{sequence:03d}", 12, DIM),
        label(f"[{tag}]", 12, TAG_INK.get(tag, CHROME)),
        label(body, 12, CHROME if tag == "TRACE" else BONE).textFx(
            textFx.entrance(textFx.typeOn())
        ),
    ]
    if newest:
        parts.append(cursor())
    row = (
        compose.box(*parts)
        .row()
        .gap(12)
        .alignItems("center")
        .key(f"row{sequence}")
        .opacity(motion.animate(from_=0, to=1, duration=0.35))
        .translateX(motion.animate(from_=-16, to=0, duration=0.3, ease=ease.outCubic))
    )
    if tag == "BREACH":
        row = row.fill("#2a0d12").padding(2, 6).borderRadius(2)
    return row
```

```python
    def scrollback(self, ctx: Scene2DContext) -> compose.Element | compose.Text:
        if not self.rows:
            return label("nothing has arrived on either door", 12, DIM)
        newest = self.rows[-1][0]
        return (
            compose.box(*(log_row(*row, newest=row[0] == newest) for row in self.rows))
            .column()
            .gap(8)
        )
```

When a row arrives the whole list is described again, but every row
that was there before has the same key, so it is the same node: its
entrance has already played and its picture is kept. Only the new key
mounts, and only it fades in. Cards work the same way, keyed by their
title, so a card sent again with a new value changes in place.

## Step 5 — the alert chip and the marquee

The chip is the console's breathing chip with its colours and its pace
read from the level:

```python
def alert_chip(content: str, level: str) -> compose.Element:
    """A chip whose fill breathes between two colours, faster the worse
    the level."""
    rest, breath = LEVEL_PULSE[level]
    ink = LEVEL_INK[level]
    pulse = motion.ColorTween(rest, ink, duration=breath, loop=-1, alternate=True)
    return (
        compose.box(label(content, 12, BONE))
        .padding(6, 12)
        .borderRadius(3)
        .fill(pulse)
        .filter(material.Filter.glow(ink, 10))
        .key("alert")
    )
```

The marquee carries the newest alert. Its text has no fixed width, so
one pass is measured, enough passes are laid in a row to cover the band,
and the row slides by exactly one pass on a loop:

```python
        text, level = self.alert
        run = ctx.measure(marquee_pass(text, LEVEL_INK[level])).width()
        passes = ceil((WIDTH - 56) / run) + 1
        slide = motion.animate(from_=0, to=-run, duration=run / MARQUEE_SPEED, loop=-1)
```

Measuring needs the context, which is valid only inside a callback the
host hands it to. That is why handlers note what changed and `update`,
which is handed the context, does the describing.

## Step 6 — the eased meter

A binding is a function of its source: it shapes a live number every
time it is read, and a source that jumps makes a bar that jumps. So the
easing goes on the source. The load is a live value, and a meter message
hands the session's engine a tween from wherever the value stands to the
new reading:

```python
    def take_meter(self, message: data.Message) -> None:
        self.target = min(1.0, max(0.0, message["load"].number(self.target)))
        self.engine.animate(
            self.load,
            motion.Tween(to=self.target, duration=METER_EASE, ease=ease.inOutCubic),
        )
        self.changed.add("meter")
```

The bar follows that value through `bind`, which maps zero to one onto
how far the bar is slid out of its clipping track. The bar moves every
frame of the glide and the page is never described for it:

```python
        bar = (
            compose.box()
            .width(METER_WIDTH)
            .height(6)
            .fill(ink)
            .translateX(bind(self.load, to=(-METER_WIDTH, 0)))
            .key("meter-bar")
        )
```

A load sent while the bar is still gliding starts a new tween from where
the bar stands, so a stream of readings from a slider is a smooth line
rather than a stair.

## Step 7 — the browser page

`pages/index.html` is one file with nothing fetched from anywhere else,
because the only server in reach is the sketch. It opens its socket on
the address it was served from, and every button sends one message as
JSON text:

```html
  const scheme = location.protocol === "https:" ? "wss://" : "ws://";
  const socket = new WebSocket(scheme + location.host + "/wardnet");
```

```html
  for (const button of document.querySelectorAll("[data-tag]"))
    button.onclick = () => send({ kind: "row", tag: button.dataset.tag, body: field("body") });

  for (const button of document.querySelectorAll("[data-level]"))
    button.onclick = () => send({ kind: "alert", text: field("alert"), level: button.dataset.level });

  document.getElementById("load").oninput = (event) =>
    send({ kind: "meter", load: Number(event.target.value) });
```

With the sketch open in a window, browse to `http://localhost:27031/`,
or to the machine's address from a phone on the same network.

## Step 8 — TouchDesigner

`touchdesigner/scripts/wardnet_live.py` is an Execute DAT with Frame
Start on. It reads a CHOP with a channel named `load` and a Table DAT
with columns `tag` and `body`, and sends through a UDP Out DAT aimed at
`127.0.0.1`, port `27030`: the load whenever it moves, and every row
appended to the table once.

```python
def send_load():
    """The load, when it has moved since the last one sent."""
    load = float(op("wardnet_load")["load"])
    if abs(load - me.fetch("load_sent", -1.0)) >= LOAD_STEP:
        me.store("load_sent", load)
        send({"kind": "meter", "load": round(load, 3)})


def send_new_rows():
    """Every row appended to the table since the last frame, in order."""
    rows = op("wardnet_rows").rows()[1:]
    sent = me.fetch("rows_sent", 0)
    for row in rows[sent:]:
        send({"kind": "row", "tag": row[0].val.upper(), "body": row[1].val})
    # A table emptied and filled again is read from its first row.
    me.store("rows_sent", len(rows))
```

`send_alert` and `send_card` stand beside them for a button's Panel
Execute DAT to call.

## Step 9 — the sender, recording and replay

`sender.py` plays the show over either door, over and over, until it
is interrupted. The UDP door needs only the standard library. The
WebSocket door uses a SigilIO hub of its own as the client, which calls
the server on a thread of its own:

```python
    from sigil.io import Hub, registerTransports

    hub = Hub()
    registerTransports(hub, ["ws"])
    door = hub.listen(uri)
```

A recording is what a capture replays. `--record` writes one pass of
the show in SigilIO's recording format, a header line and then each
message as its time, its length and its bytes:

```python
def write_recording(path, show, seconds):
    """Every message the show sends in its first `seconds`, as a
    recording file: one arrival a message, at that message's second."""
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at, message in show:
            if at < seconds:
                payload = payload_of(message)
                recording.write(FRAME.pack(at, len(payload)))
                recording.write(payload)
```

The sketch replays it at the UDP door whenever the session is
deterministic, which a still, a sweep and a plate always are, and an
empty recording at the WebSocket door, so a capture opens no socket and
is the same picture every run:

```python
        if self.replaying:
            hub.replay(UDP_DOOR, ctx.local(RECORDING))
            hub.replay(websocket, ctx.local(SILENCE))
```

```sh
python3 sender.py                  # the show over UDP, over and over, printing replies
python3 sender.py --door ws        # the same show over the WebSocket
python3 sender.py --record         # data/wardnet.feed and data/silence.feed
Grimoire wardnet_live.py --frame wardnet_live.png   # the breach in, the last row arriving
```

## Step 10 — the test

`apps/python/sigil/test/test_wardnet_live.py` runs the sketch under a
probe with the show replaced by silence, and delivers one message of
each kind, and one of a kind nothing handles, through the inlets of both
doors, the way a transport delivers them:

```python
        hub = ctx.assets.hub()
        doors = {"udp": console.UDP_DOOR, "websocket": console.websocket_door(ctx)}
        for door, message in json.loads((HERE / "arrivals.json").read_text()):
            inletOf(hub.listen(doors[door])).deliver(json.dumps(message).encode(), arrivedAt=0)
```

It asserts that each handler wrote its part of the model whichever door
the message came through, that the stranger was counted, that the eased
load has reached the value sent, that every keyed node the messages made
is laid out on the canvas, and that the still's bar is inked across
seven tenths of its track. From `apps/python/sigil/test`:

```sh
PYTHONPATH=../../../spell-circle-canvas/build/python python3 -m unittest test_wardnet_live
```

## What to change next

- **A new kind.** Add a handler with `door.on("ping", ...)` on both
  doors, a slot for what it changes, and a button for it on the page.
- **Talk to everyone.** A WebSocket door's `send` reaches every page
  connected to it, so every browser can show the same load the reply
  tells only its sender.
- **Entrances per severity.** Give a breach row a longer fade and a
  warning a wider slide; the entrance is part of the row's description.
- **A schema.** Once the shape settles, `data.connect(hub, UDP_DOOR,
  schema=...)` reads only what fits and counts the rest, as
  [the TouchDesigner wire guide](guide:sigilio-flatbuffers-touchdesigner)
  shows.

## The fine print

- **A handler draws nothing.** Handlers run on the frame, outside any
  callback the context is valid in, so they write the model and
  `update` describes. Many messages in one frame are one description.
- **A message is whole on its own.** Every message names its kind and
  carries what it changes; nothing depends on order across the two
  doors. Messages from one door are read in the order they arrived.
- **A binding does not smooth.** `bind` reads its source through its
  stages at the moment it is read; easing over time is a tween on the
  source, and the binding maps the eased value onto the bar.
- **The page is served from the filesystem.** The directory the pages
  query names is read on every request, so a page edited on disk is the
  page the next reload gets, and a path outside that directory is
  refused.
- **The WebSocket client answers before it has reached the server.** A
  send before the handshake is through goes nowhere and returns false,
  which is why the sender offers its first message again until it is
  taken.
- **A capture never opens a socket.** `ctx.deterministic` is true for a
  still, a sweep and a plate, and the recordings stand in for the
  doors. The doors are the session's: saving the sketch keeps them open,
  and closing the window closes them.
