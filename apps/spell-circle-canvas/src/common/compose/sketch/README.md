# ComposeSketch — live-coding host for SigilCompose

A p5.js-style creative loop for the compose framework, without leaving
C++: a **sketch** is a single `.cpp` file using the full real API
(Composer, elements, decorations, Choreograph bindings, custom SkCanvas
programs). The host watches the file; saving recompiles it into a small
dylib and hot-swaps it into the running canvas in ~2 seconds. Compile
errors overlay while the **last good sketch keeps running** — the p5
behavior.

This is the pattern game development converged on for C++ live coding
(cr.h, RuntimeCompiledC++, jet-live, Cinder-Runtime): a thin host
executable plus a recompiled guest library, rather than embedding a
scripting language — so sketches never leave the real API surface.

## Run

```sh
cmake --build build --config Release --target ComposeSketch
./build/bin/Release/ComposeSketch src/common/compose/sketch/sketches/hello.cpp
```

Edit `hello.cpp` and save. That's the loop.

Headless rendering (also the CI smoke test) — the asset pipeline:

```sh
ComposeSketch <sketch.cpp> --frame out.png \
    [--at <seconds>] [--scale <n>] [--frames <count>] [--fps <n>]
```

Steps the clock to `--at` (default 1.5s) at `--fps` (default 60), then
captures `--frames` PNGs (sequences number as `out_0001.png…`) at
`--scale` (default 1: captures match the sketch's declared canvas
pixel-for-pixel — what asset generation wants). In the windowed host,
**Cmd+S** or the capture button writes the current frame to
`<sketch dir>/captures/<name>-NNN.png`.

`--fps` sets the PRE-ROLL step, not just the capture rate. A sketch using
`ticker.addFixed(hz, …)` has a catch-up clamp, so pre-rolling at a rate far
below its own — `--at 8.4 --fps 1` — discards simulated time and lands
somewhere earlier than you asked for. Keep `--fps` near the rate you would
actually draw at; the clamp is behaving correctly.

Asset sketches are the intended workflow for tilemaps, flourishes, and
nine-slice frames: declare the exact canvas (`ctx.canvas(96, 96)`, a
transparent `ctx.background`), draw, export headless, load through
SigilLoader in the demos. `sketches/frame_asset.cpp` is the template.

## Measuring: `--bench` and the 60 FPS gate

```sh
ComposeSketch <sketch.cpp> --bench [--at <seconds>] [--bench-frames <n>]
                                   [--fps <n>] [--scale <n>]
```

```
BENCH penrose_paving 1400x900 frames=120 p50=7.31ms p99=11.02ms mean=7.61ms \
      max=14.90ms fps50=136.8 VERDICT=PASS
  phases (mean ms): update 0.42 [reconcile 0.31] · draw 7.19 [layout 0.04 · paint 7.10]
  last frame: 214 nodes painted, 0 pictures recorded, 96 pictures live, …
  PASS — p99 11.02 ms is inside the 16.6 ms budget (60 FPS gate)
```

The gate is **p99 < 16.6 ms**, i.e. a sustained 60 FPS at the sketch's own
declared canvas size. `--bench` always exits 0; the verdict is the output,
not the exit code, so it can sit in a pipeline.

What it does, and why it is not `--frame`'s "work ms": the capture path
steps the clock on an **8×8 scratch surface**, where every draw is clipped
away — so its number measures describe + reconcile with the pixels
missing, and a sketch whose whole cost is one full-canvas `patterns::grain`
reads as free. `--bench` allocates the real canvas (the surface
`capture()` would make), warms it to `--at` so SkSL programs, `Cache::
Texture` bakes, brush `snapshot()`s and glyph atlases are hot, then times
`--bench-frames` (default 120) genuine frames: clear → tick → ticker →
`Sketch::update` (your `describe()` + `render()`) → `Composer::draw` →
readback.

The phase line answers "what dominates" first:

- **`update` / `reconcile` high** → you are rebuilding the tree every
  frame. Describe once in `setup()` and bind `choreograph::Output`s, or
  `memo()` the subtrees whose props did not change.
- **`paint` high** → per-pixel cost. `debug::coverage` finds the node; a
  static SkSL Material's *shader* caches but its *pixels do not* (a
  picture records the draw call, not the result), so
  `.cache(Cache::Texture)` on a full-canvas material is the usual
  hundred-millisecond win.
- **`pictures recorded` non-zero every frame** → something incomparable
  (a raw `custom()` program, a `StampModFn`, an `ops::PathOp`) is
  defeating the cache. `memo()` the host node or keep the callable
  pointer-stable.

## Writing a sketch

