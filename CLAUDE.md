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
- `src/sigilweave/README.md` — text shaping and layout
- `src/common/compose/README.md` — data-driven drawable components
- `src/sketch/README.md` — SigilSketch: every renderable thing as one
  sketch file, with Sketchbook, the live host and the plates
- `src/common/geometry/README.md` — higher-level drawing over Skia
- `src/common/world/README.md` — 3D surfaces on Diligent Engine
- `src/common/substance/README.md` — Substance `.sbsar` materials rendered
  to images (needs the Adobe SDK; optional)
- `src/common/usd/README.md` — the world's data written to and read from
  USD (OpenUSD from vcpkg; optional)
- `src/common/motion/README.md` — animation clock and animatable values
- `src/common/core/README.md` — the kernels a retained runtime hosts: the
  reconciler and the caching proof
- `src/common/image/README.md` — image decoding
- `src/common/loader/README.md` — resource access: URIs, caching, reload
- `src/common/scry/README.md` — HTML and CSS rendered to Skia images
- `src/common/skia/README.md` — SigilSkia: Skia Graphite brought up on a
  Metal or Vulkan device someone else owns
- `src/common/ui/README.md` — reusable Qt Quick controls
- `docs/README.md` — the generated C++ API reference: the `docs`
  target, its theme, and serving it

`archive/` directories hold superseded documents. **Nothing in an
`archive/` is current — do not build from it, quote it, or cite it.**
Several of them state things the code contradicts.

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
- **No performance measurements.** Benchmarks own numbers — the `*_bench`
  binaries (one per feature target, which the `benches` target builds),
  `scripts/bench_ledger.py` which judges their medians against a
  committed baseline, and the plate ledger. A behavioral constant is
  different and belongs in the comment: if only editing the code could
  falsify it, keep it; if re-running a benchmark could, cut it.
- **No history.** No dates, no "renamed", no "used to be", no campaign
  names. When a past attempt revealed a real constraint, state the
  constraint, never the attempt.
- **One marker is allowed**, and only one: a comment line beginning
  `workaround:` above a place where this repository compensates for a
  defect or a gap in something it depends on. It names the defect in a
  line or two and the prose beneath it explains as any other comment
  does; it earns its exception because the set of such places has to be
  enumerable — `grep -r 'workaround:'` is what says how much of the
  build exists to absorb somebody else's packaging.
- This applies to strings that ship too — assertion messages, runtime
  warnings, `#error` text.

## Build and test

### How to work

These libraries are nascent and break often, and the way to work with
them is to expect that. A change is made by editing, building the one
target it touches, and looking at the result — a `--frame`, the
window, a test binary. Commit freely; a pass of work carries several
breaking changes at once and fixes forward rather than stopping to
re-verify. None of the checks below runs between changes: no gate, no
ledger sweep, no window sweep, no tidy, no sanitizer.

Verification is ONE refinement pass, right before a push or an
integration point, and it does all of it at once: `scripts/check.py`,
the tests, the plate tiers (rebasing the scenes a change was meant to
move, with the cause in the commit), the window lane, and the
sanitizers only when memory ownership changed. Byte identity is
confirmed there and nowhere earlier.

The sketches come last. Library work is expected to break them, so a
library pass does not build `SigilSketches` or Sketchbook and does not
edit sketches to keep them compiling; when the libraries' new features
are in, one final pass brings every sketch up to the new vocabulary
and the plates are rebased then. "Everything builds" during a library
pass means the libraries and their tests.

A change that cannot be made without the whole tree re-verifying is
telling you the code is too integrated: split the library or feature
so the change is local. That is a design signal, never a reason for
more checking. Performance is designed in while writing and measured
with `--bench` or `--window-bench` when something feels slow, not
gated per change.

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The workflows on this page are also wrapped as mise tasks in the
repository-root `mise.toml` — `mise tasks` lists them, and each is a thin
wrapper over the command documented here rather than a second way of
doing things. Anything after `--` is forwarded (`mise run test -- -R
compose`, `mise run check -- --fix`). mise is optional; nothing in the
build depends on it.

