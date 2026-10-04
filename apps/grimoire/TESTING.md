# Testing Grimoire

From `apps/spell-circle-canvas`, where the one build tree stands:

```sh
python3 scripts/sigil.py setup --config Release
cmake --build build --config Release --target Grimoire grimoire_test
ctest --test-dir build -C Release --output-on-failure
```

Every suite of the application's own is named for it, so
`ctest -R '^Grimoire'` selects them. The application has one test binary,
`grimoire_test`, built from `test/`,
and one bench binary, `grimoire_bench`, built from `bench/`. Its cases
cover what is the application's own: the command line, the workspace a
folder makes, the pipeline store, the pane's view of the canvas, the
browser's rows and the endpoint a window and a sweep mount. Their fixtures
— the font context, the asset store, a surface's pixels and a scratch
directory — are `test/Fixtures.h`, this application's own, so nothing
here reaches into the framework's test support. The browser's QML
components are tested by `sketch_browser`, `sketch_publication_controls`
and `sketch_workspace_controls`, each a `qmltestrunner` over a file in
`test/`.

The rest of the entries drive the built `Grimoire` binary as a script
does. What SigilSketch tests of itself, in its own binary and with its own
fixture sketches, is [its testing
chapter](../spell-circle-canvas/src/sketch/TESTING.md).

## The documents' sketch stems

Two of the entries run no C++ at all. `sketch_readme_stems` resolves every
sketch stem the repository's documents name against this catalogue: a
backticked snake_case token in a paragraph that is talking about
sketches, studies or a study must name a file under `sketches/`, and one
that does not is either exempted by name and reason in
`test/readme_sketch_stems.py` or fails the run. It refuses a count of a
list too — a cardinal qualifying "studies", "sketches" or "scenes"
beside a named stem, when it claims the list's own length, is maintained
by hand in lockstep with the list and goes stale the moment the list
grows, so the count is deleted and the list is the count. It is the
catalogue's check — the stems are this catalogue's — which is why it is
here rather than beside each document, and why it reads the library
tree's documents as well as this application's. `sketch_readme_stems_self_test` runs the checker's own
fixtures, and is the only thing that would notice the extractor
narrowing: a checker that silently resolves fewer stems still passes over
the corpus.

## Three ways a sketch is put through a host, and why they are all here

The `sketch_reload_*` entries in `CMakeLists.txt` run
`Grimoire <file.cpp> --frame out.png`, which compiles the file with the
captured response file, dlopens the result and runs it — the DYNAMIC
path, and the only one that can see a missing archive in the force-load
list. SigilSketch's plate cases call `sweep()` IN PROCESS against fixture
sketches their own binary registered. `scripts/sigil.py plates` runs
`Grimoire --headless --ledger` over the COMPILED-IN registry and judges
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
no Grimoire links at all. `-DSIGIL_SKETCH_ONLY=stem;stem` at configure
time narrows the registry a tree compiles to those stems — the directory
is still the only list of what a sketch IS; this says which of them one
tree carries — so a pass over the host can be looked at through the one
sketch it is studying. Leave it empty, the default, for every sketch.
