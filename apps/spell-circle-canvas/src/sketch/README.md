# SigilSketch — authoring and hosting scenes

Sigil is a suite of libraries. Use Compose, World, Draw or another library
directly by including its own headers, spelling its namespace and linking its
target. SigilSketch is the framework a scene is hosted through: registration,
sessions, the canvas and set runtimes, the live host that compiles and swaps
sketches, and the workspace convention for sketches kept outside this
repository. The drawing libraries do not depend on it. Grimoire, at
`apps/grimoire/`, is one application built on SigilSketch — the stock host,
and an example of using the framework — that lists and runs the sketches it
is pointed at and carries this repository's catalogue of them.

A sketch is a plain C++ type that describes a canvas or a lit set, or a
Python class doing the same. The host supplies the clock, assets and
rendering runtime. A sketch reaches a host compiled into it, as a C++ source
file the host compiles against itself, or as a Python file; the host watches
the file, swaps each good build in and keeps the last working session when
one fails.

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

A sketch can stand anywhere on disk. Start a folder for one and open it; the
host compiles the file with its own flags and swaps every saved build in:

```sh
python3 scripts/sigil.py workspace new ~/sketches/hello
Grimoire ~/sketches/hello/hello.cpp
Grimoire ~/sketches/hello/hello.cpp --frame out.png
Grimoire --workspace ~/sketches/hello
```

