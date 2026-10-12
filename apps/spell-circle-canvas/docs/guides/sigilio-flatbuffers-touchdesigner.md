# A wire from TouchDesigner: JSON first, OSC for channels, a schema when it settles

A TouchDesigner patch holds a show: levels on a CHOP, a colour, markers
on a bar, a beat. This guide sends that show to a Python sketch, which
draws it, and Grimoire publishes the drawing over Syphon so the patch
can show it again. The cue starts as JSON text, because JSON needs
nothing declared ahead of it: add a field in the patch and the sketch
can read it on the next frame. Channel-shaped data can go over OSC
instead, with no script on the TouchDesigner side at all. And once the
message stops changing, a FlatBuffers schema makes it a contract.

What makes that last move cheap is the way the sketch reads. SigilData
reads a FlatBuffer through its schema into the schema's JSON form, which
is the same JSON the patch would have sent as text. So the sketch reads
`latest().payload` as a `data.Json` whichever wire carried it, and moving
a door onto the schema changes one argument, `schema=`, on `data.connect`.

The sketch is `apps/grimoire/sketches/cue_deck/`, and the TouchDesigner
side is `touchdesigner/scripts/cue_deck.py`. Every code block below is
copied from those files or the sketch's test.

## Who does what

| Piece | What it owns here |
| --- | --- |
| [SigilIO](doxygen:SigilIO) | The doors: datagrams arriving on a UDP port, a recording played back at the same URI, a publication of drawn frames |
| [SigilData](doxygen:SigilData) | The connection: every arrival read as a `data.Json`, handlers by OSC address, and a count of the arrivals that are no message |
| The sketch, `cue_deck.py` | Three doors, the page drawn from the newest cue, and the name of the door it came through |
| `sender.py` | The same cues from a terminal, on any of the three wires, or as a recording |
| Grimoire | The window, the still, and `--publish`, which offers the canvas over Syphon |
| TouchDesigner | The patch: reads its operators, sends the cue, shows what comes back |

## Step 1 — JSON from a short TouchDesigner script

On the TouchDesigner side, an Execute DAT reads the operators into a
dictionary in the shape the sketch reads and sends it as one datagram
through a UDP Out DAT:

```python
def read_cue():
    """The patch's cue, in the shape the sketch reads."""
    levels, color, markers = op("cue_levels"), op("cue_color"), op("cue_markers")
    return {
        "show": SHOW,
        "channels": [
            {"name": channel.name, "level": float(channel[0])}
            for channel in levels.chans()
        ],
        "color": {name: float(color[name]) for name in ("r", "g", "b")},
        "markers": [
            {"name": row[0].val, "beat": float(row[1].val)}
            for row in markers.rows()[1:]
        ],
        "beat": absTime.seconds * TEMPO / 60.0 % BAR,
    }


def send_json():
    """This frame's cue as JSON text, one datagram."""
    op("cue_out").sendBytes(json.dumps(read_cue()).encode())
```

A cue is the whole show at one moment, so a lost datagram costs
nothing: the next one is true on its own.

On the sketch's side, the JSON door is one line. It takes no schema:
every datagram on the port is read as JSON text into a `data.Json`.

```python
JSON_DOOR = "udp://:27025"  # a cue as JSON text
```

```python
        # JSON TEXT: every datagram is read as one data.Json.
        self.json_door = data.connect(hub, JSON_DOOR)
```

`latest()` is the newest message whole, kept until the next replaces
it, and its `payload` is the cue. The page reads it by field name, and
every reading says what to use when the field is absent, so the moment
before the first cue is drawn by the same code:

```python
                            meter(
                                channel["name"].string(),
                                channel["level"].number(),
                                color,
                                self.look,
                            )
                            for channel in channels
```

## Step 2 — OSC, with no code in TouchDesigner

A CHOP is already a set of named numbers, and an OSC Out CHOP sends
them as they stand. Merge the CHOPs, give every channel the address it
goes out as with a Rename CHOP (`/cue/level/KEY`, `/cue/color/r`,
`/cue/beat`), and wire the merge into an OSC Out CHOP aimed at
`127.0.0.1`, port `27026`, sending one message per channel. Nothing is
written in the patch.

