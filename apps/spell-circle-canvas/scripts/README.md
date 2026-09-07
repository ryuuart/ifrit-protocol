# Scripts

The build's administration is ONE command with nine verbs:

```sh
python3 scripts/sigil.py <verb> [flags]
python3 scripts/sigil.py <verb> --help
```

`setup`, `check`, `plates`, `bench`, `sanitize`, `docs`, `assets`,
`flags`, `flatbuffers`. Each is a module in `scripts/sigil/`, over two
shared ones: `tree.py` is where the build tree is and how to talk to it —
which directory a preset builds into, where a configuration's binaries
land, where Sketchbook sits inside its bundle, what tests a configured
tree registers — and `baseline.py` is how a ledger reads, merges, writes
and judges what it keeps, in both spellings a baseline comes in. A verb
never re-derives either.

Every workflow is also a mise task under a one-word name (the
repository-root `mise.toml`; anything after `--` is forwarded to the
verb). THIS README IS THE CANON for what each verb does and what it
refuses; `--help` on a verb is the canon for its flags.

## Tests

A unit test asserts one behaviour a library promises through its public
headers to a caller who has read only its README, and its name is that
promise written as a sentence — `ASettledTreeBakesOnceAndReplaysAfter` — so a
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
committed. For a face that is `src/test/assets/`, whose generated
instruments carry one property each and are reached as
`sigil::test::instrument::sans()` and its siblings; a fixture only one
library asks for stays in that library's own `test/assets/`.

## Configuring — `setup`

The setup verb discovers Qt 6.11 or newer and vcpkg, writes the uncommitted
`CMakeUserPresets.json`, then configures and builds. Qt is looked for
under `~/.local/opt/Qt`, `/usr/local/opt/Qt`, `/opt/homebrew/opt/qt` and
`/opt/Qt`, each holding one directory per version with a `macos/` inside
it, and `Qt6_DIR`, `QT_DIR` or `QTDIR` overrides the search; vcpkg under
`~/.local/share/vcpkg`, `~/vcpkg`, `/usr/local/share/vcpkg` and
`/opt/vcpkg`, or `VCPKG_ROOT`. The highest version that satisfies the
minimum wins.

The file it writes composes three things. The `vcpkg` preset carries the
toolchain file and the asset cache; the `qt` preset the prefix path and
Qt's `bin/` on `PATH`; `main` inherits both over the Ninja Multi-Config
generator. `main` names both
`CMAKE_DEFAULT_BUILD_TYPE` and `CMAKE_BUILD_TYPE` as Release: the
multi-config generator builds the configuration a build names and reads
neither, but vcpkg's toolchain reads `CMAKE_BUILD_TYPE` to put the
release prefix ahead of the debug tree and reads it unset as Debug, so
naming it is what keeps a Release link free of debug archives.

THE PRESET FILE IS GENERATED, so it is rewritten whenever what generates
it would say something different — another secondary tree, a different
instrumentation flag, a Qt that moved — and left alone, timestamp and
all, when it would not. An edit to the generator reaches the tree on the
next run rather than waiting for someone to remember a flag; an edit to
the file itself does not survive one.

No library is named there. A library whose dependency needs finding
carries its own find module (`src/common/substance/cmake`,
`src/common/scry/cmake`), so the setup verb composes the toolchain, the
Qt prefix, the asset cache and the instrumented trees, and nothing about
what is built with them.

The asset cache is `~/.local/opt/vcpkg-assets`, written into the `vcpkg`
preset as the one asset source, consulted before the network and written
back to. It is where `sigil.py assets --stage` puts an archive that
cannot be fetched at all, so a port whose file is there never reaches the
network and every other port still downloads normally.

## Formatting and linting — `check`

One command covering clang-format (Google C++ style, stock), ruff (lint
and format) and qmllint. THE DEFAULT SCOPE IS THE BRANCH'S WORK:
everything this branch changed since it left `main`, committed or not,
plus untracked files that are not ignored — work is committed freely and
verified once, so a scope that saw only uncommitted changes would miss
most of what a branch is by the time anyone runs this. `--all` checks the
whole tree, `--fix` applies the format fixes, and explicit file arguments
check exactly those files. The
configs are at the repository root: `.clang-format` with
`.clang-format-ignore`, `ruff.toml`. The discipline is check-forward:
the tools police changes and never mass-reformat. A commit that only
reformats belongs in `.git-blame-ignore-revs`, which
`git config blame.ignoreRevsFile .git-blame-ignore-revs` makes local
blame skip. Every tool is required — a missing one fails the run rather
than passing it. clang-format rides the Xcode toolchain through
`xcrun`, qmllint the Qt prefix the setup verb recorded, ruff comes from
`brew install ruff`.