A folder of sketches outside this repository is a workspace. [HOST.md](HOST.md) covers the
three ways a sketch reaches a host, why a C++ sketch is always compiled by
its host, what binds a workspace to one host build, and custom hosts.
[Grimoire's running chapter](../../../grimoire/RUNNING.md) covers its
window, capture and benchmark commands.

A sketch compiled into a host is one entry in that host's catalogue, which
its build discovers by file stem; Grimoire's is `apps/grimoire/sketches/`.
`SIGIL_SKETCH` supplies the collection and description; `SIGIL_SKETCH_AS`
supplies an explicit filed name when needed.
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
| `ctx.local("data/x.csv")` | beside the entry source |
| `res://x.csv` | the resource root selected by the host |
| a bare path | relative to the process's working directory |

For a single source opened by path, the default resource root is its
adjacent `assets/` directory. A directory source sketch shares the
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

A C++ sketch can be one file or a directory named for its entry, whether
it stands in a host's catalogue or in a workspace:

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
the source watcher follows literal quoted includes recursively. A sketch
outside the tree follows the same rule beside its own entry.

Keep subject-specific geometry, colours and helpers with their sketch.
General-purpose helpers belong to the library that owns their purpose.
Specimen furniture belongs to SigilSketchKit. Sharing an owner's helper
header does not register or compile that owner's scene.

A directory sketch in a host's catalogue may carry `<stem>.fbs`. The
host's build generates
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
capture. [Grimoire's running chapter](../../../grimoire/RUNNING.md) defines
its stills, sweeps, video, comparisons and benchmark commands.

## Pictures between runtimes

A canvas and a set can exchange pictures without sharing their runtime:

| Context method | Purpose |
| --- | --- |
| `ctx.textureScene(size, background)` | a Compose scene painted into a texture |
| `ctx.bakeSet(frame, camera, size, background, seconds)` | one World frame baked into an image on a canvas |
| set `ctx.surfaceScene(size)` | a Compose page painted as a surface's maps, for the set's lights to shade |

The session retains texture scenes until the body declares again. Create one
in `setup()`, keep the returned pointer, and update it at the scene's forward
time. Creating one every frame keeps every such scene until redeclaration.
Include `<sigilcompose/texture/Texture.h>` for its `render()`, `image()` and
`texture()` operations.

A set's `surfaceScene` is held the same way. Its `maps()`, from
`<sigilcompose/texture/SurfaceScene.h>`, dress a surface through
SigilMaterial's surface `program(maps)`: Compose paints each lit material's maps
and the page's flat content as its own colour, and the set's lights,
environment and camera shade the page.

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

### Another application's picture

A host may publish what its canvas draws to other applications on the
machine — Grimoire's `--publish` does — and `sigil::sketch::Guest`, from
`<sigilsketch/canvas/Guest.h>`, is the same door read from the inside of
a sketch: made from the context a sketch was handed — a page's
`SketchContext` or a set's `SetContext` — and the name a publication
announces, it answers with the newest frame two ways.
`sigil::sketch::Guest::frame` is the frame as an image on the recorder
the canvas is being drawn on, one per frame that arrived and null while
nothing is publishing; `guest_picture` is the page that wears one. The
frame arrives with its first row at the image's BOTTOM — the order the
surface a publication is carried on is written and read — so it is drawn
once into a target of its own on that recorder and what comes back is
upright, which is the one thing done to it on the way in — the
subscription's own doing, since a subscription is a `media::PixelSource`
whose frames are turned as they are bound. It takes the
`SkCanvas` as well as the recorder, so a caller inside a paint program
asks with what it is already holding and names Graphite nowhere.
`sigil::sketch::Guest::texture` is the same frame as a
`material::Texture`, which is what a surface's base-colour slot takes, so
a body in a set wears the publication the way it wears any other picture;
`guest_body` is the set that turns one under a light. That one reads the
pixels back into host memory, because the renderer that shades a body
does not stand where a publication arrives — a frame is a Metal texture
and the world draws through Vulkan — and a slot that works on every tier
is worth a copy where no handle can cross.
`sigil::sketch::Guest::publishing` and
`sigil::sketch::Guest::application` are what a scene says about the
publication it is wearing. A capture subscribes to nothing at all: what
another application happens to be offering while a still is taken is not
a function of the sketch that took it, so a plate of such a scene is what
it draws with nobody publishing.

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
of opening an incomplete session. A host's build leaves out a compiled-in
sketch whose optional target is absent; Grimoire's catalogue states those in
the stem-to-target table of `apps/grimoire/sketches/CMakeLists.txt`.

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

A host opens a `.py` module declaring a `@sketch` class in the same native
session system; Grimoire is one that does. Saving imports a fresh body without compiling C++.
`uv run sigil open sketch.py` launches Grimoire with that project's Python
dependencies. A `pyproject.toml` configures the environment; it does not select
the entry module. Helpers remain ordinary imports.

Reusable bindings belong to SigilPython. SigilSketchPython adds sketch sessions
and kit support without adding Python to the core library, and its
`sigil_python_sketches()` turns a host's Python sketches into registry
entries of that host's build.
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
`sketch::Host` compiles a C++ sketch with the flags its own build captured,
against its own headers, and refuses to when those headers are newer than
the running image, because the boundary between a sketch and the libraries is
their whole C++ vocabulary; it imports a Python sketch through its importer.
Either way it adopts a replacement only once the replacement's session opens.
Grimoire is one host over it and owns its windows, Python environments and
navigation; it reaches the framework through the public headers and
targets alone, as any custom host would. The exported framework symbols, the libraries a sketch can reach,
workspaces and mapped-code lifetime are defined in [HOST.md](HOST.md). The protocol API is in [PROTOCOL.md](PROTOCOL.md).

## Source layout and targets

| Directory | Responsibility |
| --- | --- |
| `core/` | registry, kind, session, assets and crash reporting |
| `canvas/`, `set/` | the two rendering runtimes |
| `live/` | watching, compilation, the build cache, adoption and residency |
| `plate/` | captures, sweeps, comparisons and thumbnails |
| `kit/`, `scry/` | specimen furniture and optional web integration |
| `python/` | the Python adapter and a host's Python sketch registry |
| `cmake/` | the captured sketch flags and the exported link surface a host's build calls |
| `testing/`, `test/support/` | host harnesses and shared test fixtures |

`SigilSketch` is the runtime archive. `SigilSketchKit` supplies specimen
components; `SigilSketchRegistryAgent` and `SigilSketchSessionAgent` supply
protocol agents. `SigilSketchTesting` and `SigilSketchTestingHarness` supply
test support. `SigilSketchVocabulary` names every library a sketch may
include, so a host's catalogue links it and adds what its own sketches need
beyond it. This directory holds no sketch and no application: Grimoire's
`SigilSketches` is its catalogue's object library and `Grimoire` its host.

A host's build calls two functions the framework defines.
`sigil_sketch_flags(<host> ANCHOR <source> OUTPUT <file>)` lifts the compile
line of the host's own anchor unit into the response file its live host
compiles a sketch with, and `sigil_sketch_link_surface(<host> <catalogue>)`
force-loads and re-exports every Sigil archive a hot-reloaded sketch
resolves out of the executable; [HOST.md](HOST.md) says why each is needed.

`SigilSketch` builds with `SIGIL_BUILD_APPS=OFF` and `SIGIL_BUILD_PYTHON=OFF`
for a host that carries no applications or Python bindings.
[TESTING.md](TESTING.md) covers building this library, isolated cases,
benchmarks and host fixtures.
