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
and format), qmllint and clang-tidy, scoped by default to the files git
sees as changed. `--all` checks the whole tree, `--fix` applies the
format fixes, `--tidy-all` analyses every translation unit, and
`--skip-tidy` / `--tidy-only` split the command in half. The configs are
at the repository root: `.clang-format` with `.clang-format-ignore`,
`.clang-tidy`, `.clangd`, `ruff.toml`. The discipline is check-forward:
the tools police changes and never mass-reformat; the one whole-tree
reformat that adopted the style is listed in `.git-blame-ignore-revs`,
which `git config blame.ignoreRevsFile .git-blame-ignore-revs` makes
local blame skip. clang-tidy comes from `brew install llvm`, ruff from
`brew install ruff`; the rest ride the Xcode and Qt installs.

## The gate — `gate.py`

The quick checks together, one verdict. Each lane is its own
subprocess with its output held back: `check.py` over the changed
files, the ctest tests those files belong to, the plate ledger's quick
tier, its world tier when the change can move a set, and clang-tidy
with `--tidy`. They run side by side, so the wait is the slowest lane;
each prints one PASS/FAIL line with its wall time as it lands, then
`GATE: pass` or `GATE: fail — <lane>` with only the failing lanes'
output under it. The test scope comes from the build graph — the tests
a changed source or header is linked into, read through `ninja -t
inputs` and the deps log — and a file the graph cannot place runs the
whole suite and says so. `--all` widens lint and tests to everything,
`--config` picks the configuration (Release, which the plate baselines
are), `--timeout-seconds` fails a wedged lane by name. It builds
nothing itself; `mise run gate` builds first. The full plate tier, the
sanitizers and `--tidy-all` are outside it by design.

## Plates — `plate_ledger.py`

Four tiers, each with its own baseline, all through the Sketchbook
binary. The two that rasterise on the CPU are judged on byte identity,
the two that rasterise on a device within stated per-channel ceilings —
a device plate is not a function of the drawing code alone. `--tier
quick` is the iteration loop: GPU renders at a uniform early capture,
seconds for the registry; its baseline keeps the plates beside the hash
manifest, and every successful sweep also refreshes Sketchbook's
separate canvas-thumbnail cache without adopting the baseline. A hash
miss is compared per colour channel rather than reported, because two
builds of the host render the same sketch a scatter of channels apart.
The default full tier steps every canvas
sketch to its declared moment on the CPU and is the byte-identity
verdict; a scene over the per-scene budget fails by name. `--tier
world` does the same for the sketches that light a set and retains every
successful sweep in a separate cache as Sketchbook's set thumbnails;
the cache does not adopt or change the hash baseline. `--tier
world-gpu` renders those on the device and compares against the CPU
tier's plates within a per-sketch tolerance. `--rebase` adopts a
baseline for the active tier, merging atomically when given `--scenes`;
`--stability N` separates sketch flap from code changes; `--fps-gate`
is a separate serial lane. Progress prints one line per scene as it
finishes.

**A sketch that FAULTS names itself.** The sweep opens every sketch in
one process, so a bad draw takes the whole run down — and it used to die
with a bare signal, leaving the last `=== sketch <name>` marker on
unbuffered stderr as the only clue. The crash reporter is now installed
on this lane too: the report names the entry it was on, how many plates
finished before it, the phase (setup, draw, capture), and the stack. The
ledger's verdict is unchanged — a faulting run is still a failed run —
but the next run can be narrowed to the sketch the report names with
`--sketch`.

## Frame rates — `app_fps_ledger.py` and `bench_ledger.py`

`app_fps_ledger.py` (`mise run fps`) presents each sketch in the real
window at a stated size and device pixel ratio through Sketchbook's
`--window-bench` and judges the presented rate against
`bench/app_fps_<config>.json`, keyed by the sketch's stem; the host's
own overhead is in the number, which a raster `--bench` cannot see.
Two window sweeps cannot share one display, so it runs alone.
`bench_ledger.py` (`mise run bench`) runs every `*_bench` binary (the
`benches` target builds them) on a quiet machine and compares medians
against `bench/baseline_<config>.json`; `--rebase` adopts. Use a Release
build for either.

## Coverage — `coverage.py`

Configures a dedicated instrumented tree (`build-coverage/`, reusing the
primary build's preset composition and its `vcpkg_installed/`
read-only; `build/` is never touched), builds the test targets, runs
ctest under LLVM source-based profiling, and writes the `llvm-cov`
summary to the console plus an HTML report under `build/coverage/html/`,
with `RUN.txt` beside it recording the invocation. `--filter <regex>`
runs a subset and builds only what it needs; `--export-lcov <file>`
emits lcov.

## Sanitizers — `sanitize.py`

ASan+UBSan by default (`build-asan/`), the TSan lane with `--thread`
(`build-tsan/`); `--filter` / `--targets` / `--config` as in
`coverage.py`; both share `buildtree.py` for preset resolution and the
shared-vcpkg configure. A lane deletes its own tree once ctest has had
its verdict and refuses to start while the other lane's tree stands;
`--keep` holds a tree for a debugger. A configure or build failure
leaves the tree standing. UBSan findings abort, so a finding fails its
test. vcpkg archives are uninstrumented, which is why container-overflow
checking is off at runtime and the address lane pins Abseil's table
layout and Skia's array layout to agree with the prebuilt archives (a
header that grows a member under instrumentation moves every field
behind it); `cmake/SkiaSanitizerAbi.h` asserts the pin took, and the
`version>=` on `skia` in `vcpkg.json` names the port it was written
against. LeakSanitizer is unsupported on Apple Silicon and off. The
thread lane watches a dependency whose headers this tree instantiates
in half, so each such dependency has an entry in
`ThreadSanitizerSuppressions.txt` stating why its ordering is real.

## Docs, assets, flags, schema

`build_docs.py` generates the Doxygen site per library from the manifest
`sigil_add_docs()` calls write (`docs/README.md` is the canon; the
`docs` target is absent without Doxygen). `fetch_assets.py` (`mise run
assets`) fetches the open-licensed demo assets into `build/assets/`
from its hash-pinned manifest and needs no configured tree.
`extract_sketch_flags.py` lifts the sketch compile command out of the
compilation database into the flags file the live host uses.
`regen_flatbuffers.sh` (`mise run flatbuffers`) regenerates the
committed Python schema modules. `setup.py` configures: it discovers Qt
6.11+, vcpkg and the optional SDKs and writes the uncommitted
`CMakeUserPresets.json`.