Two paths no checker touches — the FlatBuffers-generated sources and
`vcpkg_installed/` — are named by the verb itself, in `EXCLUDED_FRAGMENTS`
(`scripts/sigil/check.py`), which is the one list. Both live under the
build tree, which no scope reaches on its own, so the list is what holds
when a path under it is named on the command line; `.clang-format-ignore`
carries no patterns and says so, and `ruff.toml` excludes the generated
Python beside it.

## Plates — `plates`

Three tiers, one binary. `--tier cpu` (the default) steps every sketch —
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
scene to the scene.

**Reaching the moment costs the sketch's own work, not the rasteriser's.**
A declared moment is reached by stepping a reopened session one frame at
a time, and only the last of those frames is photographed — a sketch
whose moment is twenty seconds out spends its whole render on frames
nobody reads. Those frames are described onto a canvas that keeps the
size and the clip and rasterises nothing, so the body runs, the tree is
reconciled, laid out and painted exactly as it would be, and only the
fill is skipped. It is byte-neutral by construction and was shown to be:
`chaucer_astrolabe` fell from 259 s to 3.6 s with the same hash. The ledger renders plates for the verdict and
nothing else: Sketchbook owns the thumbnails it shows, rendering them on
demand into its own cache and warming them with `Sketchbook
--thumbnails`.

Every render a hash judges carries `--no-promotion`. Automatic texture
promotion re-bakes by a measured per-frame cost, which load can tip
either way, so it is the one renderer feature a byte-identity gate has
to hold off; with it off, hashes are load-immune.

Every scene prints one line as it finishes — its running count, how it
stands, its name and what it took — in completion order, so the scene
the sweep is still waiting on is the one that has not printed yet. Each
render runs under a per-scene ceiling (`--timeout-seconds`, default
300 s); a scene still running at the ceiling is killed and reported
FAILED-TIMEOUT by name while the rest of the sweep continues, because
one runaway scene must not hang the verdict that protects everything
else. There is no per-scene override: an exception would assert that one
scene's cost cannot be reduced, which a declared cache and an earlier
settled capture moment almost always disprove.

A sketch this machine cannot render is skipped by name. A sketch written
over an optional SDK is only compiled in where that SDK was found, and
the data it needs at run time can still be absent on the machine running
the binary. The registry answers for that rather than the sweep
guessing: `--list` marks such a sketch with what it is missing, the tier
prints SKIPPED and the reason, and no plate is rendered, hashed or
judged. A skip is not a failure and not a mover — and the plates for
those scenes therefore exist only where the SDK does, so a rebase that
could not ask a scene anything keeps the baseline line already there
instead of discarding it.

There is no list of scenes allowed to move. Every mover is a finding
until it is shown to be one, and the showing is `--stability N`: a scene
that disagrees with ITSELF across N+1 renders is attributed to the scene
rather than to the change under test. A list would have to be believed;
this is measured on the machine in front of you, every time. A sketch
that draws a number it measured about its own execution — a build time,
a bake cost, a live node count — would be such a scene by construction,
so the renderer pins those: a headless session is opened with
`ctx.deterministic` set and `ctx.measured(value, pinned)` returns the
pinned number. A sketch that reads a clock and does not go through
`measured()` is the one thing `--stability` still has to catch.

### The device tier

`--tier device` renders the same sketches through the device and
compares each against the CPU plate of the same run per colour channel
— mean and p99 judged within per-sketch ceilings, max reported — because
a device plate is not a function of the drawing code alone. It has no
baseline and refuses `--rebase`; without a device runtime it says so and
exits 0. The two directories of plates are differenced by
`Sketchbook --compare <dir-a> <dir-b>`, which decodes and reports the
distances; what stays here is the judgement, because a ceiling is a
tolerance about a machine and not a fact about two files.