```cpp
#include <sigilsketch/Sketch.h>
using namespace sigil::compose;

struct MySketch : sketch::Sketch {
  choreograph::Output<float> pulse{0};

  void setup(sketch::SketchContext &ctx) override {
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt; pulse = (float)std::sin(t); return true; });
    ctx.composer.render(
        box().inset(40).fill(Fill::color({0.1f, 0.1f, 0.2f, 1}))
             .child(text(u8"hello", style)) /* … the full API … */);
  }
  void update(double elapsed, sketch::SketchContext &ctx) override {}
};
SIGIL_SKETCH(MySketch)
```

- `ctx.canvas(w, h)` / `ctx.background(color)` — p5's
  createCanvas/background: declare the canvas in `setup()`; the window
  letterboxes to it and headless captures honor it. Calling
  `ctx.canvas()` later resizes live. Defaults: 900×640, near-black.
- `setup(ctx)` runs on every (re)load and when an asset file changes.
- `update(elapsed, ctx)` runs every frame; re-`render()` for
  data-driven changes, use bindings/transitions for motion.
- `ctx.assets.image("name.png")` loads from the sketch's `assets/`
  directory (override with `--assets <dir>`) through SigilImage —
  PNG/JPEG/WebP/GIF/AVIF. Missing files render the magenta checker and
  heal when the file appears; editing an asset hot-swaps it.

## Rules of the road

- Keep state in members. Every reload constructs a fresh instance and a
  fresh Composer/Ticker; the shared FrameClock keeps running, so
  `elapsed` is continuous across reloads.
- Old sketch libraries are **never dlclosed** (their statics stay
  valid); one small leak per reload is the standard trade.
- After rebuilding the framework itself, restart the host. The ABI
  version in `Sketch.h` guards stale sketch binaries, not framework
  drift. The host additionally refuses to compile a sketch when any
  framework header on the include path is NEWER than the host binary
  ("framework headers are NEWER than this host binary") — rebuild the
  `ComposeSketch` target and try again. The check is mtime-based, so
  header-only library changes trip it too; that is deliberate.
- **`SketchContext` is a per-frame value the host rebuilds**, so
  capturing it by reference in a `ticker.add()` lambda dangles.
  Capture `this` and reach for the context through the argument
  `update()` and `setup()` are handed. `hello.cpp` sidesteps the trap
  by capturing only `this`, which makes it invisible until a steppable
  needs `ctx.composer`.
- Open-licensed demo assets (fonts, so far) come from
  `cmake --build build --config Release --target fetch_assets`, which
  writes `build/assets/`. Point the host at them with
  `--assets build/assets` or read the `SIGIL_ASSET_DIR` define.

## How it works

- The flags are never hand-maintained: `SigilSketchKit`'s PUBLIC
  dependencies define what sketches may use, `SketchAnchor.cpp`
  compiles the sketch prelude inside the real target graph, and
  `ExtractSketchFlags.cmake` lifts that TU's exact command out of
  `compile_commands.json` into `sketch_flags.rsp` at build time —
  adding a dependency to the kit automatically reaches sketches.
- On save, the host runs
  `clang++ @sketch_flags.rsp -shared -undefined dynamic_lookup` into a
  versioned dylib in a temp dir, `dlopen`s it, and swaps the sketch.
- Symbols resolve from the **host executable**, which force-loads the
  full SDK archives (`-force_load` + `-export_dynamic`) so any compose /
  textflow / image / tick call a sketch makes is present. The one
  exception is Skia: vcpkg builds it with hidden visibility, so its
  symbols cannot be re-exported — sketch dylibs therefore link
  `libskia.a` directly (the linker pulls only referenced objects, which
  keeps builds fast). Practical consequence: the sketch carries its own
  copies of the Skia code it calls; objects passed to the host stay
  self-consistent through their vtables.
- macOS-first, like the repo's GPU paths; the Linux/Windows port mirrors
  this with `--whole-archive`/`/WHOLEARCHIVE` and default dylib symbol
  semantics.

## The same file, compiled in

A sketch is also a **scene**. Compile one with `SIGIL_SKETCH_STATIC` set
to its file stem and the `SIGIL_SKETCH` macro registers a factory in the
host's binary instead of exporting the dylib entry points:

```cmake
set_source_files_properties(sketches/penrose_paving.cpp PROPERTIES
  COMPILE_DEFINITIONS "SIGIL_SKETCH_STATIC=\"penrose_paving\"")
```

`sigil::compose::sketch::findStaticSketch(key)` then hands back a factory
for it. ComposeGallery does exactly this for every study
(`SigilSketchStudies`, an OBJECT library — an archive would drop the
registration, since nothing references it), which is why the gallery's
study list needs no copy of the sketch: the file you hot-reload and the
scene the gallery shows are the same file.

Two consequences worth knowing. The split-Skia rule does not apply to a
statically linked sketch — it shares the host's Skia — so a sketch can
pass the gallery and still break the dylib path; `compose_sketch_stock`
remains the guard for that. And a sketch declares its canvas from inside
`setup()`, so a host cannot know the size until after it has run.
