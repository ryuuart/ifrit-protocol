# SigilSketch — authoring and hosting scenes

The Sigil libraries are the SDK. Use Compose, World, Draw or another library
directly by including its own headers, spelling its namespace and linking its
target. SigilSketch adds registration and runtime sessions for scenes hosted
in Sketchbook or another sketch host. The drawing libraries do not depend on
this adapter.

A sketch is a plain C++ type that describes a canvas or a lit set. The host
supplies the clock, assets and rendering runtime. Sketchbook opens native
plugins built with CMake, watches replacements and keeps the last working
session when a replacement fails. It also supports compiling source files
and opening Python sketches.

## Write a canvas sketch

```cpp
#include <sigilcompose/core/Core.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Registry.h>

namespace compose = sigil::compose;
namespace sketch = sigil::sketch;

struct Hello {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1000, 700);
    ctx.background({0.05f, 0.04f, 0.10f, 1});
    ctx.captureAt(2.4);
    ctx.composer.render(
        compose::box().width(200).height(120).fill({0.86f, 0.30f, 0.40f, 1}));
  }
};

SIGIL_SKETCH(Hello, "Example", "A coloured panel.")
```

`setup()` declares the scene once per new session or redeclaration.
`update(elapsed, ctx)` is optional and reacts to data that changes.
A body may take fewer of the offered arguments: `setup()`,
`update(elapsed)` and `update()` are accepted too. No base class is needed.

In a CMake tree that defines the Sigil libraries, build and open the module:

```cmake
sigil_sketch_plugin(hello
  SOURCES Hello.cpp
  LIBRARIES SigilComposeCore)
```

```sh
cmake --build build --config Release --target hello
Sketchbook --plugin /path/to/hello.dylib
Sketchbook --plugin /path/to/hello.dylib --frame out.png
```

The helper supplies registration metadata and hidden implementation symbols;
the host supplies the framework implementation. Ship the generated
`<module>.sigil-build` beside the module. [HOST.md](HOST.md) covers the
matching-build contract, separate CMake projects and custom hosts.
[RUNNING.md](RUNNING.md) covers window, capture and benchmark commands.

For the bundled catalogue, put one entry under `sketches/`; the build discovers
it by its file stem. `SIGIL_SKETCH` supplies the collection and description;
`SIGIL_SKETCH_AS` supplies an explicit filed name when needed.
[REGISTRY.md](REGISTRY.md) covers names, tags and plates.

## Canvas, set and pen

| Header | Body contract | Result |
| --- | --- | --- |
| `<sigilsketch/canvas/Sketch.h>` | `sketch::CanvasSketch`: `setup(ctx)`, optional `update(elapsed, ctx)` | a Compose tree |
| `<sigilsketch/set/Set.h>` | `sketch::SetSketch`: `describe(seconds)`, optional `setup(ctx)` | a World frame |