Why a distance and not a hash: for a set the two tiers are two
rasterisers. The host paints shaded vertices through a per-triangle sort
with Skia's antialiasing; the device rasterises the same shading through
a depth buffer with none. They agree about what the scene is and they
differ along every edge, and a post pass's blur is a box approximation
on one side and a separable Gaussian on the other. Asking for equal
bytes would fail on the first pixel and tell no one anything. So three
numbers are taken per colour channel in 0..255 over every pixel: the
**mean** absolute difference, which is the number that says the two
pictures ARE the same picture; **p99**, the value 99 channels in a
hundred stay under, which says the disagreement is confined rather than
spread; and **max**, the worst channel anywhere, which is an edge or a
body a centroid sort ranked wrongly on the host and a depth buffer
rightly on the device, and is reported rather than judged.

A sketch names its own mean and p99 ceilings in the script's
`GPU_TOLERANCE`, set from what the two tiers actually do rather than
from a wish, and tightened when one of them gets closer to the other
rather than loosened when a change moves them apart. A sketch with no
entry is judged by the default, `(12.0, 128)`. What each named ceiling
is answering:

| sketch | mean, p99 | why it stands where it does |
|---|---|---|
| `first_light` | 10.0, 96 | A comet of twelve hundred stamped beads is nothing but silhouettes, and the host antialiases those edges where the device does not. Its broad ground plate is also FOUR vertices wide: the host clamps each shaded vertex to a byte and interpolates the bytes, the device interpolates the shading and clamps per pixel, and across a quad that large the two readings drift mildly apart everywhere at once. |
| `glow_trail` | 4.0, 32 | Neither of those. The two tiers stand a per-channel unit or two apart over almost all of it, and its worst channel is where a centroid sort puts a far post behind the plate on the host and the depth buffer puts it in front on the device — the host being wrong rather than the device. |
| `material_lab` | 10.0, 192 | The loosest entry, and the one study whose two tiers are MEANT to disagree. Its five cards are chosen because the device shades them — a stack composed through a mask, a normal map, a packed roughness-and-metallic map, an emission — and the CPU tier can read a base colour and a base-colour map and nothing else, so on four of the five the two pictures are simply different pictures. That is what puts the p99 where it is. The mean is still the number that says a card landed where it belongs. On top of that it carries the drift every 3D scene here has: a broad ground plane wearing a check repeated five times across itself and seen nearly edge on, which the two tiers minify differently everywhere at once. |
| `key_light` | 3.0, 32 | A still set under a ramping key is nearly all interior: the two tiers agree to a channel or two everywhere but the silhouettes. |
| `dart_flight` | 2.0, 24 | A swept rail, a few gates and a dart on it are almost entirely smooth interior over an empty background — where the two tiers agree most closely of anything in this registry. |
| `deformed_cloud` | 2.0, 24 | A densely packed cloud of flakes reads the same way for the opposite reason: every flake stands against its neighbour rather than against the background, so there is hardly a silhouette to disagree about. |
| `scattered_model` | 4.0, 128 | The other extreme: a scatter thin enough to see through is nearly all silhouette edge, one rasteriser antialiases those and the other does not, so the p99 says so while the mean says the two are the same picture. |
| `lantern_room` | 4.0, 64 | Four coloured lamps read as directions on the host and as attenuated emitters on the device, so the bodies between them are shaded from slightly different strengths — a low mean over a picture that is mostly dark, and a p99 at the lit edges. |

A ceiling is for a scene the two tiers draw the same way and read
slightly apart. There is no list of scenes the tier declines to judge: a
scene that draws differently on the two backends is a defect in what
drew it, and a sheet whose subject is a call one backend does not
implement says so with a call every backend performs.

### The promotion tier

`--tier promotion` renders the same sketches twice on the CPU — once
with automatic texture promotion held off, once with every promotable
node eagerly baked — and judges the pair against the contract compose
states. It exists because nothing else exercises the promoter: a
headless session is opened deterministic and a deterministic session
holds promotion off, so every other tier renders the runtime with that
feature switched out.

