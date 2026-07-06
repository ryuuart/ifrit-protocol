# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Configure (first time or after CMakeLists changes):
```
cmake --preset main
```

Build (Debug by default):
```
cmake --build build --config Debug
```

Run:
```
./build/bin/Debug/SpellCircle
```

The `main` preset (`CMakeUserPresets.json`) inherits `vcpkg` + `qt` + `ninja` presets. It assumes Qt 6.11.1 at `~/.local/opt/Qt/6.11.1/macos` and vcpkg at `~/.local/share/vcpkg`.

## FlatBuffers code generation

After editing `src/network/SpellCircle.fbs`, regenerate both the C++ header and the Python bindings:
```
flatc --cpp -o src/network/include src/network/SpellCircle.fbs
flatc --python -o scripts src/network/SpellCircle.fbs
```

## Testing with the Python scripts

Send a UDP test packet to the running app:
```
python scripts/send_spell_circles.py [--host 127.0.0.1] [--port 27015] [--seed N] [--canvas UNITS] [--circles N] [--boxes N]
```

`--canvas` sets the authoring coordinate space (default 4000); the app scales it to the native 4K texture automatically. `--seed` seeds the RNG for a reproducible layout (omit for random). Requires `pip install flatbuffers`. The Python FlatBuffers bindings live in `scripts/SpellCircle/`.

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
- `Models` (`src/network/include/Models.h`) — QML singleton (`Models.spellCircleModel`) that wires `NetworkManager` → `SpellCircleModel`.
- `SpellCircleModel` (`src/network/include/SpellCircleModel.h`) — parses FlatBuffers scenes, scales geometry from the author canvas (via `Scene.width`/`height`) to `canvasSize()` (set from `GraphicsConfig::canvasSize`, default 4000), stores resolved geometry, and exposes a timestamped log feed for the sidebar. Changing the canvas size discards existing geometry (it was scaled into the old size) but leaves the feed log intact.
- `SpellCircleRenderer` (`src/qml/SpellCircle/`) — render-thread object; `synchronize()` copies circle geometry and `GraphicsConfig` style values (including `canvasSize`) from the main thread; `prePaint()` draws to a `QCanvasOffscreenCanvas` sized to `canvasSize`, discarding and recreating it whenever that size changes; `paint()` blits it (with mipmaps) into the DPI-scaled preview; `render()` publishes the texture to Syphon.
- `TextPathPainter` (`src/qml/SpellCircle/include/TextPathPainter.h`) — places each glyph tangent to a `QPainterPath` at a fractional arc-length offset; caches per-character advance widths.
- `SyphonBridge` (`src/qml/SpellCircle/SyphonBridge.mm`) — ObjC++ wrapper around `SyphonMetalServer`; keep ObjC out of headers by using a pimpl (`SyphonBridgePrivate`).

**UI (`src/Main.qml`):**
- Collapsible sidebar shows the `SpellCircleModel` log feed.
- Main area is a zoomable/pannable viewport (sized to `Models.graphicsConfig.canvasSize`, default 4000×4000) with a checkerboard transparency background. Supports mouse-wheel zoom, two-finger trackpad scroll (pan) or ⌘/⌃ + scroll (zoom), and native macOS pinch-to-zoom.
- The `SpellCircle` QML element is placed inside the canvas and sized to match it.
- "Reset" button calls `Models.spellCircleModel.clear()`.

## Network protocol

UDP port **27015** (default). Each datagram is a FlatBuffers-encoded `SpellCircle.Scene` (defined in `src/network/SpellCircle.fbs`). The verifier in `NetworkManager::onReadyRead` drops malformed packets before they reach the model.
