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

Asset sketches are the intended workflow for tilemaps, flourishes, and
nine-slice frames: declare the exact canvas (`ctx.canvas(96, 96)`, a
transparent `ctx.background`), draw, export headless, load through
SigilLoader in the demos. `sketches/frame_asset.cpp` is the template.

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
