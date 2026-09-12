# SpellCircle

SpellCircle draws network-driven vector diagrams for live production. An
external process — a Python script, a TouchDesigner patch — describes a
scene of circles, points placed on those circles' perimeters, edges
between points, and labelled boxes, and sends it as a FlatBuffers
datagram over UDP. SpellCircle receives it, draws it with Skia on the
GPU, and publishes the result as a transparent-background texture over
Syphon, where a VJ or compositing tool picks it up.

The app only receives. Nothing is authored inside it. Scenes typically
arrive at animation frame rates, so the sender is free to treat it as a
live output surface rather than a document viewer.

What you get on screen is a viewer and a control surface: a canvas you
pan and zoom, a timestamped feed of arriving packets, a packets-per-second
readout, and settings for accent colour, stroke width, scale, fonts, box
geometry, label offsets, canvas size, and the UDP port. Panning and
zooming never redraw the scene — they move an already-rendered image.

## Getting a picture on screen

Build (see [Build and test](#build-and-test)), start the app, then send
it something:

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
Python  ──FlatBuffers──▶  UDP :27015  ──▶  verify  ──▶  decode
                                                          │
                        Syphon ◀── draw ◀── resolveScene ◀─┘
```

`UdpReceiver` binds dual-stack on a Boost.Asio executor supplied by its
host. It delivers each payload with its source and monotonic receive time.
The front end moves to its main thread, where `SceneSession` verifies and
decodes changed payloads into an `entt` registry. Malformed packets leave
the current scene intact. Byte-identical packets count toward the arrival
rate without decoding or invalidating the scene again.

Binding is asynchronous. Status callbacks report the actual bound port,
bind errors, and terminal receive errors. A stop or rebind retires the old
binding's callbacks immediately; the front ends also discard deliveries
already queued for an obsolete binding. Socket operations and callbacks
are serialized even when several threads run the supplied context.

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

Link `SpellCircleNet` for UDP transport and `SpellCircleDocument` for
verified scene state. Neither target depends on Qt, AppKit, Skia, or a
renderer. `SpellCircleScene` adds geometry resolution and drawing.

```cpp
#include "SceneSession.h"
#include "UdpReceiver.h"
#include <boost/asio/io_context.hpp>
#include <iostream>

boost::asio::io_context context;
spellcircle::SceneSession scene;
spellcircle::UdpReceiver receiver(context.get_executor());
receiver.start(
    27015,
    [&](spellcircle::Datagram packet) {
      scene.ingest(packet.payload.data(), packet.payload.size(),
                   packet.receivedAt);
    },
    [](spellcircle::UdpReceiver::Status status) {
      if (status.error) std::cerr << status.error.message() << '\n';
    });
context.run();
```

An existing host passes its executor and continues running its context.
The receiver creates no thread and never runs, restarts, or stops that
context. Stop and destruction wait for an executing callback to finish,
except when invoked by that callback, then queue socket cancellation.
They do not wait for the event loop to run. Keep the context alive until
the receiver is destroyed; drain or destroy the context to release queued
operations. Callbacks must not wait for a thread that is stopping or
rebinding their receiver.

`SceneSession` is synchronous and belongs to one owner thread. In the
example it belongs to the network callback; the two apps instead dispatch
packets to the UI thread before ingesting them. A host can also feed the
session directly from another transport. Its generation changes only
when the accepted document changes or is cleared. Arrival rates use the
packet's receive time, so a busy UI queue cannot inflate them, and expire
after two seconds of silence. Feed presentation remains the host's choice.
`SceneDocument::decode()` also verifies its input when used directly and
returns no statistics for an invalid payload.

## Layout

The Qt-free core is shared; the two front ends are not.

| Path | What it is |
| --- | --- |
| `src/spellcircle/shared/schema/` | `SpellCircle.fbs` and its generated header — the wire format |
| `src/spellcircle/shared/net/` | Executor-supplied `UdpReceiver`, datagrams, binding status |
| `src/spellcircle/shared/scene/` | `SpellCircleDocument`: verified ingestion and session state; `SpellCircleScene`: resolve, draw, ring-label geometry |
| `src/spellcircle/qt/` | The Qt app — QML front end, cross-platform target |
| `src/spellcircle/mac/` | `SpellCircleMac` — SwiftUI over an ObjC++ bridge, macOS only |

`SceneRenderer` is not thread-safe. It builds its font context lazily on
the first `draw()`, and every later `draw()` must come from that same
thread.

The Mac app is a separate executable rather than a Qt build. Its
`SpellCircleMacBridge` is a shared library that absorbs the entire C++
side — scene core, Skia, SigilWeave, ICU, HarfBuzz, Syphon — so the whole
of it links through the clang++ driver and the Swift executable links one
dylib.

The Qt executable creates the network context and injects `Models` into
the QML root. The Swift app creates `SCKNetworkRuntime` and passes it to
its engine; multiple engines can share that runtime. An ObjC++ host can
wrap an existing Boost executor through `SCKNetworkRuntimeInternal.h`
without giving the runtime ownership of the external context.

## Libraries

The app is thin. Most of the code is in libraries under `src/common/`,
`src/sigilweave/` and `src/sketch/`, each of which has its own README:

| Library | What it does |
| --- | --- |
| [SigilCore](src/common/core/README.md) | The kernels a retained runtime hosts: the reconciler, the caching proof, the hardware device seam, and the compute values a drawing is drawn from |
| [SigilSkia](src/common/skia/README.md) | Skia Graphite on a device someone else owns |
| [Ifrit.Ui](src/common/ui/README.md) | Reusable Qt Quick controls |
| [SigilImage](src/common/image/README.md) | Still-image and animated-image decoding and encoding, and signed distance fields over a coverage mask |
| [SigilVideo](src/common/video/README.md) | Streaming video decoding, GPU composition, and MP4 encoding |
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

```sh
cd apps/spell-circle-canvas
python3 scripts/sigil.py setup --config Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`sigil.py setup` finds Qt and vcpkg and writes the uncommitted
`CMakeUserPresets.json`. It is one of nine verbs over the build's
administration; `scripts/README.md` is the canon for all of them.

The test suite covers the libraries and the receiver layers:

- `spellcircle_net_test` uses loopback UDP to exercise shared contexts,
  cancellation, rebinding, callback teardown, and concurrent controls.
- `spellcircle_document_test` checks accepted scene state, malformed
  input, deduplication, clearing, and receive-time arrival rates.
- `spellcircle_test` builds wire payloads and checks decode, resolution,
  box placement, and ring-label geometry.
- `spellcircle_qt_test` checks asynchronous status and queued-delivery
  cancellation through the Qt adapter and its scene model.
- `spellcircle_mac_test` checks main-queue status cancellation and the
  lifetime of an externally supplied runtime through the ObjC++ adapter.

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
