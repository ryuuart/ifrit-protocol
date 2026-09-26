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

Feature demonstrations use **specimen sheets**: a titled, footed page
with named sections, captioned comparisons and grounded wells. Their look
is one value, `sketch::kit::Theme`, carried through the scope where the tree
is described. `sketch::kit::featureTheme()` gives them a shared slate, ivory
and pale gold palette with interface labels and terminal controls.

```cpp
#include <sigilsketch/kit/Kit.h>

const sketch::kit::Provide look(sketch::kit::featureTheme());
sketch::kit::stage(ctx, {.size = {1100, 424}, .captureAt = 0.05});
ctx.composer.render(sketch::kit::page(
    {.title = "The rule and the strands",
     .subtitle = "Three ways to draw a crossing",
     .footer = "A crossing is discovered, not declared"},
    sketch::kit::section(
        {.label = "CROSSING RULES"},
        sketch::kit::cells({.cells = {a, b, c}}))));
```

The default density is `sketch::kit::Density::Compact`, for grids that need
their fixed specimen measures. `sketch::kit::Density::Spacious` gives pages
of explanations and comparisons larger headings and margins. The theme
does not resize a figure. `sketch::kit::section` supplies the heading and
its gap; `sketch::kit::comparison` aligns the title, control, figure and note
tracks of cases beside each other. Bind the theme again wherever an update
describes another tree.

Reference studies and authored catalog reconstructions keep the faces,
colours and geometry their subject requires. Full-frame drawings and 3D
subjects need no surrounding page merely to join the catalog.

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

**A simulation is stepped by the context's engine, not by the frame
delta.** `ctx.engine` is the session's `motion::Engine`, advanced by the
session on every frame — including the frames a `frameRate(fps)` request
or a `noLoop` skipped, since the node is painted on those and time passed
on them. A timer with a `motion::TimerOptions::stepRate` runs its body at
exactly that rate from accumulated time, and `motion::Timer::betweenSteps`
is the leftover fraction of a step, so a piece drawn as
`lerp(previous, current, solver.betweenSteps())` is one picture at every
draw rate and a capture of it is a claim about the piece rather than
about the machine. Start timers in `setup` and keep the handles on the
sketch: a timer started in `draw` is started again every frame. A fresh
setup gets a fresh engine, so a sketch set up twice is stepped once.

```cpp
struct Cloth {
  motion::Timer solver;
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(640, 480);
    ctx.oversample(2);
    solver = ctx.engine.timer([this] { solve(); }, {.stepRate = 60.0});
    ctx.composer.render(
        compose::graphics("cloth.loop",
                          [this](Pen& pen) { paint(pen, solver.betweenSteps()); })
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
photographed at. A run of entrances staggered by `motion::stagger` is a
schedule of transitions that begin at the mount, so at zero every one of them is still at its
start pose and the picture is the set before it arrived. The clock is the
bake's and not the sketch's: reaching the moment on the sketch's own
engine would step the sketch, and a document photographing a set in one
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
   live values, transitions, the engine's animations and timers. The runtime then
   animates every frame without re-describing anything. Reach for this
   first.
2. `custom()` leaves with `Cache::None` are the immediate-mode floor:
   their paint program runs per frame with the elapsed time.
3. `update()` is the **data** path: when state changes, describe again
   and let the reconciler diff it. Do not re-render every frame out of
   habit — bindings are cheaper.

The first path is the one most sketches reach for last, because the
familiar move is a timer that computes a position and writes it into a
live value. A **bound** value does that at declaration time instead: one
live value carries the clock, and every value derived from it is a named
`motion::Binding` on the property that reads it.

```cpp
motion::Animatable<float> seconds = motion::animatable(0.0f);  // the only thing ticking
ctx.engine.timer([this, &engine = ctx.engine] {
  seconds = static_cast<float>(engine.elapsed().count());
});

// hold, glide down over five seconds, hold, four seconds back — the four
// corners are positions in one 14 s cycle, and the ease rounds both
// shoulders without moving them
list.translateY(motion::bind(
    seconds, {.from = {0, 14.0f},
              .envelope = motion::envelope::trapezoid(3 / 14.f, 8 / 14.f,
                                                      9 / 14.f, 13 / 14.f),
              .ease = motion::ease::inOutQuad,
              .to = {0, -overflow}}));

// one second lit out of every eight, starting at 2 s: a pulse, folded on
// its own period, so it repeats for as long as the clock runs
button.opacity(motion::bind(seconds, {.from = {2.0f, 10.0f},
                                      .envelope = motion::envelope::square(1.0f / 8.0f)}));
