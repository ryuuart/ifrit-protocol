# SigilSketch — everything renderable, as one sketch each

A **sketch** is a `.cpp` file that declares a scene — or a directory
named for that file, with the file as its entry and the rest of the
directory built with it. The C++ form uses the full native drawing API,
and it is three things at once:

* an entry in **one registry**, addressed by its own file stem;
* a **live-coding** subject: save the file and the running canvas
  reloads in a couple of seconds, with the last good build still on
  screen while a build is broken;
* a **plate**: stepped from zero to a moment it declares itself, and
  photographed, so a byte-identity sweep can ask whether a change moved
  any pixels nobody meant to move.

There is one application over all of it — **Sketchbook** — and one
headless renderer, which is the same binary. Nothing else in this
repository renders a catalogue.

Sketchbook includes a Python authoring layer that opens `.py` files as
canvas sketches inside the same session. Its bindings build with the
application. Python entries join the same registry under the Python
collection, and their subject tags join the normal browser tree. Listing
does not import sketch code; opening a session imports its current source.
A save imports a fresh sketch without
compiling C++; Python functions construct native descriptions or draw with
the pen. The public package, typing and wheel tooling live in
`apps/python/sigil/`. Reusable bindings belong to `SigilPython`; the
`SigilSketchPython` adapter adds native sketch sessions and SketchKit, without
making the core sketch library depend on Python. Files outside
the catalogue also open by path. `uv run sigil open sketch.py` launches this
same application with the uv project's Python dependencies; the launcher
checks that the environment matches the host's Python ABI first.

The **Open** menu uses native pickers for a sketch file or a workspace
folder. A workspace is an ordinary directory: its C++ and Python sketches
appear in their own Workspace collection, without the bundled catalogue. Discovery skips
helper sources, build products, virtual environments and nested Python
projects. Reopening a folder discovers added or removed sketches.
Recent files and folders persist in application settings, including the
last selected sketch in each workspace. A normal launch shows Welcome with Open Workspace, Open Sketch, recent
locations, and Browse Examples. Close Workspace returns to Welcome and
releases the window's sessions and Python environment; recents remain.
Opening a workspace restores its last selected entry when that file still
exists. A workspace with one sketch opens it directly. Several sketches
start on a chooser, with the Library showing the Workspace collection;
alphabetical order never chooses which of them runs. An empty workspace
explains how to declare a sketch. The canvas labels the exact entry file,
workspace rows show its relative path, and Details shows its full path.
Missing recent entries remain visible until the history is cleared.
`--examples` (or `--no-restore`) opens the bundled catalogue directly.

Bundled Python examples share the uv project and lockfile in `sketches/`.
Browse Examples prepares that environment, including NumPy, before opening
its window. Opening those examples by path, or rendering them headlessly,
uses the same environment. The native host supplies Sigil itself; uv installs
the examples' third-party dependencies. A nested project keeps its own
environment. No terminal preparation is required.

Opening a file or folder creates another Sketchbook window. From Welcome,
the new window replaces Welcome; from a workspace, it opens alongside it. Python project
dependencies are prepared automatically before that window starts, and each
window uses one Python environment. C++ sketches and standalone Python
files need no project configuration. `--workspace <directory>` opens the
same workspace from the command line; an optional sketch path selects its
initial source.

A Python module is a `.py` file. A discoverable Python sketch is a module
declaring a `@sketch` class; its filename is unrestricted. `__init__.py`
initializes a package when it is imported, and only becomes a sketch entry
if it explicitly declares a sketch too. Matching the enclosing folder's
name does not make a Python helper a sketch. `pyproject.toml` supplies the
Python environment and dependencies, not the entry file, and console-script
declarations do not select what Sketchbook loads. Modules and assets used
by an entry stay ordinary imports and resources.

```sh
cmake --build build --config Release --target Sketchbook
open build/bin/Release/Sketchbook.app          # the app
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook --list
```

## The sheet a sketch stands on

Before the runtimes: most sketches here are **specimen sheets** — a
titled, footed page over a run of captioned cells — and the look they are
all set in is one value, `sketch::kit::Theme`, carried down the describe
tree by the reconciler's inherited value.

```cpp
#include <sigilsketch/kit/Kit.h>

sketch::kit::stage(ctx, {.size = {1100, 424}, .captureAt = 0.05});
ctx.composer.render(sketch::kit::page(
    {.title = toUtf8("THE RULE AND THE STRANDS"),
     .footer = toUtf8("a crossing is discovered, not declared")},
    kit::cells({.cells = {a, b, c}, .gap = 10})));
```

`src/sketch/kit/README.md` is the canon for it: what the theme holds, how
a sketch binds its own, and what deliberately is not there.

## Two runtimes, one seam

A sketch declares what it draws by which header it includes, and the
registration macro reads the rest off the type:

| include | body | draws |
| --- | --- | --- |
| `<sigilsketch/canvas/Sketch.h>` | `sketch::CanvasSketch` | a compose Element tree, onto a canvas |
| `<sigilsketch/set/Set.h>` | `sketch::SetSketch` | a world Frame, on a lit set |

**A sketch derives from nothing.** A struct that spells `setup(ctx)` is a
canvas sketch and one that spells `describe(seconds)` is a set — there is
no base class to inherit, no `override` to write, and no vtable of the
sketch's own. Those two names are CONCEPTS: the registration reads the
members off the type, and builds the body a session drives out of them.
A member spelled some other way is a compile error naming what a body has
to spell, rather than a sketch that registers and never runs.

A body NAMES THE PARAMETERS IT READS and may name fewer than it is
offered — `setup()` beside `setup(ctx)`, `update(elapsed)` and `update()`
beside `update(elapsed, ctx)`, `describe()` beside `describe(seconds)` —
which is the same rule a paint program and a kit part are called under.
A canvas sketch's `update` and a set's `setup` are optional outright.

A pen is not a third: `compose::graphics(program)` is a node whose pen
paints onto a canvas KEPT between frames, so a p5 loop is a canvas sketch
whose tree holds one of those.

The two are a **seam**, not a switch. `Kind` is `core::Erased<KindOperations>`:
a value that knows one runtime and one body, and opens a `Session` on
them. Every host here — the registry listing, the live canvas, the
headless sweep, the frame-time gate — drives a `Session` and never
learns which runtime it is holding. Another runtime is therefore a
value someone constructs and hands to `SIGIL_SKETCH`, and none of the
hosts change when one arrives.

**A body the macro cannot see comes in by the door beside the kind.**
`sketch::openCanvas` and `sketch::openSet` take an owned `CanvasBody` or
`SetBody` and hand back the session the registered kind would have
opened — which is what a host whose sketches are written in another
language calls, since it holds a body and has no C++ type to register.
To be LISTED as well as opened, such a host names a supplier:
`sketch::SetBodySource` is what a `SetKind` holds where a registered set
holds the address of a factory function, and two kinds are the same kind
when they hold the same supplier.

This library sits **above** the drawing libraries and links them all.
The arrow only points this way: nothing in compose, world or draw knows
this library exists, and a drawn tree, a lit set and a pen's canvas meet
here and nowhere else.

## Writing one

```cpp
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;
using namespace sigil::compose;

namespace {

struct Hello {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1000, 700);                    // the logical canvas
    ctx.background({0.05f, 0.04f, 0.10f, 1});  // what is behind it
    ctx.captureAt(2.4);                        // when a still is worth taking
    ctx.composer.render(box().width(200).height(120).fill(
        Fill::color({0.86f, 0.30f, 0.40f, 1})));
  }
};

}  // namespace

SIGIL_SKETCH(Hello, "Kit", "The starter sketch. Copy it.")
```

Drop the file in `sketches/` and it is in the registry: the stem is its
key, and `sketches/CMakeLists.txt` finds it by looking in the directory.
There is no second list anywhere that could disagree.

### A sketch that is a directory

A sketch that outgrows one file becomes a directory named for it:

```
sketches/
  hello.cpp                            a sketch of one translation unit
  dunhuang_star_chart/
    dunhuang_star_chart.cpp            the ENTRY: the same file, in a
                                       directory of its own name
    Catalogue.h                        reached by a quoted include
    Catalogue.cpp                      a UNIT: compiled into the sketch
```

The rule is one sentence: **a `.cpp` standing in a directory that
carries its own stem is the entry of a directory sketch, and every
other `.cpp` in that directory is a unit of it.** The key is still the
stem, `SIGIL_SKETCH` still goes in the entry and nowhere else, and the
build compiles every unit into the sketch target with the entry. A
directory with no entry of its own name is not a sketch, and nothing
in it is compiled. A sketch's own files stand in its directory, under
`data/`; `res://` is the demo assets root, and `--assets` chooses another.

What goes in a unit is what an edit to the plate should never compile
again: a data table generated from a source and frozen, a construction
the entry only reads. The live host compiles the units apart and links
them once, and a unit whose source and headers have not changed since
it was last compiled is not compiled again — so a save of the entry
costs the entry, not the table. A bare `sketches/<stem>.cpp` stays what
it was, and a sketch goes from one form to the other by moving.

A sketch that carries a schema of its own is a directory sketch too:
`<stem>.fbs` beside the entry is the sketch's own FlatBuffers schema,
stating what that sketch evaluates and shows. The build compiles it into
`<stem>_generated.h`, which the entry includes by name and which
carries the binary schema beside every root; the header is the build's
and is never committed. The scene a sketch reads is then a resource
like a CSV is — the buffer itself, or the schema's own JSON form under
`data/`, which SigilData's FlatBuffer decoder converts through the schema —
and a saved scene file re-declares the sketch. A saved schema wants a
build, since the header is the build's.

A PYTHON ENTRY TAKES THE BINARY SCHEMA AND NO HEADER. There is no
translation unit to include one: `<stem>/<stem>.fbs` beside
`<stem>/<stem>.py` compiles to `<stem>.bfbs`, written beside the entry
because that is where `local("<stem>.bfbs")` names it, and the sketch
opens it with `data::Schema::fromBinarySchema`.

### Helpers have an owner

A sketch owns its reference-specific construction, palette and type in its
own directory. Related studies may include an owner's header with a relative
quoted include; the live host follows those includes recursively. Helpers
with a general purpose belong in the library that owns that purpose, or in
SigilSketchKit when they are specimen furniture.

For example, the MAGI voting study owns `eva_magi_interior/EvangelionUi.h`;
the defense study includes it as `../eva_magi_interior/EvangelionUi.h`.
Only the owner's entry registers a sketch. Including its helper does not
compile the owner's scene into the consuming sketch.

`SIGIL_SKETCH` takes the folder it files under and one line on what it
is — both shown beside it in the app. `SIGIL_SKETCH_AS` adds a name of
its own, for a sketch filed under something other than its stem because
other things already refer to that name.

`ctx.oversample(n)` names a whole number of device pixels per canvas
pixel, and the plate host renders at exactly that rather than at the
fraction its own width budget would otherwise allow; the live window is
unaffected and presents at the display's own scale. Declare it on a
pixel-exact reconstruction — a sketch whose subject's pixel is a whole
number of canvas pixels — because such a sketch is checked by
downsampling its plate by that whole number and laying the result over
the reference, and a fractional scale defeats the check: at 1.875 a
four-canvas-pixel square covers seven device pixels in one column and
eight in the next, and no downsample recovers the reference from that.
A KEPT CANVAS IS PHOTOGRAPHED, NOT REDRAWN. The surface a
`compose::graphics` node holds is formed at the density of the frame it
was first painted on, and a still taken at a larger scale re-forms it
with what it holds carried over — so what a program drew once is
magnified there rather than drawn again, and only what the program draws
on the still's own frame lands at the still's pixels.

