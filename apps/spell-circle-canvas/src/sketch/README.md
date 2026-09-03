# SigilSketch — everything renderable, as one sketch each

A **sketch** is a `.cpp` file that declares a scene — or a directory
named for that file, with the file as its entry and the rest of the
directory built with it. It is real C++ over the full drawing API — no
scripting layer, no markup — and it is three things at once:

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

```sh
cmake --build build --config Release --target Sketchbook
open build/bin/Release/Sketchbook.app          # the app
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook --list
```

## Three runtimes, one seam

A sketch declares what it draws by which header it includes, and the
registration macro reads the rest off the type:

| include | body | draws |
| --- | --- | --- |
| `<sigilsketch/canvas/Sketch.h>` | `sketch::Sketch` | a compose Element tree, onto a canvas |
| `<sigilsketch/set/Set.h>` | `sketch::Set` | a world Frame, on a lit set |
| `<sigilsketch/draw/Draw.h>` | `sketch::DrawSketch` | a pen's frames, p5's way, onto a canvas that keeps them |

The three are a **seam**, not a switch. `Kind` is `core::Erased<KindOps>`:
a value that knows one runtime and one body, and opens a `Session` on
them. Every host here — the registry listing, the live canvas, the
headless sweep, the frame-time gate — drives a `Session` and never
learns which runtime it is holding. Another runtime is therefore a
value someone constructs and hands to `SIGIL_SKETCH`, and none of the
hosts change when one arrives.

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

struct Hello final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
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
  shared/                              the layer beside them, below
```

The rule is one sentence: **a `.cpp` standing in a directory that
carries its own stem is the entry of a directory sketch, and every
other `.cpp` in that directory is a unit of it.** The key is still the
stem, `SIGIL_SKETCH` still goes in the entry and nowhere else, and the
build compiles every unit into the sketch target with the entry. A
directory with no entry of its own name is not a sketch, and nothing
in it is compiled. Its `assets/` and `captures/` stand inside it, since
both stand beside the entry.

What goes in a unit is what an edit to the plate should never compile
again: a data table generated from a source and frozen, a construction
the entry only reads. The live host compiles the units apart and links
them once, and a unit whose source and headers have not changed since
it was last compiled is not compiled again — so a save of the entry
costs the entry, not the table. A bare `sketches/<stem>.cpp` stays what
it was, and a sketch goes from one form to the other by moving.

### The shared layer

`sketches/shared/` holds the modules more than one sketch reaches for
and no library owns yet. It is not a sketch: its sources are compiled
into the sketch target once, and its headers are spelled as
`<shared/Name.h>` — the sketches directory is on the include path, so
the include names the layer it comes from, and the flags a hot-reloaded
sketch compiles with carry that path because they are lifted from the
same target. For the live host every source there is a unit of every
sketch, cached like the sketch's own and dropped from the dylib when the
sketch names nothing in it, so an unchanged module costs a reload only
its link; saving one rebuilds the sketch that is open, which is what
makes it live. A module a library should own is promoted out of here
into that library.

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
The draw runtime honours the same number one step earlier: its plate IS
the canvas it keeps, so the number is a floor on the pixels that canvas
is formed with and every frame is drawn at them from the first — a
declaration there changes the live window too, which is the only way a
kept canvas can be sharpened rather than magnified.

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
struct WebPanelSketch final : sketch::Sketch {
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
bitmaps arrive over the loader's https path, which caches on disk, and
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

`requireCached` asks the loader's cache the loader's own way and never
the network: a machine that has fetched once is available offline
forever after, and one that never has stands down with the first
missing URL as the reason.

A 3D sketch is the same shape with a different body:

```cpp
#include <sigilsketch/set/Set.h>

struct FirstLight final : sketch::Set {
  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(900, 640);
    ctx.captureAt(1.4);
  }
  world::Frame describe(float seconds) override { … }
};

SIGIL_SKETCH(FirstLight, "Set", "A lit set …")
```

`describe` is a **pure function of the scene time**. That is what makes
a plate reproducible: the host steps from zero at a fixed rate and
photographs the declared moment, so the image depends on the declaration
and never on how fast the machine ran.

A p5 sketch is the same shape with a pen — `setup` once, `draw` every
frame, a canvas that keeps what earlier frames drew:

```cpp
#include <sigilsketch/draw/Draw.h>

using namespace sigil::draw;

struct Orbit final : sketch::DrawSketch {
  void setup(sketch::DrawContext& ctx) override {
    ctx.canvas(400, 300);
    ctx.background(20);
    ctx.pen.noStroke();
  }
  void draw(Pen& pen) override {
    pen.background(20, 30);  // translucent: a trail
    pen.fill(255, 120, 80);
    pen.circle(200 + 120 * cos(pen.millis() / 900), 150, 40);
  }
};

