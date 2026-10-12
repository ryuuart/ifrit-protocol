# A FlatBuffers wire from TouchDesigner, drawn and sent back over Syphon

A TouchDesigner patch holds a show: levels on a CHOP, a colour, markers
on a bar, a beat. This guide gives that show a wire of its own. A
FlatBuffers schema says what one cue is; the patch builds a cue every
frame and sends it as one UDP datagram; a Python sketch reads the
datagram through the schema and draws it; Grimoire publishes the drawing
over Syphon; and a Syphon Spout In TOP brings the picture back into the
patch. It is the loop SpellCircle runs, built from the libraries a
sketch already has.

The whole sketch is `apps/grimoire/sketches/cue_deck/`, and the
TouchDesigner side is `touchdesigner/scripts/cue_deck.py`. Every code
block below is copied from those files.

## Who does what

| Piece | What it owns here |
| --- | --- |
| The schema, `cue_deck.fbs` | What a cue is, for every language that reads or writes one |
| `flatc` | The Python modules a sender builds a cue with, and the binary schema a reader reads one through |
| [SigilIO](doxygen:SigilIO) | The door: bytes arriving on `udp://:27025`, a recording played back at the same URI, a publication of drawn frames |
| [SigilData](doxygen:SigilData) | The connection: every arrival read through the schema as the schema's JSON form, and the arrivals that are no cue counted |
| The sketch, `cue_deck.py` | The page drawn from the newest cue |
| Grimoire | The window, the still, and `--publish`, which offers the canvas over Syphon |
| TouchDesigner | The patch: reads its operators, builds a cue, sends it, shows what comes back |

The bytes never change shape between the two ends: the patch sends a
FlatBuffer, the door carries it as bytes, and only the connection reads
it, through the binary schema the build compiled from the same `.fbs`.

## Step 1 — the schema

A cue is the whole state of the show at one moment rather than a change
to it, so a datagram that is lost costs nothing: the next one is true on
its own.

```flatbuffers
namespace CueDeck;

struct Color {
  r: float;  // each from zero to one
  g: float;
  b: float;
}

table Channel {
  name: string;  // the label under its meter
  level: float;  // from zero to one
}

table Marker {
  name: string;  // the label over its tick
  beat: float;   // where on the bar it stands, from zero
}

table Cue {
  show: string;          // the name the marquee carries
  channels: [Channel];   // one meter each, in order
  color: Color;          // the colour every meter is filled with
  markers: [Marker];
  beat: float;           // where the show stands, in beats from zero
}

root_type Cue;
```

The file is named for its directory, `cue_deck/cue_deck.fbs`: a
directory sketch's own schema is the `.fbs` whose stem is the sketch's.
The namespace, `CueDeck`, is the name of the Python package `flatc`
writes; it is spelled differently from the sketch's stem so the two
never shadow each other on an import path.

## Step 2 — the modules and the binary schema

Two things are generated from the schema, by two different hands.

**The Python modules are generated once and committed.** A sender — this
sketch's, or a TouchDesigner patch — imports them with no build in reach.
From `apps/grimoire/sketches/cue_deck`:

```sh
../../../spell-circle-canvas/build/vcpkg_installed/arm64-osx/tools/flatbuffers/flatc --python -o . cue_deck.fbs
```

That writes `CueDeck/Channel.py`, `Color.py`, `Cue.py`, `Marker.py` and
an empty `__init__.py`. They import the `flatbuffers` runtime package and
each other as `CueDeck.<Type>`, so the directory that holds `CueDeck/`
is what goes on an import path.

**The binary schema is the build's.** The catalogue's build compiles the
`.fbs` of every Python directory sketch into `<stem>.bfbs` beside the
entry, where the sketch reaches it as its own file. From
`apps/spell-circle-canvas`:

```sh
cmake --build build --config Release --target SigilSketchSchemas
```

`cue_deck.bfbs` is never committed and never edited; building Grimoire
writes it too.

## Step 3 — the sender

The sender builds a cue with the generated modules. A FlatBuffers
builder writes back to front, so every string and every child table is
made before the vector that holds it, and every vector before the table
that holds it. A struct is the one exception: it is written in place,
between `Start` and `End`.

```python
import flatbuffers
from CueDeck import Channel, Cue, Marker
from CueDeck.Color import CreateColor
```