### A sketch over an SDK this machine may not have

Such a sketch states its requirement in two places, because there are
two different absences.

`sketches/CMakeLists.txt` carries a short table of stem → target. When
the target does not exist — the SDK was not found at configure time, so
the library over it was never built — the file is dropped from the glob
and nothing tries to compile it; when it does exist, the target joins the
sketch API surface, so a hot-reloaded copy of the file compiles and links
exactly as the built-in one did.

The sketch itself declares a static `available(std::string* why)`, which
the registration macro reads off the type:

```cpp
struct WebPanelSketch {
  static bool available(std::string* why) {
    return scry::available(why);
  }
  …
};
```

That answers the other absence: an SDK present at build time is not the
same as its runtime data — a resource folder, a plugin registry, the
sample archives a piece draws — being installed on the machine running
the binary. An entry whose probe says no is UNAVAILABLE rather than
broken: `--list` greys it and names what is missing, the sweep prints
`[skipped: …]` and writes no plate, and the plate ledger and the
frame-time gate stand it down by name. A skip is not a failure and not a
mover, and the plates for such a sketch exist only on the machines where
its SDK does.

A sketch over FETCHED ART is the same shape with a different probe. Its
bitmaps arrive over SigilIO's https path, which caches on disk, and
every use site keeps a procedural stand-in so a cold cache still
renders — but it renders the stand-in, and the plate the sketch is
judged on is then not the picture its header describes. Two plates
under one name is what a byte-identity sweep cannot survive, so such a
sketch is unavailable until the art is here:

```cpp
static bool available(std::string* why) {
  return sketch::requireCached({"https://…/leftsidepanel.gif",
                                "https://…/2alogobug.svg"}, why);
}
```

`requireCached` asks SigilIO for each URL's cached byte count without
contacting the network. A nonempty resource is available offline while it
remains cached; a missing or empty resource stands down with the first URL
as the reason. It takes a `std::span<const std::string_view>` as well as
the list written above, for a host asking on behalf of a sketch whose
URLs are a value rather than a literal.

### The shared web engine is a host option

An Ultralight process creates one renderer in its lifetime and never
releases it, while Sketchbook keeps several sketches resident and loads a
fresh dylib on every edit. A web sketch therefore borrows the engine its
host configured:

```cpp
#include <sigilsketch/scry/SharedEngine.h>

const std::shared_ptr<sigil::scry::WebEngine> engine =
    sketch::scry::sharedEngine();
```

This is SigilSketch's `scry/` feature, compiled in where the SDK is
installed, not SigilScry's ordinary ownership model. A standalone SigilScry consumer still calls
`WebEngine::create(config)` and owns that explicitly configured value. A
sketch host opts into sharing by calling `configureSharedEngine(config)`
before it opens any sketches; the first borrower boots exactly that
configuration, and `shutdownSharedEngine()` ends that engine and leaves the
path unconfigured, so a host may take its web work down and stand it up
again over the renderer the process keeps.

The configuration belongs to the host rather than to whichever sketch was
selected first, and what the first bring-up built out of it — the resource
roots, the session store, the threading, the device — belongs to the
PROCESS: SigilScry refuses a later configuration naming a different one.
Differences that must coexist belong on a view or web session.

### A page arrives: a capture waits, a window never does

A page is not there when the sketch showing it is declared. The engine
loads it on its own thread and hands over one frame per painting, and
what says the page is THERE is the engine's own events — the load
callback, the frame callback, and the render-pass callback a stretch
with no repaint in it is counted in — never a stretch of clock.
`<sigilsketch/scry/SettledPage.h>` is that door, and it states every
one of them twice: as a WAIT, and as a READING that answers what the
engine has said so far and returns at once.

Which of the two a sketch drives is `ctx.deterministic`:

* **A capture waits.** Its picture is diffed against the last one, so
  the page has to be there before the frame is. The sequence is blocked
  through inside `setup()`, on the thread taking the still.
* **A window never waits.** `setup()` runs on the thread that presents,
  so a body waiting there is an application frozen for as long as its
  pages take. It starts the sequence and returns; the scene draws
  through the view itself meanwhile, and the sketch's per-frame call
  advances the sequence and describes the scene again when it finishes.

One declaration drives both — a `sketch::scry::Sequence`: the document,
an optional script, an optional wheel or press, the page's own answer
that the driving landed, whether the view must go still, and whether the
still is a whole painting.

```cpp
#include <sigilsketch/scry/Settling.h>

void setup(sketch::SketchContext& ctx) {
  m_view = engine->createView(300, 236);
  m_page = sketch::scry::settle(
      *m_view,
      sketch::scry::Sequence{.html = document(),
                             .question = "String(document.readyState)",
                             .expected = "complete",
                             .quiet = true,
                             .whole = true},
      ctx.deterministic);
  ctx.composer.render(scene());
}

void update(double, sketch::SketchContext& ctx) {
  if (m_page->advance()) ctx.composer.render(scene());
}
```

`settle()` comes back arrived or broken in a capture and at its first
step in a window. `advance()` is true exactly on the call the sequence
finishes on, which is when the sketch describes itself again; `still()`
is the frame the settle stopped on, and a scene draws the view's own
latest until there is one — a caption in the meantime says the page is
still arriving. A set reads the same answer off `SetContext` and
advances from `describe`, which is a set's per-frame call. web_script
drives one document four ways, web_panel stands a page in a compose
scene, and import_native wears one on a body in a lit set.

A 3D sketch is the same shape with a different body:

```cpp
#include <sigilsketch/set/Set.h>

struct FirstLight {
  void setup(sketch::SetContext& ctx) {
    ctx.canvas(900, 640);
    ctx.captureAt(1.4);
  }
  world::Frame describe(float seconds) { … }
};

SIGIL_SKETCH(FirstLight, "Set", "A lit set …")
```

`describe` is a **pure function of the scene time**. That is what makes
a plate reproducible: the host steps from zero at a fixed rate and
photographs the declared moment, so the image depends on the declaration
and never on how fast the machine ran.

A p5 sketch is a canvas sketch whose tree is ONE NODE — a pen over a
canvas that keeps what earlier frames drew:

```cpp
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/canvas/Sketch.h>

namespace compose = sigil::compose;
using namespace sigil::draw;

struct Orbit {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(400, 300);
    ctx.background({0.078f, 0.078f, 0.078f, 1});
    ctx.composer.render(
        compose::graphics("orbit.loop", [this](Pen& pen) { draw(pen); })
            .absolute()
            .inset(0));
  }
  void draw(Pen& pen) {
    if (pen.frameCount == 1) pen.noStroke();
    pen.background(20, 30);  // translucent: a trail
    pen.fill(255, 120, 80);
    pen.circle(200 + 120 * cos(pen.millis() / 900), 150, 40);
  }
};

SIGIL_SKETCH(Orbit, "Draw", "A ball on a rail, with a trail.")
```

The pen is SigilDraw's, whose README is the canon for its verbs, and the
node is SigilCompose's, whose README is the canon for the door: what
`compose::graphics` adds over `compose::pen` is a surface that stands
between frames, so a translucent ground is a trail, a slow accumulation
holds, and a picture drawn once stays drawn. The clock is the composer's
and the seed every session's `random` starts from is the same, so a plate
stepped from zero draws the same picture on every run. A box that changes
resizes the surface with the pixels carried over rather than cleared.
What a p5 `setup` would have set on the canvas — a style, a font, a
drawing made once — is the program's FIRST FRAME, which
`pen.frameCount == 1` names, since the pen belongs to the node and not to
the sketch's own context. `pen.noLoop()`, `pen.redraw()` and
`pen.frameRate(fps)` are read off that pen, and the surface goes on being
put down on the frames the program does not run.

A pen program is handed no context, so a figure a sketch measured about
its own execution reads `ctx.deterministic` while declaring and keeps it:
the flag says the same thing for the whole session. Nothing feeds a
pointer or a key into the node, so `pen.mouseX` stays at zero.

**A simulation is stepped by the context's ticker, not by the frame
delta.** `ctx.ticker` is the session's `motion::Ticker`, stepped by the
session's own clock on every frame — including the frames a
`frameRate(fps)` request or a `noLoop` skipped, since the node is painted
on those and time passed on them. `addFixed(hz, fn, maxCatchUp, &alphaOut)` runs the body at
exactly `hz` from accumulated time and publishes the leftover fraction
of a step into the Output, so a piece drawn as
`lerp(previous, current, alpha)` is one picture at every draw rate and a
capture of it is a claim about the piece rather than about the machine.
Register in `setup` and keep the Outputs on the sketch: a registration
made in `draw` is made again every frame, and an Output that lives no
longer than the call is read by nobody. A fresh setup gets a fresh
ticker, so a sketch set up twice is stepped once.

```cpp
struct Cloth {
  ch::Output<float> alpha{0.0f};
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(640, 480);
    ctx.oversample(2);
    ctx.ticker.addFixed(60.0, [this] { solve(); return true; }, 8, &alpha);
    ctx.composer.render(
        compose::graphics("cloth.loop", [this](Pen& pen) { paint(pen, alpha); })
            .absolute()
            .inset(0));
  }
};
```

**A live readout that is a retained tree is a guest, and the guest IS
the slot.** What `slot()` and `Composer::renderSlot` are to a described
scene — a part updated without re-describing the rest —
`pen.element(tree, box)` is to a pen program: the pen keeps one composer
per CALL SITE, so the tree handed in each frame is reconciled against
what that site already holds and its layout, its shaping and its caches
carry. Nothing has to be declared for it, and a loop that paints several
passes the index.

**A TREE THAT DOES NOT CHANGE IS DESCRIBED ONCE.** Build it in `setup`,
keep it as a member, and hand the same value to `pen.element` every
frame: the guest's own animation, its bindings and its live rows still
run, because those are read from the tree rather than rebuilt by it.
Describing it again inside `draw` cannot make it draw anything new, and
it makes the pen reconcile a whole tree against an identical one on
every frame — a cost that grows with the tree and is paid for nothing.
Only the parts whose SHAPE changes — a row appearing, a pool whose lanes
were rewritten — are worth describing again.

### One runtime's picture inside another

A sketch stays in its own runtime, and what crosses between runtimes is
a PICTURE, through two doors on the contexts.

`ctx.textureScene(size, background)` — on the canvas context and the set
context alike — is a compose scene painted into a texture: hand it a
tree with `render()` and read `image()` or `texture()` back. A canvas
sketch that wants a card as pixels paints it once while declaring itself
and keeps the image; a set that wears a live 2D screen asks for the
scene at setup, holds the pointer, and in `describe` hands it the tree
at the scene time and puts `texture()` in a material's base-colour slot.

