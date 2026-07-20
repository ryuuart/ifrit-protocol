# ComposeSketch — live-coding host for IfritCompose

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

Headless single-shot (also the CI smoke test):

```sh
./build/bin/Release/ComposeSketch <sketch.cpp> --frame out.png
```

## Writing a sketch

```cpp
#include <ifritsketch/Sketch.h>
using namespace ifrit::compose;

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
IFRIT_SKETCH(MySketch)
```

- `setup(ctx)` runs on every (re)load and when an asset file changes.
- `update(elapsed, ctx)` runs every frame; re-`render()` for
  data-driven changes, use bindings/transitions for motion.
- `ctx.assets.image("name.png")` loads from the sketch's `assets/`
  directory (override with `--assets <dir>`) through IfritImage —
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
  drift.

## How it works

- The flags are never hand-maintained: `IfritSketchKit`'s PUBLIC
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