```

`motion::envelope::cosine` is the swell, `alternate` the there-and-back,
`motion::envelope::trapezoid` the loop envelope that can cut while it is
dark, `motion::envelope::square` the pulse, and `motion::envelope::shaped`
takes a shape of your own. Each replaces the `std::sin`, `std::fmod` or
four-branch `if` ladder a timer would otherwise carry, and the value is
then a declared property the reconciler can prune on rather than a write
nobody can compare.

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

How Sketchbook is run — the window, a headless sweep, one still with
`--frame`, the video montage, two directories of plates compared, and
the frame-rate gates `--bench` and `--window-bench` — is the chapter
[RUNNING.md](RUNNING.md).

## The registry and its plates

How a sketch joins the one registry and introduces itself, what a plate
is, and the one lane that exercises the promoter, is the chapter
[REGISTRY.md](REGISTRY.md).

## The live host

How the live host builds, adopts and reloads a sketch, a workspace of
sketches outside this repository, and the one surface read twice, is
the chapter [HOST.md](HOST.md).

## Layout

```
src/sketch/
  core/       what a sketch is, what it declares, the registry, the kind seam, the crash reporter;
              agent/, the registry domain's agent
  canvas/     the 2D runtime: an engine and a Composer
  set/        the 3D runtime: an engine and a retained Scene
  kit/        the sheet a sketch stands on: the theme, the page and the furniture over it
  live/       the reload engine, the resident set and the sweep's cadence;
              agent/, the session and clock domains' agents
  testing/    a host in the test's own process, and the Harness fixture
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
| `SigilSketchRegistryAgent`, `SigilSketchSessionAgent` | static archives | the protocol's `registry`, `session` and `clock` agents, leaves beside the core and the host tier they answer for |
| `SigilSketchTesting`, `SigilSketchTestingHarness` | static archives | a host in the test's own process and the comparison, then the GoogleTest fixture over them |
| `SigilSketches` | object library | every sketch, and the one place the sketch API surface is stated |
| `Sketchbook` | application bundle | the host: the window, the browser's rows, and every headless entry |

Beside them stand `sketch_test`, `sketch_bench`, and the build step that
writes the response file a hot-reloaded sketch compiles with. Native frame
publication and subscription come from `SigilIOFrames` in `common/io/publish/`.
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
`shader()` is the door for a shader a sketch carries as a file: an `.sksl`
file beside it holding one SkSL program, `half4 main(float2 xy)` with the
uniforms and child shaders it declares, compiled into the
`sk_sp<SkRuntimeEffect>` that `material::skia::sksl`,
`material::skia::program` and a pen's shader builder all take —
`material::skia::sksl(ctx.assets.shader(ctx.local("aurora.sksl")))`. One file is one
compiled effect however often it is asked for, and an edit to it recompiles
and re-runs setup without a rebuild. It keeps the image door's forgiving
contract: an edit that does not compile leaves the sketch drawing with the
last program that did, or a magenta checker before any has — a fill, which
as an effect over a layer filters nothing — and the compiler's message,
naming the file, is the host's error log until the file compiles or the
sketch stops asking for it, so the window shows it where it shows a failed
build, after the build's own output when that failed too. A sketch
with a shader is a directory sketch, the `.sksl` beside `<stem>.cpp`. A
material recipe's body is not a whole program — it reads the declarations
the recipe adds — so it stands in a file the same way and is read as text,
`ctx.assets.hub().text(ctx.local("burn.sksl"))`, for `Recipe::body`.
`video()` is `hub.load<sigil::media::Video>` with the options given: one
clip per URI and options, cached and reopened by the hub when the source
changes. A video keeps only its small decoded-frame cache; the asset store
does not expand the whole timeline into images.

### A resource that keeps arriving

A sketch that listens rather than loads reads a FEED through the same
hub. The store registers the UDP transport on the hub it builds —
`sigil::io::registerTransports()` — so `ctx.assets.hub().feed(uri)` binds
a real port in a window, and `Hub::listen()` answers the one feed a URI
names for as long as the sketch holds it. `Feed::latest()` is the newest
message that reached it, for a scene that draws the state it was last
told; `Feed::receive()` drains in order the ones this frame has not seen,
for a scene that folds every message in. Neither ever waits.

The host moves those feeds forward, and there is nothing for a sketch to
call: both runtimes hand `sigil::io::advance()` on `Assets::hub()` the scene
time the frame is being drawn at, after the clock has advanced and before
the sketch's own body runs, which moves every feed the sketch opened.
A window and a headless capture step through the same call, so a sketch
reads what had arrived by the moment it is drawing either way.

That is what lets a CAPTURE read the port out of a file. A recording is a
feed written down, and `Hub::replay()` plays one back at a URI instead of
opening it, each arrival delivered at the scene second it was recorded
at. So a sketch replays its own recording while it is being captured and
opens the same port either way:

```cpp
void setup(SketchContext& ctx) {
  sigil::io::Hub& hub = ctx.assets.hub();
  if (ctx.deterministic)  // a plate reads the file the window heard
    hub.replay("udp://:27020", ctx.local("data/sky.feed"));
  m_sky = hub.listen("udp://:27020");
}
```

`sigil::io::mount()` is the whole of it: the code that listens is the code that
reads, so a plate is the arrivals rather than a picture of an empty port,
and a sweep that steps from zero delivers them at the seconds they were
heard at. A sketch's recording stands under `data/` beside it like any
other file it carries.

## Build and test

How this library is built and tested, the three ways a sketch is put
through a host, and a host over one sketch while the rest are broken,
is the chapter [TESTING.md](TESTING.md).