THE SESSION KEEPS THE SCENE, and lets go of everything it kept when the
body declares again. That is not a convenience: a scene standing on a
device destroys the texture it painted into when it goes, so a sketch
that took the image and dropped the scene would be holding a picture of
nothing, and a body would be wearing a texture that is not there.
Because the session keeps them, a body asking for a scene every frame
holds every frame's scene — so each session's counters say how many it
is holding, and a number that climbs is that mistake.

Nothing has to be remade when time moves. A session's clock only goes
forward: a sweep that must photograph an earlier moment opens a second
session rather than rewinding this one, so no run of the piece begins
where an earlier one left off, and a sketch needs no guard of its own.

`ctx.bakeSet(frame, camera, size, background, seconds)` — on the canvas
context —
is a lit set rendered once into an image: the picture inside a page, for
a document whose plate is re-rendered at the capture scale and cannot
drag its chrome through a texture for the sake of one panel. The
viewpoint is written onto the frame rather than handed to the draw, so a
tree carrying a camera of its own is seen from it here exactly as in the
set runtime, and forming and presenting cannot disagree. It draws on the
CPU mesh executor whatever device the process holds and declares no
passes, so the page's plate and its live picture are one picture.

`seconds` is the MOMENT of the bake, on the baked scene's own clock,
which starts when the scene mounts — what a set with an entrance is
photographed at. A `staggerChildren` cascade is a schedule of transitions
that begin at the mount, so at zero every one of them is still at its
start pose and the picture is the set before it arrived. The clock is the
bake's and not the sketch's: reaching the moment on the sketch's own
ticker would step the sketch, and a document photographing a set in one
of its panels would move everything else on the page to do it.

Each door names the other library's value by forward declaration and
nothing else of it: a sketch walking through one includes that library's
own headers — `<sigilcompose/texture/Texture.h>`,
`<sigilworld/frame/Frame.h>` — and the scene's and the frame's words are
those libraries' to define.

`sketch::painterRuntime()` is the third door, and it carries no picture:
it is the `geometry::mesh::render::Runtime` the process draws mesh
through, for a canvas sketch that stands geometry up in space rather
than baking it.

```cpp
render::MeshStyle style;
style.runtime = sketch::painterRuntime();  // the app's device, or the CPU
```

Written once, it is correct on both tiers — a process with no device
hands back the CPU mesh executor, so a sketch never asks whether a
device is here. The app installs the device one, and the sweep does not:
a plate is hashed from the CPU executor, and the two rasterise the same
picture but not the same bytes, because one sorts triangles back to
front and antialiases their edges while the other depth-tests them. It
is the 2D twin of `sketch::runtime()`, which is the whole frame a set
draws through; a process on a device installs both.

**A session keeps the painter it opened with.** What a host installed is
the default a session takes, not a value its body re-reads: while a
session draws, `painterRuntime()` answers that one on the drawing
thread, so installing another reaches nothing already running.
`onPainterRuntime(kind, painter)` is how a host opens one somewhere else
— it is the 2D twin of `onRuntime`, and a kind that stands no mesh up of
its own comes back unchanged. The thumbnail worker says both, with empty
runtimes, so a still is the CPU tier's whatever the process holds.

`sketch::device()` — from `<sigilsketch/core/Device.h>`, on both
surfaces — is the fourth, and the only one that is not a runtime: it is
the `geometry::device::Device` this process brought up, or **null**,
which is the CPU tier. Reach for it where a runtime cannot stand in,
which is a call that takes the device itself because what it does is
give the device a handle over something the graphics API already holds:

```cpp
if (auto* on = sketch::device())
  slot = world::diligent::importNative(*on, native);  // no copy, either way
```

Null is an answer, not a failure. A plate is taken on the CPU tier, so a
sketch that reaches through this door says what it draws without one,
and a sketch whose whole subject needs a device stands itself down
through its own `available()` probe rather than drawing an empty set.

### Three paths for motion, and the order to reach for them

The canvas runtime is retained-mode, not a redraw loop:

1. `setup()` **declares** the scene once, animation wiring included —
   bound outputs, transitions, ticker steppables. The runtime then
   animates every frame without re-describing anything. Reach for this
   first.
2. `custom()` leaves with `Cache::None` are the immediate-mode floor:
   their paint program runs per frame with the elapsed time.
3. `update()` is the **data** path: when state changes, describe again
   and let the reconciler diff it. Do not re-render every frame out of
   habit — bindings are cheaper.

The first path is the one most sketches reach for last, because the
familiar move is a ticker lambda that computes a position and writes it
into an Output. A **shaped bound Output** does that at declaration time
instead: one Output carries the clock, and every value derived from it is
a named envelope on the property that reads it.

```cpp
ch::Output<float> clock{0};                       // the only thing ticking
ctx.ticker.add([this, t = 0.0](double dt) mutable {
  t += dt;
  clock = (float)t;
  return true;
});

// hold, glide down over five seconds, hold, four seconds back — the four
// corners are positions in one 14 s cycle, and the ease rounds both
// shoulders without moving them
list.translateY(bind(&clock)
                    .source(0, 14.0f)
                    .trapezoid(3 / 14.f, 8 / 14.f, 9 / 14.f, 13 / 14.f)
                    .map(ch::easeInOutQuad)
                    .target(0, -overflow));

// one second lit out of every eight, starting at 2 s: a pulse, folded on
// its own period, so it repeats for as long as the clock runs
button.opacity(bind(&clock).source(2.0f, 10.0f).square(1.0f / 8.0f));
```

`cosine()` is the swell, `pingPong()` the there-and-back, `trapezoid()`
the loop envelope that can cut while it is dark, `square()` the pulse,
and `wave()` takes a shape of your own. Each replaces the `std::sin`,
`std::fmod` or four-branch `if` ladder a ticker lambda would otherwise
carry, and the value is then a declared property the reconciler can
prune on rather than a write nobody can compare.

Keep state in members. Every reload constructs a fresh instance, so a
reload restarts the piece from zero: the entrance you are editing plays
again.

### Numbers a sketch measured about itself

A sketch that draws its own build time or node count into its own plate
is not a reproducible capture — it differs from *itself* between two
runs, and a pixel sweep then reports it as changed by a patch that
changed nothing. `ctx.measured(value, pinned)` returns the real number
normally and the pinned one when the host is capturing for a diff. The
rule is broader than clocks: it covers anything computed from the
sketch's own execution rather than from its data.

Both kinds of sketch carry it on the context they are handed each frame,
so the figure is routed where it is drawn:

```cpp
std::snprintf(buf, sizeof buf, "BUILD %.2f ms", ctx.measured(buildMs));
```

## Running one

```sh
Sketchbook [--no-gpu]                       # the app
Sketchbook --sketch <name>                  # the app, on that one
Sketchbook <file.cpp>                       # the app, on that file
Sketchbook --list [--kind canvas|set]      # the registry, one per line
Sketchbook --catalog [<file.cpp>]           # the browser's rows, one JSON each
Sketchbook <file.cpp> --frame out.png [--at <sec>] [--scale <n>] [--gpu]
                                  [--frames <count>] [--fps <n>]
                                  [--deterministic | --no-deterministic]
Sketchbook <file.cpp> --bench [--bench-frames <n>] [--jitter-dt [amp]]
Sketchbook --headless <outdir> [--gpu] [--sketch <name>] [--kind <k>]
           [--ledger] [--no-promotion | --promotion] [--composites]
           [--capture-at <s>] [--timing-json <path>]
Sketchbook --video out.mp4 [--video-frames <n>] [--video-size <WxH>]
           [--video-bitrate <bits>] [--fps <n>] [--sketch <name>]
           [--kind <k>] [--gpu]
Sketchbook --compare <dir-a> <dir-b>        # two sweeps' plates, differenced
Sketchbook --window-bench [<sec>] [--window-size <WxH>] [--window-scale <n>]
Sketchbook --thumbnails [--sketch <name>] [--kind canvas|set]
           [--thumbnail-budget <sec>] [--thumbnail-heavy]
Sketchbook --publish [<name>] [--sketch <name>]
                                            # the window's frames, live, to
                                            # other applications
… [--assets <dir>]                          # what mounts at res://
… [--thumbnails-dir <dir>]                  # the app's own thumbnail store
```

`--sketch` takes a case-insensitive substring and answers to a sketch's
filed name or its file stem, which is the loop for visual iteration.
`--shot <png>` captures the app window rather than a sketch, which is
the only way to look at the browser and the inspector.

**`--publish` offers the live window's frames to other applications.**
A program on this machine subscribes to a name and receives every frame
this canvas draws, composited live in its own scene — a VJ program, a
projection mapper, a recorder. The name is the one given, or the stem of
the sketch the run opens on; a subscriber binds to it, so it is the
run's and does not follow the sketch on screen. The status bar's **Publish**
button and Ctrl-P invoke the same action. A normal launch without `--publish`
starts with publishing off and uses **Sketchbook** when enabled. The status
button shows whether publishing is enabled; the status line names a starting
or active publisher and reports failures.
If publication cannot start, the button returns to off and the canvas shows
the reason until the next attempt; CPU rendering cannot publish frames.

`Receiver`, the subscriber in the same feature, is what to check it
with: `Receiver --list` says what is being offered, `Receiver <name>`
opens a window on it and `Receiver <name> --grab <png>` writes its newest
frame to a file.

**Frames come the other way too.** `sigil::sketch::Guest`, from
`<sigilsketch/canvas/Guest.h>`, is the same door read from the inside of
a sketch: made from the context a sketch was handed — a page's
`SketchContext` or a set's `SetContext` — and the name a publication
announces, it answers with the newest frame two ways.
`sigil::sketch::Guest::frame` is the frame as an image on the recorder
the canvas is being drawn on, one wrap per frame that arrived and null
while nothing is publishing; `guest_picture` is the page that wears one,
and nothing is copied on the way in. It takes the `SkCanvas` as well as
the recorder, so a caller inside a paint program asks with what it is
already holding and names Graphite nowhere.
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

What travels is the texture the frame was drawn into, so publishing
wants the window on Graphite. On the CPU raster fallback there is no
texture of this window's to offer, and the flag is REFUSED rather than
answered with something else: the console says so and publishing stays
off. Every lane that renders without a window — a sweep, a still, a
montage, a measurement, the warm command — refuses the flag outright.

`--catalog` prints the browser's rows without opening a window, one JSON
object per line — the registry first, and a file this run was pointed at
after it. What a script reads off them is what the browser reads before
anything has been built: a compiled-in entry names the runtime it draws
through, and a file opened by path has none until it has been compiled
and says so rather than guessing.

**A capture is deterministic and a live run is not.** Anything a sketch
measured about its own execution is pinned when a still is being written
and real everywhere else, so a `--frame` can be diffed while the app and
`--bench` show the machine's own numbers. `--deterministic` and
`--no-deterministic` name either regime for either, which is how a
sketch's real figures are looked at in a written frame.

The app brings a device up and every set draws through it, because a
device is what runs a material's own body: the CPU mesh executor has no
compiler, so a surface reaches it as the colour the frame extracted and a
reader would be looking at a picture no recipe ever ran in. `--no-gpu`
keeps sets on that executor, which is what a plate is hashed from and
therefore what a window is worth putting beside one. A device that will
not come up is reported and the app carries on — unlike the sweep's
`--gpu`, which must fail rather than put two different pictures under one
plate's name.

