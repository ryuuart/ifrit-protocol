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
- `src/common/shape/README.md` — higher-level drawing over Skia
- `src/common/world/README.md` — 3D surfaces on Diligent Engine
- `src/common/substance/README.md` — Substance `.sbsar` materials rendered
  to images (needs the Adobe SDK; optional)
- `src/common/usd/README.md` — the world's data written to and read from
  USD (OpenUSD from vcpkg; optional)
- `src/common/motion/README.md` — animation clock and animatable values
- `src/common/image/README.md` — image decoding
- `src/common/loader/README.md` — resource access: URIs, caching, reload
- `src/common/scry/README.md` — HTML and CSS rendered to Skia images
- `src/common/skia/README.md` — Skia Graphite GPU plumbing
- `src/common/ui/README.md` — reusable Qt Quick controls

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
- **No performance measurements.** Benchmarks own numbers — `compose_bench`,
  `weave_bench`, `scry_bench`, `shape_bench`, `world_bench`, and the plate
  ledger. A behavioral constant
  is different and belongs in the comment: if only editing the code could
  falsify it, keep it; if re-running a benchmark could, cut it.
- **No history.** No dates, no "renamed", no "used to be", no campaign
  names. When a past attempt revealed a real constraint, state the
  constraint, never the attempt.
- This applies to strings that ship too — assertion messages, runtime
  warnings, `#error` text.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

`setup.py` discovers Qt 6.11+, vcpkg and (optionally) the Adobe Substance
3D SDK, and writes the uncommitted `CMakeUserPresets.json`. Hand-installed
SDKs live under `~/.local/opt/<name>/<version>/` (Qt and Substance both);
without the Substance SDK the `SigilSubstance` targets are simply left
out with a configure warning. OpenUSD comes from vcpkg's `usd` port; the project's overlay triplet
(`cmake/triplets/arm64-osx.cmake`, wired through `CMakePresets.json`)
builds oneTBB as a shared library, because USD's many dylibs each linking
a static TBB deadlock on the first stage open. Custom ports (`choreograph`, `skia`,
`diligent-engine`) come from the sigil-vcpkg-registry via
`vcpkg-configuration.json` — **its `repository` currently points at a
local checkout, `/Users/long/REI/sigil-vcpkg-registry`.** Update the URL
and baseline when that registry is pushed; the workflow is in its README.

Use a Release build for any performance work. Several benchmarks and
gallery scenes are deliberately stressful and Debug timings say nothing.

Some targets are conditional: Ultralight-dependent ones disable
themselves with a warning when the SDK is missing, GPU tests need Metal,
and the SigilWorld tests *skip* rather than fail without a Vulkan runtime
(`brew install molten-vk vulkan-loader`).

Open-licensed demo assets come from the opt-in `fetch_assets` target into
`build/assets/`; `cmake/FetchAssets.cmake` holds the manifest rules.

Formatting and linting run through `scripts/check.py` — one command
covering clang-format (Google C++ style, stock), ruff (lint and format),
qmllint, and clang-tidy, scoped by default to the files git sees as
changed. `--all` checks the whole tree, `--fix` applies the format
fixes, `--tidy-all` analyzes every translation unit. The configs live at
the repository root: `.clang-format` with `.clang-format-ignore`,
`.clang-tidy`, `.clangd`, and `ruff.toml`. The discipline is
check-forward: the tools police changes, never mass-reformat — the one
whole-tree reformat that adopted the style is listed in
`.git-blame-ignore-revs`, which `git config blame.ignoreRevsFile
.git-blame-ignore-revs` makes local blame skip (GitHub's blame view
honors it automatically). clang-tidy comes from `brew install llvm`,
ruff from `brew install ruff`; the other tools ride the Xcode and Qt
installs the build already needs.

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
shared-vcpkg configure. The runtimes ship with Clang — no new
dependencies. UBSan findings abort rather than scroll past, so a finding
fails its test. vcpkg archives are uninstrumented, which is why
container-overflow checking is disabled at runtime and the address lane
pins Abseil's table layout and Skia's array layout to agree with the
prebuilt archives (a header that grows a member under instrumentation
moves every field behind it, and half of those accessors are inline);
LeakSanitizer is unsupported on Apple Silicon and disabled. The TSan
lane runs `scry_test`'s web-thread handoffs, though the Ultralight
dylibs themselves are uninstrumented.

### Visual work

`ComposeGallery` is a macOS app bundle, so headless runs go through the
binary inside it:

```sh
build/bin/<config>/ComposeGallery.app/Contents/MacOS/ComposeGallery \
  --headless <outdir> [--gpu] [--scene <name>]
```

`--scene` takes a case-insensitive substring and renders just that one,
which is the loop for visual iteration. `--shot <png>` captures the app
itself rather than a scene. `ComposeSketch` is the live-coding host; a
study under `compose/sketch/sketches/` is one file that is both a
hot-reload sketch and a gallery scene, and answers to its file stem.

Byte-identity sweeps run through `scripts/plate_ledger.py` in two tiers,
each with its own baseline. `--tier quick` is the iteration loop —
GPU renders at a uniform early capture, seconds for the whole registry.
The default full tier steps every scene to its declared moment on the
CPU and is the final confirmation gate before trusting a change; one
legitimately expensive scene (`chaucer_astrolabe`) has its own timeout
ceiling in the script's override table there. `--rebase` adopts a new
baseline for the active tier (merging when given `--scenes`), and
`--stability N` separates scene flap from code changes.

## Layout

```
apps/spell-circle-canvas/src/
  common/          the libraries — see the README in each
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
SigilShape's types, never the reverse.

## Generated files — never hand-edit

- `src/spellcircle/shared/schema/include/SpellCircle_generated.h`
- `apps/python/SpellCircle/{Vec2,Circle,Point,Edge,Box,Scene}.py`

After editing `SpellCircle.fbs`, run
`apps/spell-circle-canvas/scripts/regen_flatbuffers.sh` from anywhere and
commit what it writes. It regenerates both the C++ header and the Python
modules.

`src/common/compose/README.md` is also compile-checked: a build step
extracts every API name it spells — from code blocks *and* inline code
spans — and fails the build if a name no header declares appears there.
Verify additions to it against the headers rather than writing from
memory.
