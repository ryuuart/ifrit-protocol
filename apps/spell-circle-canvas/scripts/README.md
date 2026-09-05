# Scripts

The build's administration lives here as Python, each script with a
considered interface and a one-word mise task wrapping it (the
repository-root `mise.toml`; anything after `--` is forwarded). This
README is the canon for how the checks and ledgers work; `--help` on
each script is the canon for its flags.

## Tests

A unit test asserts one behaviour a library promises through its public
headers to a caller who has read only its README, and its name is that
promise written as a sentence — `ASettledOpacityRebakesTheLeaf` — so a
failure reads as the claim that broke. It pins only what editing the
code alone could falsify: a caching count, a closed form, a field walk
over whatever a type declares; never what a rebuild, a font, a device or
a clock could move — exact pixels, byte layouts, hash permutations,
elapsed time. A claim made N times with one thing varying is one
`TEST_P` whose parameter is that thing, and a fixture two files need
lives once in the library's `test/support/`. Pixel identity is the plate
ledger's to judge and timing is the bench ledger's: a test that renders
a picture to compare it, or times a loop to bound it, belongs to one of
those instruments rather than to ctest. A case that skips on this
machine is not coverage on this machine — say what it needs with a ctest
label (`gpu`, `fonts`), and commit the instrument whenever one can be
committed.

## Formatting and linting — `check.py`

One command covering clang-format (Google C++ style, stock), ruff (lint
and format) and qmllint, scoped by default to the files git sees as
changed. `--all` checks the whole tree, `--fix` applies the format
fixes, and explicit file arguments check exactly those files. The
configs are at the repository root: `.clang-format` with
`.clang-format-ignore`, `ruff.toml`. The discipline is check-forward:
the tools police changes and never mass-reformat; the one whole-tree
reformat that adopted the style is listed in `.git-blame-ignore-revs`,
which `git config blame.ignoreRevsFile .git-blame-ignore-revs` makes
local blame skip. Every tool is required — a missing one fails the run
rather than passing it. clang-format rides the Xcode toolchain through
`xcrun`, qmllint the Qt prefix `setup.py` recorded, ruff comes from
`brew install ruff`.

## Plates — `plate_ledger.py`

Two tiers, one binary. `--tier cpu` (the default) steps every sketch —
canvas and set alike — to its declared moment and rasterises it on the
CPU, so a plate is a function of the declaration alone and the tier is
judged on byte identity against one manifest,
`build/plate_baseline_<config>.sha256`, keyed by registry name and
covering both kinds. A clean sweep is the byte-neutrality verdict; a
scene over the per-scene ceiling fails by name, and there is no
per-scene override. `--rebase` adopts; a sweep narrowed by `--kind`,
`--sketch` or `--scenes` merges into the manifest rather than
truncating it, and only an unnarrowed rebase rewrites it wholesale.
`--stability N` re-renders a mover and attributes a self-disagreeing
scene to the scene. The ledger renders plates for the verdict and
nothing else: Sketchbook owns the thumbnails it shows, rendering them on
demand into its own cache and warming them with `Sketchbook
--thumbnails`.

`--tier device` renders the same sketches through the device and
compares each against the CPU plate of the same run per colour channel
— mean and p99 judged within per-sketch ceilings, max reported — because
a device plate is not a function of the drawing code alone. It has no
baseline and refuses `--rebase`; without a device runtime it says so and
exits 0.

The manifest is machine-local by design (plates are deterministic per
machine, not across machines), so a fresh checkout runs
`plate_ledger.py --rebase` once before a sweep can judge anything. The
manifest is keyed by the registry NAME, which can carry spaces (`--scenes
"aero desktop"`); the frame-rate ledger below is keyed by the sketch's
STEM (`--sketch aero_desktop`).

A sketch that faults is named: the render for it fails, the report on
stderr says which entry it was on, the phase (setup, draw, capture) and
how many plates finished before it, and the run's verdict is a failure.
The next run can be narrowed to that sketch with `--sketch`.