SIGIL_SKETCH(Orbit, "Draw", "A ball on a rail, with a trail.")
```

The pen is SigilDraw's, whose README is the canon for its verbs; what
the runtime adds is p5's `setup`/`draw` loop over a surface it keeps
between frames, a clock it owns, and the seed every session's `random`
starts from — so a plate stepped from zero draws the same picture on
every run. `ctx.pen` in setup is for whatever a p5 setup would have set
on the canvas: a style, a font, a first drawing, which lands on the
first frame. `pen.noLoop()`, `pen.redraw()` and `pen.frameRate(fps)`
are honoured by the runtime skipping draws, since the clock is its. The
pointer and the keys arrive through `Session::pointer` and
`Session::key`, which a host feeds in canvas units; headless, nothing
arrives and `pen.mouseX` stays at zero.

**A simulation is stepped by the context's ticker, not by the frame
delta.** `ctx.ticker` is the session's `motion::Ticker`, stepped by the
session's own clock on every frame — including the frames a
`frameRate(fps)` request or a `noLoop` skipped, since time passed on
those too. `addFixed(hz, fn, maxCatchUp, &alphaOut)` runs the body at
exactly `hz` from accumulated time and publishes the leftover fraction
of a step into the Output, so a piece drawn as
`lerp(previous, current, alpha)` is one picture at every draw rate and a
capture of it is a claim about the piece rather than about the machine.
Register in `setup` and keep the Outputs on the sketch, since `draw` is
handed the pen alone; a fresh setup gets a fresh ticker, so a sketch set
up twice is stepped once.

```cpp
struct Cloth final : sketch::DrawSketch {
  ch::Output<float> alpha{0.0f};
  void setup(sketch::DrawContext& ctx) override {
    ctx.canvas(640, 480);
    ctx.oversample(2);
    ctx.ticker.addFixed(60.0, [this] { solve(); return true; }, 8, &alpha);
  }
  void draw(Pen& pen) override { paint(pen, alpha); }
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
and a sketch whose whole subject needs a device says so through
`unavailable(...)` rather than drawing an empty set.

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

## Running one

```sh
Sketchbook [--no-gpu]                       # the app
Sketchbook --sketch <name>                  # the app, on that one
Sketchbook <file.cpp>                       # the app, on that file
Sketchbook --list [--kind canvas|set]       # the registry, one per line
Sketchbook <file.cpp> --frame out.png [--at <sec>] [--scale <n>] [--gpu]
                                  [--frames <count>] [--fps <n>]
Sketchbook <file.cpp> --bench [--bench-frames <n>] [--jitter-dt [amp]]
Sketchbook --headless <outdir> [--gpu] [--sketch <name>] [--kind <k>]
           [--ledger] [--no-promotion] [--capture-at <s>]
           [--timing-json <path>]
Sketchbook --window-bench [<sec>] [--window-size <WxH>] [--window-scale <n>]
… [--assets <dir>]                          # what mounts at res://
… [--plates <dir>]                          # the stills the browser shows
```

`--sketch` takes a case-insensitive substring and answers to a sketch's
filed name or its file stem, which is the loop for visual iteration.
`--shot <png>` captures the app window rather than a sketch, which is
the only way to look at the browser and the inspector.

The app brings a device up and every set draws through it, because a
device is what runs a material's own body: the CPU mesh executor has no
compiler, so a surface reaches it as the colour the frame extracted and a
reader would be looking at a picture no recipe ever ran in. `--no-gpu`
keeps sets on that executor, which is what a plate is hashed from and
therefore what a window is worth putting beside one. A device that will
not come up is reported and the app carries on — unlike the sweep's
`--gpu`, which must fail rather than put two different pictures under one
plate's name.

The app is a macOS bundle, so a headless run goes through the binary
inside it:
`build/bin/<config>/Sketchbook.app/Contents/MacOS/Sketchbook`.

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
still lands where `ctx.captureAt` put it, so the same file photographed
here and photographed by the sweep is the same frame. The line it prints
says which of the three it used. `--bench` keeps the 1.5 s default
whatever the sketch declared: its `--at` is a warm-up that has only to
get programs, bakes and atlases hot, and pinning it keeps the measured
run the same run for every sketch.

`--fps` sets the PRE-ROLL step, not just the capture rate. A sketch
using a fixed-rate steppable has a catch-up clamp, so pre-rolling far
below its own rate discards simulated time and lands earlier than you
asked for. Keep `--fps` near the rate you would actually draw at.

### `--bench`: the 60 FPS gate

One machine-readable line, prefixed `BENCH` so a collector can find it,
carrying the sketch, its canvas, the frame count, the step regime, the
percentiles and a verdict — then a human line naming which phase
dominated, and the runtime's own lanes under it.

The gate is **p99 under 16.6 ms** — a sustained 60 FPS at the sketch's
own declared canvas size. It always exits 0; the verdict is the output,
not the exit status, so it can sit in a pipeline.

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

**It measures what `--bench` cannot.** The gate renders onto a raster
surface at the sketch's declared size and presents nothing, which is
what makes it a gate: the sketch's own cost, isolated. Here the frame is
drawn through the surface the window presents, at the window's pixels
and its device pixel ratio, and the numbers carry the host's own
overhead with them — the submit or texture upload that puts the frame on
screen, and, for a set drawn on a device, the readback and blit its
paint phase performs. Selection goes through the same property a click
sets, so the resident set is in the measurement too.

A presented rate is bounded by the compositor, which means by the
display: a sketch comfortably inside its budget reads at the refresh
rate and says nothing more. The interesting rows are the ones BELOW it,
and the work beside them says how much of that frame was the sketch.

`scripts/app_fps_ledger.py` drives it over the registry and judges each
presented rate against `bench/app_fps_<config>.json` within a stated
band, `--rebase` adopting. The baseline is per machine AND per display
mode, so it records the window size and scale it was taken at and the
run says so when they differ.

## Going through the registry

The app is a BROWSER BESIDE A CANVAS, and the canvas never gives up its
half. Going through a hundred sketches is a matter of looking at one
after another, so the two questions are kept apart:

* **selection is a look.** Arrow keys move it, a click moves it, and all
  it moves is the inspector on the right. Whatever the canvas was
  presenting keeps presenting while you read.
* **Enter presents.** So does a double click, and so does the
  inspector's Open. This is the only thing that changes what is drawn —
  and the resident set is what makes it cheap, because a sketch already
  opened comes back without being built again.
* **A click on the canvas gives it the keyboard.** The pointer over the
  canvas and the keys while it holds focus reach the running session in
  the sketch's own canvas units, through `Session::pointer` and
  `Session::key`, for a sketch that reads them; a click on the list takes
  the arrows back. A sketch with nothing for a pointer to do ignores what
  arrives, and a drag over a set still orbits it.

Two views over the same rows, and the toggle is in the top bar:

* **the list** (the default) — one line per sketch, folded into its
  folder, with the thumbnail, the blurb, the folder, the runtime, the
  canvas, the declared moment and the line count in columns. Clicking a
  column heading orders by it and clicking again reverses; ordering by
  anything but `folder` flattens the folds, because a column you asked to
  read down is one you want to read without interruptions.
* **the gallery** — every sketch as its own still, with the folders as
  chips across the top.

The filter takes free words and field words together, and every word has
to match: a free word narrows on the name, the folder, the blurb and the
file stem at once, while `folder:` and `kind:` narrow on that field
alone — so `folder:study kind:canvas rain` is one question, not three.
`/` puts the cursor in it and Escape empties it.

**The thumbnails are the quick tier's plates**, read from the baseline
the plate ledger writes beside its manifest — so a checkout that has
never run a sweep has no thumbnails, and one that has is looking at
exactly the images the sweep judged. `--plates <dir>` names another
directory. A sketch with no plate gets a drawn glyph for the runtime it
draws through.

**What is not in a row is the canvas.** A sketch declares its size, its
ground and the moment it names from inside its own setup, so those are
facts of a RUNNING session and cannot be read off a file that has not
run. They fill in as sketches are presented and the browser keeps them
afterwards, and a row that has never been presented says so rather than
guessing.

### How a sketch introduces itself

The inspector shows two blocks read from the top of the sketch's own
file. The rule is small on purpose, and it is stated here so an author
can write to it:

The header is every line from the first line of the file down to the
first line that is neither a comment nor blank — a run of line comments,
a doc block, or one after the other. The comment markers come off, and a
line reading only `@file` is dropped. What is left reads as
**paragraphs**: runs of non-blank lines, broken by blank lines and by
rule lines (a line of nothing but `=` or `-`).

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

Neither is required. A file with neither shows neither, and nothing about
a sketch depends on writing one.

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

`scripts/plate_ledger.py` drives this: four tiers over one binary, each
with its own baseline. See `CLAUDE.md` for the tiers.

## The live host

Saving a sketch recompiles it into a small dylib and hot-swaps it into
the running canvas. This is the pattern C++ live coding converged on — a
thin host executable plus a recompiled guest library, rather than
embedding a scripting language — so a sketch never leaves the real API.

* The host executable exports the framework's symbols, so a sketch dylib
  links with `-undefined dynamic_lookup` and builds in a couple of
  seconds: a few small translation units, nothing linked against the
  static libraries. The units — the entry, the sources beside it when
  the sketch is a directory, the shared layer's — compile side by side
  into cached objects and link once; a unit is compiled again only when
  its own source or any header beside the sketch or in the shared layer
  has been written since, one conservative rule that needs no
  dependency scan.
* **The guest compiles hidden**, with `-fvisibility=hidden
  -fvisibility-inlines-hidden` on top of the flags the build captured,
  and that is what makes the file on disk the thing that runs. A sketch
  reaches its host through weak definitions — the class's vtable and
  typeinfo when every virtual is inline, `kindOf<T>` and the other
  function templates the registration macro takes the address of — and
  weak definitions COALESCE. Every image exporting one names the same
  symbol, and the loader binds them all to whichever came first. The
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
  mapping goes. Nothing on disk is read across runs anyway — the
  freshness table that decides a rebuild is in memory. A run that was
  killed or that faulted never reached that removal, so before a host
  makes its own directory it removes the sibling ones whose pid no
  process holds; a live pid's directory is never touched, this process's
  own least of all.
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
  units beside it when it is a directory sketch, and the shared layer's
  sources and headers. A helper beside a sketch is reached by a quoted
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

The directory form is the same rule wherever the entry stands, and the
shared layer a workspace sketch may spell `<shared/Name.h>` against is
this repository's: the flags it compiles with are this checkout's.

`assets/` beside the sketch is the default root, and `--assets <dir>`
names another. Saving `palette.h` rebuilds the sketch that includes it.
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
time `cmake/SketchLinkSurface.cmake` walks `SigilSketches`' link closure
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
  draw/       the immediate-mode runtime: a clock, a ticker, a pen and a surface that persists
  live/       the reload engine and the resident set
  plate/      the headless sweep
  book/       Sketchbook: the app, and the headless entry point
  sketches/   every sketch, one file or one directory each; shared/ beside them
```

Each feature is its own archive with its own tests and benchmarks, and
links only what is beneath it. Every public header lives under
`include/sigilsketch/`, and the directories under it nest the way the
targets do.

## Boundaries

* **`core` draws nothing.** A consumer that only wants to know what
  sketches exist links it alone; it could not paint a pixel.
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
`assets/` **beside the sketch file** mounts at `res://`, and `--assets
<dir>` names another directory instead — one root, whichever it is.
`image()` keeps the
forgiving contract a live-edited file wants — a magenta placeholder
stands in for a missing or undecodable file and heals the moment one
appears, re-running the sketch's declaration — and `hub()` opens the
full resource surface without the sketch ever touching the filesystem.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug --target sketch_core_test \
  sketch_canvas_test sketch_set_test sketch_draw_test sketch_live_test \
  sketch_plate_test
ctest --test-dir build -C Debug -R sketch_ --output-on-failure
```

One test binary per feature, each linking that feature alone:
`sketch_core_test` over the registry, the kind seam and where a sketch
stands on disk, `sketch_canvas_test`, `sketch_set_test` and
`sketch_draw_test` over the three sessions, `sketch_live_test` over the
host and the resident set, `sketch_plate_test` over the sweep.

`test/Support.h` at the library root holds what every one of them opens a
session with — the one font context and the one asset store a process
holds, neither ever destroyed, because a context outlives everything
shaped through it. A test target adds `test/` to its include path and
spells `"Support.h"`. `live/test/Fixture.h` holds what both halves of
`sketch_live_test` need beyond that: the compiled-in square, its registry
entry, and a `Watched` file standing in a scratch directory of its own,
which the shared `src/test/ScratchDir.h` empties on the way in and
removes on the way out. `sketch_plate_test` registers its fixture sketches
the way a sketch file does, so the sweep it drives walks a real registry —
including one whose `available()` probe says no, which the sweep passes
over rather than failing on and writes no plate for.

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
list. `sketch_plate_test` calls `sweep()` IN PROCESS against fixture
sketches its own binary registered. `scripts/plate_ledger.py` runs
`Sketchbook --headless --ledger` over the COMPILED-IN registry and judges
plate hashes. Three different things, and none of them stands in for
another.

Within the dynamic entries, one per distinct surface: `shapeworks_lab`
and `first_light` are the widest canvas and set sketches by the symbols
they name, `stock_materials` paints one of every stock material,
`world_hud` is the other registration form, `dunhuang_star_chart` is
the directory form — several units compiled apart and linked once — and
the two behind an optional SDK name symbols nothing else does. A starter
sketch that names none of those adds no entry of its own: anything that
stops it compiling and loading stops the wide ones too.

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