```python
def encode(cue):
    """The bytes of one Cue: `cue` built into a FlatBuffer by the
    generated modules.

    A builder writes back to front, so every string and every table a
    vector holds is made before the vector, and every vector before the
    table that holds it.
    """
    builder = flatbuffers.Builder(512)
    show = builder.CreateString(cue["show"])

    channels = []
    for name, level in cue["channels"]:
        label = builder.CreateString(name)
        Channel.Start(builder)
        Channel.AddName(builder, label)
        Channel.AddLevel(builder, level)
        channels.append(Channel.End(builder))
    Cue.StartChannelsVector(builder, len(channels))
    for channel in reversed(channels):
        builder.PrependUOffsetTRelative(channel)
    channel_vector = builder.EndVector()

    markers = []
    for name, beat in cue["markers"]:
        label = builder.CreateString(name)
        Marker.Start(builder)
        Marker.AddName(builder, label)
        Marker.AddBeat(builder, beat)
        markers.append(Marker.End(builder))
    Cue.StartMarkersVector(builder, len(markers))
    for marker in reversed(markers):
        builder.PrependUOffsetTRelative(marker)
    marker_vector = builder.EndVector()

    Cue.Start(builder)
    Cue.AddShow(builder, show)
    Cue.AddChannels(builder, channel_vector)
    # A struct is written in place, inside the table that holds it, so it
    # is made between Start and End rather than before them.
    Cue.AddColor(builder, CreateColor(builder, *cue["color"]))
    Cue.AddMarkers(builder, marker_vector)
    Cue.AddBeat(builder, cue["beat"])
    builder.Finish(Cue.End(builder))
    return bytes(builder.Output())
```

Sending is one datagram a cue, from the standard library:

```python
def send(host, port, rate):
    """One cue a tick to a port, until interrupted."""
    door = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    print(f"sending cues to {host}:{port} at {rate:g}/s")
    started = time.monotonic()
    index = 0
    while True:
        at = (index + 0.5) / rate
        pause = started + at - time.monotonic()
        if pause > 0:
            time.sleep(pause)
        door.sendto(encode(cue_at(at)), (host, port))
        index += 1
```

`cue_at(seconds)` makes the show's values from the time alone, so a live
send and a recording carry the same cues at the same seconds.

## Step 4 — the sketch reads the door through the schema

`setup` reads the binary schema beside the sketch, opens the door
through it, and keeps the connection. In a capture it first tells the
hub to replay the recording at the door's URI, so the same
`data.connect` reads the file instead of the network and nothing else in
the sketch knows the difference.

```python
PORT = 27025
DOOR = f"udp://:{PORT}"
SCHEMA = "cue_deck.bfbs"
RECORDING = "data/cue.feed"
```

```python
    def setup(self, ctx: Scene2DContext) -> None:
        self.look = kit.feature_theme(kit.Density.Spacious)
        with kit.provide(self.look):
            kit.stage(ctx, size=(WIDTH, HEIGHT), capture_at=CAPTURE_AT)
        hub = ctx.assets.hub()
        # THE SCHEMA THE BUILD WROTE beside this file from cue_deck.fbs.
        binary = hub.read(ctx.local(SCHEMA))
        if binary is None:
            raise RuntimeError(
                f"{SCHEMA} is missing: build the SigilSketchSchemas target"
            )
        # A CAPTURE READS THE RECORDING, a window opens the door: every
        # later ask for the URI answers the replaying feed.
        self.replaying = ctx.deterministic
        if self.replaying:
            hub.replay(DOOR, ctx.local(RECORDING))
        self.door = data.connect(hub, DOOR, schema=data.Schema.fromBinarySchema(binary))
        self.shown = self.door.state()
        # One clock for the marquee: the name slides on a binding of it, so
        # nothing is described again between two cues.
        self.clock = animatable(0)
        ctx.engine.timer(lambda delta, elapsed: self.clock.set(elapsed))
        self.describe(ctx)
```

The page is described again only when the door's state moves — a new
cue raises its revision, a datagram that is no cue raises its
`undecodable` count:

```python
    def update(self, elapsed: float, ctx: Scene2DContext) -> None:
        """A door whose vitals moved has a new cue, or a new state to show."""
        state = self.door.state()
        if state != self.shown:
            self.shown = state
            self.describe(ctx)
```

`latest()` is the newest cue whole, kept until the next replaces it, and
its `payload` is the cue in the schema's JSON form: fields by name,
vectors as arrays, the struct as an object.

```python
    def describe(self, ctx: Scene2DContext) -> None:
        # The newest cue whole, kept by the door until the next replaces it.
        cue = self.door.latest().payload
```

Every reading names a field of the schema and says what to use when it
is absent, so the page draws the moment before the first cue arrives
from the same code:

```python
def hex_of(color: data.Json) -> str:
    """A cue's colour as the hex a fill takes; white for a cue with none."""
    if color.null():
        return "#ffffff"
    channels = (color[name].number(1) for name in ("r", "g", "b"))
    return "#" + "".join(
        f"{round(min(1, max(0, value)) * 255):02x}" for value in channels
    )
```

