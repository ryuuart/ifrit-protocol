# Building and testing a library

Every library in this tree is built, tested and measured the same way.
This page is that contract, written once. A library's own README states
only what is true of it alone: which targets it builds, which suites its
one test binary holds, which of them carry a label, and which fixtures
are its own.

## Configuring, building, running

From `apps/spell-circle-canvas`:

```sh
python3 scripts/sigil.py setup --config Release
cmake --build build --config Release --target <library>_test
ctest --test-dir build -C Release --output-on-failure
```

`setup` discovers Qt and vcpkg and writes the uncommitted
`CMakeUserPresets.json`; it is one of ten verbs over the build's
administration, and `scripts/README.md` is the canon for all of them.
Every workflow is also a mise task. Use a Release build for anything
that is timed: the benchmarks and several sketches are deliberately
stressful and a Debug timing says nothing.

## One test binary, one benchmark binary

A library has ONE test binary, `<library>_test`, built from every
feature directory's `test/` and landing in `build/bin/<config>/tests/`,
and ONE benchmark binary, `<library>_bench`, built from every feature
directory's `bench/` into `build/bin/<config>/benches/`. The first
`sigil_test()` or `sigil_bench()` call creates the binary and every later
one adds its own sources to it, which is how a feature keeps declaring
its cases beside the code they cover without a target per file.

ctest discovers one entry per CASE, so a suite is selected by name with
no target behind it — `ctest -R '^Suite\.'` — and one case the same way,
`ctest -R 'Suite.ACaseNamedAsAClaim'`. What locates a case is that a
suite is named for the feature it covers and its file sits in that
feature's directory.

The benchmarks are executables and never tests. They hang off the
`benches` target and run through `sigil.py bench`, which runs each binary
one at a time on a quiet machine and judges the median real time of each
arm against the committed `bench/baseline_<config>.json`. Any number
about how long something takes belongs to that ledger, and any claim
about pixel identity to the plate ledger — never to prose.

## What a case asserts

A case asserts ONE behaviour the library promises through its public
headers to a caller who has read only its README, and its name is that
promise written as a sentence, so a failure line reads as the claim that
broke.

It pins only what editing the code alone could falsify: a caching count,
a closed form, a field walk, one description drawn two ways, two
executors of one kernel agreeing bit for bit. It never pins what a
rebuild, a font, a device or a clock could move — an anti-aliased byte, a
fitted tolerance, a byte layout the compiler chose, elapsed time. A test
that renders a picture to compare it belongs to the plate ledger and one
that times a loop to bound it belongs to the bench ledger.

A claim made N times with one thing varying is one `TEST_P` whose
parameter is that thing, with its rows named. One file per subject, named
for what it asserts, so a case is found by opening the file its subject
names rather than by searching for its case name.

## Labels

A case that skips on this machine is not coverage on this machine. What
a case needs is said with a ctest label, attached to the suites that need
it rather than to the whole binary wherever the binary holds cases that
do not:

| label | what a runner must supply |
|---|---|
| `gpu` | a GPU: Metal on Apple, or a Vulkan runtime (`brew install molten-vk vulkan-loader`) |
| `fonts` | the machine's own installed faces, because the machine's font set is what the case is about |
| `network` | a route to the internet |
| `oiio` | OpenImageIO, found at configure time |
| `ocio` | OpenColorIO's view transforms |
| `svg` | the SVG decode backend |
| `usd` | OpenUSD's plugin registry |
| `substance` | the Substance SDK's sample archives |
| `ultralight` | the Ultralight SDK |
| `cocoa`, `window` | a window server, and for `window` a real window |

`ctest -L <label>` is the run a verdict about that thing may be read out
of; `ctest -LE <label>` is how a machine without it checks the rest.

## Fixtures

A fixture more than one file needs lives once, in the library's
`test/support/`; a helper one file uses stays in that file. A fixture
only one library asks for is committed under that library's
`test/assets/` and reached through the `SIGIL_TEST_ASSET_DIR` compile
definition, so a test and the benchmark beside it both run from any
working directory.

What more than one library asks for is the tree's own, under `src/test/`,
on every test and bench binary's include path:

- `Fonts.h` — `sigil::test::fonts()`, the one font context a process
  shapes through, and `sigil::test::instrument::sans()` and its siblings,
  the generated faces under `src/test/assets/` that carry one property
  each, so a claim about a face is a claim about a face this repository
  ships rather than about the machine.
- `ScratchDir.h` — `sigil::test::ScratchDir`, a directory named for the
  case and the process, emptied both ways, so two runs side by side never
  read each other's files.
- `GlyphCanvas.h` — the glyphs handed to Skia, captured with no device
  and no rasterization behind them.
- `ShaderTable.h` — the one question every embedded shader table is
  asked: whether it holds the whole of the directory its library keeps
  its shaders in, and each one's bytes.

## The documentation's names

Every library's README compiles, itself and its chapters.
`sigil_doc_probes()` in `cmake/Sigil.cmake` reads the documents a library
registers, extracts every qualified name an author could copy out of them
— and the bare names of a bullet that opens with a header path — and
generates a translation unit of probes that only builds if the headers
still spell those names that way. A documented name no header declares
fails the generator, so a rename the prose missed is a build break rather
than a confident wrong answer.

`sigil_header_self_test()` is the companion guard a library may add: one
generated translation unit per public header, holding that header twice
— the first line proving it stands alone, the second that it can be
included again — which is what makes "each header stands on its own" a
build fact rather than a claim.

`apps/spell-circle-canvas/scripts/README.md` is the canon for what the
probe guard checks and what it structurally cannot see.

## Looking at it

Everything renderable is a **sketch**, under `src/sketch/sketches/`, in
one registry, drawn by one application. `src/sketch/README.md` is the
canon for how one is written, run and hot-reloaded, and for the plate
ledger that judges pixel identity.
