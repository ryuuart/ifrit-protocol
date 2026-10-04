# Grimoire — the stock sketch host

Grimoire is an application built on SigilSketch: the stock host, and an
example of using the framework. It lists the sketches it is pointed at —
its compiled-in catalogue, a file anywhere on disk, or a workspace folder —
and runs any of them live in a window, rebuilding a C++ sketch or
re-importing a Python one on every save. Without a window it is the
headless renderer the plate ledger drives: stills, sweeps, a video
montage, plate comparisons, frame-time measurements and a host a protocol
client drives.

It carries this repository's catalogue of sketches, `sketches/`: the
studies, specimens and starters every library is exercised through.

What a sketch is, how it is registered and how a host compiles, loads and
swaps one are [SigilSketch's](../spell-circle-canvas/src/sketch/README.md).
Grimoire reaches the framework and every library it draws with only
through their public targets and headers — `<sigilsketch/…>`,
`<sigilcompose/…>` and the rest — as any other host would; nothing here
includes a file from inside the library tree's sources or reaches a
private include directory.

## Layout

| Directory | What it holds |
| --- | --- |
| `src/` | the application: `main.cpp`, the lanes a command line selects, the window's view and renderer, and its QML under `src/qml/` |
| `sketches/` | the catalogue: every entry, its data, the `SigilSketches` object library its `CMakeLists.txt` builds, the Python sketches' registry and `Anchor.cpp`, the unit the hot-reload flags are lifted from |
| `test/` | the application's cases (`grimoire_test`), its QML tests, and the scripts and probe files the whole-host tests run |
| `bench/` | the application's benchmarks (`grimoire_bench`) |

## Building it

**As part of the library tree's build**, which is how it is built day to
day: `apps/spell-circle-canvas` adds this directory whenever it builds its
applications (`SIGIL_BUILD_APPS`, on by default), with its binary
directory inside that tree's, so there is one build tree and one ctest.
From `apps/spell-circle-canvas`:

```sh
python3 scripts/sigil.py setup --config Release
cmake --build build --config Release --target Grimoire
```

The bundle lands at `build/bin/Release/Grimoire.app`, beside the response
file `sketch_flags.rsp` its live host compiles sketches with.

**On its own**, as the top-level project: this directory then declares its
own project and adds the library tree as a subdirectory. The vcpkg
manifest and the overlay triplets are the library tree's, and the build
points the vcpkg toolchain at them before the project is declared, so
configuring it wants the same toolchain the library tree is configured
with and the same Qt:

```sh
cmake -S apps/grimoire -B <build directory> -G "Ninja Multi-Config" \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_PREFIX_PATH=<Qt 6.11 prefix>
cmake --build <build directory> --config Release --target Grimoire
```

That tree holds a whole second build of every library.

## Running it

```sh
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire              # the window
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire --sketch hello
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire <file.cpp> --frame out.png
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire --headless <outdir>
```

`mise run sketch` builds it and launches it with the arguments after `--`.
Its settings, recent workspaces, thumbnails and recorded device programs
are kept under the platform's locations for Grimoire, or under the root a
run names with `--state <dir>`.

## Documents

- [RUNNING.md](RUNNING.md) — every command line, the window's lanes, what
  each mounts for a protocol client, the montage, stills, comparisons and
  the frame-time gates.
- [BROWSER.md](BROWSER.md) — the window's browser: selection and
  presentation, grouping and search, and the thumbnails it keeps.
- [TESTING.md](TESTING.md) — the application's cases, the whole-host
  tests that compile and load a sketch, and narrowing the catalogue to a
  few sketches while the rest are broken.
- [sketches/README.md](sketches/README.md) — what is in the catalogue,
  with [its coverage map](sketches/COVERAGE.md) and [presentation
  audit](sketches/SKETCH_REFINEMENT.md).
- `python3 scripts/sigil.py` in the library tree drives this binary for
  plates, benchmarks and workspaces; its
  [README](../spell-circle-canvas/scripts/README.md) is the canon for each
  verb.