The ON half is eager so the tier tests the same node set on every
machine. The runtime's own rule is a stopwatch — a node is baked once
its paint has measured over a millisecond for eight consecutive frames —
which on an idle machine promotes nothing and on a loaded one promotes a
different handful each run, and a tier that measures the machine
measures nothing. Eager bakes every node the rules admit, from its first
frame, and changes nothing about what a bake may do.

THE CEILING IS TWO BARS, and neither is a tolerance anyone chose. Each
differing pixel is judged by what the HELD-OFF plate — the reference —
holds in that pixel.

Where that pixel is TRANSPARENT BLACK the bar is one code value. A
promoted node is baked under the live matrix post-translated by an
integer, and inverting that matrix to find a shader's local coordinates
does not cancel the integer to the last bit at a scale whose reciprocal
is inexact, so a shaded pixel can land one value from the live paint and
nothing may land further.

Where it holds CONTENT the bar is two. There the bake lands on
something, so the node's own coverage is composited twice where the live
paint composited once — into the bake, and again when the bake is
blitted — and Skia's blit of a raster image is not the arithmetic of its
direct shader draw. A texel whose alpha is between none and all can
settle one value further out over a bright backdrop, and taking the bake
at higher precision does not remove it.

Past either bar is a picture that MOVED — a bake somewhere else,
rasterised against another clip, or gone stale — which is a defect to
file against the promoter. `Sketchbook --compare` reports the worst
difference under each bar (`clear` and `content` on its line), which is
what lets the two be judged apart. The tier keeps no baseline and
refuses `--rebase`, because there is nothing here to adopt.

### Where the plates go

THE PLATES ARE KEPT, beside the manifest, under
`build/plates_<config>/` — one directory per tier, one PNG per scene,
overwritten rather than accumulated, and never committed because
`build/` is ignored. `baseline/` holds what the manifest was baked from
and is overwritten on rebase; `cpu/` holds what the last judging sweep
rendered; the two comparing tiers keep both halves (`device/cpu`,
`device/gpu`, `promotion/off`, `promotion/on`). A hash says a scene
MOVED and nothing about where, so each mover's verdict line names its
two files and the summary prints the `Sketchbook --compare
build/plates_<config>/baseline build/plates_<config>/cpu` that
differences them channel by channel.

The manifest is machine-local by design (plates are deterministic per
machine, not across machines), so a fresh checkout runs
`sigil.py plates --rebase` once before a sweep can judge anything. The
manifest is keyed by the registry NAME, which can carry spaces (`--scenes
"aero desktop"`); the frame-rate ledger below is keyed by the sketch's
STEM (`--sketch aero_desktop`).

A sketch that faults is named: the render for it fails, the report on
stderr says which entry it was on, the phase (setup, draw, capture) and
how many plates finished before it, and the run's verdict is a failure.
The next run can be narrowed to that sketch with `--sketch`.

## Timings — `bench`

TWO LANES, ONE BODY. `sigil.py bench` (`mise run bench`) runs every
`*_bench` binary — one per library, under `build/bin/<config>/benches/`,
which the `benches` target builds — one at a time on a quiet machine and
compares medians against `bench/baseline_<config>.json`, keyed by
`binary:arm`. `sigil.py bench --lane fps` (`mise run fps`) presents each
sketch in the real window at a stated size and device pixel ratio through
Sketchbook's `--window-bench` and judges the presented rate against
`bench/app_fps_<config>.json`, keyed by the sketch's stem. Two window
sweeps cannot share one display, so the fps lane runs alone; that is why
the two stay separate lanes rather than one command, and everything that
happens to a number after it is taken is the same for both. A narrowed
sweep merges on `--rebase`, a SLOWER row beyond its band fails the run,
NEW and MISSING rows do not. Use a Release build for either.