```python
                self.marquee(ctx, cue["show"].string("WAITING FOR A CUE"), color),
```

A channel becomes a meter, a marker a flag on the ruler:

```python
                            meter(
                                channel["name"].string(),
                                channel["level"].number(),
                                color,
                                self.look,
                            )
                            for channel in channels
```

The door's column reads the connection's state directly — its readiness,
the address it bound, how many cues have arrived, how many arrivals were
no cue, how many the queue dropped:

```python
                door_row("STATE", status, ink),
                door_row("ADDRESS", state.localAddress or DOOR),
                door_row("CUES", f"{state.revision}"),
                door_row("NOT A CUE", f"{state.undecodable}"),
                door_row("DROPPED", f"{state.dropped}"),
```

## Step 5 — run it live, and as a still

From `apps/grimoire/sketches/cue_deck`, start the sender and open the
sketch:

```sh
python3 sender.py &
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire cue_deck.py
```

The door's column says `LISTENING`, its address and a count of cues that
climbs ten a second; the meters and the playhead follow every cue. Save
`cue_deck.py` and the page is replaced while the door stays open.

A still is a capture, so it replays `data/cue.feed` at the door's URI
and opens no socket:

```sh
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire cue_deck.py --frame cue_deck.png
```

It is the same picture every run: three seconds in, thirty cues landed,
the door's state `REPLAY`. `--no-deterministic` makes a still open the
real door instead, which is a way to see a live sender land without a
window.

## Step 6 — publish the canvas over Syphon

```sh
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire cue_deck.py --publish "Cue Deck"
```

Every frame the sketch draws is offered to other applications under
`Cue Deck`. What leaves is the canvas the sketch declared, 1280 × 800 at
one texture pixel per canvas unit, never the window around it. Ctrl-P in
the window turns publishing off and on.

Publishing needs the window on its Metal backend; a run on the CPU
raster fallback refuses the flag and says so. Seer checks it from the
other side:

```sh
../../../spell-circle-canvas/build/bin/Release/Seer.app/Contents/MacOS/Seer --list-textures
```

## Step 7 — the TouchDesigner patch

`touchdesigner/scripts/cue_deck.py` is an Execute DAT's callbacks: on
every frame start it reads the patch's operators, builds a cue with the
same generated modules and sends it.

**The environment.** The patch's Python environment is the venv
`touchdesigner/scripts/configure_editors.py` creates with TouchDesigner's
own interpreter and fills with `uv sync` from `touchdesigner/pyproject.toml`.
That project already depends on the SpellCircle package, which brings
the `flatbuffers` runtime. Run it once, with TouchDesigner installed:

```sh
python3 touchdesigner/scripts/configure_editors.py
```

**The generated modules.** The `CueDeck` package is not a distribution;
it is a directory of generated modules beside the schema. The script puts the sketch's
directory on the import path, so the modules are read where `flatc`
wrote them and a regenerated schema is what the next frame sends, with
no reinstall:

```python
# THE GENERATED MODULES. flatc writes the CueDeck package beside the
# sketch's schema, and they are read where they stand, so a schema edited
# and regenerated is what the next frame sends. project.folder is the
# folder this .toe is saved in, one level below the repository root.
CUE_DECK = Path(project.folder).parent / "apps" / "grimoire" / "sketches" / "cue_deck"
if str(CUE_DECK) not in sys.path:
    sys.path.append(str(CUE_DECK))

import flatbuffers
from CueDeck import Channel, Cue, Marker
from CueDeck.Color import CreateColor
```

**The operators**, beside the Execute DAT, by name:

1. **`cue_levels`** — any CHOP whose channels are the meters: a Constant
   CHOP to start with, or a Noise CHOP through a Rename CHOP, or an Audio
   Analysis CHOP. Each channel's name is its meter's label and its first
   sample, from zero to one, is the level.
2. **`cue_color`** — a Constant CHOP with three channels named `r`, `g`
   and `b`, each from zero to one.
3. **`cue_markers`** — a Table DAT with a header row `name`, `beat` and
   one row per marker, for example `DROP` and `8`.
4. **`cue_out`** — a UDP Out DAT: Network Address `127.0.0.1`, Network
   Port `27025`.
5. **`cue_send`** — an Execute DAT with Frame Start on, its text synced
   to `touchdesigner/scripts/cue_deck.py` (or pasted in).
6. **`show_in`** — a Syphon Spout In TOP with Sender Name `Cue Deck`.
   This is the drawn page coming back, at the sketch's own size, ready
   to composite over the rest of the show.

The script reads the operators into plain values:

```python
def read_cue():
    """The patch's cue as plain values: what the operators hold this frame."""
    levels = op("cue_levels")
    color = op("cue_color")
    markers = op("cue_markers")
    return {
        "show": SHOW,
        "channels": [(channel.name, float(channel[0])) for channel in levels.chans()],
        "color": (float(color["r"]), float(color["g"]), float(color["b"])),
        "markers": [(row[0].val, float(row[1].val)) for row in markers.rows()[1:]],
        "beat": absTime.seconds * TEMPO / 60.0 % BAR,
    }
```

and sends one datagram a frame:

```python
def send_cue():
    """This frame's cue, as one datagram."""
    op("cue_out").sendBytes(encode(read_cue()))


def onFrameStart(frame):
    send_cue()
    return
```

`encode` is the sender's from Step 3, line for line. To send only when a
level moves, a CHOP Execute DAT on `cue_levels` calls `send_cue()` from
its `onValueChange` instead.

With Grimoire publishing and the patch running, the door's column in the
sketch counts the patch's cues, and `show_in` shows the page the patch
just described. A datagram on the port that is no cue — another
program's, or a patch sending bytes of its own — is counted under
`NOT A CUE` and never drawn as a page missing its fields.

## Step 8 — record, replay, and test

A recording is the file a capture replays. `sender.py --record` writes
one in SigilIO's recording format — a header line, then each cue as its
time, its length and its bytes:

```python
def write_recording(path, seconds, rate):
    """Every cue over a run of `seconds`, as a recording file."""
    times = frame_times(seconds, rate)
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at in times:
            payload = encode(cue_at(at))
            recording.write(FRAME.pack(at, len(payload)))
            recording.write(payload)
    return len(times)
```

```sh
python3 sender.py --record
```

A live door can record itself too: `self.door.record(path)` on the
connection writes every arrival until the returned recording stops, so a
patch's own performance becomes the file a capture replays.

A test stands in for the transport. `sigil.io.testing.inletOf(feed)`
answers the one thing a reader of a feed does not have, `deliver`, and
the hub hands the same feed to anybody asking for the same URI. The test
in `apps/python/sigil/test/test_cue_deck.py` runs the sketch under a
probe that delivers one cue the sender encoded:

```python
@sketch(size=(deck.WIDTH, deck.HEIGHT), capture_at=0)
class Probe(deck.Deck):
    def setup(self, ctx):
        super().setup(ctx)
        payload = (HERE / "cue.bin").read_bytes()
        if payload:
            inletOf(ctx.assets.hub().listen(deck.DOOR)).deliver(payload, arrivedAt=0)
```

and asserts what the door decoded, field by field, and that the still
holds the cue's colour where the meters stand. A second case delivers
bytes that are no cue and finds them counted and nothing drawn. From
`apps/python/sigil/test`:

```sh
PYTHONPATH=../../../spell-circle-canvas/build/python python3 -m unittest test_cue_deck
```

The catalogue's plate of the sketch is the still at its capture moment,
so a change to the page is judged like every other sketch's.

## What to change next

- **More of the show.** Add a field to `Cue` — a `tempo`, a `cue_number`,
  a vector of `Color` for a palette — regenerate the modules and the
  recording, rebuild the binary schema, and read it in the sketch by name.
  An older sender keeps working: a field it does not write reads as
  absent.
- **Talk back.** `self.door.send(value, to=message.sender())` answers the
  patch that sent a message; a door read through a schema writes the
  value as the buffer the schema makes, and a UDP In DAT on the
  TouchDesigner side receives it.
- **Several patches.** `latest()` is the newest cue whoever sent it; the
  `sender` of each `receive()`d message tells them apart.
- **A transparent ground.** Declare `background=(0, 0, 0, 0)` on
  `@sketch` and the published canvas carries alpha, so `show_in`
  composites over the patch's own picture with no key.

## The fine print

- **The schema is the contract, both ways.** The modules a sender builds
  with and the binary schema the sketch reads through are both made from
  `cue_deck.fbs`. Regenerate both after every edit; a stale side reads
  the other's new fields as absent rather than failing.
- **Bytes on the wire, JSON in the sketch.** The patch never sends JSON.
  The connection reads the buffer through the schema and hands the
  sketch the schema's JSON form, which is why `sigil.data.Json` is the
  only type the page reads.
- **A datagram is a whole cue.** One cue is a few hundred bytes, well
  inside one datagram, and nothing on this wire depends on order or on
  every datagram landing.
- **The door is the session's.** Saving the sketch keeps the door open;
  closing the window closes it. A door that cannot open says why in its
  state's `error`, and the door's column shows it as `UNAVAILABLE`.
- **A capture never opens a socket.** `ctx.deterministic` is true for a
  still, a sweep and a plate, and the recording stands in for the wire.
