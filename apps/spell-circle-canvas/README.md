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

`UdpReceiver` binds dual-stack and hands each datagram to the front end
on its own I/O thread. The front end moves to its main thread,
`verifyScenePayload()` rejects malformed buffers, and
`SceneDocument::decode()` fills an `entt` registry with circle, point,
edge, and box components.

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

## Layout

The Qt-free core is shared; the two front ends are not.

| Path | What it is |
| --- | --- |
| `src/spellcircle/shared/schema/` | `SpellCircle.fbs` and its generated header — the wire format |
| `src/spellcircle/shared/net/` | `UdpReceiver`. Transport only; callers verify |
| `src/spellcircle/shared/scene/` | Decode, resolve, draw, ring-label geometry |
| `src/spellcircle/qt/` | The Qt app — QML front end, cross-platform target |
| `src/spellcircle/mac/` | `SpellCircleMac` — SwiftUI over an ObjC++ bridge, macOS only |

`SceneRenderer` is not thread-safe. It builds its font context lazily on
the first `draw()`, and every later `draw()` must come from that same
thread.

The Mac app is a separate executable rather than a Qt build. Its
`SpellCircleMacBridge` is a shared library that absorbs the entire C++
side, because Swift's linker rejects the raw `-framework` flags that
Skia's package config carries in its link interface; keeping those on the
clang++ side of a dylib boundary is what makes the Swift target link at
all.

## Libraries

The app is thin. Most of the code is in libraries under `src/common/` and
`src/sigilweave/`, each of which has its own README:

| Library | What it does |
| --- | --- |
| [SigilWeave](src/sigilweave/README.md) | Text shaping and layout on HarfBuzz, ICU and Skia |
| [SigilCompose](src/common/compose/README.md) | Data-driven drawable components — layout, caching, animation |
| [SigilGeometry](src/common/geometry/README.md) | Higher-level drawing over Skia: geometry, curves, materials |
| [SigilWorld](src/common/world/README.md) | 3D surfaces on Diligent Engine |
| [SigilMotion](src/common/motion/README.md) | Animation clock and animatable values |
| [SigilImage](src/common/image/README.md) | Image decoding and probing |
| [SigilLoader](src/common/loader/README.md) | Resource access: URIs, mounts, caching, hot reload |
| [SigilScry](src/common/scry/README.md) | HTML and CSS rendered to Skia images |

## Build and test

```sh
cd apps/spell-circle-canvas
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

`setup.py` finds Qt and vcpkg and writes the uncommitted
`CMakeUserPresets.json`.

The test suite covers the libraries and the shared scene core —
`spellcircle_test` builds wire payloads with the FlatBuffers API and runs
them through decode, resolution, box placement, and ring-label geometry.
The two front ends themselves have no automated tests: verifying a change
to app code means running it and sending it a scene.

Use a Release build for any performance work. Several library
benchmarks and sketches are deliberately stressful and Debug
timings say nothing useful. The benchmarks are not tests: `cmake --build
build --config Release --target benches` builds every `*_bench` binary,
and `scripts/bench_ledger.py` runs them one at a time on a quiet machine
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
`scripts/regen_flatbuffers.sh` from anywhere and commit what it writes:

- `apps/python/SpellCircle/{Vec2,Circle,Point,Edge,Box,Scene}.py`

### Demo assets

Several library examples reproduce real reference designs, and a
reference typeset in whatever face the host happens to ship is only half
a reference. An opt-in target fetches the open-licensed ones:

```sh
cmake --build build --config Release --target fetch_assets
```

They land in `build/assets/` (gitignored), reach code as the
`SIGIL_ASSET_DIR` compile definition, and are also accepted directly by
tools that take `--assets <dir>`. Nothing here runs during a normal
build, and configuring the project never touches the network.

The manifest is [`cmake/FetchAssets.cmake`](cmake/FetchAssets.cmake).
Anything added to it carries an open licence with its licence file
fetched alongside, is pinned to an immutable commit rather than a branch,
and declares an `EXPECTED_HASH` so a changed byte is a hard failure.