**What the two measure.** The frame-time gate behind `--bench` renders
onto a raster surface at the sketch's declared size and presents
nothing: it is the sketch's own cost isolated, which is what makes it a
gate. The window lane draws through the surface the window presents, at
the window's pixels and its device pixel ratio, and the numbers carry
the host's own overhead — the submit or texture upload that puts the
frame on screen, and for a set the device readback and blit its paint
phase performs. Neither replaces the other. A presented rate is bounded
by the compositor, which means the display: a sketch comfortably inside
its budget reads at the refresh rate and says nothing more, so the
interesting rows are the ones BELOW it and `work` beside them says how
much of the frame was the sketch. A row of the window lane is the sketch
that was on screen and the stretch that was measured: the warm-up starts
at the first frame of the selection's own session, the rolling windows
are emptied where the measured stretch begins, one session is held at a
time and no thumbnail is written while a rate is being taken — so a
sketch reads in the sweep what it reads presented alone. A sketch the
window never presented is stood down by name, and a sweep that stood one
down exits non-zero rather than reporting a rate of zero. A baseline is therefore per machine
AND per display mode, and the file records the window size and scale it
was taken at so a mismatch is visible rather than silently compared.

**The screen has to be unlocked.** The lane asks before the sweep and
again after it, and refuses either way without touching the baseline. A
window behind a lock screen is one nothing can see, and the system runs
an invisible window's work several times slower once it has been that
way for about half a minute: the first few rows of a sweep read their
true rate, everything after them reads the slowed one, and no row says
so. Every sketch measured alone inside that half minute reads what the
baseline holds, which is what makes such a sweep look like a regression
in the sketches rather than a measurement taken in the dark.

**How a timing is taken.** Each benchmark runs `--repetitions` times
(default 5), every repetition long enough to satisfy `--min-time`
(default 0.1 s) after an untimed `--warmup` (default 0.1 s). The first
repetition is discarded as well — it is the one that pays for page
faults, lazily built caches and frequency ramp — and the median of the
rest is the number. Real (wall-clock) time is what is compared, since it
is what a frame budget is spent in; CPU time is kept in the baseline for
reference and never judged.

**How a number is judged.** Each row has a tolerance band, ±10 % by
default. Within the band a row is IDENTICAL, past it in the good
direction FASTER, in the bad direction SLOWER, and any SLOWER row fails
the run. A row the baseline has never seen is NEW and a baseline entry
no run produced is MISSING; neither fails the run, since the fix for
both is `--rebase`. A benchmark whose noise is honestly wider gets its
own band by name rather than widening everyone's — a widened band is a
statement about that benchmark's run-to-run spread on a quiet machine,
never a way past a finding:

| pattern | band | why |
|---|---|---|
| `:BM_Noise` | 15 % | Sub-microsecond bodies: a hash, a lookup, a resolve. The timer's own resolution and the loop overhead are a visible fraction of each. |
| `_Cold`, `ReplaceWholeParagraph_Cold` | 15 % | The cold arms purge and refill a cache inside each repetition and pay HarfBuzz for every word; allocator state moves them more than the warm arms. |
| `weave_bench:BM_Draw` | 15 % | Raster painting through Skia's CPU backend has thread-pool warm-up inside it that the warm-up period does not fully settle. |
| `scry_bench:BM_Page_ChangeLatency` | 75 % | A latency, not a cost: the arm waits for the web thread's own repaint, which is paced at a fixed cadence, so the figure moves by a whole frame interval from one run to the next. |
| `world_bench:BM_ChainOnDevice/20000/` | 35 % | The smallest device cook is mostly the fixed cost of putting work on the device and taking the answer back, and the driver pays that in one of two states — the arm's median lands in one or the other, a quarter apart, on a machine doing nothing else. Its own band, not the benchmark's: the arms with ten and fifty times the points hold to a percent, and they are where a change to the cook shows. |

**The machine must be quiet**, for both lanes. Timing wants the opposite
conditions from hashing: the binaries run one at a time and a build, a
browser or a sync client running beside them corrupts every number. A
SLOWER row on a busy machine is the machine, not the code — rerun the
lane alone before reading it as a finding. The window lane needs the
compositor as well as the CPU: it opens a real window and a compositor
busy compositing something else is measured along with the sketch.

**The baselines are committed**, one file per build configuration, so a
change that moves a number moves it in review. Numbers are per machine:
a baseline taken on one host says nothing about another, and each file
records the host it was taken on so a mismatch is visible rather than
silently compared. A subset is a subset at every level — `--benches`
names some binaries and `--filter` some arms inside them, and either way
what was not measured survives the rebase, because adopting one
deliberately changed number must not drop the numbers this run never
took. Only an unnarrowed sweep writes a file wholesale, which is what
drops a row that no longer exists.

