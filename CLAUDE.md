# Repository guide

SpellCircle is a receiver application for network-driven vector diagrams:
scene descriptions arrive as FlatBuffers over UDP, are drawn with Skia on
the GPU, and are published as a texture over Syphon. The app is thin —
most of the code is a set of independent libraries under `src/common/`
and `src/sigilweave/`.

## Where the documentation is

**Each library's `README.md` is the canon for that library.** It is
written for someone with no prior context and is verified against the
code. Read the one next to the code you are changing before changing it;
do not reconstruct a library's rules from another library's document.

- `apps/spell-circle-canvas/README.md` — the product: the data path,
  authoring scenes in Python, building and running
- `src/sigilweave/README.md` — text shaping and layout, with
  `src/sigilweave/FEATURES.md` beside it for the control-by-control
  catalogue and the parity table; the paragraph
  engine is rooted here
- `src/common/compose/README.md` — data-driven drawable components, with
  `src/common/compose/TYPOGRAPHY.md` beside it for the type chapter (both
  compile-checked: every API name they spell must exist in a header)
- `src/common/draw/README.md` — SigilDraw: an immediate-mode pen with
  p5's verbs, the imperative way beside compose
- `src/sketch/README.md` — SigilSketch: every renderable thing as one
  sketch, with Sketchbook, the live host and the plates
- `src/common/geometry/README.md` — higher-level drawing over Skia, the
  geometry kit, the point operators
- `src/common/world/README.md` — 3D surfaces on Diligent Engine
- `src/common/material/README.md` — recipes, textures, environment maps
- `src/common/motion/README.md` — animation: clock, values, bindings
- `src/common/core/README.md` — the kernels a retained runtime hosts:
  the reconciler and the caching proof
- `src/common/image/README.md` — image decoding and encoding
- `src/common/loader/README.md` — resource access: URIs, caching, reload
- `src/common/scry/README.md` — HTML and CSS rendered to Skia images
- `src/common/skia/README.md` — SigilSkia: Skia Graphite on a device
  someone else owns
- `src/common/substance/README.md`, `src/common/usd/README.md` —
  optional SDK integrations
- `src/common/ui/README.md` — reusable Qt Quick controls
- `apps/spell-circle-canvas/scripts/README.md` — the checks and ledgers
- `docs/README.md` — the generated C++ API reference

`archive/` directories hold superseded documents. **Nothing in an
`archive/` is current — do not build from it, quote it, or cite it.**

Defects found while working go to `apps/spell-circle-canvas/FINDINGS.md`
— create it when needed. Each entry states what the code does, what it
was evidently intended to do, and what a test should assert once intent
is restored. It is a work queue: delete entries as they are fixed, and
delete the file when it is empty.

## Documentation conventions

Comments describe the code they sit next to, and must be evaluable by a
reader who has never opened any other document.

- **No citations.** No section numbers, no document names, no "see the
  design doc". State the constraint itself.
- **No performance measurements.** Benchmarks and ledgers own numbers. A
  behavioural constant is different: if only editing the code could
  falsify it, keep it; if re-running a benchmark could, cut it.
- **No history.** No dates, no "renamed", no "used to be", no campaign
  names. When a past attempt revealed a real constraint, state the
  constraint, never the attempt.
- **One marker is allowed**: a comment line beginning `workaround:`
  above a place where this repository compensates for a defect or gap in
  something it depends on, so `grep -r 'workaround:'` enumerates them.
- This applies to strings that ship too — assertion messages, runtime
  warnings, `#error` text.

## How to work

**Consult what exists before writing anything.** The dependencies
(Diligent with DiligentFX and DiligentTools, Skia, choreograph, glm,
HarfBuzz, ICU, OpenImageIO, OpenUSD, Slang), the code already in this
tree, and how other engines solved it. DiligentFX carries a PBR
renderer with image-based lighting, an environment-map renderer and
post-processing; Skia carries path ops, contour measures, runtime
effects and encoders. Read the dependency's headers and the docs under
`build/vcpkg_installed/.../share` first. A library that has what is
needed but is not in vcpkg gets a port in the sigil-vcpkg-registry,
never a vendored copy. What is still written here says in its comment
what was found and why it did not serve.

**Code as if the libraries are nascent, because they are.** A change is
made by editing, building the one target it touches, and looking at the
result. Commit freely; a pass of work carries several breaking changes
at once and fixes forward. No check, ledger, tidy or sanitizer runs
between changes. Verification is ONE refinement pass right before a
push or an integration point, which the owner calls: `check.py`, the
tests, the plate tiers (rebasing the scenes a change was meant to move,
with the cause in the commit), the window lane, the sanitizers only
when memory ownership changed.

