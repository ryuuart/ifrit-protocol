# Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/sigil.py setup --config Release
cmake --build build --config Release --target sketch_test
ctest --test-dir build -C Release --output-on-failure
```

A suite is selected by its own name — `ctest -R '^SketchRegistry\.'` — and
a case by its full one, with no target behind either.
The library has one test binary, `sketch_test`, built from every
feature's `test/` directory; ctest discovers one entry per CASE out of
it. `core/test/` covers the registry, the kind seam, the crash reporter
and where a sketch stands on disk; `canvas/test/` and
`set/test/` the two sessions;
`kit/test/` the sheet a specimen stands on; `live/test/` the
host, the resident set and the cadence a window sweep keeps;
`plate/test/` the sweep, the comparison
of two directories of plates and the montage MP4 exporter;
`book/test/` the reload path and the catalog's rows, each through the
`Sketchbook` binary as a script; and `scry/test/` the shared web engine
beside the case that
takes a page's still — two cases that must not meet in one process because
the engine allows one renderer per process and the shared-engine case
ends by shutting its one down for good. Both are labelled `ultralight`
and are absent altogether without that SDK. Beside the test binary
stands one bench binary, `sketch_bench`, built from every feature's
`bench/` directory — a Google Benchmark executable rather than a test,
reached through the `benches` target.

A case asserts one behaviour a session or a host promises to a caller who
has read only this page, and its name is that promise written as a
sentence. It pins only what editing this library could falsify — a step
count off a clock the host steps rather than reads, a projection's own
arithmetic, the bytes two runs of one declaration agree on, the width a
plate comes out at read from the constant the sweep uses — never a fitted
tolerance, an anti-aliased byte or elapsed time. Pixel identity across a
change is the plate ledger's to judge and what a frame costs is the bench
ledger's. A claim made N times with one thing varying is one `TEST_P`
with its rows named: which file an edit landed in and whether it is part
of the sketch, over `AHeaderBesideABareSketch`,
`AUnitBesideADirectorySketch`, `AnotherBareSketchBesideIt` and
`AModuleInTheSharedLayer`.

**What every session promises is written once.** A host steps, repaints
and photographs a session without ever learning which runtime it is
holding, so the six claims that follow from that live in
`test/support/Sessions.h` — the canvas the body declared while it opened,
the runtime the kind names, the lanes the runtime spends, a frame as the
body's own time plus the runtime's, the oversample a still is worth
taking at, and a repaint that draws the state the frames left and
advances nothing. Each session's cases instantiate them with a traits
type naming its own fixture sketch, and what is left in each session's
file is what only that runtime does: a canvas re-renders for its still
and so takes one more step, a set is formed at the resolution of the
canvas it is handed rather than magnified onto it.

Two of the entries run no C++ at all. `sketch_readme_stems` resolves every
sketch stem the documents in this tree name against the registry: a
backticked snake_case token in a paragraph that is talking about
sketches, studies or a study must name a file under `sketches/`, and one
that does not is either exempted by name and reason in
`test/readme_sketch_stems.py` or fails the run. It refuses a count of a
list too — a cardinal qualifying "studies", "sketches" or "scenes"
beside a named stem, when it claims the list's own length, is maintained
by hand in lockstep with the list and goes stale the moment the list
grows, so the count is deleted and the list is the count. It is the
registry's check, which is why it is here rather than beside each
document. `sketch_readme_stems_self_test` runs the checker's own
fixtures, and is the only thing that would notice the extractor
narrowing: a checker that silently resolves fewer stems still passes over
the corpus.

Fixtures live in `test/support/`, reached as `"support/<name>.h"`, and
nothing is written twice. `Fixtures.h` is the one asset store a process
holds — never destroyed, because it outlives every session opened over it
— beside the font context the whole tree shares, which is
`src/test/Fonts.h`'s and not this library's. `Pixels.h` takes the
readings off a picture: what a surface or an image holds, whether two
plates are one picture, the box the drawn pixels stand in, and where that
box stands as a fraction of the plate so two plates of different sizes
can be compared. `Sessions.h` is the contract above. `live/test/Fixture.h`
holds what both halves of the live feature's cases need beyond them: the
compiled-in square, its registry entry, and a `Watched` file standing in
a scratch directory of its own — bare, or in a directory named for it,
which is the other shape a sketch takes — which the shared
`src/test/ScratchDir.h` empties on the way in and removes on the way out.
The plate cases register their fixture sketches the way a sketch file
does, so the sweep it drives walks a real registry — including one whose
`available()` probe says no, which the sweep passes over rather than
failing on and writes no plate for.

A wait inside a test is a COUNT OF TURNS and never an open loop: a build
polled to completion and a forked child read to its fault both give up
and say so, because a run that hangs reports nothing at all where a run
that fails names the claim that broke.

`Host::Options::siblingScanInterval` names how long the host waits
between re-reads of the headers standing beside the sketch. It defaults
to a quarter second, because reading a directory is cheap but not free
and a header is saved by hand a moment before the sketch is; a test that
edits a header and polls sets it to zero, so the edit is seen when it is
made rather than whenever the cadence next comes round.

## Three ways a sketch is put through a host, and why they are all here

The `sketch_reload_*` entries in `book/CMakeLists.txt` run
`Sketchbook <file.cpp> --frame out.png`, which compiles the file with the
captured response file, dlopens the result and runs it — the DYNAMIC
path, and the only one that can see a missing archive in the force-load
list. The plate cases call `sweep()` IN PROCESS against fixture
sketches its own binary registered. `scripts/sigil.py plates` runs
`Sketchbook --headless --ledger` over the COMPILED-IN registry and judges
plate hashes. Three different things, and none of them stands in for
another.

Within the dynamic entries, one per distinct surface: `shapeworks_lab`
and `first_light` are the widest canvas and set sketches by the symbols
they name, `stock_materials` paints one of every stock material,
`video_compose` reaches the decoder and encoder archives no
geometry-heavy sketch names, `world_hud` is the other registration form,
`dunhuang_star_chart` is the directory form — several units compiled
apart and linked once — and the entries behind an optional SDK name
symbols nothing else does. The archives only the HOST links, a device
backend among them, stand behind a probe file beside that list rather
than behind a sketch, because nothing in the registry names one and what
it asserts is a dlopen and not a picture. A starter sketch that names
none of those adds no entry of its own: anything that stops it compiling
and loading stops the wide ones too.

Every one of those judges a compile and a load, and none of them judges
WHOSE code drew: a host that quietly ran its own copy of the sketch
passes all of them. `sketch_reload_runs_the_file` is the one that looks,
by rendering a copy of a registry sketch whose ground colour has been
replaced and reading the corner pixel back, with the registry's own copy
of the same sketch as the control.

## A host over one sketch while the rest are broken

The sketches come last: library work is expected to break them, and a
host links every sketch it carries, so in the middle of a library pass
no Sketchbook links at all. `-DSIGIL_SKETCH_ONLY=stem;stem` at configure
time narrows the registry a tree compiles to those stems — the directory
is still the only list of what a sketch IS; this says which of them one
tree carries — so a pass over the host can be looked at through the one
sketch it is studying. Leave it empty, the default, for every sketch.