A set's `describe()` returns a `world::Frame` or a World element that becomes
one. Describe a reproducible scene as a function of scene time and its data.
The optional `setup(sketch::SetContext&)` declares canvas size, background,
capture time and fallback camera. Include World's own headers for its scene
vocabulary; [SigilWorld's README](../common/world/README.md) defines it.

A Canvas body that uses the host's device mesh painter, foreign textures or
device compute declares `static constexpr bool needsDevice = true;`.
Ordinary Compose drawing needs no World executor. A supplied non-CPU
`CanvasKind` painter also declares device work. The host reads
`KindOperations::needsDevice` before opening a session; this declaration does
not prevent the body from providing a CPU fallback.

An immediate pen program is a canvas sketch with a
`compose::graphics` node:

```cpp
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>

namespace compose = sigil::compose;
namespace draw = sigil::draw;
namespace sketch = sigil::sketch;

struct Orbit {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(400, 300);
    ctx.composer.render(
        compose::graphics("orbit.loop", [this](draw::Pen& pen) { paint(pen); })
            .absolute().inset(0));
  }
  void paint(draw::Pen& pen) {
    if (pen.frameCount == 1) pen.noStroke();
    pen.background(20, 30);
    pen.fill(255, 120, 80);
    pen.circle(200 + 120 * std::cos(pen.millis() / 900), 150, 40);
  }
};

SIGIL_SKETCH(Orbit, "Draw", "A moving ball with a trail.")
```

The node keeps its canvas between frames, so a translucent background leaves
a trail. Set persistent pen style on the first frame.
`pen.noLoop()`, `pen.redraw()` and `pen.frameRate(fps)` control when the program
runs; its last surface remains visible between runs. Resizing carries its
pixels forward. The pen reads the host's current pointer and key state each
frame; it has no separate event callback lifecycle.
[SigilDraw](../common/draw/README.md) defines the pen, and
[SigilCompose](../common/compose/README.md) defines the node.

## Retained updates and motion

A canvas scene is retained. Declare it in `setup()`, including its bindings,
transitions and timers. Re-describe only when data changes its structure.
Use `Composer::renderSlot` to replace one changing part, or a `custom()` leaf
with `Cache::None` for drawing that needs a program every frame.

Keep sketch state in members. A reload creates a new instance and restarts
its clock. `SketchContext` is valid only during its callback; do not capture it.
The session's composer and engine remain valid for the session, so a callback
may capture those references explicitly.

`ctx.engine` is the session's `motion::Engine`. Start timers in `setup()`
and keep their handles on the sketch:

```cpp
#include <sigilmotion/clock/Engine.h>

namespace motion = sigil::motion;

motion::Timer solver;

void setup(sketch::SketchContext& ctx) {
  solver = ctx.engine.timer([this] { solve(); }, {.stepRate = 60.0});
}
```

A fixed-step simulation uses `motion::Timer::betweenSteps` to interpolate
between its previous and current states when drawing. The engine advances
even on frames a pen program skips. Starting the timer in the draw callback
would create a new timer on every frame.

Capture pre-roll calls `Session::discardedFrame`. It paints by default so
draw callbacks can accumulate strokes and advance persistent state. A Canvas
sketch whose capture state advances entirely through its engine, feeds and
`update()` may declare `ctx.paintDiscardedFrames(false)` in setup. Those
steps omit drawing; real frames, repaints and stills continue to paint.

For animated properties, use `motion::Animatable`, `motion::bind` and
transitions before writing a per-frame update loop.
[SigilMotion](../common/motion/README.md) defines the clock, bindings,
envelopes and timers. [SigilCompose](../common/compose/README.md) defines how
properties read them.

`pen.element(tree, x, y, w, h)` draws a retained Compose tree inside a pen
program. It keeps a composer per call site; supply an index when several
instances share a site. Describe an unchanged tree once and keep it as a
member. Its bindings and animation continue without rebuilding that value.

## Assets and live inputs

Read resources through `ctx.assets.hub()`. The host registers SigilMedia and
SigilData decoders, caches results and watches requested files. A changed
resource re-runs the sketch's declaration. A missing or undecodable resource
returns null and remains watched.

| Name | Location |
| --- | --- |
| `ctx.local("data/x.csv")` | beside the entry source or native module |
| `res://x.csv` | the resource root selected by the host |
| a bare path | relative to the process's working directory |

For a module or a single source opened by path, the default resource root is
its adjacent `assets/` directory. A directory source sketch shares the
`assets/` directory above it. Bundled sketches use the demo asset root;
`--assets <dir>` overrides it.

```cpp
#include <sigilmedia/core/Image.h>

auto mark = ctx.assets.hub().load<sigil::media::Image>(ctx.local("mark.png"));
```

An image may be still or animated; `media::Image::frameAt` selects its frame.
Other resource meanings include `data::Table`, `data::Json`,
`data::Database` and `media::Video`. Include the originating library's
headers and name its target when using one. The hub owns access; each library
owns decoding and interpretation.

`material::shader(ctx.assets.hub(), ctx.local("aurora.sksl"), Params{})`
loads a shader resource whose uniforms are the fields of `Params`.
Its compilation, caching and fallback behavior belong to
[SigilMaterial](../common/material/README.md).
[SigilIO](../common/io/README.md), [SigilData](../common/data/README.md) and
[SigilMedia](../common/media/README.md) define the resource meanings and access.

Live inputs use the same hub:

```cpp
#include <sigilio/hub/Hub.h>

void setup(sketch::SketchContext& ctx) {
  auto& hub = ctx.assets.hub();
  if (ctx.deterministic)
    hub.replay("udp://:27020", ctx.local("data/sky.feed"));
  m_sky = hub.listen("udp://:27020");
}
```

`Feed::latest()` reads the newest message; `Feed::receive()` drains unseen
messages in order. Neither waits. The session advances feeds to its scene time
before running the body, so a deterministic capture can replay a recording
through the same URI that a window listens to.

## Files and reusable helpers

A bundled C++ sketch can be one file or a directory named for its entry:

```text
sketches/
  hello.cpp
  rain/
    rain.cpp
    Drops.h
    Drops.cpp
    data/
```

Only the entry registers a sketch. Every other `.cpp` directly beside a
same-stem entry is a unit of that sketch. Quoted includes reach local helpers;
the source watcher follows literal quoted includes recursively. A native
plugin lists its sources in its own CMake target instead.

Keep subject-specific geometry, colours and helpers with their sketch.
General-purpose helpers belong to the library that owns their purpose.
Specimen furniture belongs to SigilSketchKit. Sharing an owner's helper
header does not register or compile that owner's scene.

A bundled directory sketch may carry `<stem>.fbs`. Its build generates
`<stem>_generated.h` for C++ or `<stem>.bfbs` for Python; generated files are
not edited by hand. A Python sketch reads the binary schema with
`data::Schema::fromBinarySchema`.

## Captures

Declare a capture moment with `ctx.captureAt(seconds)`. The host opens a
fresh session, steps forward and photographs that moment. A request for an
earlier moment opens another session rather than rewinding this one.

`ctx.oversample(n)` requests a whole number of capture pixels per canvas
pixel. It is useful when a reference's pixels must survive exact
downsampling. It does not change the live window's display scale.
A kept pen surface carries its existing pixels into a larger capture surface;
it is not rerun from scratch merely to increase capture density.

`ctx.deterministic` distinguishes a reproducible capture from a live window.
A value measured from the sketch's own execution can vary between runs;
`ctx.measured(value, pinned)` supplies a stable readout during a deterministic
capture. [RUNNING.md](RUNNING.md) defines stills, sweeps, video, comparisons
and benchmark commands.

## Pictures between runtimes

A canvas and a set can exchange pictures without sharing their runtime:

| Context method | Purpose |
| --- | --- |
| `ctx.textureScene(size, background)` | a Compose scene painted into a texture |
| `ctx.bakeSet(frame, camera, size, background, seconds)` | one World frame baked into an image on a canvas |

The session retains texture scenes until the body declares again. Create one
in `setup()`, keep the returned pointer, and update it at the scene's forward
time. Creating one every frame keeps every such scene until redeclaration.
Include `<sigilcompose/texture/Texture.h>` for its `render()`, `image()` and
`texture()` operations.

A set bake uses its own clock, so sampling an entrance at `seconds` does not
advance the surrounding canvas sketch. It uses the CPU mesh executor for a
reproducible plate. Include `<sigilworld/frame/Frame.h>` for the frame.

A canvas that draws meshes directly uses `sketch::painterRuntime()`.
A set uses `sketch::runtime()`. Each session keeps the runtime it opened
with; `sketch::onPainterRuntime` and `sketch::onRuntime` let a host choose
one for a particular kind. Without a device runtime, the CPU executor is used.
`sketch::device()` is the optional device handle for operations such as native
texture import; a sketch needing it supplies a CPU fallback or an availability
probe.

## Optional authoring features

### Specimen sheets

Feature demonstrations can use SigilSketchKit's headings, sections and wells:

```cpp
#include <sigilsketch/kit/Kit.h>

const sketch::kit::Provide look(sketch::kit::featureTheme());
sketch::kit::stage(ctx, {.size = {1100, 424}, .captureAt = 0.05});
ctx.composer.render(sketch::kit::page(
    {.title = "Crossing rules", .subtitle = "Three constructions"},
    sketch::kit::section({.label = "COMPARISON"},
                        sketch::kit::cells({.cells = {a, b, c}}))));
```

Bind the theme wherever a callback describes another tree.
`sketch::kit::Density::Compact` preserves fixed specimen measures;
`sketch::kit::Density::Spacious` gives explanations more room.
Reference studies keep their subject's own typography and geometry.
[The kit README](kit/README.md) defines its components.

### Optional dependencies and fetched art

A sketch may declare `static bool available(std::string* why)`.
The registry and host use that probe to report missing runtime data instead
of opening an incomplete session. Bundled sketches needing an optional target
also appear in the stem-to-target table in `sketches/CMakeLists.txt`, so an
absent build dependency excludes their source.

`sketch::requireCached(urls, why)` checks cached network assets without
fetching them. Use it when a capture requires the real art rather than the
procedural fallback. A skipped sketch is reported separately from a failed one.

### Web scenes

A web sketch borrows `sketch::scry::sharedEngine()` from
`<sigilsketch/scry/SharedEngine.h>`. A custom host opts in before opening
sessions with `sketch::scry::configureSharedEngine`, and releases all borrowers
before `sketch::scry::shutdownSharedEngine`. This sharing belongs to the sketch
adapter; an ordinary SigilScry consumer owns its explicitly configured engine.
The first process configuration fixes engine resources, threading and device.

A page arrives asynchronously. Use `sketch::scry::settle` from
`<sigilsketch/scry/Settling.h>` with one `sketch::scry::Sequence`:

```cpp
m_page = sketch::scry::settle(
    *m_view,
    sketch::scry::Sequence{.html = document(),
                           .question = "String(document.readyState)",
                           .expected = "complete", .quiet = true, .whole = true},
    ctx.deterministic);
```

In a deterministic capture, settling waits for the engine's load, script and
paint events. In a window, it returns immediately; call `m_page->advance()`
from the per-frame callback and re-describe when that call reports completion.
`m_page->still()` is the settled frame. Draw the view's latest frame while it
is still arriving. Do not block the window's rendering callback.
`<sigilsketch/scry/SettledPage.h>` exposes individual waits and readings.
[SigilScry](../common/scry/README.md) defines views and engine ownership.

### Python

Sketchbook opens a `.py` module declaring a `@sketch` class in the same native
session system. Saving imports a fresh body without compiling C++.
`uv run sigil open sketch.py` launches Sketchbook with that project's Python
dependencies. A `pyproject.toml` configures the environment; it does not select
the entry module. Helpers remain ordinary imports.

Reusable bindings belong to SigilPython. SigilSketchPython adds sketch sessions
and kit support without adding Python to the core library.
[The Python authoring guide](../../../../apps/python/sigil/README.md) describes
the package, typing and environment contract.

## Hosts and library boundaries

`sketch::Kind` opens a `sketch::Session`; a host drives that session without
knowing whether it paints a canvas or a set. An adapter with no C++ sketch type
can use `sketch::openCanvas` with an owned `sketch::CanvasBody`, or
`sketch::openSet` with an owned `sketch::SetBody`.
`sketch::SetBodySource` supplies set bodies for a kind.

The runtimes own their clocks, retained scenes and caches. A host owns assets,
runtime selection and session lifetime. Device objects must outlive sessions
using them. `sketch::useRuntime`, `sketch::usePainterRuntime` and
`sketch::useDevice` install process defaults; individual sessions keep the
runtimes with which they opened.

The canvas runtime installs the default text material resolver unless a host
already supplied one. Runtimes link no device backend or Qt.
The live host watches and adopts replacements; Sketchbook owns its windows,
Python environments and navigation. The host's exported framework symbols,
plugin compatibility and mapped-code lifetime are defined in
[HOST.md](HOST.md). The protocol API is in [PROTOCOL.md](PROTOCOL.md).

## Source layout and targets

| Directory | Responsibility |
| --- | --- |
| `core/` | registry, kind, session, assets and crash reporting |
| `canvas/`, `set/` | the two rendering runtimes |
| `live/` | watching, compilation, artifact adoption and residency |
| `plate/` | captures, sweeps, comparisons and thumbnails |
| `kit/`, `scry/` | specimen furniture and optional web integration |
| `book/` | Sketchbook's application and headless entry point |
| `cmake/` | native plugin build helpers and exported link surface |
| `testing/`, `test/support/` | host harnesses and shared test fixtures |
| `sketches/` | bundled scene entries |

`SigilSketch` is the runtime archive. `SigilSketchKit` supplies specimen
components; `SigilSketchRegistryAgent` and `SigilSketchSessionAgent` supply
protocol agents. `SigilSketchTesting` and `SigilSketchTestingHarness` supply
test support. `SigilSketches` is the bundled scenes' object library, and
`Sketchbook` is their application host.

`SigilSketchSDK` generates native boundary metadata and the separate-build
plugin helper from library usage requirements. The helper package exports
the Sigil libraries' own targets, with what they need from third-party
packages flattened onto them as compile requirements; a consumer finds any
third-party package it uses itself. Test support is neither an origin nor
exported. It is available with `SIGIL_BUILD_APPS=OFF` and
`SIGIL_BUILD_PYTHON=OFF`; it does not require Sketchbook or replace the
existing Sigil library targets.
[TESTING.md](TESTING.md) covers building this library, isolated cases,
benchmarks and host fixtures.
