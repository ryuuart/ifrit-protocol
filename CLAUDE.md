# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

## Repository layout

```
apps/spell-circle/   — Qt/C++ app (builds, renders, and publishes spell circles)
apps/python/
  SpellCircle/       — Python package (FlatBuffers bindings + canvas/network API)
    test/            — Demo/test scripts that send scenes over UDP
```

## Build (apps/spell-circle)

Configure (first time or after CMakeLists changes):
```
cd apps/spell-circle
cmake --preset main
```

Build (Debug by default):
```
cmake --build apps/spell-circle/build --config Debug
```

Run:
```
apps/spell-circle/build/bin/Debug/SpellCircle
```

The `main` preset (`CMakeUserPresets.json`) inherits `vcpkg` + `qt` + `ninja` presets. It assumes Qt 6.11.1 at `~/.local/opt/Qt/6.11.1/macos` and vcpkg at `~/.local/share/vcpkg`.

## FlatBuffers code generation

After editing `apps/spell-circle/src/network/SpellCircle.fbs`, regenerate both the C++ header and the Python bindings:
```
flatc --cpp -o apps/spell-circle/src/network/include apps/spell-circle/src/network/SpellCircle.fbs
flatc --python -o apps/python apps/spell-circle/src/network/SpellCircle.fbs
```

The Python bindings land in `apps/python/SpellCircle/` (the generated `Box.py`, `Circle.py`, etc.).

## Testing with the Python scripts

Send a single UDP test packet to the running app:
```
python apps/python/SpellCircle/test/send_spell_circles.py [--host 127.0.0.1] [--port 27015] [--seed N] [--canvas UNITS] [--circles N] [--boxes N]
```

Stream an animated scene at ~60 fps:
```
python apps/python/SpellCircle/test/animate_spell_circles.py [--host 127.0.0.1] [--port 27015] [--fps N]
```

`--canvas` sets the authoring coordinate space (default 4000); the app scales it to the native 4K texture automatically. `--seed` seeds the RNG for a reproducible layout (omit for random). Requires `pip install flatbuffers`.

## Python SpellCircle package

`apps/python/SpellCircle/` is the Python package for authoring and sending scenes:

| Module | Purpose |
|---|---|
| `canvas.py` | `SCCanvas` — records circles, points, edges, boxes; serializes on demand |
| `builder.py` | `SCBuilder` — encodes scene data into a FlatBuffers `Scene` |
| `network.py` | `SCSender`, `send_once` — UDP transport |

`__init__.py` re-exports the full public API, so scripts can do `from SpellCircle import SCCanvas, SCSender, send_once`.

## Architecture

The app receives spell-circle scene data over UDP, renders it to an offscreen canvas using Qt CanvasPainter, and publishes the result to other apps via Syphon (macOS Metal texture sharing).

**Data flow:**
```
UDP packet (FlatBuffers Scene)
  → NetworkManager         (verifies + emits signal)
  → SpellCircleModel       (scales geometry to native coords, stores CircleGeometry, appends feed entries)
  → SpellCircle (QML item) → SpellCircleRenderer (render thread)
      → draws circles + text-along-path labels onto 4K offscreen canvas
      → publishes Metal texture (with mipmaps) to Syphon server "SpellCircle"
```

**CMake targets / QML modules:**

| Target | QML URI | What it is |
|---|---|---|
| `SpellCircleNetwork` (shared lib) | — | `NetworkManager` + `SpellCircleModel` |
| `SpellCircleModels` (QML module) | `SpellCircle.Models` | `Models` QML singleton (owns network + model) |
| `SpellCircleCanvas` (QML module) | `SpellCircle.Canvas` | `SpellCircle` item + renderer + `TextPathPainter` + `SyphonBridge` |
| `SpellCircle` (app executable) | `qtquick1` | `main.cpp`, `Main.qml`, `SquareButton.qml` |

**Key classes:**
- `Models` (`apps/spell-circle/src/network/include/Models.h`) — QML singleton (`Models.spellCircleModel`) that wires `NetworkManager` → `SpellCircleModel`.
- `SpellCircleModel` (`apps/spell-circle/src/network/include/SpellCircleModel.h`) — parses FlatBuffers scenes, scales geometry from the author canvas (via `Scene.width`/`height`) to `canvasSize()` (set from `GraphicsConfig::canvasSize`, default 4000), stores resolved geometry, and exposes a timestamped log feed for the sidebar. Changing the canvas size discards existing geometry (it was scaled into the old size) but leaves the feed log intact.
- `SpellCircleRenderer` (`apps/spell-circle/src/qml/SpellCircle/`) — render-thread object; `synchronize()` copies circle geometry and `GraphicsConfig` style values (including `canvasSize`) from the main thread; `prePaint()` draws to a `QCanvasOffscreenCanvas` sized to `canvasSize`, discarding and recreating it whenever that size changes; `paint()` blits it (with mipmaps) into the DPI-scaled preview; `render()` publishes the texture to Syphon.
- `TextPathPainter` (`apps/spell-circle/src/qml/SpellCircle/include/TextPathPainter.h`) — places each glyph tangent to a `QPainterPath` at a fractional arc-length offset; caches per-character advance widths.
- `SyphonBridge` (`apps/spell-circle/src/qml/SpellCircle/SyphonBridge.mm`) — ObjC++ wrapper around `SyphonMetalServer`; keep ObjC out of headers by using a pimpl (`SyphonBridgePrivate`).

**UI (`apps/spell-circle/src/Main.qml`):**
- Collapsible sidebar shows the `SpellCircleModel` log feed.
- Main area is a zoomable/pannable viewport (sized to `Models.graphicsConfig.canvasSize`, default 4000×4000) with a checkerboard transparency background. Supports mouse-wheel zoom, two-finger trackpad scroll (pan) or ⌘/⌃ + scroll (zoom), and native macOS pinch-to-zoom.
- The `SpellCircle` QML element is placed inside the canvas and sized to match it.
- "Reset" button calls `Models.spellCircleModel.clear()`.

## Network protocol

UDP port **27015** (default). Each datagram is a FlatBuffers-encoded `SpellCircle.Scene` (defined in `apps/spell-circle/src/network/SpellCircle.fbs`). The verifier in `NetworkManager::onReadyRead` drops malformed packets before they reach the model.
