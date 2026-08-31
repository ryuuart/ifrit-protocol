# SigilSketch — everything renderable, as one file each

A **sketch** is a single `.cpp` file that declares a scene. It is real
C++ over the full drawing API — no scripting layer, no markup — and it is
three things at once:

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

## Two runtimes, one seam

A sketch declares what it draws by which header it includes, and the
registration macro reads the rest off the type:

| include | body | draws |
| --- | --- | --- |
| `<sigilsketch/canvas/Sketch.h>` | `sketch::Sketch` | a compose Element tree, onto a canvas |
| `<sigilsketch/set/Set.h>` | `sketch::Set` | a world Frame, on a lit set |

The two are a **seam**, not a switch. `Kind` is `core::Erased<KindOps>`:
a value that knows one runtime and one body, and opens a `Session` on
them. Every host here — the registry listing, the live canvas, the
headless sweep, the frame-time gate — drives a `Session` and never
learns which runtime it is holding. A third runtime is therefore a value
someone constructs and hands to `SIGIL_SKETCH`, and none of the hosts
change when one arrives.

This library sits **above** both drawing libraries and links both. The
arrow only points this way: nothing in compose or world knows this
library exists, and a drawn tree and a lit set meet here and nowhere
else.

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

`SIGIL_SKETCH` takes the folder it files under and one line on what it
is — both shown beside it in the app. `SIGIL_SKETCH_AS` adds a name of
its own, for a sketch filed under something other than its stem because
other things already refer to that name.

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
Sketchbook                                  # the app
Sketchbook --sketch <name>                  # the app, on that one
Sketchbook --list [--kind canvas|set]       # the registry, one per line
Sketchbook <file.cpp> --frame out.png [--at <sec>] [--scale <n>]
                                  [--frames <count>] [--fps <n>]
Sketchbook <file.cpp> --bench [--bench-frames <n>] [--jitter-dt [amp]]
Sketchbook --headless <outdir> [--gpu] [--sketch <name>] [--kind <k>]
           [--ledger] [--no-promotion] [--capture-at <s>]
           [--timing-json <path>]
```

`--sketch` takes a case-insensitive substring and answers to a sketch's
filed name or its file stem, which is the loop for visual iteration.
`--shot <png>` captures the app window rather than a sketch, which is
the only way to look at the sidebar and the metrics panel.

The app is a macOS bundle, so a headless run goes through the binary
inside it:
`build/bin/<config>/Sketchbook.app/Contents/MacOS/Sketchbook`.

### `--frame`: the asset workflow

Steps the clock to `--at` (default 1.5 s) at `--fps` (default 60), then
captures `--frames` PNGs (sequences number as `out_0001.png…`) at
`--scale` (default 1: captures match the declared canvas pixel for
pixel, which is what asset generation wants). Declare the exact canvas,
give it a transparent background, draw, export. `sketches/frame_asset.cpp`
is the template.

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

The two runtimes make a plate differently, and both ways are
load-bearing. A drawn tree is resolution-independent, so its still is
one more frame re-rendered at up to twice the canvas — a texture bake
re-runs at the capture scale rather than being upsampled. A lit set is
drawn from shaded vertices, so a larger canvas would be a different
picture rather than a sharper one, and its plate is the frame it just
finished. `Session::still()` is that seam.

`scripts/plate_ledger.py` drives this: four tiers over one binary, each
with its own baseline. See `CLAUDE.md` for the tiers.

## The live host

Saving a sketch recompiles it into a small dylib and hot-swaps it into
the running canvas. This is the pattern C++ live coding converged on — a
thin host executable plus a recompiled guest library, rather than
embedding a scripting language — so a sketch never leaves the real API.

* The host executable exports the framework's symbols, so a sketch dylib
  links with `-undefined dynamic_lookup` and builds in a couple of
  seconds: one small translation unit, nothing linked against the static
  libraries.
* Compile errors overlay while the **last good sketch keeps running**.
* Old libraries are never unloaded. Their statics stay valid — a running
  session may hold a vtable or a string literal that lives in one — and
  one small leak per reload is the trade.
* A sketch this binary already carries opens **instantly**, and the file
  is watched from where it stands: an edit builds, an unedited file
  never does.
* After rebuilding the framework itself, restart the host. The ABI
  version guards deliberate changes to the sketch surface; a separate
  guard refuses to compile while any repository header on the include
  path postdates the running binary, because a dylib built against newer
  headers loads into a host whose structs have the old layout and the
  crash points nowhere near the cause.

### The two lists that must agree

What a sketch may `#include` is `SigilSketches`' PUBLIC dependencies —
the flags a hot-reloaded sketch compiles with are lifted out of the
compilation database from `sketches/Anchor.cpp`, a source of that same
target, so the include surface cannot drift between a compiled-in sketch
and a reloaded one.

What a sketch may **link** is the force-load list in
`book/CMakeLists.txt`. These are one fact stated twice, and when they
disagree the symptom is invisible everywhere but one place: every sketch
still compiles, every compiled-in sketch still runs, and the reloaded
one fails at `dlopen` with a symbol not found in the flat namespace. The
`sketch_reload_surface` tests exist for exactly that, one per runtime,
and they must go through the dynamic path to see it.

## Layout

```
src/sketch/
  core/       what a sketch is, what it declares, the registry, the kind seam
  canvas/     the 2D runtime: a clock, a ticker and a Composer
  set/        the 3D runtime: a ticker and a retained Scene
  live/       the reload engine, and the crash reporter around the guest
  plate/      the headless sweep
  book/       Sketchbook: the app, and the headless entry point
  sketches/   every sketch, one file each
```

Each feature is its own archive with its own tests and benchmarks, and
links only what is beneath it. Every public header lives under
`include/sigilsketch/`, and the directories under it nest the way the
targets do.

## Boundaries

* **`core` draws nothing.** A consumer that only wants to know what
  sketches exist links it alone; it could not paint a pixel.
* **The runtimes do not know each other.** `canvas` links compose,
  `set` links world, and neither names a type from the other's library.
* **`set` links no device.** The runtime a session draws through is a
  value the process installs once — one device, one queue, every
  session — so a machine with no device runs every set on the CPU mesh
  executor and the plates it makes are the ones the byte-identity tier
  hashes.
* **The live host is Qt-free.** Everything about watching, compiling and
  swapping is in `live/`; `book/` is the only place a window appears.

## Assets

A sketch reaches for what it did not generate through `ctx.assets`. The
sketch assets directory mounts at `res://`; `image()` keeps the
forgiving contract a live-edited file wants — a magenta placeholder
stands in for a missing or undecodable file and heals the moment one
appears, re-running the sketch's declaration — and `hub()` opens the
full resource surface without the sketch ever touching the filesystem.
