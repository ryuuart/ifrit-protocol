# Sigil

The Sigil libraries make a GPU-forward 2D renderer whose pages are
materials on 3D surfaces. A page — boxes, text, paths and pictures, laid
out and styled the way a web page is — is drawn with Skia on the GPU, and
SigilWorld renders it as the material of a lit surface, live, at the pace
of a game. Each library is independent, with its own README as its canon
and its own test and benchmark binaries.

Grimoire, at [`apps/grimoire`](../grimoire/README.md), is the host. It
runs a sketch — one scene, written in C++ or in Python — live in a
window, renders it headless for the plate ledger, and with `--publish`
offers what it draws as a shared texture for another program to pick up.
A sketch hears the outside world through SigilIO's doors and reads what
arrives through SigilData, so an external patch can drive it; the
[TouchDesigner guide](docs/guides/sigilio-flatbuffers-touchdesigner.md)
walks one patch from JSON over UDP to a FlatBuffers schema of its own.

Python is a first-class authoring language. The `sigil-sketch`
distribution in `apps/python/sigil/` imports as `sigil`, renders headless
with `sigil render` and opens the live host with `sigil open`; Grimoire
also opens a Python sketch directly through its file picker.

## Libraries

Most of the code is in libraries under `src/common/`, `src/sigilweave/`
and `src/sketch/`, each of which has its own README:

| Library | What it does |
| --- | --- |
| [SigilCore](src/common/core/README.md) | The vocabulary every library shares: comparable erasure, the compute values a drawing is drawn from, the time unit, the keyed memo, where work runs, and the reporting door |
| [SigilGpu](src/common/gpu/README.md) | The GPU device itself: one device and its queue, owned or adopted, textures and fences named by handles |
| [SigilSkia](src/common/skia/README.md) | Skia Graphite on a device someone else owns |
| [Sigil.Qt](src/common/qt/README.md) | Reusable Qt Quick controls |
| [SigilMedia](src/common/media/README.md) | Pictures, still and moving: image and animation decoding and encoding, streaming video decoding with GPU composition and MP4 encoding, distance fields and pixel differences |
| [SigilIOPublish](src/common/io/publish/README.md) | Optional SigilIO native texture publication and subscription |
| [SigilIO](src/common/io/README.md) | Resource access and export: URIs, mounts, caching, hot reload, byte sinks |
| [SigilData](src/common/data/README.md) | Tabular data and scales: typed columns, and the one value that maps a domain onto a range |
| [SigilScry](src/common/scry/README.md) | HTML and CSS rendered to Skia images |
| [SigilMeasure](src/common/measure/README.md) | Timing, statistics and check reporting |
| [SigilTree](src/common/tree/README.md) | The kernels a retained runtime hosts: the reconciler and the caching proof |
| [SigilMotion](src/common/motion/README.md) | Animation: the clock, animatable values, bindings, and the physics steppers |
| [SigilGeometry](src/common/geometry/README.md) | Higher-level drawing over Skia: paths, contours, meshes, splines, point operators |
| [SigilMaterial](src/common/material/README.md) | Recipes, textures, environment maps, and colour: ramps, palettes, harmonies, dithering |
| [SigilDraw](src/common/draw/README.md) | An immediate-mode pen with p5's verbs |
| [SigilWorld](src/common/world/README.md) | 3D surfaces on Diligent Engine |
| [SigilSubstance](src/common/substance/README.md) | Adobe Substance 3D materials rendered to images, where the SDK is installed |
| [SigilCompose](src/common/compose/README.md) | Data-driven drawable components — layout, caching, animation |
| [SigilPython](src/common/python/README.md) | Reusable Python bindings over the native libraries; callback and value ownership shared with host integrations |
| [SigilSketch](src/sketch/README.md) | The framework a scene is hosted through: registration, sessions, the live host and plates |
| [SigilWeave](src/sigilweave/README.md) | Text shaping and layout on HarfBuzz, ICU and Skia |

## Build and test

The build requires Python 3.12 or newer with development headers and an
embedding library. The dependency manifest supplies pybind11, and every
Grimoire build supports Python sketches alongside C++ sketches.

Reusable native bindings live in `src/common/python/`. The sketch-specific
adapter lives in `src/sketch/python/`; the import package, type declarations,
Python tests and wheel tooling live in the `apps/python/sigil/` workspace member.
From `apps/python`, `uv build --package sigil-sketch` selects that distribution;
wheel builds need the explicit toolchain and dependency settings in
[`apps/python/sigil/README.md`](../python/sigil/README.md) and a separate package
build directory. Native application builds remain rooted here.

```sh
cd apps/spell-circle-canvas
python3 scripts/sigil.py setup --config Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`sigil.py setup` finds Qt and vcpkg and writes the uncommitted
`CMakeUserPresets.json`. It is one of nine verbs over the build's
administration; `scripts/README.md` is the canon for all of them.

How a library under this one is built, tested and measured — its one
test binary and its one benchmark binary, ctest's per-case discovery,
what a case may pin, what a label promises and where the shared
fixtures are — is written once, in
[docs/overview/testing.md](docs/overview/testing.md).

Use a Release build for any performance work. Several library
benchmarks and sketches are deliberately stressful and Debug timings
say nothing useful. The benchmarks are not tests: the `benches` target
builds every `*_bench` binary and `scripts/sigil.py bench` (`mise run
bench`) runs them one at a time on a quiet machine against the
committed baseline.

### The SDKs that are not in vcpkg

Two dependencies are downloads behind an account, so no port can fetch
them: the Ultralight SDK (SigilScry) and the Adobe Substance 3D SDK
(SigilMaterial's substance feature). SigilScry, and everything that links
it, leaves the build when its SDK is absent; the substance feature still
builds and answers that it is unavailable.

An archive that cannot be fetched is put into the vcpkg asset cache
once per machine, and every configure after that resolves it locally:

```sh
scripts/sigil.py assets --stage <downloaded-archive>
```

The verb copies the archive into `~/.local/opt/vcpkg-assets/` under
the SHA-512 of its contents — the name vcpkg looks it up by — and prints
that hash. `sigil.py setup` writes the cache into the `vcpkg` preset's
environment as the one asset source, consulted before the network and
written back to, so every other dependency still downloads normally.

Where each SDK has to go until its port exists is in
[SigilScry's README](src/common/scry/README.md) and
[SigilSubstance's](src/common/substance/README.md).

### Demo assets

Several library examples reproduce real reference designs, and a
reference typeset in whatever face the host happens to ship is only half
a reference. One verb fetches the open-licensed ones, and the build reaches
the same verb through an opt-in target:

```sh
scripts/sigil.py assets
cmake --build build --config Release --target fetch_assets
```

They land in `build/assets/` (gitignored), reach code as the
`SIGIL_ASSET_DIR` compile definition, and are also accepted directly by
tools that take `--assets <dir>`. Nothing here runs during a normal
build, and configuring the project never touches the network. The verb
writes files and nothing else, so it runs on a fresh checkout with no
build tree.

The manifest is `scripts/sigil/assets.py`.
Anything added to it carries an open licence with its licence file
fetched alongside, is pinned to an immutable commit rather than a branch,
and declares a sha256 so a changed byte is a hard failure.