**The sketches come last.** Library work is expected to break them, so
a library pass builds the libraries and their tests, never
`SigilSketches` or Sketchbook, and does not edit sketches to keep them
compiling; one final pass brings every sketch onto the new vocabulary.

**Import the origin.** A consumer includes a library's own headers,
links its target and spells its namespace; no library re-exports
another's vocabulary. A namespace groups a catalog with peers; a prefix
on one function is a name. Names say what they act on; a domain's own
word is allowed in the library that is that domain; no invented
abbreviations.

**Seams, kits, primitives.** A primitive is irreducible and lives in a
library's core; a stock value over a seam is kit; a device executor
stands beside the CPU executor of the seam it serves; GPU-focused mesh
and point work is a point operator. Motion primitives live in
SigilMotion, the paragraph engine in SigilWeave: if either cannot
express what a consumer needs, that library grows.

**Several hands share one index.** Stage by explicit path and commit
with `git commit --only -m <message> -- <paths>`; a bare commit takes whatever
another pass left staged. Two concurrent builders per build tree at
most; a third configures its own tree over the shared
`vcpkg_installed/`.

**Too integrated to change locally means split it.** A change that
cannot be made without the whole tree re-verifying is a design signal,
never a reason for more checking. Performance is designed in while
writing and measured with `--bench` or `--window-bench` when something
feels slow, not gated per change.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Every workflow is also a mise task (`mise tasks`; arguments after `--`
are forwarded); mise is optional. `setup.py` discovers Qt 6.11+, vcpkg
and the optional SDKs under `~/.local/opt/<name>/<version>/` and writes
the uncommitted `CMakeUserPresets.json`. Ports for `choreograph`,
`skia` and `diligent-engine` come from
https://github.com/ryuuart/sigil-vcpkg-registry (checked out at
`~/REI/sigil-vcpkg-registry`); a port change is pushed there and the
`baseline` in `vcpkg-configuration.json` bumped, together with the
`version>=` on `skia` that the sanitizer pin names. The overlay triplet
builds oneTBB shared because USD's dylibs deadlock on a static one.

Qt's moc runs only where Qt is: `CMAKE_AUTOMOC` is off, and a target
that declares Qt types calls `sigil_qt_target()` right after it is
created, before any `qt_add_qml_module()`. Use a Release build for any
performance work. Some targets are conditional: Ultralight-dependent
ones disable themselves without the SDK, GPU tests need Metal, and
`world_diligent_test` skips without a Vulkan runtime (`brew install
molten-vk vulkan-loader`). Demo assets come from `mise run assets`.

The checks and ledgers — `check.py`, `gate.py`, `plate_ledger.py`,
`app_fps_ledger.py`, `bench_ledger.py`, `coverage.py`, `sanitize.py` —
are documented in `apps/spell-circle-canvas/scripts/README.md`.

### Visual work

Everything renderable is a **sketch**: one file (or one directory) under
`src/sketch/sketches/`, addressed by its stem, in one registry;
`src/sketch/README.md` is the canon. **Sketchbook** drives all of it and
is an app bundle, so headless runs go through the binary inside it:

```sh
build/bin/<config>/Sketchbook.app/Contents/MacOS/Sketchbook \
  --headless <outdir> [--gpu] [--sketch <name>] [--kind canvas|set]
```

Pointed at a file with no `--headless`, Sketchbook opens on it, from
anywhere on disk, and hot-swaps the recompiled sketch on every save;
`--frame out.png` renders one still, `--bench` measures it against the
60 FPS gate on a raster surface, `--window-bench` presents it in the
real window, `--shot <png>` captures the app.

## Layout

```
apps/spell-circle-canvas/src/
  common/          the libraries — see the README in each
  sketch/          SigilSketch: the sketches, and Sketchbook over them
  sigilweave/      the text engine
  spellcircle/     the product: shared/ core embedded by qt/ and mac/
apps/python/       scene authoring and UDP transport
touchdesigner/     TouchDesigner project and editor tooling
```

**Naming**: libraries intended for extraction into their own repositories
carry the `Sigil` prefix; product-side integrations keep `Ifrit`.
**Boundaries are deliberate** and each README states its own. Two that
are easy to get backwards: SigilLoader owns resource *access* while
SigilImage owns image *meaning*; SigilWorld consumes SigilGeometry's
types, never the reverse.

## Generated files — never hand-edit

The FlatBuffers wire schema generates for two consumers. **C++ is
generated by the build** (`SpellCircleSchema` runs `flatc` into the
build tree; edit `SpellCircle.fbs` and rebuild). **Python is committed
and regenerated by hand**: `apps/python/SpellCircle/{Vec2,Circle,Point,
Edge,Box,Scene}.py` are imported without a build in reach, so after
editing the schema run `mise run flatbuffers` and commit what it writes;
the script copies only the schema modules, since the hand-written
`SpellCircle/__init__.py` is the public API.