## Frame rates — `app_fps_ledger.py` and `bench_ledger.py`

`app_fps_ledger.py` (`mise run fps`) presents each sketch in the real
window at a stated size and device pixel ratio through Sketchbook's
`--window-bench` and judges the presented rate against
`bench/app_fps_<config>.json`, keyed by the sketch's stem; the host's
own overhead is in the number, which a raster `--bench` cannot see.
Two window sweeps cannot share one display, so it runs alone.
`bench_ledger.py` (`mise run bench`) runs every `*_bench` binary (the
`benches` target builds them) on a quiet machine and compares medians
against `bench/baseline_<config>.json`. Both read, merge and write
their baseline and judge their rows through `ledger.py`: a narrowed
sweep merges on `--rebase`, a SLOWER row beyond its band fails the run,
NEW and MISSING rows do not. Use a Release build for either.

## Secondary trees — `coverage.py` and `sanitize.py`

`setup.py` writes three secondary presets into `CMakeUserPresets.json`
beside `main`: `coverage`, `asan` and `tsan`, each the `main`
composition plus its instrumentation switch in its own `build-<name>/`,
reading the primary tree's `vcpkg_installed/` as-is with the manifest
install disabled. Their test presets carry the runtime environment —
the raw-profile path for coverage, `ASAN_OPTIONS`/`UBSAN_OPTIONS`/
`TSAN_OPTIONS` for the sanitizers — so each script is configure, build
and ctest through its preset plus what CMake cannot do. `testtree.py`
is what the two share: the tests a configured tree registers, read from
`ctest --show-only`, to derive the targets a `--filter` needs built and
the binaries the selected tests ran.

`coverage.py` builds the test targets, runs ctest under LLVM
source-based profiling, and writes the `llvm-cov` summary to the console
plus an HTML report under `build/coverage/html/`, with `RUN.txt` beside
it recording the invocation. `--filter <regex>` runs a subset and builds
only what it needs; `--export-lcov <file>` emits lcov.

`sanitize.py` is ASan+UBSan by default (`build-asan/`), the TSan lane
with `--thread` (`build-tsan/`); `--filter` / `--targets` / `--config`
as in `coverage.py`. A lane deletes its own tree once ctest has had its
verdict and refuses to start while the other lane's tree stands;
`--keep` holds a tree for a debugger. A configure or build failure
leaves the tree standing. UBSan findings abort, so a finding fails its
test. vcpkg archives are uninstrumented, which is why container-overflow
checking is off at runtime and the address lane pins Skia's array layout
to agree with the prebuilt archive (a header that grows a member under
instrumentation moves every field behind it);
`cmake/SkiaSanitizerAbi.h` asserts the pin took, and the
`version>=` on `skia` in `vcpkg.json` names the port it was written
against. LeakSanitizer is unsupported on Apple Silicon and off. The
thread lane watches a dependency whose headers this tree instantiates
in half, so each such dependency has an entry in
`ThreadSanitizerSuppressions.txt` stating why its ordering is real.

## Docs, assets, flags, schema

`build_docs.py` generates the Doxygen site per library from the manifest
`sigil_library_root()` and `sigil_add_docs()` calls write (`docs/README.md` is the canon; the
`docs` target is absent without Doxygen). `fetch_assets.py` (`mise run
assets`) fetches the open-licensed demo assets into `build/assets/`
from its hash-pinned manifest and needs no configured tree.
`extract_sketch_flags.py` lifts the sketch compile command out of the
compilation database into the flags file the live host uses.
`regen_flatbuffers.sh` (`mise run flatbuffers`) regenerates the
committed Python schema modules. `setup.py` configures: it discovers Qt
6.11+, vcpkg and the optional SDKs, writes the uncommitted
`CMakeUserPresets.json`, and builds Release unless `--config` says
otherwise.
