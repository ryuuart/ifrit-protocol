# Repository guide

## Layout

```
apps/spell-circle-canvas/  All C++/Swift code; src/ splits into:
  src/common/              shared libraries: skia/ (Graphite GPU plumbing),
                           ui/ (Ifrit.Ui Qt Quick controls), image/
                           (SigilImage — PNG/JPEG/WebP/GIF/AVIF import,
                           stills + animations, for canvas drawing), web/
                           (IfritWeb — Ultralight HTML/CSS layout rendered
                           to SkImage frames for the canvases; GPU via a
                           Metal GPUDriver, CPU fallback), tick/
                           (SigilTick — FrameClock + Ticker driving
                           choreograph timelines, event-driven redraw
                           contract), compose/ (SigilCompose —
                           data-driven drawable components over
                           Yoga+SigilWeave+Choreograph: phase-1 kernel
                           implemented — see DESIGN.md / API.md /
                           STRESS_TESTS.md for architecture, surface,
                           and measured numbers)
  src/sigilweave/            the SigilWeave layout engine + kit/ports/qt/shaders,
                           test/, bench/, and examples/{gallery,demo}
  src/spellcircle/         the receiver product: shared/{schema,net,scene}
                           core embedded by qt/ (Qt app) and mac/ (SwiftUI)
apps/python/               Python scene-authoring and UDP transport package
touchdesigner/             TouchDesigner project and editor tooling
```

Qt executables keep their own `src/`, `include/`, and `qml/` folders
(`spellcircle/qt/`, `sigilweave/examples/gallery/`).

Generated files are
`apps/spell-circle-canvas/src/spellcircle/shared/schema/include/SpellCircle_generated.h`
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
`CMakeUserPresets.json`. Custom ports (currently `choreograph`) come from
the sigil-vcpkg-registry via `vcpkg-configuration.json` — note its
`repository` currently points at the local checkout
`/Users/long/REI/sigil-vcpkg-registry`; update the URL and baseline when
that registry is pushed to GitHub (workflow documented in the registry's
README). The primary executables are `SpellCircle`,
`SpellCircleMac` (macOS only, needs a Swift toolchain), `WeaveGallery`,
`weave_test`, `weave_bench`, `weave_demo`, `ifritweb_demo`
(CPU/lockstep), `ifritweb_gpu_demo` (Metal + Graphite), and `web_bench`
(IfritWeb path costs; plain = CPU engine, `--gpu` = GPU engine — see the
performance table in `src/common/web/README.md`), `compose_test`,
`compose_bench`, and `compose_demo` (headless PNG panels of the
compose stress catalog).

The Ultralight SDK is required for IfritWeb;
`SPELLCIRCLE_ENABLE_ULTRALIGHT` auto-disables with a warning when it's
missing. Installation (headers/dylibs to `/usr/local`, the dylib
re-signing step, and the runtime-resource locations) is documented in
`src/common/web/README.md`. Executables that link `IfritWeb` call
`ultralight_copy_resources(<target>)` so each build stages the resources
next to the binary, which is where the engine looks first at runtime.

## FlatBuffers generation

Run `apps/spell-circle-canvas/scripts/regen_flatbuffers.sh` (from anywhere)
after editing `SpellCircle.fbs`, then commit the regenerated files. It wraps:

```sh
flatc --cpp -o apps/spell-circle-canvas/src/spellcircle/shared/schema/include \
  apps/spell-circle-canvas/src/spellcircle/shared/schema/SpellCircle.fbs
flatc --python -o apps/python \
  apps/spell-circle-canvas/src/spellcircle/shared/schema/SpellCircle.fbs
```

## Architecture

The Qt-free scene core under `src/spellcircle/shared/scene/`
(`SpellCircleScene`) is shared by both receiver apps: `SceneModel`
(FlatBuffers verify/decode into an `entt::registry` of components),
`SceneGeometry` (`resolveScene()` — author-space to native-canvas
resolution, the only place scaling and point-on-circle math happens),
`SceneRenderer` (the Skia/SigilWeave scene drawing), and `SceneLabels`
(measured ring-label geometry). The Graphite GPU plumbing under
`src/common/skia/` is split the same way: `SpellCircleSkia` (Qt-free; Metal
bring-up from raw handles) and `SpellCircleSkiaQt` (QRhi adapters; also the
Vulkan draft TUs).

In the Qt app (`src/spellcircle/qt/`), incoming UDP datagrams flow through
`NetworkManager` verification into `SpellCircleModel`, which holds the
decoded `spellcircle::SceneDocument`. `SpellCircleRenderer` resolves it via
`resolveScene()` and draws through `SkiaSceneBackend` — the Qt frame around
the shared `spellcircle::SceneRenderer`, drawing with Graphite on Qt's
native GPU device (Metal on macOS; Vulkan draft elsewhere). Without a Skia
backend (stub build or unsupported RHI) the canvas renders no scene content
— the former QCanvasPainter fallback is gone.

