# SpellCircle

SpellCircle draws network-driven vector diagrams for live production. An
external process — a Python script, a TouchDesigner patch — describes a
scene of circles, points placed on those circles' perimeters, edges
between points, and labelled boxes, and sends it as a FlatBuffers
datagram over UDP. SpellCircle receives it, draws it with Skia on the
GPU, and publishes the result as a transparent-background texture over
Syphon, where a VJ or compositing tool picks it up.

Seer hosts the Qt receiver beside its wire inspector, sender and recorder.
The receiver accepts externally authored scenes; Sketchbook is the authoring
application. Scenes may arrive at animation frame rates, so a sender can use
the receiver as a live output surface.

What you get on screen is a viewer and a control surface: a canvas you
pan and zoom, a timestamped feed of arriving packets, a packets-per-second
readout, and settings for accent colour, stroke width, scale, fonts, box
geometry, label offsets, canvas size, and the UDP port. Panning and
zooming never redraw the scene — they move an already-rendered image.

## Getting a picture on screen

Build (see [Build and test](#build-and-test)), open the Receiver in Seer,
then send it something:

```sh
build/bin/Release/Seer.app/Contents/MacOS/Seer --receiver udp://:27015
```

A plain Seer launch opens no ports. **Open Receiver** uses the saved source,
or UDP port 27015 when there are no saved settings. Its source stays pinned
while another wire is inspected. The receiver panel has start/stop, fit,
actual-size, clear, activity, and graphics settings controls. Graphics and
source settings save together under Seer's `receiver` configuration directory
and import existing
SpellCircle graphics and port settings when no Seer receiver settings exist.
Cancel restores the values present when settings opened. Syphon continues to
publish under the server name `SpellCircle`.

```sh
python3 apps/python/SpellCircle/test/send_spell_circles.py --seed 1
python3 apps/python/SpellCircle/test/animate_spell_circles.py --fps 60
```

The first sends a single randomized sigil; the second streams an animated
one. Both default to `127.0.0.1:27015`.

## Authoring a scene

The Python package builds and sends scenes:

```python
from SpellCircle import SpellCircleCanvas, SceneSender

canvas = SpellCircleCanvas(width=1000, height=1000)
ring = canvas.circle("outer", center_x=500, center_y=500, radius=400)

# Points live at a fraction clockwise from 12 o'clock.
top = canvas.point(ring, position=0.0, value="north")
right = canvas.point(ring, position=0.25, value="east")
canvas.edge(top, right)

# A radius-0 circle is an invisible anchor: it places things at an
# arbitrary coordinate instead of on a visible perimeter.
canvas.box("readout", canvas.point(canvas.anchor(500, 900)))

with SceneSender("127.0.0.1", 27015) as sender:
    sender.send(canvas.to_bytes())
```

`SpellCircleCanvas` records calls as plain values; `to_bytes()` is the
only thing that serializes. Points shared between edges become a single
point on the wire.

The package exports `SpellCircleCanvas`, `PointReference`,
`CircleDefinition`, `SceneBuilder`, `SceneSender`, and `send_once`.
`SceneBuilder` is the lower-level path if you want to emit FlatBuffers
tables yourself.

## How a packet becomes pixels

```
Python  ──FlatBuffers──▶  UDP :27015  ──▶  drain  ──▶  verify  ──▶  decode
                                                                      │
                            Syphon ◀── draw ◀── resolveScene ◀─────────┘
```

The port is a door on a resource hub: `sigil::io::Hub::feed()` on a
`udp://:27015` URI binds a dual-stack socket and takes every datagram
that reaches it on a thread of its own. Each front end drains that door
on its owner thread. Seer drains each observed wire once and shares arrivals
with raw inspection and its pinned receiver; `-[SCKEngine readArrivals]`
drains the native macOS frontend. Each hands accepted input to
`SceneSession`, which verifies and decodes a changed payload into an
`entt` registry. Malformed packets leave the current scene
intact. Byte-identical packets count toward the arrival rate without
decoding or invalidating the scene again.

Nothing is delivered onto the user interface thread from outside it: the
transport puts arrivals in the door and the frame takes them out, in
order. `sigil::io::Feed::receivedAt()` converts the arrival's timestamp onto the
steady clock, so arrival rates retain transport timing independently of
when the queue is drained. Replayed arrivals preserve their recorded spacing
relative to the first playback advance.

Binding is synchronous: when the door is asked for,
`sigil::io::Feed::error()` says whether the port was free and
`sigil::io::Feed::address()` says which one was bound, so `listening` and
the status line are right the moment `start()` returns. Closing the door
— dropping the last reference to it — stops its socket and discards what
it still holds.

`resolveScene()` then converts that registry into a `ResolvedScene` of
absolute native pixels, and `SceneRenderer::draw()` puts it on an
`SkCanvas`. The drawing order is edges, then circles with their curved
ring labels, then point labels, then boxes. The canvas clears to
transparent so downstream tools composite over their own background.

**`resolveScene()` is the only place scaling and point-on-circle math
happens.** Both front ends consume `ResolvedScene`, which holds nothing
but absolute pixels, so the two apps cannot drift apart on geometry. If
you are changing where something lands on screen, that function is where
the change belongs.

### Embedding the receiver

Link `SigilIOHub` and `SigilIOTransport` for the door, and
`SpellCircleDocument` for verified scene state. None of them depends on
Qt, AppKit, Skia, or a renderer. `SpellCircleScene` adds geometry
resolution and drawing.

```cpp
#include "SceneSession.h"
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/transport/Transport.h>
#include <iostream>

sigil::io::Hub hub;
sigil::io::registerUdp(hub);          // only UDP: the product speaks nothing else

const std::shared_ptr<sigil::io::Feed> door =
    hub.feed("udp://:27015", {.capacity = 64});
if (!door->error().empty()) std::cerr << door->error() << '\n';

spellcircle::SceneSession scene;
for (;;) {                            // once a frame, on the thread that draws
  while (const std::optional<sigil::io::Arrival> arrival = door->receive()) {
    scene.ingest(arrival->bytes->bytes.data(), arrival->bytes->bytes.size(),
                 door->receivedAt(*arrival));
  }
}
```

The transport runs the socket on a thread of its own and the host never
names it. `sigil::io::Feed::receive()` never waits: a frame that finds
nothing gets on with itself. The door keeps the last `capacity` arrivals,
so a reader that misses a frame loses nothing and one that falls a whole
second behind loses the oldest scenes rather than the newest —
`sigil::io::Feed::dropped()` counts those. `sigil::io::Arrival::from`
names the sender, spelled `udp://127.0.0.1:52341`; both front ends show
it with the scheme taken off.

`SceneSession` is synchronous and belongs to one owner thread — the one
that drains the door. The Qt renderer copies scene state during synchronization. A host
can also feed the session from anything else that produces bytes. Its
generation changes only when the accepted document changes or is cleared.
Arrival rates use the datagram's receive time, so a busy frame cannot
inflate them, and expire after two seconds of silence. Feed presentation
remains the host's choice. `SceneDocument::decode()` also verifies its
input when used directly and returns no statistics for an invalid payload.

## Layout

The Qt-free core is shared; the two front ends are not.

| Path | What it is |
| --- | --- |
| `src/spellcircle/shared/schema/` | `SpellCircle.fbs` and its generated header — the wire format |
| `src/spellcircle/shared/scene/` | `SpellCircleDocument`: verified ingestion and session state; `SpellCircleScene`: resolve, draw, ring-label geometry |
| `src/spellcircle/qt/` | Reusable scene models, canvas and graphics form hosted by Seer; the canvas uses the shared Qt publication adapter |
| `src/seer/app/` | The Qt application: wire inspection and the pinned scene receiver |
| `src/spellcircle/mac/` | `SpellCircleMac` — SwiftUI over an ObjC++ bridge, macOS only |

`SceneRenderer` is not thread-safe. It builds its font context lazily on
the first `draw()`, and every later `draw()` must come from that same
thread.

The Mac app is a separate executable rather than a Qt build. Its
`SpellCircleMacBridge` is a shared library that absorbs the entire C++
side — scene core, Skia, SigilWeave, ICU, HarfBuzz, Syphon — so the whole
of it links through the clang++ driver and the Swift executable links one
dylib.

Seer owns a session with one resource hub, a scene model and graphics
settings. Its QML root receives that session; raw inspection and scene
ingestion share each drained arrival. The Swift app's `EngineModel` owns one
`SCKEngine`. Both frontends reuse the verified document and scene renderer.

## Libraries

The app is thin. Most of the code is in libraries under `src/common/`,
`src/sigilweave/` and `src/sketch/`, each of which has its own README:

| Library | What it does |
| --- | --- |
| [SigilCore](src/common/core/README.md) | The kernels a retained runtime hosts: the reconciler, the caching proof, the hardware device seam, and the compute values a drawing is drawn from |
| [SigilSkia](src/common/skia/README.md) | Skia Graphite on a device someone else owns |
| [Ifrit.Qt](src/common/qt/README.md) | Reusable Qt Quick controls |
| [SigilImage](src/common/image/README.md) | Still-image and animated-image decoding and encoding, and signed distance fields over a coverage mask |
| [SigilVideo](src/common/video/README.md) | Streaming video decoding, GPU composition, and MP4 encoding |
| [SigilPublish](src/common/publish/README.md) | Native inter-application texture publication and subscription, plus the macOS Receiver monitor and capture tool |
| [SigilIO](src/common/io/README.md) | Resource access and export: URIs, mounts, caching, hot reload, byte sinks |
| [SigilData](src/common/data/README.md) | Tabular data and scales: typed columns, and the one value that maps a domain onto a range |
| [SigilScry](src/common/scry/README.md) | HTML and CSS rendered to Skia images |
| [SigilMeasure](src/common/measure/README.md) | Timing, statistics and check reporting |
| [SigilMotion](src/common/motion/README.md) | Animation: the clock, animatable values, bindings, and the physics steppers |
| [SigilGeometry](src/common/geometry/README.md) | Higher-level drawing over Skia: paths, contours, meshes, splines, point operators |
| [SigilMaterial](src/common/material/README.md) | Recipes, textures, environment maps, and colour: ramps, palettes, harmonies, dithering |
| [SigilDraw](src/common/draw/README.md) | An immediate-mode pen with p5's verbs |
| [SigilWorld](src/common/world/README.md) | 3D surfaces on Diligent Engine |
| [SigilSubstance](src/common/substance/README.md) | Adobe Substance 3D materials rendered to images, where the SDK is installed |
| [SigilUsd](src/common/usd/README.md) | OpenUSD read and write, where the package is installed |
| [SigilCompose](src/common/compose/README.md) | Data-driven drawable components — layout, caching, animation |
| [SigilSketch](src/sketch/README.md) | Every renderable thing as one sketch, with Sketchbook over them |
| [SigilWeave](src/sigilweave/README.md) | Text shaping and layout on HarfBuzz, ICU and Skia |

## Build and test

The build requires Python 3.12 or newer with development headers and an
embedding library. The dependency manifest supplies pybind11, and every
Sketchbook build supports Python sketches alongside C++ sketches.

```sh
cd apps/spell-circle-canvas
python3 scripts/sigil.py setup --config Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`sigil.py setup` finds Qt and vcpkg and writes the uncommitted
`CMakeUserPresets.json`. It is one of ten verbs over the build's
administration; `scripts/README.md` is the canon for all of them.

The test suite covers the libraries and the receiver layers:

- `spellcircle_document_test` checks accepted scene state, malformed
  input, deduplication, clearing, and receive-time arrival rates.
- `spellcircle_test` builds wire payloads and checks decode, resolution,
  box placement, and ring-label geometry.
- `seer_qt_test` checks shared trace/scene delivery, pinned-source independence,
  malformed and duplicate packets, rebind/stop/retry, recording replay, and
  settings import/save/cancel.
- `spellcircle_mac_test` does the same through the ObjC++ engine, down to
  the feed entry a changed scene appends and the source it names.

App presentation also needs a live run with incoming scenes.

Use a Release build for any performance work. Several library
benchmarks and sketches are deliberately stressful and Debug
timings say nothing useful. The benchmarks are not tests: `cmake --build
build --config Release --target benches` builds every `*_bench` binary,
and `scripts/sigil.py bench` runs them one at a time on a quiet machine
and judges each benchmark's median real time against the committed
`bench/baseline_<config>.json` (`--rebase` writes it, `--benches` picks a
subset; `mise run bench` wraps both steps).

### Changing the wire format

Edit `src/spellcircle/shared/schema/SpellCircle.fbs`. The two sides are
then handled differently.

The **C++ header** is generated into the build tree by the
`SpellCircleSchema` target, so the next build picks the edit up on its
own — there is nothing to run and nothing to commit.

The **Python modules** are committed, because `apps/python` is installed
and imported without a CMake build in reach. Run
`scripts/sigil.py flatbuffers` and commit what it writes:

- `apps/python/SpellCircle/{Vec2,Circle,Point,Edge,Box,Scene}.py`

### The SDKs that are not in vcpkg

Two dependencies are downloads behind an account, so no port can fetch
them: the Ultralight SDK (SigilScry) and the Adobe Substance 3D SDK
(SigilSubstance). Both libraries, and everything that links them, leave
the build when their SDK is absent.

An archive that cannot be fetched is put into the vcpkg asset cache
once per machine, and every configure after that resolves it locally:

```sh
scripts/sigil.py assets --stage <downloaded-archive>
```

The verb copies the archive into `~/.local/opt/vcpkg-assets/` under
the SHA-512 of its contents — the name vcpkg looks it up by — and prints
that hash. `sigil.py setup` writes the cache into the `vcpkg` preset's
environment as the one asset source, consulted before the network and
written back to, so every other dependency still downloads normally.

Where each SDK has to go until its port exists is in
[SigilScry's README](src/common/scry/README.md) and
[SigilSubstance's](src/common/substance/README.md).

### Demo assets

Several library examples reproduce real reference designs, and a
reference typeset in whatever face the host happens to ship is only half
a reference. One verb fetches the open-licensed ones, and the build reaches
the same verb through an opt-in target:

```sh
scripts/sigil.py assets
cmake --build build --config Release --target fetch_assets
```

They land in `build/assets/` (gitignored), reach code as the
`SIGIL_ASSET_DIR` compile definition, and are also accepted directly by
tools that take `--assets <dir>`. Nothing here runs during a normal
build, and configuring the project never touches the network. The verb
writes files and nothing else, so it runs on a fresh checkout with no
build tree.

The manifest is `scripts/sigil/assets.py`.
Anything added to it carries an open licence with its licence file
fetched alongside, is pinned to an immutable commit rather than a branch,
and declares a sha256 so a changed byte is a hard failure.