**The canvas zooms without redrawing what it is showing.** The live view
sits in a pan-zoom pasteboard, so a ctrl-wheel spin grows the item the
frame is drawn into. It does not grow the frame: the view renders at the
scale it had when the gesture began and the scene graph stretches that
last frame over the growing item, so the picture follows the wheel
immediately and pays for it only in sharpness. It re-renders at the
settled scale once the gesture has been quiet — one pending resize, the
last scale winning, however many steps the spin had — and a pure pan
never re-renders at all, because the frame on the texture is the same
frame wherever the item stands. Underneath it, the sketch's cached
rasters are pinned to the screen's density rather than to the viewport's
scale (`Session::setBakeDensity`), so a generated material is baked once
and magnified through the zoom the way a bitmap the sketch loaded would
be, instead of being rasterized again at every rung of the composer's
bake ladder the gesture passes through. The **Capture** action raises the
density for the photograph, so an explicitly asked-for still is written
at its own resolution rather than at the reader's. The plate ledger does
the same before the first frame it steps: it declares the plate's density
to the session it reopens, so a kept canvas or a bake formed on the way
to the capture moment is drawn on the plate's grid rather than magnified
to it.

The app is a macOS bundle, so a headless run goes through the binary
inside it:
`build/bin/<config>/Sketchbook.app/Contents/MacOS/Sketchbook`.

### `--video`: the video montage

Encodes every selected, available registry sketch into one vertical H.264
MP4. The default frame is 1080×1920 at 30 FPS, with ten output frames per
sketch. Each session is opened and advanced in fixed display-sized steps to
the moment it declared with `captureAt`; a sketch that declared no moment uses
1.5 seconds. Recording begins there, so a long entrance or loading sequence is
settled before its cut begins. Each cut is the sketch in one fixed fitted
rectangle on black with its title in white. The sketch's own animation remains
live; the montage adds no border, progress chrome, pulse, scan, or reveal wipe.

Before the first selected session opens, Sketchbook preloads the stock shader
directories through SigilIO and warms their SkSL programs concurrently. The
montage, headless sweep, capture path and live browser all cross that loading
barrier before they render, so no SkSL program is compiled inside a captured
loading frame or the first interactive frame.

The device program a draw runs through is a second compile, built per distinct
draw out of the whole inlined paint tree, and warming the SkSL does not reach
it. Every Graphite context is given a thread pool to build those programs on,
so the stages of a scene wearing a chain of effects are built beside each
other rather than one after another, and the window's canvas stands them up
before its first frame rather than inside it.

**Which programs a launch stands up is what the last launch needed.** As the
app comes up, and before any Graphite context exists, it declares every SkSL
body the effects are made of and every stock recipe's program to SigilSkia —
the precondition for a program built over one of them having a name that
survives the run. Every program the run then builds is recorded, with the key
that rebuilds it and the description it was built under, and written at exit
under the platform cache location beside the thumbnails
(`Sketchbook/pipelines/<digest>.keys`; `SIGIL_SKETCHBOOK_PIPELINES` names
another). The digest is over the declared bodies, in the order they were
declared, and the backend's name: an edited shader, a recipe added or a
different device is a different file rather than a set of keys describing
other programs. The next launch replays that set on a worker as soon as the
canvas's context exists, and **the canvas draws nothing until it lands** —
skipping frames rather than blocking the thread that presents, and for a
bounded number of them, because a warm-up gone wrong must be a late sketch
and not an empty one. A first frame recorded beside the warm-up would ask for
the very programs it is building, which is the stall moved rather than
removed.

**What is written back is what the run's DRAWS wanted**, not everything it
built: a program a draw asked for, whether it was built for that draw or
found standing because the replay had stood it up. A replayed program is
reported as built again, so a run writing back everything it recorded would
write back its own replay, and the set would grow into every program every
sketch ever opened here needed — each one a program a later launch stands
up before its canvas draws, whether that launch is opening that sketch or
not, and the canvas holds its frames for the warm-up only so long. A sketch
that stops being opened therefore falls out of the set rather than being
stood up forever, and a run that walked the whole registry is cut at a
ceiling no later launch should be made to warm.

**A headless sweep on the device fills a store that stands empty.** It
draws the programs an open window draws, with nobody waiting on any of
them, so it declares and records exactly as a launch does and leaves its
set behind, and the FIRST interactive open of a machine then has
something to replay rather than nothing. What that is worth follows what
was swept: a selection of a few sketches leaves those sketches'
programs, while the whole registry wants far more than the ceiling a
written set is cut at, and what survives the cut is what the run drew
first — not a set chosen for the sketch someone opens next. It never
replaces a set either: a run that drew a whole selection knows less
about what the next launch will open than a window run that drew one
sketch, so a store that already answers for this declaration keeps its
answer and the sweep's set is dropped. And it stands nothing up ahead of
itself — there is no frame to protect, and replaying would put the
store's state inside a lane whose picture has to depend on nothing but
the sketch. A sweep with no `--gpu` does not warm at all, because it
builds no device program to record. Neither does `--frame`: it
photographs a canvas on a raster surface so the picture is reproducible,
and a set is drawn by the device's own renderer, so a capture builds no
Graphite program either.

A key is replayed only if its description still reads back the same. A key
names the pieces a program is inlined out of by number, and a piece the
reading run cannot yet put a name to reads back as a hole — the backend makes
its own blur and lighting pieces on first use, so a key recorded after a draw
that made one is unreadable by a run where nothing has. Such a key is dropped
and its program is built when a draw asks for it, which is one program rather
than a walk off the end of a name.

With no set to replay — a fresh machine, an edited shader, a Skia that no
longer reads the keys — the same worker stands up the effects' own bodies as
stages instead, and only those that declare no child: a described paint is
expanded into every combination it allows, so a body with two children is
hundreds of programs a device is asked to hold and a driver that stops
compiling. That reaches a stage drawn alone; it cannot reach a STACK, because
a backend inlines a whole chain into one program and which chains a sketch
wears is not known before the sketch is read. Those are built as the draws
ask for them, and recorded, which is what makes the second launch the cheap
one. Each run says on stderr what it spent: how many programs it built for a
draw, how many it stood up ahead of one, how many draws found a program
already standing, and how many of the keys it recorded it wrote down. A run
that found a set says how much of it stood up and how much of it no longer
described what it described — a store gone wholly stale reads as a
warm-up that is neither helping nor free, and is the one state worth
seeing. A run with no Graphite behind its window says nothing at all: it
neither records nor replays, so the tally would be zeroes.
`SIGIL_SKETCHBOOK_PIPELINE_NAMES` adds one line per program, naming it, which
is how to see which program a first frame still had to build.

`--video-frames` changes each sketch's share of the edit, `--video-size`
changes the even output dimensions, `--video-bitrate` sets H.264 bits per
second, and `--fps` changes both the encoder rate and the fixed scene clock.
`--sketch` makes a one-sketch video and `--kind` limits the registry by
runtime. Hardware H.264 is preferred and OpenH264 is the fallback. Unavailable
sketches are named and skipped rather than encoded as failure cards.

`--gpu` is REQUIRED for a selection that holds a set, exactly as it is for
the sweep, and for the same reason: a set is lit by the device renderer,
so a montage that included one without a device would put a picture no
recipe ran in under that sketch's name. A selection that holds a set and
did not ask is refused, naming `--kind` as the other way out; a run that
asks and cannot have the device fails; a run whose selection needs none
brings none up.

The app's **Export video** action writes the full registry through this path.
The selected sketch's **Video** action writes a one-sketch cut; both use a
native save dialog and run the encoder in a child Sketchbook process so the
browser and its live canvas remain responsive.

### `--compare`: two directories of plates

Prints how far every plate in one directory stands from the plate of the
same name in the other, decoded and differenced channel by channel:

```
compared <name> mean <mean> p99 <p99> max <max> clear <max> content <max>
         graze <max> <how many> composited <per composite> <how many stacked>
size <name> <W>x<H> <W>x<H>
missing <name> first|second
unreadable <name> first|second
```

Every distance is an absolute difference of one 8-bit channel, in 0..255,
over every channel of every pixel. `clear`, `content` and `graze` are that
worst difference split three ways over the pixels it stands on, because a
caller's tolerance can depend on which it is. `clear` is where the FIRST
plate — the reference — holds transparent black, so nothing was
composited under the difference at all. `graze` is where the difference is
CONFINED TO AN ANTIALIASED EDGE BOTH PLATES DRAW: the picture varies by at
least the difference within a pixel of that point in each of them, and so
does every differing pixel beside it, so what changed is one pixel's
coverage of an edge the two agree about — which is what a mark standing a
fraction of a device pixel from where the other drew it looks like, and
the count beside it says on how many pixels. `content` is everything else:
a difference that reaches a pixel no edge explains, which is what a
picture that MOVED shows — pixels taken off the edges, a mark that is
gone, a wash at another value.

`composited` is that same `content` figure divided by how many CACHED
RASTERS were blitted over each pixel, rounded up, with the count of
content pixels that stood under more than one beside it. A cached raster
is a composite the picture beside it did not make and every composite
rounds, so a difference of four under four of them is the same fact as a
difference of one under one — and a caller whose tolerance is a bound per
composite reads this rather than `content`. The counts come from a plane
the SECOND directory carries beside its plates, named `counts_<sketch>`
and written by a headless sweep asked for `--composites`; with no plane
there every pixel stands under one composite and `composited` is
`content`.

It opens no
sketch, needs no
fonts, no assets and no device, and it JUDGES NOTHING — how close is
close enough is a tolerance about a machine, which is the plate ledger's
to hold. The ledger's device and promotion tiers are the callers: each
renders two directories of plates in one run and asks this which
pictures moved.

### `--frame`: the asset workflow

Steps the clock at `--fps` (default 60) to the moment the sketch
declared with `ctx.captureAt`, then captures `--frames` PNGs
(sequences number as `out_0001.png…`) at
`--scale` (default 1: captures match the declared canvas pixel for
pixel, which is what asset generation wants). Declare the exact canvas,
give it a transparent background, draw, export. Any sketch answers to the
flag, so the sketch that draws the asset is the template.

**`--gpu` puts the run on the device**, exactly as it does for a sweep: a
set draws its frame there, and a canvas sketch's mesh painter
(`sketch::painterRuntime()`) rasterises there. It is fatal when no device
comes up, because a run that asked for the device and quietly gave the
CPU's picture puts two different pictures under one name. Without it a
file renders on the CPU mesh executor, which is what a plate is hashed
from.

**The moment is the sketch's, not the flag's.** `--at <sec>` overrides
it, and a sketch that declared none falls back to 1.5 s; otherwise a
still uses the same declared moment as the plate sweep. The line it prints
says which of the three it used. A declared zero runs one update without
advancing time; a moment between fixed steps uses a final fractional step.
The standalone Python renderer uses the same host preparation. Fractional
canvas dimensions round up to whole pixels at the requested scale.
`--bench` keeps the 1.5 s default
whatever the sketch declared: its `--at` is a warm-up that has only to
get programs, bakes and atlases hot, and pinning it keeps the measured
run the same run for every sketch.