`setup.py` discovers Qt 6.11+, vcpkg and (optionally) the Adobe Substance
3D SDK, and writes the uncommitted `CMakeUserPresets.json`. Hand-installed
SDKs live under `~/.local/opt/<name>/<version>/` (Qt and Substance both);
without the Substance SDK the `SigilSubstance` targets are simply left
out with a configure warning. OpenUSD comes from vcpkg's `usd` port; the project's overlay triplet
(`cmake/triplets/arm64-osx.cmake`, wired through `CMakePresets.json`)
builds oneTBB as a shared library, because USD's many dylibs each linking
a static TBB deadlock on the first stage open. Custom ports (`choreograph`, `skia`,
`diligent-engine`) come from the sigil-vcpkg-registry at
https://github.com/ryuuart/sigil-vcpkg-registry via
`vcpkg-configuration.json`; bump its `baseline` to the registry's new
HEAD when a port changes (the workflow is in its README). `skia` also
carries a `version>=` in `vcpkg.json`, naming the port
`cmake/SkiaSanitizerAbi.h` reads a header guard out of; raise the two
together, and the assertion in that header says so if they part. A
library that is not in upstream vcpkg gets a port there rather than
being vendored.

Qt's moc runs only where Qt is. The tree configures with `CMAKE_AUTOMOC`
off, and each target that declares Qt types names itself with
`sigil_qt_target()` (`cmake/QtTarget.cmake`) — right after the target is
created, and before its `qt_add_qml_module()` when it has one, since a
QML module registers its types out of moc's output. A missing call fails
at link with an undefined vtable or `staticMetaObject`.

Use a Release build for any performance work. Several benchmarks and
sketches are deliberately stressful and Debug timings say nothing.
`cmake --build build --config Release --target benches` builds every
benchmark binary and `scripts/bench_ledger.py` (or `mise run bench`) runs
them on a quiet machine, comparing each benchmark's median against
`bench/baseline_<config>.json`; `--rebase` adopts new numbers.

Some targets are conditional: Ultralight-dependent ones disable
themselves with a warning when the SDK is missing, GPU tests need Metal,
and `world_diligent_test` *skips* rather than fails without a Vulkan
runtime (`brew install molten-vk vulkan-loader`).

Open-licensed demo assets come from the opt-in `fetch_assets` target into
`build/assets/`; `scripts/fetch_assets.py` holds the manifest and the
rules for adding to it, and runs on its own without a configured tree.

Formatting and linting run through `scripts/check.py` — one command
covering clang-format (Google C++ style, stock), ruff (lint and format),
qmllint, and clang-tidy, scoped by default to the files git sees as
changed. `--all` checks the whole tree, `--fix` applies the format
fixes, `--tidy-all` analyzes every translation unit, and `--skip-tidy`
and `--tidy-only` split the command in half. The configs live at
the repository root: `.clang-format` with `.clang-format-ignore`,
`.clang-tidy`, `.clangd`, and `ruff.toml`. The discipline is
check-forward: the tools police changes, never mass-reformat — the one
whole-tree reformat that adopted the style is listed in
`.git-blame-ignore-revs`, which `git config blame.ignoreRevsFile
.git-blame-ignore-revs` makes local blame skip (GitHub's blame view
honors it automatically). clang-tidy comes from `brew install llvm`,
ruff from `brew install ruff`; the other tools ride the Xcode and Qt
installs the build already needs.

The quick checks run together through `scripts/gate.py` — one command,
one verdict, at the refinement pass before a push, or whenever one
wants a fast answer. Each lane is its own subprocess with its
output held back: `scripts/check.py` over the changed files, the ctest
tests those files belong to, the plate ledger's quick tier, its world
tier when the change can move a set, and clang-tidy with `--tidy`. They
run side by side, so the wait is the slowest lane rather than the sum;
each prints one PASS/FAIL line with its wall time as it lands, and then
`GATE: pass` or `GATE: fail — <lane>`, with only the failing lanes'
output under it. The test scope comes from the build graph rather than
from a rule about directory names — the tests a changed source or header
is linked into, read through `ninja -t inputs` and the deps log — and a
file the graph cannot place means the graph is behind the tree, so the
lane runs the whole suite and says so on its line. `--all` widens the
lint and the tests to everything, `--config` picks the configuration the
lanes read (Release, which is what the plate baselines are), and
`--timeout-seconds` fails a wedged lane by name with everything it
started. It builds nothing itself; `mise run gate` builds first. The
plate ledger's full tier, the sanitizers and `--tidy-all` are
deliberately outside it — those are the gate before a push and stay
separate commands.

