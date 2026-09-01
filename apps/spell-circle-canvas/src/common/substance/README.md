# SigilSubstance

SigilSubstance renders Adobe Substance 3D archives (`.sbsar`) to images.
A `.sbsar` is a procedural material: a graph with named, typed parameters
(sliders, colours, toggles, images) and named outputs, each output tagged
with the material channel it feeds — `baseColor`, `normal`, `roughness`,
`metallic`, `ambientOcclusion`, `height`, `emissive`. This library loads a
package, exposes its graphs' parameters as plain values, renders on the
CPU Substance Engine, and hands every output back as an `SkImage` keyed
by identifier and by usage. Nothing here knows about surfaces or the GPU:
SigilMaterial's texture-set door takes the by-usage map and makes a
`Material` of it.

Namespace `sigil::substance`, target `SigilSubstance`. One static target
over two subjects; every public header lives under
`include/sigilsubstance/<subject>/` and is spelled
`<sigilsubstance/<subject>/X.h>`:

| subject | headers | holds |
|---------|---------|-------|
| `graph`   | `graph/Parameter.h`, `graph/Output.h`, `graph/Graph.h` | `Parameter` and `Output`, the described inputs and outputs; `Graph`, one graph described, changed, cooked and read |
| `package` | `package/Package.h` | `Package`, the loaded archive that owns its graphs and the engine renderer they share |

`<sigilsubstance/Substance.h>` is the umbrella header over both. The two
subjects are one target because neither exists without the other: a
`Graph` is only ever constructed by its `Package`, and a `Package` is
nothing but its graphs, so a test of one is a test of both.

## Using it

```cpp
#include <sigilmaterial/kit/Surface.h>
#include <sigilmaterial/texture/TextureSet.h>
#include <sigilsubstance/Substance.h>

using namespace sigil;

std::string error;
std::unique_ptr<substance::Package> package =
    substance::Package::load("Autumn_Leaves.sbsar", &error);
if (!package) return;  // `error` says why

substance::Graph& graph = package->graph(0);
for (const substance::Parameter& p : graph.parameters())
  std::printf("%s (%s)\n", p.identifier.c_str(), p.label.c_str());

graph.setResolution(10, 10);       // 1024 x 1024, as log2
graph.set("$normalformat", 1.0f);  // OpenGL normals
graph.set("Season", 0.8f);
graph.render();

sk_sp<SkImage> normal = graph.output("normal");  // by usage or identifier
material::Material leaves = material::kit::surface(material::textures::
    fromUsageMap(graph.outputsByUsage(), graph.normalsAreDirectX()));
```

The `substance_swatches` sketch renders a sample archive's channels and
lays them out as cards, which is this library's output looked at rather
than described; it reports itself unavailable, with the path it looked
in, on a machine whose SDK carries no sample archives.

## The mental model

**A package is graphs; a graph is parameters in, images out.** `Package`
owns the archive description, one `Graph` per graph in it, and the
engine renderer. `Graph::parameters()` and `Graph::outputs()` describe;
`set()`/`setImage()`/`setText()`/`setResolution()` change; `render()`
cooks synchronously; `output()` and `outputsByUsage()` read. Parameters
carry their authored default, range, widget and combobox choices, all as
floats regardless of kind, so a UI can be built over them without
touching the SDK's types.

**Usage is the key.** Every output the graph tagged with a channel is
returned under that channel's canonical name — the vocabulary
`material::textures::roleForUsage()` reads. Untagged outputs are keyed by
identifier. Both spellings a graph may use for the same slot
(`diffuse` and `baseColor`) land on the same `Material` slot downstream.

**Two inputs every graph has.** `$outputsize` (an Int2, log2 per axis)
is what `setResolution()` sets. `$normalformat` (0 DirectX, 1 OpenGL)
selects the normal map's green convention; the engine's default is
DirectX, which is why `material::textures::fromUsageMap()` defaults
`normalDirectX` to true. `Graph::normalsAreDirectX()` reads the
input back, so the material builder can be handed the graph's own
answer rather than a remembered one.

**Images are 8-bit.** The package is opened with output options that
allow only RGBA8 and L8 without mip pyramids, and the engine substitutes
those for anything the graph authored otherwise (16-bit, float,
compressed). `output()` therefore always yields an N32 or grey `SkImage`
that uploads anywhere. Image *inputs* are flattened to 8-bit RGBA on
the way in.

## Conventions that will bite you

**A `Graph&` lives as long as its `Package`.** Graphs hold pointers into
the package's instance list and renderer; move the package, not the
graph.

**`render()` is synchronous and per graph.** It pushes that one graph
and runs the engine to completion. Rendering the same graph twice
recomputes only what its changed parameters dirtied.

**The engine is the CPU one.** The build links the `neon_blend` (Apple
Silicon) or `sse2_blend` engine — results in system memory, headless, no
GPU context. The SDK's Metal and Vulkan engines exist but are not wired;
choosing one is a build-time change in the top-level CMake, not a runtime
switch.

**`Parameter::values` is a snapshot.** It reflects the value at the
`parameters()` call; a later `set()` does not update a vector you kept.

## Boundary

Public dependency: Skia (`SkImage` out). Private: the Substance 3D SDK's
framework library and one engine dylib — no public header names an SDK
type; the SDK is included only by the sources and the internal headers
beside them. Deliberately absent: any GPU device, any material or
surface type (SigilMaterial's), any file-set discovery (that is
`sigilmaterial/texture/TextureSet.h`), and the SDK's own sources — nothing from the
SDK is vendored into this repository.

## The SDK

The Substance 3D SDK is a licensed Adobe download. Unpack it under a
versioned directory in one of the roots `scripts/setup.py` searches —
`~/.local/opt/substance/<version>/` is the one the development machines
use — or point `SUBSTANCE_SDK_DIR` at the directory holding
`substance-config.cmake`. `setup.py` writes the location into
`CMakeUserPresets.json`; without an SDK the top-level configure warns and
leaves this library, `substance_test` and `substance_bench` out of the
build, and the sketch that draws a package is left out of the sketch
registry. Executables that link SigilSubstance
carry the SDK's `bin/release` in their runtime search path, which is
where the engine dylib lives.

## Build and test

Targets: `SigilSubstance`, `substance_test` (ctest) and `substance_bench`
(Google Benchmark, through the `benches` target and
`scripts/bench_ledger.py`).

```sh
ctest --test-dir build -C Release -R substance_test --output-on-failure
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
    --sketch substance_swatches
```

The test and the benchmark render the SDK's own sample archives
(`assets/Autumn_Leaves.sbsar`, and `assets/Post_Illumination.sbsar` for
the composition test), found through the SDK directory the build was
configured from. When a sample is not there, the test skips with a
message naming the file and the benchmark registers nothing, so an SDK
installed without its samples reports the fact rather than failing. The
engine dylib itself is a link-time dependency: a binary built against
the SDK does not start without it.