`--fps` sets the PRE-ROLL step as well as the capture rate. Steps longer
than the session clock's maximum delta are subdivided so the clock reaches
the requested moment. A sketch
using a fixed-rate steppable has a catch-up clamp, so pre-rolling far
below its own rate discards simulated time and lands earlier than you
asked for. Keep `--fps` near the rate you would actually draw at.

### `--bench`: the 60 FPS gate

One machine-readable line, prefixed `BENCH` so a collector can find it,
carrying the sketch, its canvas, the frame count, the step regime, the
percentiles and a verdict — then a human line naming which phase
dominated, and the runtime's own lanes under it.

The gate is **p99 under 16.6 ms** — a sustained 60 FPS at the sketch's
own declared canvas size. It exits 0 whenever it measured: the verdict is
the output, not the exit status, so a slow sketch can sit in a pipeline.
A sketch that never built, or a surface that could not be allocated,
exits 1.

**A sketch that declares `ctx.plate()` is judged on its capture cost, not
on 60 FPS.** Some sketches are plates rather than live scenes: a large
sheet over an expensive material stack whose subject is the sheet's own
size. A canvas the sketch cannot present at is a different statement from
a live sketch that drops frames, so a marked sketch reports the cost of
the still it is photographed as and the verdict reads `PLATE` rather than
`PASS`/`FAIL`. It is never a timeout override, and it changes nothing
about the plate sweep — only what the interactive gate asserts.
`chaucer_astrolabe` is one.

What it does, and why it is not `--frame`'s numbers: the capture path
steps the clock on a tiny scratch surface where every draw is clipped
away, so a sketch whose whole cost is one full-canvas shader reads as
free. `--bench` allocates the real canvas, warms it to `--at` so
programs, bakes, snapshots and glyph atlases are hot, then times real
frames. On a failure it prints the most expensive nodes with how each
produced its pixels, and under any expensive one that is not a bake, a
line saying **why** — because each refusal to bake is individually
correct and individually invisible.

`--jitter-dt` steps a varying frame interval instead of the fixed one. A
fixed step is not a neutral simplification for anything that memoizes on
a per-frame value: under it the values a scene visits repeat on the
scene's own period, so a cost that grows per distinct value reads as
free. A wall-clock host never revisits a value. The sequence is a
golden-ratio rotation — irrational, so it never repeats a step, and
deterministic, so two runs measure the same frames.

### `--window-bench`: the same frames, in the real window

```sh
Sketchbook --window-bench [<sec>] [--window-size <WxH>] [--window-scale <n>]
           [--sketch <name>] [--kind canvas|set]
```

Opens the window at a stated size and device pixel ratio, presents each
selected sketch for a stretch after a warm-up, and prints one
machine-readable line each — prefixed `WINDOW`, the way `--bench`
prefixes `BENCH` — carrying the presented rate, the frame's work mean
and p99, its paint phase, the submit, and the headroom the work alone
would allow. A sketch this machine cannot run is named `SKIPPED` with
what is missing, and no line is written for it.

**Each row is the sketch that was on screen.** Selecting a sketch is an
ask: the session opens on the render thread and its first frame — the
program compiles, the texture bakes, the glyph atlases — can cost
seconds. So the warm-up starts at the first frame of the selection's own
session and not at the ask, and the rolling windows the readout comes
from are emptied where the measured stretch begins, so a row is that
sketch's frames over that stretch and carries nothing of what opening it
cost. The rate is the whole stretch — the frames that reached the screen
over the time they took — so a hitch inside it weighs what it was, while
the panel's own readout beside the canvas stays the short rolling one a
reader watches change. A selection that does not reach the screen within
the ceiling is named `SKIPPED` with how long it was waited for, as is a
stretch that ended with all but no frames in it; a run that stood any
sketch down that way exits non-zero, because a rate it could not take is
not a rate of zero.

**Nothing else runs inside a measurement.** The store's still is not
written while the lane is measuring, and the window keeps one session at
a time — the one on screen goes as the next opens, rather than standing
warm behind it and being let go in the middle of a later sketch's
frames. So what a sketch reads in a sweep is what it reads presented
alone, which is the only way a row means anything on its own.

**It measures what `--bench` cannot.** The gate renders onto a raster
surface at the sketch's declared size and presents nothing, which is
what makes it a gate: the sketch's own cost, isolated. Here the frame is
drawn through the surface the window presents, at the window's pixels
and its device pixel ratio, and the numbers carry the host's own
overhead with them — the submit or texture upload that puts the frame on
screen, and, for a set drawn on a device, the readback and blit its
paint phase performs. Selection goes through the same property a click
sets, so a switch takes the path a reader's click takes.

A presented rate is bounded by the compositor, which means by the
display: a sketch comfortably inside its budget reads at the refresh
rate and says nothing more. The interesting rows are the ones BELOW it,
and the work beside them says how much of that frame was the sketch.

`scripts/sigil.py bench --lane fps` drives it over the registry and judges each
presented rate against `bench/app_fps_<config>.json` within a stated
band, `--rebase` adopting. The baseline is per machine AND per display
mode, so it records the window size and scale it was taken at and the
run says so when they differ.

## Going through the registry

The window composes four responsibilities: `SketchCatalog` supplies rows
and thumbnails, `Browser` filters and selects those rows,
`CanvasPane` owns the viewport, input gestures and compile-error panel, and
`SketchActions` runs frame, video and benchmark commands from a supplied
row. Selection does not launch work. Commands own their subprocess and
status, so exporting a sketch does not change the catalog or the live
canvas. A shared notice keeps command progress and its result visible outside
the details drawer until dismissed. An empty command row selects the full
registry for video export.

- `core/Sources.h` — `SourceMetadata` and `sourceMetadata` read author prose
  without Qt; `sourceOf`, `directorySketch`, `sourcesUnder` and `unitsOf`
  resolve the files and translation units belonging to a sketch. `headersOf`
  follows its local quoted includes across owner directories.

The thumbnail worker is the existing `ThumbnailQueue`; the catalog
marshals its results onto the GUI thread. Captures and device readback
remain on the render thread, sharing the context that owns the live
session's images. What follows a readback does not: `Host::still` hands
back the pixels and `ThumbnailWriter` encodes and writes them on a
worker of its own, one still in flight, so a frame that photographs a
sketch for the store pays the readback and nothing after it.

The library and canvas occupy two resizable panels. Search, grouping, view
mode and sorting live together in the library; the canvas has explicit Fit
and actual-size controls. Details opens a drawer over the right edge at any
window size and closes on Escape or a click outside it. The drawer starts
closed, leaving the artwork its space. Both panel headings align their title
and contextual detail; the canvas heading identifies the current sketch's
collection, language and dimensions. Browsing and presentation stay separate:

* **selection is a look.** Arrow keys move it, a click moves it, and all
  it moves is the selection and its details. Whatever the canvas was
  presenting keeps presenting while you read.
* **Enter presents.** So does a double click, the Open action beneath
  the results, and the inspector's Open. This is the only thing that changes what is drawn —
  and the resident set is what makes it cheap, because a sketch already
  opened comes back without being built again. It is also what ends the
  thumbnail fill: from there on the canvas is what draws.
* **A click on the canvas gives it the keyboard.** The pointer over the
  canvas and the keys while it holds focus reach the running session in
  the sketch's own canvas units, through `Session::pointer` and
  `Session::key`, for a sketch that reads them; a click on the list takes
  the arrows back. A sketch with nothing for a pointer to do ignores what
  arrives, and a drag over a set still orbits it.

The library's group picker opens a navigation tree grouped by **subject**, using tags from each
sketch's opening comment. Paths such as `Typography/Paragraph` make an
expandable tree. Selecting a parent includes all its descendants; a sketch
with several tags appears in several groups, while every count and result
list includes that sketch only once. Expanding a branch changes the tree
without rebuilding the result views or changing the canvas.

The tree's **Collections** option builds a tree from registration categories:
`Study · Type` becomes Study → Type. These are logical groups independent of
source directories. **All sketches** clears the group filter, and **Untagged**
keeps sketches without subject tags reachable. The selected group, grouping
mode and expanded branches survive relaunches; each group and search keeps
separate list and gallery scroll positions during the run. The inspector's
tag buttons open the corresponding subject group.

Two views share the selected group, search and sort order, with a toggle in
the library header. The adjacent sort menu selects a field and direction for
either view. Switching views preserves the selection and each view's scroll
position; it does not jump to the selected sketch. Equal sort values,
including unknown session facts, are ordered by name so narrowing a search
does not reshuffle ties.

* **the list** — one row per sketch, with the thumbnail,
  blurb, collection, runtime, canvas, declared moment and line count in
  columns where width permits. A compact metadata line keeps collection,
  runtime and known dimensions visible in narrow panes. Clicking a column
  heading orders by it; clicking again reverses.
* **the gallery** (the default when no view preference is saved) — every
  matching sketch as its own still, a two-line description, collection,
  source language and line count, with canvas dimensions when known. The
  Open action and current-canvas status stay visible in each card.

Sketchbook uses the shared `Ifrit.Qt` system-palette theme and controls.
It follows the operating system's light or dark appearance; rendered
sketches keep their own palettes. Panels, headings, search, selection, status
and buttons reuse the same controls as the SigilWeave gallery. View controls and sortable headings are
keyboard controls, search has an accessible clear action, and an empty
result offers to clear the filters. The selected sketch's Open action
remains visible when the details panel is hidden; a presented sketch offers
Replay to restart its animation.

The filter takes free words and field words together, and every word has
to match. Free words search names, categories, tags, blurbs and file stems;
`folder:`, `tag:` and `kind:` narrow on their respective fields. For example,
`tag:typography tag:motion` finds sketches tagged with both subjects.
Navigation counts show search hits before the selected group narrows them,
so another branch says how many results selecting it would show. A selected
group with no hits keeps its name and shows an empty-state message.
`/` puts the cursor in the search field and Escape empties it. **Clear
filters** clears both the search text and selected group in either view;
the search clear action removes only the search; All sketches clears only the group.

**The thumbnails are the app's own.** Sketchbook keeps one store — one PNG
per sketch, under the platform cache location (`--thumbnails-dir` and the
`SIGIL_SKETCHBOOK_THUMBNAILS` environment variable name another). Each
file's name carries a KEY: a hash of the sketch's source files and local
headers, including quoted includes followed across owner directories.
A thumbnail whose key no longer matches is stale and is drawn again.

**The host is not in the key.** A library edit changes what a sketch
draws while its source stands still, and every still on disk goes on
claiming to be fresh. That is the trade taken deliberately: keying on the
host would throw all of them away on every rebuild, and the refresh on
opening writes back the frame that was just presented — so a still a
rebuild made wrong heals the moment it is looked at.

They are filled at two moments, and never while a sketch is being
presented.

**The fill, at launch.** The window comes up on the browser with the
canvas dark, and draws a still for every sketch that has none: the
sketch's kind opened and stepped to its declared moment — the same
capture the CPU plate tier takes — scaled to the thumbnail size, one at a
time, on the CPU and never touching the device. That holds whatever the
process installed: a still opens every kind on the CPU runtime — a set's
whole frame and a 2D body's mesh painter alike — so a sketch's thumbnail
is drawn on the mesh executor even in a window whose live canvas is
lighting sets on a device. The status strip counts
them off, `thumbnails 12/41 …`, and each row fills in as its file lands
without remounting the others; a row on screen is moved to the front of
the queue, so what you are looking at is drawn first. **Opening a sketch
ends the fill** — the walk in flight is let go at its next frame and the
queue is dropped — and the fill finishing opens the sketch the run was
pointed at. A run that named a sketch (`--sketch`, a file on the command
line) or that is here to photograph or measure one (`--shot`,
`--window-bench`) opens at once and never fills.