Code coverage runs through `scripts/coverage.py` — one command that
configures a dedicated instrumented tree (`build-coverage/`, reusing the
primary build's preset composition and its `vcpkg_installed/`
dependencies read-only; the primary `build/` is never touched), builds
the test targets, runs ctest under LLVM source-based profiling, and
writes the `llvm-cov` summary to the console plus an HTML report under
`build/coverage/html/` in the primary build directory. Each run replaces
the report wholesale, and `RUN.txt` beside it records the invocation and
scope that produced it. `--filter <regex>` runs a test subset and builds
only the targets it needs; `--export-lcov <file>` emits an lcov file for
CI consumers.

Sanitizer runs go through `scripts/sanitize.py`: ASan+UBSan by default
(`build-asan/`), the TSan lane with `--thread` (`build-tsan/`);
`--filter`/`--targets`/`--config` work as in `coverage.py`, and both
orchestrators share `scripts/buildtree.py` for preset resolution and the
shared-vcpkg configure. A lane deletes its own tree once the tests have
had their verdict, pass or fail, and refuses to start while the other
lane's tree stands, so the two never share the machine; `--keep` holds a
tree for a debugger at the price of removing it by hand before the other
lane can run. A configure or build failure leaves the tree standing —
only a finished ctest run makes it disposable. The runtimes ship with
Clang — no new dependencies. UBSan findings abort rather than scroll past, so a finding
fails its test. vcpkg archives are uninstrumented, which is why
container-overflow checking is disabled at runtime and the address lane
pins Abseil's table layout and Skia's array layout to agree with the
prebuilt archives (a header that grows a member under instrumentation
moves every field behind it, and half of those accessors are inline);
LeakSanitizer is unsupported on Apple Silicon and disabled. The thread
lane meets the same boundary from the other side: a dependency whose
headers this tree instantiates is watched in half — its accesses
visible, the ordering compiled into its archive not — so it gets an
entry in `ThreadSanitizerSuppressions.txt`, which states per dependency
why the ordering is real and holds nothing of this repository's own. The
TSan lane runs `scry_test`'s web-thread handoffs, though the Ultralight
dylibs themselves are uninstrumented.

API documentation is a Doxygen site per library, built with `cmake
--build build --target docs` (or `--target docs-SigilGeometry` for one of
them) and landing at `build/docs/index.html`. Everything driving it
lives in `docs/`, whose README is the canon for it — the theme, the
two-pass cross-linking, and the container that serves the result. Each
library registers itself by calling `sigil_add_docs()` in its own
`CMakeLists.txt`, so a new library joins by adding that call. The
target is simply absent when Doxygen is not installed (`brew install
doxygen graphviz`).

The convention the settings assume: a type carries a `/** */` block
whose first sentence is its summary, and self-evident fields and
one-line accessors under a documented type do not repeat it. Undocumented
entities are therefore not warned about by default —
`-DSPELLCIRCLE_DOCS_WARN_UNDOCUMENTED=ON` turns that on when auditing.

### Visual work

Everything renderable in this repository is a **sketch**: one file under
`src/sketch/sketches/`, addressed by its own stem, in one registry.
`src/sketch/README.md` is the canon. One application drives all of it —
**Sketchbook** — and it is a macOS app bundle, so headless runs go
through the binary inside it:

```sh
build/bin/<config>/Sketchbook.app/Contents/MacOS/Sketchbook \
  --headless <outdir> [--gpu] [--sketch <name>] [--kind canvas|set]
```

`--sketch` takes a case-insensitive substring and renders just that one,
which is the loop for visual iteration. `--list` prints the registry;
`--kind` narrows it to the sketches drawn onto a canvas or the ones that
light a set. `--shot <png>` captures the app itself rather than a sketch.
Pointed at a file with no `--headless`, Sketchbook opens on it — from
anywhere on disk, listed under its own stem with `assets/` beside it —
and hot-swaps the recompiled sketch on every save; `--frame out.png`
renders one headlessly and `--bench` measures it against the 60 FPS
gate.