## Instrumented trees — `sanitize`

THREE LANES, ONE BODY: `--lane address` (ASan+UBSan, the default, in
`build-asan/`), `--lane thread` (TSan, `build-tsan/`) and `--lane
coverage` (LLVM source-based profiling, `build-coverage/`). The setup
verb writes one secondary preset per lane into `CMakeUserPresets.json`
beside `main`, each the `main` composition plus its instrumentation
switch in its own directory, reading the primary tree's
`vcpkg_installed/` as-is with the manifest install disabled. Their test
presets carry the runtime environment — the raw-profile path for
coverage, `ASAN_OPTIONS`/`UBSAN_OPTIONS`/`TSAN_OPTIONS` for the
sanitizers — so each lane is a preflight, a configure, a build, a ctest
and what CMake cannot do. Which targets a `--filter` needs built, and
which binaries the selected tests ran, come from `ctest --show-only`, so
nothing here parses generated files.

A tree of its own per lane, because switching a tree's flags recompiles
every object in it. Only the C++ flags are set: every `.mm` in this tree
compiles through the C++ compiler, and SpellCircleMac's Swift sources go
through swiftc, which rejects them, so `src/spellcircle/mac/` takes them
back off that one executable's link line. `-fno-omit-frame-pointer` is
on every lane because reports unwind through frame pointers and without
them the stacks degrade to unusable fragments; the instrumented trees
build RelWithDebInfo, since the instrumentation reads optimised code
fine and only a debugging session asks for Debug.

The coverage lane builds the test targets, runs ctest under LLVM
source-based profiling, and writes the `llvm-cov` summary to the console
plus an HTML report under `build/coverage/html/`, with `RUN.txt` beside
it recording the invocation. `--filter <regex>` runs a subset and builds
only what it needs; `--export-lcov <file>` emits lcov. The report lands
in the PRIMARY build directory beside the other build artifacts, and
each run replaces it wholesale — what sits there is always the last run,
never an accumulation — so `RUN.txt` is what keeps a filtered subset
from passing for the full suite. The profile path carries `%p` (process
id) to keep concurrently running tests from clobbering one profile and
`%m` (module signature) to keep profiles from differently instrumented
binaries apart so the merge stays well-formed. Test sources are left out
of the report unless `--include-tests` asks for them, matching both
`test/` and `tests/`, because a directory named the other way would
otherwise start counting toward coverage silently and a regression that
shows up as a BETTER number is one nobody goes looking for. The pipeline
is Clang's: on macOS the tools go through `xcrun`, which pins them to
the same toolchain that produced the instrumented objects so the profile
and coverage-map formats agree, and elsewhere they come from
`$LLVM_ROOT/bin` or `PATH`.

The two sanitizer lanes take `--filter` / `--targets` / `--config` the
same way. A sanitizer lane deletes its own tree once ctest has had its
verdict and refuses to start while the other one's tree stands;
`--keep` holds a tree for a debugger. A configure or build failure
leaves the tree standing, on purpose: only a finished ctest run is a
verdict, and only a verdict makes the tree disposable. UBSan's default
is print-and-continue, which would let a finding scroll past inside a
passing test, so `-fno-sanitize-recover=undefined` makes a finding fail
its test; UBSan also prints a stack on every line rather than the first
frame, without which a report names a `file:line` and nothing about how
execution reached it. TSan halts on the first error instead of
accumulating a scroll of reports from one root cause, and prints both
acquisition stacks of a lock inversion, without which one side is a
guess.

vcpkg archives are uninstrumented. That still catches this repository's
bugs — the interceptors wrap every allocation and libc call regardless
of who compiled the caller — and the casualties are three, all the same
boundary. The container-overflow check compares a container's size
against annotations the contained memory only gets when the code that
grew it was instrumented, so a `std::string` or `vector` that crossed
the dependency boundary reports false overflows; it is off at runtime
and every other ASan check is unaffected. A dependency header that grows
a member under instrumentation moves every field behind it, so the
address lane pins Skia's array layout to agree with the prebuilt
archive; `cmake/SkiaSanitizerAbi.h` asserts the pin took, and the
`version>=` on `skia` in `vcpkg.json` names the port it was written
against. And a dependency whose headers this tree instantiates in half
has accesses the thread lane sees while the ordering compiled into its
archive stays invisible, so each such dependency has an entry in
`ThreadSanitizerSuppressions.txt` stating why its ordering is real.
Nothing of this repository's own is in that file. LeakSanitizer is
unsupported on Apple Silicon and off — with it on the runtime aborts at
startup before any test runs.

