# SigilSubstance

SigilSubstance renders Adobe Substance 3D archives (`.sbsar`) to images.
A `.sbsar` is a procedural material: a graph with named, typed parameters
(sliders, colours, toggles, images) and named outputs, each output tagged
with the material channel it feeds — `baseColor`, `normal`, `roughness`,
`metallic`, `ambientOcclusion`, `height`, `emissive`. This library loads a
package, exposes its graphs' parameters as plain values, renders on the
CPU Substance Engine, and hands every output back as an `SkImage` keyed
by identifier and by usage. Nothing here knows about surfaces or the GPU:
SigilWorld's texture-set door takes the by-usage map and makes a
`Material` of it.

Namespace `sigil::substance`, target `SigilSubstance`, header
`sigilsubstance/Substance.h`.

## Using it

```cpp
#include <sigilsubstance/Substance.h>
#include <sigilworld/TextureSet.h>

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
world::Material leaves = world::textures::material(
    graph.outputsByUsage(), {}, graph.normalsAreDirectX());
```

`substance_demo <file.sbsar> [outdir] [log2size] [name=value ...]` prints
a package's parameters and outputs, applies any `name=value` pairs, and
writes one PNG per output.

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
`world::textures::roleForUsage()` reads. Untagged outputs are keyed by
identifier. Both spellings a graph may use for the same slot
(`diffuse` and `baseColor`) land on the same `Material` slot downstream.

**Two inputs every graph has.** `$outputsize` (an Int2, log2 per axis)
is what `setResolution()` sets. `$normalformat` (0 DirectX, 1 OpenGL)
selects the normal map's green convention; the engine's default is
DirectX, which is why `world::textures::material()`'s by-usage overload
defaults `normalDirectX` to true. `Graph::normalsAreDirectX()` reads the
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
framework library and one engine dylib. Deliberately absent: any GPU
device, any material or surface type (SigilWorld's), any file-set
discovery (that is `sigilworld/TextureSet.h`), and the SDK's own
sources — nothing from the SDK is vendored into this repository.

## The SDK

The Substance 3D SDK is a licensed Adobe download. Unpack it under a
versioned directory in one of the roots `scripts/setup.py` searches —
`~/.local/opt/substance/<version>/` is the one the development machines
use — or point `SUBSTANCE_SDK_DIR` at the directory holding
`substance-config.cmake`. `setup.py` writes the location into
`CMakeUserPresets.json`; without an SDK the top-level configure warns and
leaves this library, `substance_test` and `substance_demo` out of the
build, and `world_demo`'s material lab renders without its Substance
props. Executables that link SigilSubstance carry the SDK's
`bin/release` in their runtime search path, which is where the engine
dylib lives.

## Build and test

Targets: `SigilSubstance`, `substance_test` (ctest), `substance_demo`.

```sh
ctest --test-dir build -C Debug -R substance_test --output-on-failure
./build/bin/Debug/substance_demo ~/.local/opt/substance/9.4.6/assets/Autumn_Leaves.sbsar out 9
```

The test renders one of the SDK's sample archives, so it needs the SDK
and nothing else.