**One still is bounded.** A sketch whose walk runs past the per-sketch
budget, and a sketch that declared itself a plate with `ctx.plate()`
(which is a statement that its subject costs what a plate costs), is
abandoned and gets a one-line NOTE beside where its still would have
gone, under the same key: the note stands in for the picture, the fill
moves on, and the question is asked again only when the sketch's source
changes. `--thumbnail-budget <sec>` names another budget and
`--thumbnail-heavy` walks the declared plates as well. A sketch that
could not be drawn at all is named once in the status strip and not tried
again this run.

**The refresh, on opening.** Once a sketch is presented, its session is
photographed once — as it reaches the moment it declared, or after a
second of its own clock when it declares none — and that frame is written
into the store under the sketch's current key. The encode and the write
are not in that frame — only the repaint and the readback are, because
only they need the thread the frames are drawn on; the pixels go to a
worker beside it and the row is told once the file has landed. A run
measuring frames (`--window-bench`) is out of the refresh entirely: the
photograph's repaint and readback are still taken on the render thread
and inside a frame, which is the one thing a stretch whose whole
subject is how long a frame takes cannot have in it. So the stills
refresh as you browse, they are the frames you were looking at, and nothing renders
in the background to keep them current. A sketch with no thumbnail yet
gets a drawn glyph for the runtime it draws through.

`Sketchbook --thumbnails` fills the store headless, over the same budget
and writing the same notes, and exits non-zero naming the sketches that
failed. It is the same render the window's fill takes, down to the
runtime: a set is drawn on the CPU mesh executor either way.

**What is not in a row is the canvas.** A sketch declares its size, its
ground and the moment it names from inside its own setup, so those are
facts of a RUNNING session and cannot be read off a file that has not
run. They fill in as sketches are presented and the browser keeps them
afterwards, and a row that has never been presented says so rather than
guessing.

### How a sketch introduces itself

The inspector reads prose and subject tags from the top of the sketch's
own file. The rule is small on purpose, so an author can write to it:

The header is every line from the first line of the file down to the
first line that is neither a comment nor blank — a run of line comments,
a doc block, or one after the other. The comment markers come off, and a
line reading only `@file` is dropped. What is left reads as
**paragraphs**: runs of non-blank lines, broken by blank lines and by
rule lines (a line of nothing but `=` or `-`). Python headers accept the
opening module docstring and `#` comments; their first paragraph is the
subject, without a separate title paragraph.

* **The subject** is the first paragraph after the title paragraph — the
  title being the first one, which by convention opens `stem.cpp — …`.
  A one-line paragraph that ends no sentence is a heading: it is kept and
  read on into the paragraph below it, so a file that puts `THE PATTERN`
  over its opening prose shows both.
* **Edit these first** is the paragraph opening with a line that reads
  exactly `EDIT THESE FIRST`, minus that line — the knobs the author says
  to reach for, stated once, beside the code they name. It keeps one line
  per knob: a line indented deeper than the first is an entry that ran
  past the file's own margin, and it rejoins the line above.

* **Tags** come from lines beginning `TAGS:`. Commas separate paths and
  slashes nest subjects: `// TAGS: Typography/Paragraph, Motion/Text` files
  one sketch in both groups. Spaces around components and empty components
  are removed; repeated paths count once. Tag lines never become subject
  prose or editing instructions.

All three are optional. A file without prose omits those blocks, and a file
without tags remains available under Untagged and its collection.

## Plates

`--headless <outdir>` renders every selected sketch to
`<outdir>/plate_<name>.png` and prints a timing table beside it.

The capture is a function of the **declared moment** and of nothing a
machine decides. Everything the timing table does is a time budget, so
the frames it spends depend on how fast the machine is; a plate cannot
be allowed to. So a sketch that names its moment is reopened and stepped
from zero at a fixed 1/60 to that moment, and one that names none is
topped up to a frame derived from the benchmark caps. `--ledger` skips
the benchmark phases entirely and goes straight there, which is most of
a sweep's wall clock — and produces a bit-identical plate, because the
capture never depended on the phases in the first place.

The runtimes make a plate differently, and each way is load-bearing. A
drawn tree is resolution-independent, so its still is one more frame
re-rendered at up to twice the canvas — a texture bake re-runs at the
capture scale rather than being upsampled. A lit set is FORMED at one
resolution and its still describes nothing, so there is nothing to form
again larger and its plate is the frame it just finished. A pen's
canvas holds every frame's residue at the resolution it was formed at,
so its plate, too, is the frame just finished. `Session::still()` is
that seam.

Which resolution that is comes off the canvas a host hands over. A
plate's canvas is the declared size and carries no transform; a live
window's carries the fit AND the screen's own scale, and a set formed at
its declared size and then fitted upward would be a magnified picture of
a smaller one. So a set reads the scale off the canvas it is given,
forms its frame at that many pixels, and puts the result back on the
declared canvas — which on a plate's canvas is the identity, and is why
the two hosts agree to the byte.

### The promoter, and the one lane that exercises it

A headless session is opened DETERMINISTIC, and a deterministic session
holds the composer's automatic texture promotion off. The promoter
decides by a stopwatch — a node whose paint measures over a millisecond
for eight frames is baked and blitted thereafter — so whether it fires
depends on how busy the machine is, and with it on the same binary draws
two different plates. Holding it off is what makes a hash a verdict.

The cost is that the whole sweep renders the runtime with one of its
features switched out. `--promotion` is the door back: it opens every
session with promotion ON and changes nothing else — same clock, same
fixed step, same declared capture moment, same `ctx.measured()` pins —
so the only difference between the two renders of a scene is the
promoter. `--no-promotion` and `--promotion` ask for opposite runs and
naming both is refused.

IT OPENS THEM EAGER. The stopwatch that makes the promoter load-dependent
would make the lane load-dependent too: on an idle machine nothing
crosses the bar and the run reports a clean sweep it did not earn, while
on a loaded one a different handful of nodes crosses it each time. So
`--promotion` asks for the eager policy — every node the composer's rules
admit is baked from its first frame, whatever it costs — and nothing
about what a bake is allowed to do changes. One scene therefore exercises
the same node set on every machine, and it is the whole promotable set
rather than the few nodes that happened to be slow.

…UNLESS THE SKETCH DECLARED OTHERWISE. `ctx.nonlinearPicture()` says the
sketch's picture is not linear in what went into it: it ends on a step, a
round, a gate or a reciprocal — a view transform quantizing each channel
to a palette, a bright pass through a smoothstep, anything that
unpremultiplies and so carries a gain of 1/alpha. There is then no bound
between a difference UNDER that stage and the difference it shows, so one
code value the promoter is allowed to cost arrives as a whole step, in a
place the difference was never in. Such a sketch holds the promoter off
from its own setup whatever a host asks for, its picture is drawn from
live paint everywhere, and a sweep that asked for the promoter prints
`<name>: declared nonlinear` so the tier can say the scene stood under
its own declaration rather than reporting an agreement it never tested.
The declaration belongs to the sketch, which is where the ablation
showing that the picture UNDER the stage is right has to be stated;
`spacejam_1996` and the three `eva_magi_*` plates carry it.

What comes out is not byte-comparable and is not meant to be. A promoted
node is baked under the live matrix post-translated by an integer, and
inverting that matrix to find a shader's local coordinates does not
cancel the integer to the last bit at a scale whose reciprocal is
inexact, so a shaded pixel can land ONE code value from the live paint
and nothing may land further. Where the held-off plate holds CONTENT the
bake lands on something, and there the bound is two — the node's own
coverage rounded into the bake and the bake rounded onto what it lands
on — PER CACHED RASTER the pixel stood under. Nothing nests, but
independent nodes overlap, and a stack of concentric rings each promoted
on its own puts seven or nine of them over one pixel; each rounds, and
the rounding does not decay. `--composites` writes the count beside each
plate, `counts_<sketch>.png`, one grey level per device pixel, and
`--compare` prices the content difference by it. A difference past that
is a picture that moved — a bake somewhere else, rasterised against
another clip, or gone stale — and that is a defect in the promoter rather
than a plate to adopt.

`scripts/sigil.py plates` drives this: three tiers over one binary. The
CPU tier judges every sketch, canvas and set alike, on byte identity
against one baseline manifest; the device tier renders the same sketches
through the device and judges each against the CPU plate of the same
run, per colour channel; the promotion tier renders each scene with the
promoter held off and again with every promotable node eagerly baked,
and judges the pair within one code value. Only the CPU tier keeps a baseline. The judgement itself is
`scripts/README.md`'s.

## The live host

Saving a sketch recompiles it into a small dylib and hot-swaps it into
the running canvas. This is the pattern C++ live coding converged on — a
thin host executable plus a recompiled guest library, rather than
embedding a scripting language — so a sketch never leaves the real API.

* The host executable exports the framework's symbols, so a sketch dylib
  links with `-undefined dynamic_lookup` and builds in a couple of
  seconds: a few small translation units, nothing linked against the
  static libraries. The units — the entry, the sources beside it when
  the sketch is a directory — compile side by side
  into cached objects and link once; a unit is compiled again only when
  its preprocessed contents, compiler settings, or native host build differ.
  The live watcher follows literal quoted local includes recursively;
  the cache lookup resolves all includes through the compiler.
* **The guest compiles hidden**, with `-fvisibility=hidden
  -fvisibility-inlines-hidden` on top of the flags the build captured,
  and that is what makes the file on disk the thing that runs. A sketch
  reaches its host through weak definitions — the vtable and typeinfo
  of the body the registration instantiates for it, `kindOf<T>` and the
  other function templates the macro takes the address of — and
  weak definitions COALESCE. Every image exporting one names the same
  symbol, and the dynamic loader binds them all to whichever came first. The
  executable is always first and carries its own copy of every sketch in
  the registry, so a guest at default visibility would hand back an entry
  whose factory is the host's: the build reports, the dlopen succeeds,
  and the picture is of the file as it stood when the host was built.
  The same rule runs the other way between two builds of one guest,
  since old libraries are never unloaded — build 1 would beat build 2 and
  an edit would never appear, for a sketch outside the registry too.
  Hidden visibility closes both directions at once, because a definition
  that is private to its image joins no coalescing set in either. What
  hidden does NOT touch is an UNDEFINED reference, so the framework still
  resolves out of the host exactly as before; the two entry points the
  registration macro exports carry `visibility("default")` explicitly, so
  `dlsym` finds them. The cost is that a guest gets its own copy of every
  inline the host also has, which is right for code and would be wrong
  only for a mutable static inside one, and typeinfo equality survives
  because a duplicated typeinfo is compared by name. `--frame` on a
  registry sketch with one colour changed is the whole of the proof, and
  the `sketch_reload_runs_the_file` test is exactly that.