The backend renders to a native-size `QCanvasOffscreenCanvas`; a
`TexturePublisher` (`SyphonBridge` on macOS/Metal, `SpoutBridge` draft on
Windows/D3D11) publishes that texture. The visible QML item blits the
registered image and handles zoom and pan independently of scene
rerendering. The Windows paths are untested bring-up drafts until the
Windows port lands.

The Qt app's QML lives under `src/spellcircle/qt/qml/` (components in its
`components/` directory); its C++ sources and headers under
`src/spellcircle/qt/{src,include}/` build the `SpellCircle.Canvas` and
`SpellCircle.Models` modules and the executable. Reusable Qt Quick controls
live in `src/common/ui/` (`Ifrit.Ui`).

`SpellCircleMac` (`src/spellcircle/mac/`) is the native macOS companion app — a separate
executable, not a Qt build: SwiftUI Liquid Glass chrome over an ObjC++ bridge
(`SpellCircleMacBridge`, a SHARED library that absorbs the C++ world —
Swift's linker rejects vcpkg's raw `-framework` interface flags). `SCKEngine`
receives UDP on a GCD source, decodes/resolves/draws through the same shared
core into an offscreen Metal texture, publishes it over Syphon under the same
"SpellCircle" server name, and blits into `SCKCanvasView`'s CAMetalLayer
(pan/zoom, event-driven redraws). Swift imports the bridge via
`bridge/module.modulemap`; the Qt app remains the cross-platform build.

IfritWeb (`src/common/web/`, namespace `ifrit::web`) renders HTML/CSS/JS
through the Ultralight SDK (WebKit-derived) for advanced layout on the
scene canvases. `WebEngine` owns the single-per-process
`ultralight::Renderer` on a dedicated web thread; each `WebView` is an
offscreen page. With `WebEngineConfig::metalDevice/metalCommandQueue` set
(pass the same pair the Graphite context shares), rendering is
hardware-accelerated: `UltralightMetalDriver`
(`src/common/web/metal/`, executing the SDK's stock Metal shaders,
vendored as `UltralightShaders.metal`) runs Ultralight's command lists
into MTLTextures, publishes each repaint by blitting into per-view
ping-pong textures, and `WebView::frame(recorder)` /
`WebView::draw()` wrap the published texture zero-copy as a
Graphite-backed SkImage — everything rides one MTLCommandQueue, so
ordering is implicit. Without a device the CPU renderer publishes
immutable raster SkImages instead (Ultralight paints straight into
SkBitmap memory via a custom `ultralight::SurfaceFactory`); the API is
identical across modes. Integration paths: pull
(`frame(recorder)`/`frameVersion()`), push (`setFrameCallback()`),
or lockstep (`threaded=false` + `update()`/`renderFrame()` from the host
loop, with zero-copy `peekPixels()` in CPU mode). Compositing also runs
the other way: `WebEngine::createImage()` returns a `WebImage` pages
display as `<img src="<name>.imgsrc">`; `WebImage::paint(callback)` is
the safe runtime-collaboration path (canvas handed in, GPU flush +
invalidate atomic, on the web thread — the engine's Metal driver keeps
its own Graphite recorder for this), with raster `update()`, `updateTexture()`, and raw
`nativeTexture()` as alternatives, and unregistered slot names log a
warning (both directions covered by round-trip pixel tests in web_test /
web_gpu_test). A custom FileSystem maps the `resources/` prefix to the
SDK runtime data, synthesizes the `.imgsrc` indirection files, and
resolves other paths against `WebEngineConfig::fileSystemDir`; loadHTML
pages get a `file:///` base URL so those resources are reachable. Engine shutdown must purge WebCore
caches before destroying the renderer (see `threadMain`) — GPU glyph
textures otherwise dangle into pthread TSD cleanup. The engine internals are
backend-neutral: everything codes against `WebGpuDriver`
(`src/common/web/WebGpuDriver.h`), with `UltralightMetalDriver` as the
Metal implementation and one factory seam in `setupPlatform` — the
Windows/Linux ports add a Vulkan/D3D driver there alongside the repo's
other Vulkan draft targets. Sharing an SkCanvas directly is not
architecturally possible (Ultralight records its own render passes), so
texture-backed SkImage is the supported compositing model.

SigilWeave is a Qt-independent library under `src/sigilweave/`, with its
interactive gallery and headless demo under `src/sigilweave/examples/`. Its
main pipeline is:

```
Paragraph -> ICU analysis -> cached HarfBuzz words -> FlowGeometry
          -> LineBreakStrategy -> ParagraphLayout -> draw/drawBatched
```

`ParagraphLayoutOptions` groups line metrics, hyphenation, justification,
Knuth–Plass, overflow, and path-rendering concerns. SpellCircle-specific
ring label geometry belongs in `src/spellcircle/shared/scene/SceneLabels.*`;
the core exposes only reusable `SingleLineParagraphCache` and
`layoutSingleLine()` support.

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