The sketch's OSC door names its addresses with OSC address patterns.
`*` matches any run of characters within one part of an address and
`{r,g,b}` any one of those words; each handler writes its value into
the cue the messages add up to:

```python
        # OSC: one message a value, gathered into the cue they add up to.
        self.osc_door = data.connect(hub, OSC_DOOR)
        self.gathered = {"channels": {}, "color": {}, "markers": {}}
        self.osc_door.on("/cue/show", self.take_show)
        self.osc_door.on("/cue/level/*", self.take_level)
        self.osc_door.on("/cue/color/{r,g,b}", self.take_color)
        self.osc_door.on("/cue/marker", self.take_marker)
        self.osc_door.on("/cue/beat", self.take_beat)
```

A handler is handed a `data.Message`: `address()` is its OSC path,
`number()` its first argument as a number, and `message[1]` any other
argument.

```python
    def take_level(self, message: data.Message) -> None:
        name = message.address().rsplit("/", 1)[1]
        self.gathered["channels"][name] = message.number()
```

`osc_cue()` lays what was gathered out in the same shape the JSON door
receives, so the page reads it with the same code. A CHOP carries
numbers only, so the show's name and the markers stay off this route
when it starts in a patch; `sender.py --wire osc` sends them anyway, as
`/cue/show` with a string and `/cue/marker` with a name and a beat:

```python
def osc_datagrams(cue):
    """The OSC wire: one message a value, each its own datagram. A level
    and a colour channel are one float each, so a CHOP's channels can be
    sent as they stand; a marker is its name and its beat."""
    messages = [osc_message("/cue/show", [cue["show"]])]
    for channel in cue["channels"]:
        messages.append(
            osc_message(f"/cue/level/{channel['name']}", [channel["level"]])
        )
    for name, value in cue["color"].items():
        messages.append(osc_message(f"/cue/color/{name}", [value]))
    for marker in cue["markers"]:
        messages.append(osc_message("/cue/marker", [marker["name"], marker["beat"]]))
    messages.append(osc_message("/cue/beat", [cue["beat"]]))
    return messages
```

## Step 3 — run it live, and as a still

From `apps/grimoire/sketches/cue_deck`, start a sender and open the
sketch:

```sh
python3 sender.py &
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire cue_deck.py
```

The door column says `LISTENING`, the `JSON` count climbs ten a second,
and `NEWEST` names the door the drawn cue came through. Run
`python3 sender.py --wire osc` beside it and the `OSC` count climbs a
message a value; whichever door spoke last is drawn. Saving
`cue_deck.py` replaces the page while the doors stay open.

The page asks each door for its newest message and draws the one that
arrived last:

```python
    def newest(self) -> tuple[str, data.Json]:
        """The door the newest message came through, and the cue it holds."""
        arrived = [
            (door.latest().receivedAt(), name)
            for name, door in self.doors.items()
            if not door.latest().empty()
        ]
        if not arrived:
            return "", data.Json()
        name = max(arrived)[1]
        if name == "OSC":
            return name, self.osc_cue()
        return name, self.doors[name].latest().payload
```

A still is a capture, and a capture opens no socket. It replays the JSON
recording at the JSON door and a recording with nothing in it at the
other two, so it is the same picture every run: three seconds in, thirty
cues landed, the state `REPLAY`, the newest door `JSON`.

```python
        # A CAPTURE OPENS NO SOCKET: every later ask for a door's URI
        # answers the recording replayed there instead.
        self.replaying = ctx.deterministic
        if self.replaying:
            hub.replay(JSON_DOOR, ctx.local(RECORDING))
            hub.replay(OSC_DOOR, ctx.local(SILENCE))
            hub.replay(SCHEMA_DOOR, ctx.local(SILENCE))
```

```sh
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire cue_deck.py --frame cue_deck.png
```

## Step 4 — publish the canvas over Syphon

```sh
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire cue_deck.py --publish "Cue Deck"
```

Every frame the sketch draws is offered to other applications under
`Cue Deck`: the canvas the sketch declared, 1280 × 800, never the window
around it. Ctrl-P in the window turns publishing off and on. Publishing
needs the window on its Metal backend; a run on the CPU raster fallback
refuses the flag and says so.