* **The build directory belongs to the run that made it.** The objects
  and one dylib per build stand in `<temp>/sigil_sketch_<pid>`, shared by
  every host in the process, and it is removed when the last of them is
  destroyed and again on normal exit — a `--frame` or `--bench` run,
  which ends right after its build, takes its own with it, and a
  `--headless` sweep walks the compiled-in registry, hosts nothing and
  makes none. Removing it disturbs nothing: no dylib is ever dlclosed,
  and an unlinked file that is mapped stays readable until the last
  mapping goes. Reusable builds live separately in the persistent cache.
  A run that was
  killed or that faulted never reached that removal, so before a host
  makes its own directory it removes the sibling ones whose pid no
  process holds; a live pid's directory is never touched, this process's
  own least of all.
* **Successful C++ builds survive session eviction and application restarts.**
  The persistent cache lives in `~/Library/Caches/SigilSketch/builds` on macOS,
  or `SigilSketch/builds` under the XDG cache directory on other platforms.
  `SIGIL_SKETCH_CACHE` selects another directory; an empty value disables it.
  The compiler preprocesses each unit to validate all resolved includes and
  macros. Compiler version, flags, and the running native image's path and
  pinned build timestamp also participate in the key. A matching artifact
  is copied into a unique runtime path before loading, so sessions do not
  share sketch-owned static state merely because they share a cached build.
  Missing artifacts compile normally; failed builds do not populate the cache.
  Unchanged C++ source still needs preprocessing on a cache lookup, but no
  code generation or link. Bundled examples open their compiled-in bodies.
* **A build is named for the host that made it** —
  `sketch_<host>_<build>.dylib` — because every host in a process links
  into that one directory. Named by
  its build number alone, the three resident hosts would all write
  `sketch_1.dylib`: two of them building at once race for the path, and
  the file standing there when one of them dlopens is whichever link
  finished last, so a host adopts a sketch it did not build. The image
  already loaded is safe either way — the linker replaces its output
  rather than rewriting it, so the inode a mapped dylib is reading stays
  alive under it — and it is the gap between a link and the dlopen after
  it that an id per host closes.
* Compile errors overlay while the **last good sketch keeps running**.
* Old libraries are never unloaded. Their statics stay valid — a running
  session may hold a vtable or a string literal that lives in one — and
  one small leak per reload is the trade.
* A sketch this binary already carries opens **instantly**, and the file
  is watched from where it stands: an edit builds, an unedited file
  never does.
* The last **three** sketches opened stay resident. Selecting one swaps
  which of them the window presents rather than building it again, so
  setup runs once per sketch instead of once per visit and the rolling
  frame windows behind the readout survive a look at something else — a
  sketch you come back to shows its own numbers, not a ring filling from
  zero. What leaves is the one presented longest ago. An EDIT is not a
  switch: a rebuild restarts its own session from nothing, which is
  exactly what an edit wants.
* The watch covers **everything the sketch is built from**: the entry
  every poll, and on a short cadence the headers standing beside it, the
  units beside it when it is a directory sketch, and local headers reached
  through quoted includes. A helper beside a sketch is reached by a quoted
  include, which resolves relative to the including file and needs no
  include path — so saving the header rebuilds, rather than leaving the
  code that stood before the edit on screen with nothing saying so.
  Beside a BARE sketch the other sources are other sketches, and saving
  one of them is nothing to this one.
* After rebuilding the framework itself, restart the host. The ABI
  version guards deliberate changes to the sketch surface; a separate
  guard refuses to compile while ANY of the framework libraries' public
  headers postdates the running binary, because a dylib built against
  newer headers loads into a host whose structs have the old layout and
  the crash points nowhere near the cause. Every one of those headers
  counts, whatever it happens to declare: a sketch fills a pool the host
  then resizes and builds an element the host then reconciles, so a
  layout read one way on each side corrupts wherever the object is next
  touched.

### A workspace: sketches outside this repository

A `.cpp` path is taken **wherever it stands**, and the app opens on it:

```sh
Sketchbook ~/sketches/my_experiment.cpp
```

One verb writes such a folder, so a new one starts from a sketch that
runs rather than from an empty directory:

```sh
python3 scripts/sigil.py workspace new ~/sketches/aurora_drift
```

It writes the folder's name as the sketch — `aurora_drift.cpp`, a sketch
on this vocabulary that stages a canvas, reads a file through
`ctx.assets` and states the theme's sheet on its root — beside `assets/`,
`captures/` and a README stating the contract below. Files and nothing
else: no build tree, no CMake package, no install step.

The file joins the app's list under its own stem, filed under
**Workspace** with the directory it came from beside the name, and it
compiles, hot-swaps and captures exactly as a sketch in this repository
does. The registry is the compiled-in table and settles the first time
it is read, so the file cannot join it; it joins a session-local list
the listing reads after it, which is why the stem is the name — the
dylib a hot-loaded sketch exports carries neither key nor name of its
own.

So a workspace is just a directory:

```
~/sketches/
  my_experiment.cpp     one sketch, opened by path
  palette.h             a helper, reached by a quoted include
  rain/
    rain.cpp            a sketch that is a directory, opened by its entry
    drops.cpp           a unit of it
  assets/               what mounts at res://
  captures/             where the app's Capture writes
```

The directory form and relative local includes work the same way wherever
the entry stands. Framework headers come from the flags this checkout builds.

For a sketch opened by path, `assets/` beside it is the `res://` root and
`--assets <dir>` names another; in this repository `res://` is the demo
assets root, `build/assets`, which `mise run assets` fills. A sketch's
OWN files stand in its directory, under `data/`: `ctx.local("data/x.csv")`
is the URI of `data/x.csv` under the directory the sketch's entry stands
in — `sketch://<key>/data/x.csv`, the sketches folder mounted at
`sketch://` — which `ctx.assets.table()`, `ctx.assets.image()`,
`ctx.assets.database()` and the hub take as they take any URI. For a
directory sketch that directory is its own, for a bare file it is the
folder the sketches share, so a sketch that carries data of its own is
written as a directory, and a workspace sketch opened by path has the
files beside that path. A `.sqlite` or `.duckdb` file is a data source
like a CSV is: `ctx.assets.database(ctx.local("data/cities.sqlite"))` opens
it in place, cached and reopened when it changes, and its `query()`
answers the same `Table` the CSV decodes to. Saving `palette.h` rebuilds
the sketch that includes it.
Compiling is what makes a workspace file visible, so the flags the build
captured have to be there: the workspace and the `Sketchbook` it opens
in are the same machine and the same checkout, and after rebuilding the
framework the host is restarted like any other.

What a workspace does not get: the plate sweep. `--headless` walks the
registry, which is the compiled-in table — a workspace file is
photographed with `--frame` and measured with `--bench`, one file at a
time.

### One surface, read twice

What a sketch may `#include` is `SigilSketches`' PUBLIC dependencies —
the flags a hot-reloaded sketch compiles with are lifted out of the
compilation database from `sketches/Anchor.cpp`, a source of that same
target, so the include surface cannot drift between a compiled-in sketch
and a reloaded one.

What a sketch may **link** is read off the same target: at configure
time `src/sketch/cmake/SketchLinkSurface.cmake` walks `SigilSketches`' link closure
and force-loads into Sketchbook every archive of this repository's in it
— the public ones, the private ones riding beneath them, and the ones an
optional SDK produced on the machines where it did — with Skia, the one
vendored archive a sketch calls directly, named beside them. An archive
added to the sketch target is therefore in the host without a second
list to keep in step. The failure that list guards against is invisible
everywhere but one place: every sketch still compiles, every compiled-in
sketch still runs, and only a reloaded one fails at `dlopen` with a
symbol not found in the flat namespace — and only for a symbol no
compiled-in sketch happened to pull in, which is why a full tree hides
it and a narrowed one bites. The `sketch_reload_surface` tests exist for
exactly that, one per runtime, and they must go through the dynamic path
to see it.

## Layout

```
src/sketch/
  core/       what a sketch is, what it declares, the registry, the kind seam, the crash reporter
  canvas/     the 2D runtime: a clock, a ticker and a Composer
  set/        the 3D runtime: a ticker and a retained Scene
  kit/        the sheet a sketch stands on: the theme, the page and the furniture over it
  live/       the reload engine, the resident set and the sweep's cadence
  scry/       the opt-in shared Ultralight engine a web sketch borrows
  plate/      the headless sweep, the montage, the plate comparison, the thumbnail store
  book/       Sketchbook: the app, and the headless entry point, with the browser's rows
  cmake/      SketchLinkSurface.cmake, the link surface a reloaded sketch is read against
  test/       support/, the fixtures every feature's cases share
  sketches/   every sketch, one file or one directory each
```

Directories and headers are the same outline — a feature at `canvas/`
keeps its headers under `include/sigilsketch/canvas/` and its own
`test/` and `bench/` — and the sketch targets are:

| Target | Kind | What it is |
|---|---|---|
| `SigilSketch` | static archive | `core/`, `canvas/`, `set/`, `live/`, `plate/`, and `scry/` where the SDK is installed: the registry, the two runtimes, the reload engine and the headless renderer. Links no device backend and no Qt. |
| `SigilSketchKit` | static archive | the sheet a sketch stands on, over the canvas runtime alone |
| `SigilSketches` | object library | every sketch, and the one place the sketch API surface is stated |
| `Sketchbook` | application bundle | the host: the window, the browser's rows, and every headless entry |

Beside them stand `sketch_test`, `sketch_bench`, and the build step that
writes the response file a hot-reloaded sketch compiles with. Native frame
publication and subscription come from `SigilIOPublish` in `common/io/publish/`.
Sketchbook uses the shared Qt publication adapter; `Guest` wraps a native
subscription for canvas and material use. Seer owns the texture
monitor and PNG capture workflow.

## Boundaries

* **`core` draws nothing.** What a sketch is, what it declares and the
  registry it joins are stated without a runtime in reach; nothing under
  `core/` could paint a pixel.
* **Every host has a guest, so the crash reporter is core's.** The live
  host calls into a dylib it just loaded; the sweep opens a hundred
  sketches in one process and calls into each. A fault inside one is a
  fault inside the host either way, and without a handler the process
  dies with a bare signal and says nothing — on a sweep, the last line
  another sketch happened to print is then the only evidence of which one
  it was. `installCrashReporter` names the file a host watches,
  `noteSketch` the entry a walking host is on, `notePlates` how far the
  run got, and `PhaseMark` what the host was doing. The handlers write
  with `write(2)` and `backtrace_symbols_fd(3)` alone and read only
  buffers filled before any fault could land.
* **The runtimes do not know each other's bodies.** `canvas` links
  compose, `set` links world, `draw` links SigilDraw, and none describes
  through another's runtime. What crosses between them is a picture,
  through the two doors on the contexts — a compose tree painted into a
  texture, a world frame baked to an image — and each door names the
  other library's value by forward declaration alone, with the archive
  behind it linking that library privately. A sketch that walks through
  a door includes that library's own headers.
* **No runtime links a device.** The runtime a session draws through is
  a value the process installs once — one device, one queue, every
  session — so a machine with no device runs every set on the CPU mesh
  executor and the plates it makes are the ones the byte-identity tier
  hashes. `book/` is the only place that installs one, and it installs
  three: `sketch::useRuntime` for the frame a set draws,
  `sketch::usePainterRuntime` for the mesh draws a canvas sketch takes,
  and `sketch::useDevice` for the device itself, which a call that
  imports a foreign texture names and no runtime can stand in for.