`--window-bench` is the other frame-rate lane: it presents each sketch in
the REAL window at a stated size and device pixel ratio and prints what
was presented, so the host's own overhead — readback, upload,
presentation — is in the number, which `--bench` on a raster surface
cannot see. `scripts/app_fps_ledger.py` (or `mise run fps`) sweeps the
registry with it and judges each presented rate against
`bench/app_fps_<config>.json`; `--rebase` adopts. Per machine and per
display mode, and the baseline records which.

Plate sweeps run through `scripts/plate_ledger.py` in four tiers, each
with its own baseline and all through that one binary. The two that
rasterise on the CPU are judged on byte identity, the two that rasterise
on a device within stated per-channel ceilings — a device plate is not a
function of the drawing code alone. `--tier quick` is the iteration loop
— GPU renders at a uniform early capture, seconds for the whole
registry; its baseline keeps the plates beside the hash manifest, and a
hash miss is decoded and compared per colour channel rather than
reported, because two builds of the host render the same sketch a
scatter of channels apart. The default full tier steps every canvas
sketch to its declared moment on the CPU and is the byte-identity gate,
the final confirmation before trusting a change; one legitimately
expensive sketch (`chaucer_astrolabe`) has its own timeout ceiling in
the script's override table there. `--tier world` does the same for the
sketches that light a set, and `--tier world-gpu` renders those on the
device and compares them against the CPU tier's plates within a stated
per-sketch tolerance. `--rebase` adopts a new baseline for the active
tier (merging when given `--scenes`, and writing the quick tier's plates
beside its manifest), and `--stability N` separates sketch flap from
code changes. `--fps-gate` is a separate serial lane over either kind.

## Layout

```
apps/spell-circle-canvas/src/
  common/          the libraries — see the README in each
  sketch/          SigilSketch: the sketches, and Sketchbook over them
  sigilweave/      the text engine, with its examples and benchmarks
  spellcircle/     the product: shared/ core embedded by qt/ and mac/
apps/python/       scene authoring and UDP transport
touchdesigner/     TouchDesigner project and editor tooling
```

Qt executables keep their own `src/`, `include/` and `qml/` folders.

**Naming**: libraries intended for extraction into their own repositories
carry the `Sigil` prefix. Product-side integrations keep `Ifrit`
(`Ifrit.Ui` only).

**Boundaries between libraries are deliberate** and each README states
its own. Two that are easy to get backwards: SigilLoader owns resource
*access* while SigilImage owns image *meaning*; and SigilWorld consumes
SigilGeometry's types, never the reverse.

## Generated files — never hand-edit

The FlatBuffers wire schema generates for two consumers, and they are
handled differently.

**C++ — generated by the build, not committed.** `SpellCircleSchema`
runs `flatc` into the build tree, so editing `SpellCircle.fbs` is enough:
the next build regenerates the header and recompiles what includes it.
`flatc` comes from the same vcpkg manifest as the runtime library, so
this costs no dependency a build does not already have. Nothing to run
by hand, and the header cannot drift from the schema compiled against it.

**Python — committed, regenerated manually.**

- `apps/python/SpellCircle/{Vec2,Circle,Point,Edge,Box,Scene}.py`

That package is installed and imported on its own — by TouchDesigner
among others — with no CMake build in reach, so its modules cannot live
in a build tree. After editing `SpellCircle.fbs` run
`apps/spell-circle-canvas/scripts/regen_flatbuffers.sh` from anywhere
(or `mise run flatbuffers`) and commit what it writes. The script
generates aside and copies only the schema modules: `flatc --python`
also emits an empty `SpellCircle/__init__.py`, and that name belongs to
the hand-written public API.

`src/common/compose/README.md` is also compile-checked: a build step
extracts every API name it spells — from code blocks *and* inline code
spans — and fails the build if a name no header declares appears there.
Verify additions to it against the headers rather than writing from
memory.