## Docs, assets, flags, schema

`sigil.py docs` generates the Doxygen site per library from the manifest
`sigil_library_root()` and `sigil_add_docs()` calls write (`docs/README.md` is the canon; the
`docs` target is absent without Doxygen). Generation runs in TWO PASSES
because the libraries reference each other's types in both directions —
SigilWorld takes SigilGeometry's meshes, SigilCompose takes SigilMotion's
animatables — and a Doxygen tag file can only be read after it has been
written: the first pass writes every tag file and no HTML, the second
reads all of them and writes the HTML. A single pass would resolve only
the edges that happen to run in registration order. The theme is
jothepro/doxygen-awesome-css (MIT), pinned to a commit and hashed per
file, fetched through the same downloader the assets use, because
Doxygen's stock HTML is close to unusable on a phone and Doxygen Awesome
replaces the stylesheet without touching the generated structure. The
HTML header is generated by `doxygen -w` rather than checked in — a copy
frozen in the source tree would drift silently on a Doxygen upgrade and
show as a half-styled page rather than an error — and only the script
tags are ours. A Doxyfile is written only when its content moved, since
Doxygen re-runs from a timestamp.

`sigil.py docs` with no `--manifest` drives the `docs` build target
instead, opening the pages when they are written or serving them from a
container with `--serve`; a fresh checkout is configured first, since
documentation is parsed from headers and the tree never has to be
compiled for it.

`sigil.py assets` (`mise run assets`) fetches the open-licensed demo assets into `build/assets/`
from its hash-pinned manifest and needs no configured tree. The hash is
the contract: a file already in place is kept only when it hashes to
what the manifest says, and a download whose bytes hash to something
else is never written, so a mismatch cannot later be read as the asset
it is not. Anything added to that manifest carries an OPEN licence with
the licence file fetched alongside, is pinned to an immutable commit
rather than a branch so the URL cannot change under the hash, and
declares a sha256. No game, film or museum rips: the studies reproduce
GEOMETRY and PALETTES, which are facts about a design, and do not ship
its art.

`sigil.py flags` lifts the sketch compile command out of the compilation
database into the flags file the live host uses. The step itself is
`src/sketch/cmake/SketchFlags.py`, beside the sketch host whose build
runs it, and the verb is the front door for running it by hand. The
single
source of truth for how a sketch builds is the target graph itself, not
a hand-maintained flag list, and the fully composed compile line —
toolchain flags, sysroot, `-std`, vcpkg include directories — is
something CMake exposes only through that database. It strips the
per-object bookkeeping (the two flags naming this object and the
dependency-file flags the generator appends) and re-escapes the double
quotes the database unquoted, so a define whose value is a string
literal still means what the compile line meant.

`sigil.py flatbuffers` (`mise run flatbuffers`) regenerates the
committed Python schema modules. Only the Python side: the C++ header is
generated into the build tree by the `SpellCircleSchema` target and is
not committed, while `apps/python` is installed and imported — by
TouchDesigner among others — with no CMake build in reach. The generator
writes a package, and the `SpellCircle/__init__.py` it emits is a name
already taken by the hand-written public API, so generation goes to a
staging directory and only the schema modules are copied over.

`sigil.py assets --stage <archive>`
puts an archive nobody can download without an account into the vcpkg
asset cache under the SHA-512 the port declares, so every configure
after that resolves it locally. vcpkg finds a file in that cache by the
SHA-512 of its contents — the file's own name there — which is what lets
a port name such an archive at all: the port declares the file name and
the hash, the archive is staged once per machine, and the hash this
prints is the one `vcpkg_download_distfile` declares.

The overlay triplet `cmake/triplets/arm64-osx.cmake` is hashed WHOLE into
every port's ABI key — its comments with the rest of it — so editing one
character of it rebuilds every dependency from source. Say what has to be
said there, and say it once.
