# Repository guide

## Layout

```
apps/spell-circle-canvas/  Qt/C++ receiver, renderer, gallery, TextFlow, and
                           the native macOS SwiftUI app (src/mac)
apps/python/               Python scene-authoring and UDP transport package
touchdesigner/             TouchDesigner project and editor tooling
```

Generated files are `apps/spell-circle-canvas/src/network/include/SpellCircle_generated.h`
and the schema modules `apps/python/SpellCircle/{Box,Circle,Edge,Point,Scene,Vec2}.py`.
Regenerate them after editing `SpellCircle.fbs`; do not hand-edit them.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The setup script discovers Qt 6.11+ and vcpkg, then writes the uncommitted
`CMakeUserPresets.json`. The primary executables are `SpellCircle`,
`SpellCircleMac` (macOS only, needs a Swift toolchain), `TextFlowGallery`,
`textflow_test`, `textflow_bench`, and `textflow_demo`.

## FlatBuffers generation

Run `apps/spell-circle-canvas/scripts/regen_flatbuffers.sh` (from anywhere)
after editing `SpellCircle.fbs`, then commit the regenerated files. It wraps:

```sh
flatc --cpp -o apps/spell-circle-canvas/src/network/include \
  apps/spell-circle-canvas/src/network/SpellCircle.fbs
flatc --python -o apps/python \
  apps/spell-circle-canvas/src/network/SpellCircle.fbs
```

## Architecture

The Qt-free scene core under `src/scene/` (`SpellCircleScene`) is shared by
both receiver apps: `SceneModel` (FlatBuffers verify/decode into an
`entt::registry` of components), `SceneGeometry` (`resolveScene()` —
author-space to native-canvas resolution, the only place scaling and
point-on-circle math happens), `SceneRenderer` (the Skia/TextFlow scene
drawing), and `SceneLabels` (measured ring-label geometry). The Graphite GPU
plumbing under `src/platform/skia/` is split the same way: `SpellCircleSkia`
(Qt-free; Metal bring-up from raw handles) and `SpellCircleSkiaQt` (QRhi
adapters; also the Vulkan draft TUs).

In the Qt app, incoming UDP datagrams flow through `NetworkManager`
verification into `SpellCircleModel`, which holds the decoded
`spellcircle::SceneDocument`. `SpellCircleRenderer` resolves it via
`resolveScene()` and delegates drawing to one of two `CanvasSceneBackend`
implementations:

- `SkiaSceneBackend` — the Qt frame around the shared
  `spellcircle::SceneRenderer`, drawing with Graphite on Qt's native GPU
  device (Metal on macOS; Vulkan draft elsewhere).
- `QCanvasPainterSceneBackend` is the always-available fallback.

Both render to a native-size `QCanvasOffscreenCanvas`; a `TexturePublisher`
(`SyphonBridge` on macOS/Metal, `SpoutBridge` draft on Windows/D3D11)
publishes that texture. The visible QML item blits the registered image and
handles zoom and pan independently of scene rerendering. The Windows paths
are untested bring-up drafts until the Windows port lands.

The main QML lives under `src/qml/app/`. Reusable controls live in its
`components/` directory. The `SpellCircle.Canvas` module and its C++ backends
live under `src/qml/SpellCircle/`.

`SpellCircleMac` (`src/mac/`) is the native macOS companion app — a separate
executable, not a Qt build: SwiftUI Liquid Glass chrome over an ObjC++ bridge
(`SpellCircleMacBridge`, a SHARED library that absorbs the C++ world —
Swift's linker rejects vcpkg's raw `-framework` interface flags). `SCKEngine`
receives UDP on a GCD source, decodes/resolves/draws through the same shared
core into an offscreen Metal texture, publishes it over Syphon under the same
"SpellCircle" server name, and blits into `SCKCanvasView`'s CAMetalLayer
(pan/zoom, event-driven redraws). Swift imports the bridge via
`bridge/module.modulemap`; the Qt app remains the cross-platform build.

TextFlow is a Qt-independent library under `src/text/`. Its main pipeline is:

```
Paragraph -> ICU analysis -> cached HarfBuzz words -> FlowGeometry
          -> LineBreakStrategy -> ParagraphLayout -> draw/drawBatched
```

`ParagraphLayoutOptions` groups line metrics, hyphenation, justification,
Knuth–Plass, overflow, and path-rendering concerns. SpellCircle-specific ring
label geometry belongs in `src/scene/SceneLabels.*`; the core exposes
only reusable `SingleLineParagraphCache` and `layoutSingleLine()` support.

## Python API

`apps/python/SpellCircle` exposes:

- `SpellCircleCanvas` and `PointReference` for scene authoring;
- `CircleDefinition` and `SceneBuilder` for lower-level serialization;
- `SceneSender` and `send_once` for UDP transport.

Example scripts are under `apps/python/SpellCircle/test/`:

```sh
python3 apps/python/SpellCircle/test/send_spell_circles.py --seed 1
python3 apps/python/SpellCircle/test/animate_spell_circles.py --fps 60
```

## TouchDesigner

`touchdesigner/scripts/configure_editors.py` creates/synchronizes the local
TouchDesigner-compatible virtual environment and writes machine-local VS Code
settings. The generated environment and editor files are ignored by Git.
