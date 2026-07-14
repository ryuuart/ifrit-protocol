# Repository guide

## Layout

```
apps/spell-circle-canvas/  Qt/C++ receiver, renderer, gallery, and TextFlow
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
`TextFlowGallery`, `textflow_test`, `textflow_bench`, and `textflow_demo`.

## FlatBuffers generation

From the repository root:

```sh
flatc --cpp -o apps/spell-circle-canvas/src/network/include \
  apps/spell-circle-canvas/src/network/SpellCircle.fbs
flatc --python -o apps/python \
  apps/spell-circle-canvas/src/network/SpellCircle.fbs
```

## Architecture

Incoming UDP datagrams flow through `NetworkManager` verification into
`SpellCircleModel`, which stores the decoded scene in an `entt::registry`.
`SpellCircleRenderer` resolves model entities to native canvas coordinates and
delegates drawing to one of two `CanvasSceneBackend` implementations:

- `SkiaSceneBackend` draws with Graphite on Qt's Metal device and uses TextFlow.
- `QCanvasPainterSceneBackend` is the always-available fallback.

Both render to a native-size `QCanvasOffscreenCanvas`; `SyphonBridge` publishes
that Metal texture. The visible QML item blits the registered image and handles
zoom and pan independently of scene rerendering.

The main QML lives under `src/qml/app/`. Reusable controls live in its
`components/` directory. The `SpellCircle.Canvas` module and its C++ backends
live under `src/qml/SpellCircle/`.

TextFlow is a Qt-independent library under `src/text/`. Its main pipeline is:

```
Paragraph -> ICU analysis -> cached HarfBuzz words -> FlowGeometry
          -> LineBreakStrategy -> ParagraphLayout -> draw/drawBatched
```

`ParagraphLayoutOptions` groups line metrics, hyphenation, justification,
Knuth–Plass, overflow, and path-rendering concerns. SpellCircle-specific ring
label geometry belongs in `src/qml/SpellCircle/SceneLabels.*`; the core exposes
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
