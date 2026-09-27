# SigilMaterial — Substance archives

The chapter on the substance feature: an Adobe Substance 3D archive (a
`.sbsar`) cooked into a Material. `README.md` beside this file is the
library; this is the one base whose pictures come from a vendor's engine.

In tiers: tier 1 is `material::substance(hub, uri)` and its generated-struct
form; tier 2 is `SubstanceOptions` and the description
`material::sbsar::describe` answers; tier 3 is the archive and the cook
under `sigilmaterial/substance/advanced/`.

## One more base

```cpp
#include "Autumn_Leaves.substance.h"   // struct AutumnLeaves, written by the build

AutumnLeaves autumn;
autumn.hueShift = 0.1f;
material::Material leaves =
    material::substance(hub, "res://Autumn_Leaves.sbsar", autumn);
leaves.set("Season", 0.8f);             // one cook, apart from the caller
leaves.bind("Season", seasonDial);      // a cook whenever the dial moves
box().fill(material::from(leaves).layer(material::noise(0.8f),
                                        {.blend = BlendMode::SoftLight, .opacity = 0.1f}));
```

`material::substance` answers a Material like any other. Its base is a
part that holds the cook and draws nothing itself; over it stands the
graph's base-colour output (`baseColor`, or `diffuse` in an older
archive) as an image layer, masked by the `opacity` output where the
graph has one. The surface is filled from the outputs by the channel each
declares: `normal` (with the graph's green convention as
`normalDirectX`), `roughness`, `metallic`, `ambientOcclusion` and
`emissive`. A `height` has no surface channel; it is cooked only when
asked for and read with `material::sbsar::output`.

**The graph's inputs are written through the Material's own entrances.**
`Material::set` with a name, a number or a generated struct, and
`Material::bind` with an `Animatable<float>`, reach the base part, which
writes the input and schedules a cook. Cooks run on the engine's thread,
the newest request replacing one still waiting, and `isRunning()` stays
true until every cooked picture has been handed out. A host capturing a
still calls `material::sbsar::settle` first, so the picture is the inputs'
values at that moment.

**Copies share one cook.** A Material is a value, but the pictures a cook
produces are shared by every copy of the answer, so an input written
through one copy re-cooks what every copy shows. Call
`material::substance` again for an independent graph.

**Without the SDK** the feature still links: `material::sbsar::available`
is false, `material::substance` answers an empty material and says so
once, and `material::sbsar::describe` answers an empty description. A
consumer needs no build-time branch.

## The keyed form and the description

```cpp
material::Material green = material::substance(hub, uri, {
    .inputs = {{"Hue_Shift", 0.1f}},
    .preset = "Green",
    .seed = 7,
    .resolution = 512,
    .outputs = {{"diffuse"}, {"normal"},
                {"height", material::sbsar::Format::Float16}}});
for (const material::sbsar::Input& input : material::sbsar::describe(hub, uri).inputs)
  ;  // name, label, group, description, type, widget, defaultValue, minimum,
     // maximum, step, clamp, choices, visibleIf
```

`SubstanceOptions` holds initial values by identifier (written after the
preset), a preset by label — embedded in the archive or in a `.sbsprs` of
the same name beside it — the seed, the size in pixels (rounded up to a
power of two; zero keeps the author's), the outputs to cook with their
precision, and the graph of a many-graph archive. Left empty, `outputs`
cooks what the material reads.

A `material::sbsar::Description` lists the graph's inputs
(`material::sbsar::Input`), its presets and its outputs
(`material::sbsar::Output`), each output with the channel it feeds and its
`material::sbsar::Encoding`: an sRGB base colour is tagged sRGB, a linear
colour linear, and data — a normal, a roughness, a height — carries no
colour space, so nothing converts it on its way to a surface. A normal and
a height cook at 16 bits unless an `material::sbsar::OutputRequest` says
otherwise (`material::sbsar::Format::Automatic`).

## The generated struct

Every `.sbsar` under a directory sketch's `data/` has its input struct
written into `<stem>.substance.h` in the build tree by
`sigil_substance_header`: one `std::optional` field per numeric input,
named by the identifier camelCased as the author wrote it
(`Hue_Shift` → `hueShift`), a combobox as an enumeration of its labels,
a toggle as a `bool`, and `inputs()` listing the set fields as the
`material::sbsar::InputValue` list the keyed form takes. Fields stand in
the author's order, which is the order designated initialisers follow.
Without the SDK there is no tool and no header; a sketch that must compile
everywhere writes the keyed form.

## Advanced

- `substance/advanced/Archive.h` — `Archive`, decoded once
  through the hub (`registerDecoder`, `load`) with every graph described
  once and shared by every cook of it; `engineVersion`.
- `substance/advanced/Cook.h` — `CookScheduler`, one graph
  instance and its renderer: `set`, `setText` and `setImage` by raw
  identifier (`$outputsize` in log2, `$randomseed`, `$normalformat`),
  `isHeavyDuty`, `applyPreset` with a `PresetMode`, `bind` and `follow`,
  `cook`, `cookNow`, `wait`, and `output` as a `media::PixelSource` whose
  revision bumps when a cook lands; `engine`.

The engine is the CPU one: a cook lands in host memory and reaches a
renderer as an image. The SDK's Metal and Vulkan engines, whose results
stand on the device, are not wired.