## Step 5 — the TouchDesigner patch

`touchdesigner/scripts/cue_deck.py` is an Execute DAT's callbacks. The
operators beside it, by name:

1. **`cue_levels`**: any CHOP whose channels are the meters, such as a
   Constant, Noise or Audio Analysis CHOP. Each channel's name labels
   its meter and its first sample, from zero to one, is the level.
2. **`cue_color`**: a Constant CHOP with channels `r`, `g` and `b`.
3. **`cue_markers`**: a Table DAT with a header row `name`, `beat` and a
   row per marker, for example `DROP` and `8`.
4. **`cue_out`**: a UDP Out DAT, Network Address `127.0.0.1`, Network
   Port `27025`.
5. **`cue_send`**: an Execute DAT with Frame Start on, its text synced to
   the script.
6. **`show_in`**: a Syphon Spout In TOP with Sender Name `Cue Deck`, the
   drawn page coming back at the sketch's own size.

Every frame sends the JSON cue:

```python
def onFrameStart(frame):
    send_json()
    return
```

To send only when a level moves, a CHOP Execute DAT on `cue_levels`
calls `send_json()` from its `onValueChange` instead. With Grimoire
publishing and the patch running, the sketch counts the patch's cues and
`show_in` shows the page the patch just described.

## Step 6 — when the message settles: the schema

While the patch is changing what a cue carries, JSON costs nothing to
change. Once it stops changing, a schema says what a cue is for every
language that reads or writes one, and `flatc` generates the code that
builds it:

```flatbuffers
table Cue {
  show: string;          // the name the marquee carries
  channels: [Channel];   // one meter each, in order
  color: Color;          // the colour every meter is filled with
  markers: [Marker];
  beat: float;           // where the show stands, in beats from zero
}
```

`cue_deck.fbs` holds it with `Channel`, `Marker` and the `Color` struct;
a directory sketch's schema is the `.fbs` named for the sketch. Two
things are generated from it. **The Python modules a sender builds a
cue with** are generated once and committed, so a sender needs no build.
From `apps/grimoire/sketches/cue_deck`:

```sh
../../../spell-circle-canvas/build/vcpkg_installed/arm64-osx/tools/flatbuffers/flatc --python -o . cue_deck.fbs
```

That writes the `CueDeck` package. **The binary schema the sketch reads
through** is the build's: building Grimoire, or the `SigilSketchSchemas`
target, writes `cue_deck.bfbs` beside the sketch, never committed and
never edited.

**The switch is one argument.** The FlatBuffer door is the JSON door
with the schema handed to it, and what it hands the page is the same
`data.Json`:

```python
        schema = data.Schema.fromBinarySchema(binary)
        self.schema_door = data.connect(hub, SCHEMA_DOOR, schema=schema)
```

This sketch keeps it on its own port, `27027`, so all three wires can
be compared side by side. A door read through a schema also takes the
schema's JSON form as text, so handing `schema=` to the JSON door
instead keeps the patch sending JSON while the sketch starts checking
it.

On the sending side the schema costs code. A FlatBuffers builder writes
back to front, so every string and child table is made before the vector
that holds it, and every vector before its table:

```python
    builder = flatbuffers.Builder(512)
    show = builder.CreateString(cue["show"])

    channels = []
    for channel in cue["channels"]:
        label = builder.CreateString(channel["name"])
        Channel.Start(builder)
        Channel.AddName(builder, label)
        Channel.AddLevel(builder, channel["level"])
        channels.append(Channel.End(builder))
```

The vector of channels follows, the markers the same way, and the `Cue`
table last. The TouchDesigner script carries the same `encode` and sends
through it once `cue_out`'s port is `27027` and `onFrameStart` calls
this instead:

```python
def send_flatbuffer():
    """This frame's cue as one Cue FlatBuffer, one datagram."""
    op("cue_out").sendBytes(encode(read_cue()))
```

`encode` puts the sketch's directory on the import path before importing
`CueDeck`, so a regenerated schema is what the next frame sends. The
`flatbuffers` package comes from the venv
`touchdesigner/scripts/configure_editors.py` creates with
TouchDesigner's own interpreter.

What the schema buys:

- **Validation.** The JSON door draws any JSON it is handed, including
  a cue missing a field or carrying a word where a number belongs. A
  schema door reads only what fits the schema.
- **The undecodable count.** An arrival that does not fit is counted in
  the door's `state().undecodable`, shown under `NOT A CUE`, and never
  drawn as a page missing its fields.
- **Compactness.** A buffer carries no field names and holds its
  numbers as binary rather than as digits, so the same cue is fewer
  bytes.

## Step 7 — record, replay, and test

A recording is the file a capture replays. `sender.py --record` writes
one in SigilIO's recording format, a header line and then each datagram
as its time, its length and its bytes, on whichever wire `--wire`
names:

```python
def write_recording(path, wire, seconds, rate):
    """Every cue over a run of `seconds` on `wire`, as a recording file:
    each datagram one arrival, every datagram of a cue at that cue's time."""
    datagrams = WIRES[wire]
    times = frame_times(seconds, rate)
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at in times:
            for payload in datagrams(cue_at(at)):
                recording.write(FRAME.pack(at, len(payload)))
                recording.write(payload)
    return len(times)
```

```sh
python3 sender.py --record
python3 sender.py --wire osc --record
```

The first writes `data/cue.feed`, the JSON recording a capture replays;
the second writes `data/cue-osc.feed`. A live door records itself too:
`self.json_door.record(path)` writes every arrival until the returned
recording stops.

A test stands in for the transport. `sigil.io.testing.inletOf(feed)`
delivers bytes to a door as a datagram would arrive, and the hub hands
the same feed to anybody asking for the same URI. The test,
`apps/python/sigil/test/test_cue_deck.py`, runs the sketch under a probe
that delivers the datagrams the sender makes of one cue at the door that
wire is read at:

```python
    def setup(self, ctx):
        super().setup(ctx)
        arrivals = json.loads((HERE / "arrivals.json").read_text())
        inlet = inletOf(ctx.assets.hub().listen(getattr(deck, arrivals["door"])))
        for datagram in arrivals["datagrams"]:
            inlet.deliver(bytes.fromhex(datagram), arrivedAt=0)
```

Once per wire it asserts that the cue drawn is the cue sent, field for
field, that `NEWEST` names that wire's door, and that the still holds
the cue's colour; a fourth case finds bytes that are no cue counted and
nothing drawn. From `apps/python/sigil/test`:

```sh
PYTHONPATH=../../../spell-circle-canvas/build/python python3 -m unittest test_cue_deck
```

## What to change next

- **More of the show.** Add a key to `read_cue()` and read it in the
  sketch by name. Once the schema is in play, add the field to `Cue`
  too, then regenerate the modules and rebuild the binary schema; an
  older sender keeps working, a field it does not write reading as
  absent.
- **Talk back.** `self.json_door.send(value, to=message.sender())`
  answers the patch that sent a message, in the door's own wire, and a
  UDP In DAT on the TouchDesigner side receives it.
- **A transparent ground.** Declare `background=(0, 0, 0, 0)` on
  `@sketch` and the published canvas carries alpha, so `show_in`
  composites over the patch's own picture with no key.

## The fine print

- **One shape, three spellings.** A schema's JSON form has fields by
  name, vectors as arrays and a struct as an object, which is the JSON
  the patch sends as text.
- **The JSON door checks nothing.** Valid JSON of any shape is a
  message, and a missing field reads through its fallback; only text
  that is not JSON counts as undecodable.
- **OSC has no "gone".** A level or a marker the sender stops sending
  keeps the last value it had, and every number on the wire is a 32-bit
  float. An OSC bundle reaches the sketch as one message no address
  pattern names, so the OSC Out CHOP sends one message per channel.
- **A datagram is a whole cue** on the JSON and FlatBuffer wires, and
  nothing depends on order or on every datagram landing.
- **The doors are the session's.** Saving the sketch keeps them open;
  closing the window closes them. A door that cannot open says why in
  its state's `error`, its count turns to the trouble colour and the
  state reads `UNAVAILABLE`.
- **A capture never opens a socket.** `ctx.deterministic` is true for a
  still, a sweep and a plate, and the recordings stand in for the wire.
  `--no-deterministic` makes a still open the real doors instead.