* **The canvas runtime installs the text material resolver.** A text
  pass carrying a material shades through a resolver SigilWeave's paint
  feature holds and does not supply, because that feature links no
  renderer, and an unresolved pass draws its plain paint instead. Unlike
  a device runtime this needs nothing the machine may lack, so it is the
  runtime's own rather than the application's: the first canvas session
  a process opens installs SigilMaterial's Skia backend over the pass's
  bounds, and a host that installed its own resolver first keeps it.
* **The force-load list is every archive the host links.** A sketch
  dylib resolves the framework out of the host, so a symbol the host
  does not contain stops the load. The list is walked from two roots —
  what `SigilSketches` hands its consumers, and what the host links
  itself — because an archive only an application brings up, a device
  backend among them, is in the second and not the first. A gap there is
  invisible in every compile and every picture and appears at one
  dlopen, so each root has a reload test that names symbols nothing else
  does.
* **The live host is Qt-free.** Everything about watching, compiling and
  swapping is in `live/`; `book/` is the only place a window appears.

## Assets

A sketch reaches for what it did not generate through `ctx.assets`.
`res://` is one root, whichever it is: the demo assets a machine fetched
(`build/assets`) in this repository, `assets/` beside a sketch opened by
path, or the directory `--assets <dir>` names. A sketch's own files stand
in its directory under `data/` and are named by `ctx.local()`.
A `.json` file there is a document: `ctx.assets.json(ctx.local("data/content.json"))`
reads it whole as one nested `data::Json` — a sketch whose words, lists
and settings stand in such a file reads them in `setup()`, and an edit to
the file re-runs setup without a rebuild, which is the live-editing door
for content. A key that is not there reads as a null value, so the sketch
states its fallback where it reads.
`image()` keeps the
forgiving contract a live-edited file wants — a magenta placeholder
stands in for a missing or undecodable file and heals the moment one
appears, re-running the sketch's declaration — and `hub()` opens the
full resource surface without the sketch ever touching the filesystem.
`video()` opens encoded bytes as a streaming SigilVideo clip, caches one clip
per URI and decode policy, and drops those clips when the hub observes the
source changing. A video keeps only its small decoded-frame cache; the asset
store does not expand the whole timeline into images.

### A resource that keeps arriving

A sketch that listens rather than loads reads a FEED through the same
hub. The store registers the UDP transport on the hub it builds —
`sigil::io::registerTransports()` — so `ctx.assets.hub().feed(uri)` binds
a real port in a window, and `Hub::feed()` answers the one feed a URI
names for as long as the sketch holds it. `Feed::latest()` is the newest
message that reached it, for a scene that draws the state it was last
told; `Feed::receive()` drains in order the ones this frame has not seen,
for a scene that folds every message in. Neither ever waits.

The host moves those feeds forward, and there is nothing for a sketch to
call: both runtimes hand `Assets::dispatch()` the scene time the frame is
being drawn at, after the clock has advanced and before the sketch's own
body runs, which is `Hub::dispatch()` over every feed the sketch opened.
A window and a headless capture step through the same call, so a sketch
reads what had arrived by the moment it is drawing either way.

That is what lets a CAPTURE read the port out of a file. A recording is a
feed written down, and a URI that resolves through the mount table to one
is played back from it instead of being opened, each arrival delivered at
the scene second it was recorded at. So a sketch mounts its own recording
while it is being captured and opens the same port either way:

```cpp
void setup(SketchContext& ctx) {
  sigil::io::Hub& hub = ctx.assets.hub();
  if (ctx.deterministic)  // a plate reads the file the window heard
    hub.mount("udp://:27020", hub.resolve(ctx.local("data/sky.feed")));
  m_sky = hub.feed("udp://:27020");
}
```

`Hub::mount()` is the whole of it: the code that listens is the code that
reads, so a plate is the arrivals rather than a picture of an empty port,
and a sweep that steps from zero delivers them at the seconds they were
heard at. A sketch's recording stands under `data/` beside it like any
other file it carries.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/sigil.py setup --config Release
cmake --build build --config Release --target sketch_test
ctest --test-dir build -C Release --output-on-failure
```

A suite is selected by its own name — `ctest -R '^SketchRegistry\.'` — and
a case by its full one, with no target behind either.
The library has one test binary, `sketch_test`, built from every
feature's `test/` directory; ctest discovers one entry per CASE out of
it. `core/test/` covers the registry, the kind seam, the crash reporter
and where a sketch stands on disk; `canvas/test/` and
`set/test/` the two sessions;
`kit/test/` the sheet a specimen stands on; `live/test/` the
host, the resident set and the cadence a window sweep keeps;
`plate/test/` the sweep, the comparison
of two directories of plates and the montage MP4 exporter;
`book/test/` the reload path and the catalog's rows, each through the
`Sketchbook` binary as a script; and `scry/test/` the shared web engine
beside the case that
takes a page's still — two cases that must not meet in one process because
the engine allows one renderer per process and the shared-engine case
ends by shutting its one down for good. Both are labelled `ultralight`
and are absent altogether without that SDK. Beside the test binary
stands one bench binary, `sketch_bench`, built from every feature's
`bench/` directory — a Google Benchmark executable rather than a test,
reached through the `benches` target.

A case asserts one behaviour a session or a host promises to a caller who
has read only this page, and its name is that promise written as a
sentence. It pins only what editing this library could falsify — a step
count off a clock the host steps rather than reads, a projection's own
arithmetic, the bytes two runs of one declaration agree on, the width a
plate comes out at read from the constant the sweep uses — never a fitted
tolerance, an anti-aliased byte or elapsed time. Pixel identity across a
change is the plate ledger's to judge and what a frame costs is the bench
ledger's. A claim made N times with one thing varying is one `TEST_P`
with its rows named: which file an edit landed in and whether it is part
of the sketch, over `AHeaderBesideABareSketch`,
`AUnitBesideADirectorySketch`, `AnotherBareSketchBesideIt` and
`AModuleInTheSharedLayer`.

**What every session promises is written once.** A host steps, repaints
and photographs a session without ever learning which runtime it is
holding, so the six claims that follow from that live in
`test/support/Sessions.h` — the canvas the body declared while it opened,
the runtime the kind names, the lanes the runtime spends, a frame as the
body's own time plus the runtime's, the oversample a still is worth
taking at, and a repaint that draws the state the frames left and
advances nothing. Each session's cases instantiate them with a traits
type naming its own fixture sketch, and what is left in each session's
file is what only that runtime does: a canvas re-renders for its still
and so takes one more step, a set is formed at the resolution of the
canvas it is handed rather than magnified onto it.

Two of the entries run no C++ at all. `sketch_readme_stems` resolves every
sketch stem the documents in this tree name against the registry: a
backticked snake_case token in a paragraph that is talking about
sketches, studies or a study must name a file under `sketches/`, and one
that does not is either exempted by name and reason in
`test/readme_sketch_stems.py` or fails the run. It refuses a count of a
list too — a cardinal qualifying "studies", "sketches" or "scenes"
beside a named stem, when it claims the list's own length, is maintained
by hand in lockstep with the list and goes stale the moment the list
grows, so the count is deleted and the list is the count. It is the
registry's check, which is why it is here rather than beside each
document. `sketch_readme_stems_self_test` runs the checker's own
fixtures, and is the only thing that would notice the extractor
narrowing: a checker that silently resolves fewer stems still passes over
the corpus.

Fixtures live in `test/support/`, reached as `"support/<name>.h"`, and
nothing is written twice. `Fixtures.h` is the one asset store a process
holds — never destroyed, because it outlives every session opened over it
— beside the font context the whole tree shares, which is
`src/test/Fonts.h`'s and not this library's. `Pixels.h` takes the
readings off a picture: what a surface or an image holds, whether two
plates are one picture, the box the drawn pixels stand in, and where that
box stands as a fraction of the plate so two plates of different sizes
can be compared. `Sessions.h` is the contract above. `live/test/Fixture.h`
holds what both halves of the live feature's cases need beyond them: the
compiled-in square, its registry entry, and a `Watched` file standing in
a scratch directory of its own — bare, or in a directory named for it,
which is the other shape a sketch takes — which the shared
`src/test/ScratchDir.h` empties on the way in and removes on the way out.
The plate cases register their fixture sketches the way a sketch file
does, so the sweep it drives walks a real registry — including one whose
`available()` probe says no, which the sweep passes over rather than
failing on and writes no plate for.

A wait inside a test is a COUNT OF TURNS and never an open loop: a build
polled to completion and a forked child read to its fault both give up
and say so, because a run that hangs reports nothing at all where a run
that fails names the claim that broke.

`Host::Options::siblingScanInterval` names how long the host waits
between re-reads of the headers standing beside the sketch. It defaults
to a quarter second, because reading a directory is cheap but not free
and a header is saved by hand a moment before the sketch is; a test that
edits a header and polls sets it to zero, so the edit is seen when it is
made rather than whenever the cadence next comes round.

### Three ways a sketch is put through a host, and why they are all here

The `sketch_reload_*` entries in `book/CMakeLists.txt` run
`Sketchbook <file.cpp> --frame out.png`, which compiles the file with the
captured response file, dlopens the result and runs it — the DYNAMIC
path, and the only one that can see a missing archive in the force-load
list. The plate cases call `sweep()` IN PROCESS against fixture
sketches its own binary registered. `scripts/sigil.py plates` runs
`Sketchbook --headless --ledger` over the COMPILED-IN registry and judges
plate hashes. Three different things, and none of them stands in for
another.

Within the dynamic entries, one per distinct surface: `shapeworks_lab`
and `first_light` are the widest canvas and set sketches by the symbols
they name, `stock_materials` paints one of every stock material,
`video_compose` reaches the decoder and encoder archives no
geometry-heavy sketch names, `world_hud` is the other registration form,
`dunhuang_star_chart` is the directory form — several units compiled
apart and linked once — and the entries behind an optional SDK name
symbols nothing else does. The archives only the HOST links, a device
backend among them, stand behind a probe file beside that list rather
than behind a sketch, because nothing in the registry names one and what
it asserts is a dlopen and not a picture. A starter sketch that names
none of those adds no entry of its own: anything that stops it compiling
and loading stops the wide ones too.

Every one of those judges a compile and a load, and none of them judges
WHOSE code drew: a host that quietly ran its own copy of the sketch
passes all of them. `sketch_reload_runs_the_file` is the one that looks,
by rendering a copy of a registry sketch whose ground colour has been
replaced and reading the corner pixel back, with the registry's own copy
of the same sketch as the control.

### A host over one sketch while the rest are broken

The sketches come last: library work is expected to break them, and a
host links every sketch it carries, so in the middle of a library pass
no Sketchbook links at all. `-DSIGIL_SKETCH_ONLY=stem;stem` at configure
time narrows the registry a tree compiles to those stems — the directory
is still the only list of what a sketch IS; this says which of them one
tree carries — so a pass over the host can be looked at through the one
sketch it is studying. Leave it empty, the default, for every sketch.
