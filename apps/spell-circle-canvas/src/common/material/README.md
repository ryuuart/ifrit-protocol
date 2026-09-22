# SigilMaterial

Materials as recipe instances. A **recipe** is a material's definition: a
plain C++ struct of uniform-typed fields that is its ABI, one shader body
per shading language, the slots it samples and the per-frame values
it reads. A **material** is one instance of a recipe: the field values,
mirrored as the bytes the shader will receive; live bindings that overwrite
fields every frame; other materials filling its slots; and the
settings a renderer reads off the instance. A renderer asks a material to
**resolve** against a frame and receives the compiled **program** for its
shading language plus the bytes to upload — the same answer, memoised,
until an input changes.

Beside the recipe model sits the image side: a **texture** is an image
and how it is sampled, a comparable value that fills a recipe's child
slot as a **leaf** — bound by the backend rather than compiled. The
texture feature also knows the folders material tools export (a texture
set by role), bakes the two textures a reflective surface is shaded from
(an environment and a bevel normal map), and cuts an atlas into regions
and frame sequences.

The shading model the authoring tools export for — metallic-roughness,
with a map per role — is a preset like any other: one parameter struct is
its ABI, one slot per map, and the choice between the lit and the
unlit recipe is what the surface IS. Local variation on top of it is not
a bespoke recipe per pair but a composition: `over(base, top, mask)`
stacks two materials where a mask says.

Above those sit the PRIMITIVES — fully parameterised generators, one
feature each: **sdf** (shape,
border, glow and shadow in one pass over a signed distance), **pattern**
(a tile baked once with a mapping and an explicit reseed, the stock
tiles over it, and the woven cloth a sett and a weave make), and **field** (the halftone ramp, Perlin noise, luminance
grain, the ripple, the CRT overlay). Under the core sits **colour**, the
leaf: the colour value a parameter struct holds, the OKLab, OKLCH and
CIELAB round trips, the ramp as one value, the harmonies read around a
hue, the dither threshold a pixel is rounded against and the table a run
of pixels is made of — all of it linking nothing; above the texture
feature sits **ocio**, OpenColorIO's view transforms baked to materials.
The **kit** holds PRESETS — functions that fix
colours, proportions or a named style over the primitives: the
metallic-roughness surface and the masks that stack it; gold, chrome
and glass over a normal map and an environment; the girih panel and its
palettes; the named colormaps; the gel and chrome colour tables; the six
text paints and the chrome-type ramps.

The core links the colour leaf, glm (for the vector types a struct may
hold), SigilMotionValues (for the animatable a field may bind to, and
choreograph with it), Boost.PFR (for the reflection that reads a struct's field
names off the type), and Boost.Container for its ordered stores. The core has
no renderer in it: compilers arrive from backend features, and two of
them ship here. The Skia one turns a recipe's SkSL body into an
`SkRuntimeEffect`. The Slang one compiles Slang source to SPIR-V and
reports the layout every uniform's bytes go at, which is what a device
renderer writes a draw's uniforms into — the renderer supplies the
scaffold its body is appended to and registers the result, so what lives
here is the compile and the layout and nothing that knows a pass or a
device.

Namespace `sigil::material`. Twelve feature libraries, one per
directory, each a static archive that links only what sits beneath it:

| target | holds | links |
|--------|-------|-------|
| `SigilMaterialColor` | `Color`, `rgb()`, `hsv()`, the three mixes and `luminance()`, `RampStop` with `sampleRamp()`, the OKLab, OKLCH and CIELAB round trips with `fitToSrgb`, `Ramp` (the ramp as one value) with `palette()` both ways, `harmony()` and `rotateHue()`, `Dither`, and `palette(pixels)` with `closestEntry()` — the leaf, which the core's `Parameters.h` includes | SigilCoreCompute |
| `SigilMaterialCore` | the value model: `Target`, `Parameters`, `Recipe`, `Program` and the cache, `Material`, `Leaf`, `UniformBlock`, `FrameData`; `Bank`, the bounded seeded bank of a field's instances; `termsSource`, the shading terms a surface is composed of; and `over()`, the combinator that stacks one material on another through a mask | SigilMaterialColor, SigilMotionValues, glm, Boost.PFR, Boost.Container; Boost.Unordered privately |
| `SigilMaterialTexture` | `Texture` and its sources, `ShaderLeaf`, `texture::` (the tools' sets by role), `EnvironmentMap` and `bevelNormals`, `Atlas` | SigilMaterialCore, SigilImageAsset, Skia, Boost.Container; simdjson privately |
| `SigilMaterialMask` | the third operand of `over()`: `maskConstant`, `maskMap`, `maskSlope`, `maskHeight`, and `fitMask` / `invertMask`, which reshape a mask and nothing else | SigilMaterialTexture, glm |
| `SigilMaterialOcio` | `ocio::` — `available()`, and the OCIO `viewTransform`, `convert`, `exponent` as baked materials, over the 3D-LUT `lutRecipe()` and the per-channel `responseRecipe()` | SigilMaterialTexture; OpenColorIO privately, when found |
| `SigilMaterialSdf` | `sdf::` — `Shape`, `Style`, `pad`, `material`, `everyRecipe` | SigilMaterialCore, SigilMaterialColor |
| `SigilMaterialPattern` | `pattern::Tile` and the stock tiles; `pattern::Cloth`, the woven cloth, with `threadcount`, `pivots`, `Weave` and `warpUp` under it | SigilMaterialTexture, SigilMaterialColor; SigilCoreCompute privately |
| `SigilMaterialField` | `field::` — `halftoneRamp`, `noise`, `grain`, `ripple`, `crtOverlay`, the screen `crt` and the three subjects it composes, `crtBeam`, `crtBloom` and `crtGlass`, `everyRecipe` | SigilMaterialTexture, SigilMaterialColor |
| `SigilMaterialSkia` | the SkSL compiler and `SkiaProgram`, whose builder uploads resolved bytes; `skia::builder` and `skia::shader` binding leaves into slots; `skia::fill`; the colour bridge `skia::toColor` / `skia::toSkColor`; `skia::verticalRamp` and `skia::unitRamp`, the two crossings a list of `RampStop`s reaches Skia through (a span, with a brace list written where it is used), with `skia::paletteImage` and `skia::paletteLookup` the palette's two beside them; `skia::palette`, the picture read down to the table it is made of; `skia::Paint`, the model as ONE shader, with `skia::PassInputs` for a pass over a layer; and `skia::Effect`, the post-processing recipe over a rendered layer | SigilMaterialTexture, SigilMaterialColor, SigilMotionValues |
| `SigilMaterialSlang` | the Slang compiler: `slang::compileModule` to SPIR-V, `slang::Compiled` with the reflected `slang::UniformSlot` per uniform, `slang::SlangProgram`, and `slang::Uniforms`, the buffer one draw is written into; `Portable.slang`, the subset a host and a device answer alike, loaded into every session by name | SigilMaterialCore, Boost.Container; Slang privately |
| `SigilMaterialKit` | the presets: the colour CRT `kit::crt`; the named ramps `kit::viridis`, `kit::magma`, `kit::inferno`, `kit::plasma`, `kit::turbo`, `kit::redBlue`, `kit::brownTeal` and the generated `kit::cubehelix`; the metallic-roughness `kit::surface` and `kit::unlit`; `kit::gold`, `kit::chrome`, `kit::glass`; the grained `kit::stone`, `kit::timber`, `kit::latten` and `kit::board` with `kit::lattenTone` reading the last one's ladder on the CPU; the orthographic `kit::globe`; `kit::girih8` and its palettes; the gel and chrome tables with `kit::contourRing`; the text paints and chrome-type ramps; `kit::studioEnvironment` and `kit::sunsetEnvironment`, the two named skies; and `kit::everyRecipe`, one instance of each of the above | SigilMaterialField, SigilMaterialPattern, SigilMaterialColor, SigilMaterialMask, Boost.Container |
| `SigilMaterialStock` | `stock::everyRecipe()`, one instance of every recipe this library ships gathered from the catalogues that own them, and `stock::warmup(target)`, which compiles the list into the shared program cache before a host's first frame | SigilMaterialCore; SigilMaterialField, SigilMaterialSdf, SigilMaterialKit and SigilCoreSchedule privately |

`SigilMaterial` is the umbrella, an interface over all twelve. Headers live
under `include/sigilmaterial/<feature>/` and are spelled that way —
`<sigilmaterial/core/Recipe.h>`, `<sigilmaterial/texture/Texture.h>`,
`<sigilmaterial/kit/Reflections.h>`.

## Using it

```cpp
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/FrameData.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/core/Target.h>
#include <sigilmaterial/core/UniformBlock.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

using namespace sigil::material;

// The ABI: a plain aggregate of uniform fields. Names are read off the
// type; there is nothing to register.
struct Glow {
  float uScale;
  Color uTint;
  std::array<float, 8> uBars;
};

// A shader YOU authored stays a shader file, so editors and shader tools see
// the language, and reaches the program through SigilIO from wherever you
// keep it. The Hub caches it; the lease makes its residency promise explicit
// for as long as this material catalogue lives. The shaders this library
// SHIPS are not read at run time at all — see "Where the stock shaders live".
sigil::io::Hub shaders;
shaders.mount("shader://", shaderDirectory);
auto retainedShaders = shaders.retain("shader://");
retainedShaders.preload();
auto glowSource = shaders.text("shader://Glow.sksl");
if (!glowSource) throw std::runtime_error("Glow.sksl is missing");

// The definition, made once and shared. The loaded body follows the generated
// declarations — the uniforms above, then uTime, then the slot.
auto glow = std::make_shared<const Recipe>(
    Recipe::of<Glow>("glow")
        .frame(FrameInput::Time)
        .slot("uSrc")
        .body(Target::SkSL, std::move(*glowSource)));

// An instance: values now, a bound clock and a live table later.
Material m(glow, Glow{1.0f, {1, 0.8f, 0.2f, 1}, {}});
m.bind("uScale", &scaleOutput);        // a choreograph::Output<float>
m.bind("uBars", spectrumBlock);        // a shared_ptr<UniformBlock>, 8 floats
m.slot("uSrc", Material(gradientRecipe, GradientParameters{...}));

// A renderer, per frame:
FrameData frame{.seconds = clock.now(), .resolution = {w, h}};
sk_sp<SkShader> shader = skia::shader(m, frame);
```

`skia::shader` is the whole Skia path: it resolves the material, builds
over the program's effect with every uniform set from the resolved bytes,
binds each slot — a material resolved recursively, a texture
leaf as its image shader — and makes the shader. The Skia backend prepares
its compiler on first use; drawing needs no registration step. A renderer
that fills some slots itself uses `skia::builder(m, frame, variant,
leave)`, which prepares the same program and leaves the named slots for
the caller. `skia::fill(canvas, path, m)`
is the one-call draw: clip to the path, paint the shader across it.

A renderer applying a recipe-backed paint to a layer calls
`skia::Paint::resolvePass` with `skia::PassInputs` from
`<sigilmaterial/skia/Pass.h>`. The inputs supply the content shader, unit
rectangles, progress and seeds. The paint owns shader specialization and
program reuse for each unit count.

A surface from the kit reads the same way, its slots filled with textures:

```cpp
#include <sigilmaterial/kit/Environments.h>
#include <sigilmaterial/kit/Reflections.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/texture/Surface.h>

const EnvironmentMap studio = kit::studioEnvironment();
kit::ChromeParameters steel;
steel.brushed = 0.6f;
steel.roughness = 0.2f;
// bevelNormals() places its map at the outline's bounds, so the recipe
// reads the normal under the pixel it shades.
const Material badge = kit::chrome(bevelNormals(outline, 12), studio, steel);
skia::fill(canvas, outline, badge);   // per frame; the program is cached
```

## Mental model

**A parameter struct is the ABI, and the bytes are the upload.** Every field
type is some count of floats with float alignment — `float`, `glm::vec2`,
`glm::vec4`, `std::array<float, N>`, `Color` — so a struct of them has no
padding and its memory image is exactly the uniform data in declaration
order. `schema<P>()` proves this at compile time and refuses a struct with
any other field type or with padding. The same walk emits the uniform
declarations (`declare<P>(target)`), so the names in the shader are the
names in the struct and cannot drift. A struct with NO fields is legal and
is a recipe with no ABI of its own — a body over slots and frame
inputs alone.

**A layout can also be assembled while the library runs.**
`packedSchema(fields)` lays a field list out by the rule `schema<P>()`
applies to a struct — each float count read off the kind, each offset the
running sum — and `Recipe::of(name, schema)` defines over the result. That
is the door for an ABI no C++ type stands behind: a definition composed out
of other definitions' fields, or one authored from outside C++ altogether.
A repeated name and an array of no floats are each reported once and left
out, so what comes back is a layout `find()` answers unambiguously and
`declare()` can emit.

**Writing to a field no body reads is reported at the write.** A dial
that does nothing looks, from the call site, exactly like a dial set to
the wrong value: the bytes go up and the picture does not change. So
`Material::set(name, …)` asks the recipe — `Recipe::readsField(name)`,
which is whether any body of it SPELLS the name as a whole identifier —
and names the recipe and the field on stderr once per pair, beside the
reports for an unknown field and a wrong float count. The value is still
written; the report is about the picture, not the bytes.

It is asked at the WRITE and not at the compile because a parameter struct
carrying a field this recipe's kind has no use for is a shared ABI and
not a mistake — the three `sdf` silhouettes are one struct whose `uP0..2`
mean something different in each — and a struct poured in whole says
nothing. What the compile side can still say is that a BACKEND discarded
a uniform: `Program::keeps(name)` is that question, and the program cache
names each dropped field once per (recipe, target). Skia's reflection
keeps every declared uniform, so on SkSL that answer is always yes and
the write-side check is the one that speaks.

**A recipe's identity is the object.** Two recipes built from the same
text are two definitions with two sets of programs; `operator==` compares
definitions and is for tests, while the program cache and a material's
equality use the pointer. Define a recipe once and hold it in a
`shared_ptr<const Recipe>` beside the code that owns it.

A definition a renderer can only finish at draw — a body rewritten around
an array size or a constant nothing knew earlier — is a SPECIALIZATION:
`m.withRecipe(r)` is the same instance over a second recipe of the same
parameters layout, so the values, bindings and slots carry over and the two
definitions compile and cache apart. Hold the specializations, one per
distinct constant, or the cache fills with a definition per draw.

**One body per target, and asking for a missing one is an error once.**
`Recipe::body(Target, source)` stores the body for a language;
`Recipe::source(target)` is the generated declarations followed by it.
The two targets ask a body for the same thing in their own words:

| target | what a body is | how it reads a slot |
|---|---|---|
| `Target::SkSL` | `half4 main(float2 p)`, returning premultiplied colour | `uniform shader NAME`, evaluated as `NAME.eval(p)` |
| `Target::Slang` | `float4 surface(float2 uv)`, returning STRAIGHT colour — the renderer that compiles it puts the lighting and the premultiply around it | `uniform Sampler2D NAME`, read as `NAME.Sample(uv)` |

A Slang body may also say what a colour cannot carry. A renderer that
shades declares four variables the body MAY write —
`gSurfaceNormal` (tangent space), `gSurfaceGloss` (a Blinn exponent),
`gSurfaceMetal`, and `gSurfacePerPixel` to say it wrote any of them —
and evaluates its shading again where those can be seen. A body that
writes none of them costs nothing and changes nothing. It is an
OPTIONAL half of the contract: a body that says only a colour is a
complete body, and the four exist because a MAP that varies a surface
across a face is a per-pixel answer no per-vertex shading can carry. A
material resolved for a target its recipe has no body for — or one no
compiler is registered for, or one whose body fails to compile — yields a
null program, and the cache reports it to stderr exactly once per (recipe,
target), naming both, so the mistake surfaces at the first describe rather
than scrolling past every frame. A body that compiles but leaves a parameters
field unread is reported the same way.

**One program cache.** `ProgramCache::shared()` holds every compiled
program in the process, keyed by (recipe identity, target, variant). A
backend registers its compiler with `registerCompiler(Target, Compiler)`
and the cache compiles on first use. Every Skia lowering entry prepares
the built-in SkSL compiler automatically, including `skia::builder`,
`skia::shader`, `skia::fill`, recipe-backed paints and recipe-backed effects.
An explicitly registered SkSL compiler takes precedence for subsequent
compilation, whether registered before or after the first draw. A device renderer registers the
Slang compiler, since only that renderer knows the scaffold a body is
appended to. `Variant` is a small ordered key the
backend owns the meaning of — a premultiplied build, a debug view — and
the default variant is the plain build.

**Bindings are live, and equality is by identity.** `bind(name, Output*)`
makes a float field read the output's current value at every resolve;
`bind(name, shared_ptr<UniformBlock>)` does the same for an array field
and a caller-owned table. A bound material `isAnimated()`;
`isBound(name)` is the other question — whether a field carries a binding
at all, an output or a number or a block, rather than only the bytes
`set()` last wrote. Two materials
bound to the same output or block compare equal; bound to different ones,
unequal; the values behind them never enter the comparison. A `UniformBlock`
carries a revision (`commit()` advances it) so a caller can tell an edited
frame from an untouched one, and its values are read live whether or not
they were committed.

**Frame inputs are declared, then injected.** `Recipe::frame(FrameInput)`
declares that the body reads `uTime`, `uResolution`, `uContentScale` or
`uWorld`; the declaration adds the uniform after the parameters and
`resolve()` fills it from the `FrameData`. Time and content scale make a
material `isAnimated()`; resolution and the world transform make it
`geometryDependent()`. `quantizeTime(hz)` snaps the time a material sees
to a step, so a material that need not move every frame resolves only
when the snapped clock advances.

**Slots ride everything.** A recipe declares slots (`slot("uSrc")`,
exposed to SkSL as `uniform shader uSrc`); a material fills them with other
materials or with leaves. A live source makes the parent live, a
geometry-dependent source makes it geometry-dependent, and a different
child makes it unequal — which is required, not incidental: a child left
out of equality would let a node prune while its second source had
changed.

**A slot an EXECUTOR fills comes from the layer.** A recipe run over a
rendered layer reads that layer in a slot the executor fills; declaring
one with `Recipe::slot(name, LayerFilter::Blurred, amountField)` says it
is filled from the SAME layer put through a filter first, at the amount
the named parameter carries. `Recipe::layerSlots()` is the list, and the
declaration is additive: the slot is generated to a target exactly as
any other is, and an author who fills the name himself keeps his source,
which is how a recipe with one still paints as an ordinary fill. A body
that needs a blurred copy of its input therefore reads one tap of it
instead of gathering the blur itself, per pixel, for as long as the
picture is on screen — and because the filter is Skia's, the cost of a
wide reach is Skia's reduction rather than a fixed tap count.

**A slot is declared to the target that samples it, and to no other.**
A slot belongs to the recipe, but each target's generated declarations
carry only the slots ITS body spells — `Recipe::samples(target, slot)` is
that reading, the one `readsField` takes of a parameter field, and a target
with no body answers yes. The two sets differ where one language reaches
a child material and another cannot: a composed stack declares a slot per
operand's own slot for the language handed one body per material, and the
language whose slot is a shader samples the three operands
themselves and needs none of them. It is not tidiness. A declared slot is
an IMAGE SAMPLER in the compiled program whether anything reads it or
not, a GPU backend inlines the whole tree of effects into one fragment
program, and Metal binds fragment textures at sixteen indices — so a
program carrying another language's slots spends a device's whole budget
on samplers it never reads. `skia::samplerCount(material)` is what one
lowering asks for, `skia::kSamplerLimit` is what a program may declare,
and a tree over it is refused with both counts named rather than built
into a pipeline the driver silently rejects.

**A leaf is a child no recipe computes.** `Leaf` is the core's seam for
an image with its sampling, a rendered frame, anything a backend binds
into a slot directly: it compares by value (same dynamic type, then the
type's own equality) and says whether it moves between frames.
`ShaderLeaf` is the Skia-facing refinement — a leaf that yields the
`SkShader` to bind — and `Texture` is one such leaf; a renderer's own
native sources (a gradient it built, a Perlin generator) are others. The
Skia backend binds any `ShaderLeaf`. A slot holds a material or a leaf,
never both, and `Material::slot(name)` and `Material::leaf(name)` each
answer null for the other kind.

## Textures

**A texture is a source plus sampling, and both enter equality.** The
source is type-erased behind `TextureSource`: `ImageSource` (a decoded
still, equal when it is the same image object), `AssetSource` (a frame of
an `image::ImageAsset` at a playback time; animated when the asset is),
and `ProducerSource` (a function that bakes an image on first use, keyed
by a string — the key IS the identity, so it must name the picture and
every parameter that shaped it). Any type with `image()`, `animated()`
and `==` is a source; two sources are equal only when they are the same
source type and that type agrees. Sampling is the tiling per axis, the
uv matrix placing texture space in the sampled space (`at(origin)` is
the translation), a region of the image to read, and the filter. A
region is cut once per source image and kept, so a texture sampled every
frame does not copy its pixels every frame.

**A source MAY say that its pixels already stand on a GPU.** One
optional member, `deviceImage()`, answers a `DeviceImage`: the device
that owns the texture and the texture itself, as the graphics API's own
object bridged to opaque values. This library reads none of it and
compares none of it — the source's own equality is still what says
whether two textures are the same picture. It is carried, unexamined,
from a source that painted on a device to a renderer standing on the
SAME device, which binds those pixels instead of uploading a copy of
`image()`; a renderer holding another device, or none, finds a device it
does not know and reads `image()` like any other source's. Every source
that has no device omits the member and is written exactly as it was.

**Texture sets are the tools' folders.** `texture::classify` reads a
file name into a `Role` (base colour, normal, roughness, metallic,
occlusion, emissive, packed occlusion-roughness-metallic, height,
opacity, specular), the set it belongs to and whether a normal map is
DirectX-convention; `discover` groups a directory into `TextureSet`s;
`fromFiles(set, decoder)` and `fromUsageMap(images)` decode into
`TextureMaps`, one repeating texture per role. The library opens no file:
a `Decoder` returns an image for a path, and the caller supplies it. What
a set MEANS to a renderer — which channel of a packed image feeds which
slot — is the renderer's rule, not this library's.

**A surface is shaded from two textures.** `EnvironmentMap` is the
panorama a surface sees when it looks past the lights — equirectangular,
u = azimuth, v = 0 at the zenith, with `equirectangularUv` and
`equirectangularDirection` as the one convention every consumer shares. Sources
resolve into that single form while the value is built: `baked()` runs a
radiance function over the panorama (the kit's `studioEnvironment()` and
`sunsetEnvironment()` are two written against it, and need no assets),
`fromEquirectangular()` wraps a loaded
lat-long panorama, `fromFaces()` resamples six cube faces and
`fromCubeMap()` unpacks one sheet — a 4:3 or 3:4 cross, a 6:1 row or a
1:6 column — into the same. A cube map arrives as an ordinary image
because that is what SigilImage decodes, and the two containers that
hold six faces in one file arrive as the 1:6 column: a KTX 1 or 2
through SigilImage's own reader (uncompressed texels), a DDS through
its OpenImageIO backend.

Two readings hang off the panorama, cached with it and shared by every
copy of the value. The SPECULAR side is `image(roughness)` — nine
wrap-aware blurs a reflection picks by how rough the surface is — with
`texture(roughness)` as the level a recipe's environment slot takes,
repeating in azimuth and clamped at the poles, and `chain()` as the same
nine levels shaped as a mip pyramid for a device that binds one texture
and selects a level. `prefilterSize()` bounds how wide that pyramid's
level 0 is built, since a panorama is often larger than a reflection can
show. The DIFFUSE side is `irradiance()`, the panorama convolved with a
cosine lobe at 32x16 — the value a Lambertian body multiplies its albedo
by, which for a sky of one radiance IS that radiance — with `average()`
as its single-colour fallback. `withGround(colour)` replaces everything
below the horizon, which is what a photographed sky wants when its lower
half is a tripod and a car park. Every reading is computed in F32, so a
value above one survives the blur rather than being clipped to white.

`bevelNormals(path, bevelPx)` blurs the outline's coverage into a height
ramp, differentiates it, and encodes device-space normals (+y down, +z
toward the viewer) as rgb = n * 0.5 + 0.5, flat across the interior and
tilted along the rim — placed at the outline's bounds so device xy reads
the normal beneath it. A normals pass a 3D painter rasterizes uses the
same encoding and feeds the same slot.

**An atlas is a sheet, its regions and its sequences.** `Atlas::grid`
cuts equal cells; `fromTexturePacker` and `fromAseprite` read the JSON
those tools write (hash or array form; trimmed sprites keep their source
size and offset), deriving a sequence per name stem for TexturePacker
(`walk_01`, `walk_02` become "walk") and per frame tag for Aseprite;
`pack(images)` lays loose images into one power-of-two sheet.
`region(name)` is the sheet texture cut to that region; `frame(sequence,
index)` wraps past the end.

## Stacking

**`over(base, top, mask, blend, amount)` is a material.** The three
operands
fill its slots, so the stack compares, animates and resolves as one
value, and applying `over` again builds a taller one. The MASK is any
material whose red channel is read as a scalar; `blend` is `Mix`, `Add`
or `Multiply`, one recipe each so a body carries no branch; `amount` is
how strongly the top shows where the mask is fully on, which is the
stack's own strength rather than a second answer about where it applies.
It is a parameter of the call because a COMPOSED stack has no parameters
struct to write afterwards — its ABI is its operands' fields — so a
caller who did not know to write the field by name would get a stack at
full strength and read it as a wrong mask. `under(m)`
is the material a stack stands on — one step down, so walking it reaches
the bottom — and `stackDepth(m)` counts the steps. A consumer that can
only express one material (`UsdPreviewSurface`, say) writes the bottom
and records the depth.

**Two kinds of target read a stack, and only one of them can reach the
operands.** A target whose slot is a SHADER — SkSL's is — samples
each operand's own program, so one body over the three slots `base`,
`top` and `mask` is the whole story. A target handed exactly ONE body per
material cannot reach a child material at all; for it a stack is
COMPOSED. `over()` builds a recipe out of its operands' own definitions:
the parameters are theirs under a prefix per operand (`base_`, `top_`,
`mask_`), the sampled slots are theirs under the same prefixes, the frame
inputs are the union of theirs, and the body inlines all three of their
bodies and mixes what they return. The renaming is the shading
language's own preprocessor rather than a rewrite of the text — a body
names its parameters and its slots exactly as its recipe declares them,
and a macro maps each — and each operand's helpers stand in a namespace
of its own, so three operands over one recipe are three bodies. **The one
thing a composable body may not do is give a local the name of one of its
own parameters.**

A composed stack is the same material otherwise: the same three operands
in its slots, the same walk down, and the same recipe NAME — which is what
says a material is a stack, since a composed one carries a recipe built
for its own operands rather than the shared one. The operands' values and
their sampled slots are copied in at the moment of the call, so a later
edit to an operand is not seen and a live binding on one does not reach
the composed body; the operand still rides every query as a child, so the
stack still reports itself animated. The composition costs one recipe and
one program per distinct triple of definitions and buys nothing for a
target that samples its operands, so it is built only where a compiler
that needs it is installed. The triple is identified by the definitions
themselves and the cache HOLDS them, so a definition that has been
composed stands for as long as its composition does — which is what
keeps a later recipe built at a freed one's address from inheriting a
body it never wrote. `Target::Slang` is the one such target,
and `stackName(blend)` is the name every stack of a blend carries.

A composed stack therefore carries slots for two languages at once, and
what keeps that from costing the sampling target anything is that a slot
is declared only to the target whose body spells it: the composed
recipe's SkSL program declares `base`, `top` and `mask` and none of the
prefixed ones, so a stack asks a device for exactly its operands'
samplers however many slots its operands DECLARE.

## The Slang backend

`slang::compileModule(source, vertexEntry, fragmentEntry, lit, &out,
&error)` compiles a whole module — imports, both entry points and all —
and hands back a `slang::Compiled`: the two stages' SPIR-V words, the
sampled slots in their declared order, and a `slang::UniformSlot` per
uniform saying where its bytes go. NOTHING GUESSES A LAYOUT: every offset
is the one the compiler reported for the program it just built, so a body
that declares one more parameter moves nothing a renderer has to be told
about. `slang::Uniforms` is the buffer a draw writes into at those
offsets — a matrix row by row and an array element by element where the
layout put them apart, and a name the program does not carry skipped,
since an optimiser that dropped an unused uniform is not a mistake to
report.

Both stages are linked as one program, because the layout is a property
of the linked program: linking them apart would let an unused uniform be
dropped from one and not the other, and the two would then read one
buffer at two sets of offsets.

Every session carries two modules by name, from the text the build embedded
in the library each belongs to, so a shader's `import` resolves against the
session rather than opening a file during compilation. `Portable` is the subset
whose transcendentals a host and a device answer alike — a kernel compiled for
both cannot afford two spellings of a square root. `Shading` is
`termsSource`'s own text, so a renderer's shading and every material body
compiled beside it call one definition of a term rather than a copy apiece.

`lit` is the one axis a session specialises on: it defines `SIGIL_LIT`,
so a renderer's scaffold can carry its lighting uniforms in one build and
not the other. There are therefore two sessions, and a module is loaded
into whichever one the caller asked for under a name no other module
has — a session remembers a module by its name, so two recipes under one
name would be one module and every material after the first would be
drawn with the first one's program.

## The kit

The kit is presets: functions that fix a colour, a proportion or a named
style over the primitives. `kit::girih8` is the 8-fold star-and-cross
panel as a `Tile`, with `fezPalette()` and `nasridPalette()`; its
`contactDeg` is Hankin's contact angle, the one dial of the construction
— two rays leave every edge midpoint at that angle to the edge and meet
on the bisector between neighbours, so the star sharpens as the angle
grows. At the 45° default the rays through an octagon are collinear, the
panel is the classic one, and it is drawn in the closed form it has
always had: two squares through the octagon's edge midpoints, whose union
is the {8/2} khatam and whose outlines are the interlace.

`kit::globe` is the sphere seen ORTHOGRAPHICALLY: the disc inscribed in
the node it fills, inverted back onto its own near hemisphere, carried
into the sphere's frame by undoing `yaw`, `pitch` and `roll`, and read
for two hemispheres, a graticule at three pitches and a horizon. It is a
preset and not a renderer: there is no perspective, no depth and no
mesh, so a globe on a page, a planet on a map and an aircraft's attitude
ball are one recipe at three sets of colours. Every rule in the
graticule is a PLANE DISTANCE — a meridian is the plane through the
poles at its longitude, a parallel the plane at its own sine — so a
rule's width is measured in the sphere's own space and the crowding
toward the limb and toward the poles falls out of the arithmetic instead
of being drawn. `GlobeParameters`'s `ambient` and `diffuse` are what a point
keeps at the limb and what it gains facing the eye, which is the whole of
what makes the disc read as a ball, and the alpha falls to nothing across
`edgeFeather` so nothing outside the disc is painted. The reading is ONE
TEXT: written in Slang and crossed into SkSL the way the grained four's
noise is, with each target's body that reading plus the one line that
spells the return in that target's own types, so a globe on a device and
a globe on a raster surface are the same ball.

The gel and
chrome tables — `aquaBodyRamp`, `aquaGlowRamp`, `chromeRamp`,
`contourRing` — are `RampStop` lists and alpha ladders a renderer turns
into its own gradient, and nothing else: which highlight a bundle shows
and how deep its bevel cuts are knobs on that renderer's decorations, so
its option sets are its own. The text paints
— `water`, `meshGradient`, `sparkle`, `starNest`, `clouds`, `tunnel` —
share the `TextPaintParameters` ABI of a run's origin and extent, the clock
and a slow motion vector; `sunsetChromeText()` and `silverChromeText()`
are the chrome-type ramps in unit space.

**The named colormaps are stock ramps, and that is why they are here and
not in the leaf.** Each answers a plain `Ramp` — a stock value over a
seam is kit — so a caller takes one, moves its domain onto the numbers
it is reading, reverses it or eases it, and still holds a value every
consumer of a ramp understands. What naming them buys is that these
particular stop lists were MEASURED rather than picked. `kit::viridis`,
`kit::magma`, `kit::inferno` and `kit::plasma` are the sequential four:
each rises steadily in lightness the whole way, so a difference in the
data is a difference an eye reports and none of them puts a false edge
where a rainbow puts one; they differ in where they spend their chroma,
and `plasma` is the one without a black end, for a map that has to sit
on a dark ground. `kit::turbo` is the rainbow done properly — every hue,
and no lightness cliff at the yellow or the cyan — and it still says
nothing about which end is more, because its lightness rises to the
middle and falls again: it is for telling many bands apart, not for
reading which value is larger. `kit::redBlue` and `kit::brownTeal` are
the diverging pair, palest in the middle where the quantity is neither
sign, so the sign reads as the hue and the magnitude as the depth of it;
the second stays two colours for a reader who cannot tell red from
green. `kit::cubehelix` is the one map that is GENERATED rather than
tabulated: `CubehelixOptions` — the starting hue, the turns, how far
from grey it strays, the lightness path and how many stops the curve is
sampled into — is the whole definition, so the hue path moves without
leaving the family, and the lightness still climbs evenly from black to
white, which is what makes it readable printed in grey.

**A surface is composed of TERMS.** `termsSource(target)` is one
text holding each piece of shading arithmetic as a function with a closed
form — `lambert`, `blinn`, `fresnel` and `fresnelRough`,
`specularColor`, `environmentBrdf` and `environmentSpecular` (the split
sum), `environmentReflection` (the additive one), `refraction`,
`attenuate` (Beer-Lambert, not called `absorption` because a surface's
own absorption is a uniform of that name and a term compiled beside one
would be an ambiguous reference), `emission`, `occlusion` — beside the display transform
every lit sum ends at, `luminance` and `toneMap`, and the panorama's own
geometry, `equirectangularUv`, `equirectangularDirection` and `roughnessLevel`. No
term is a whole shading model and none has to be physically complete to
be useful: a surface calls the ones it needs, the way a shader graph in
an authoring tool is a composition of nodes.

`toneMap(radiance, exposure)` is Reinhard's operator on luminance: the
radiance is multiplied by the exposure, then divided by one plus its own
luminance. A panorama holds values far above one — that is what makes a
sun a sun rather than a white disc the same brightness as the sky beside
it — and cutting the lit sum off at one would flatten every highlight to
the same white. This leaves zero at zero, barely touches a dim surface,
and lands a value a hundred times over white just under it with its
shape intact. The ratio is taken on luminance rather than per channel so
that hue and saturation survive the compression; a fully saturated
channel can still land above one, and what holds it there is the range of
the surface it is written into. The exposure is AUTHORED and never
measured — no average luminance, no adaptation — because a diagram that
dimmed itself when its content grew brighter would be a different picture
every frame.

Every term is PURE — nothing samples a texture, because sampling is
spelled differently in every shading language while arithmetic is not, so
a caller fetches the radiance and hands it in. `source(Target::Slang)` is
a MODULE a device renderer loads into its compiler session under the name
`Shading` and imports from its own shaders, which is what makes the
renderer's shading and every material body compiled beside it call one
definition of a term rather than a copy apiece;
`source(Target::SkSL)` is the same text with the module line and the
export qualifiers taken off. Nothing in it uses a construct the two
languages spell differently, the transcendentals included, which are
written out as polynomials for the reason a portable subset exists at
all: a library `atan2` is two pieces of code on two targets, and an
equirectangular lookup that disagreed between them would put a seam down the
middle of a reflection.

`skSLFromSlang` is that crossing, and any text may be handed to it. It
takes off the module line and the export qualifiers and renames the three
intrinsics the languages spell differently — `frac` to `fract`, `lerp` to
`mix`, and `atan2` to the two-argument `atan`, whose arguments SkSL takes
in the same order Slang does. Whole identifiers only, so `atan2P` and a
`fraction` are left alone, which is what lets one table serve the terms
and a kit body at once. Everything else has to be spelled the same in
both, and a source written for this crossing accepts that in exchange for
being one source: no texture sampling, no construct one language has and
the other does not.

**The metallic-roughness surface** is `kit::SurfaceParameters` — base
colour, metallic, roughness, emission, the normal convention, the channel
each packed map is read from, the cutout threshold and the glass terms,
which are transmission, index of refraction, thickness and the
Beer-Lambert absorption a medium takes out of what passes through it —
under two recipes over the same ABI: `kit::surface()` takes light,
`kit::unlit()` is its own light. Its colours are FACTORS on the maps in
their slots, and neither body transforms either side of that multiply or
the product it hands on, so a colour is held exactly as it is typed and
is in the encoding the images beside it are in: over the white fill a
surface is dressed with, `unlit()` paints the number that was written
into it. `SurfaceParameters::chrome()`, `gold()`,
`metal(tint, roughness)`, `dielectric(colour, roughness)` and `glass()`
are the compositions the kit ships. `Reflection` is how the environment
reaches a lit surface — `SplitSum`, where the surface's own reflectance
and its Fresnel decide, or `Additive` at `reflectionWeight`, with
neither — and it is one recipe each, so no body carries a branch. Seven slots, one per role
(`kBaseColorSlot`, `kNormalSlot`, `kRoughnessSlot`, `kMetallicSlot`,
`kOcclusionSlot`, `kEmissiveSlot`, `kOpacitySlot`), each dressed with a
neutral one-pixel fill when it is built so no body ever evaluates an
unbound child; `kit::map(m, slot)` answers the texture a caller placed
there and null for a fill. `kit::surface(TextureMaps)` dresses one from a
decoded set: a packed occlusion-roughness-metallic image wired to
whichever of the three channel slots no separate map fills, at channels
0, 1 and 2, the set's normal convention flagged, and the scalar a present
map multiplies started at one — left at its stock value a metallic map
would multiply zero and never be seen.

Both recipes carry a body in each language, and both bodies read the same
albedo, the same occlusion at the same strength, the same emission and
the same cutout — one ABI, two spellings. What a body can answer is
bounded by what its renderer knows: there is no surface normal, no view
vector and no light in a 2D paint, so metallic, roughness, the normal map
and the glass terms have no effect on either body. `surface()` shades the
albedo attenuated by occlusion plus its emission — the ambient-only
evaluation of the model — and `unlit()` shades the albedo alone.

A renderer that HAS the surface attributes reads the same parameters and
slots, and the lit Slang body tells it what the surface IS beyond its
colour: how rough, how metallic, how much light passes through it at what
index through what thickness of what medium, and how the environment
should reach it. Those are stated whether or not a map varies them,
because a mirror carrying no maps at all still has to reflect and only
the surface knows how rough it is. A MAP that varies the normal, the
roughness or the metallic across a face says one thing more and raises
the per-pixel flag: that is the case a shading evaluated once per vertex
cannot carry.

A Slang body writes out the intrinsics whose two targets are two
different pieces of code — a `lerp`, a `dot`, a `smoothstep` — because an
intrinsic is where one source stops producing one answer.

**Masks say where.** `maskConstant` is a number; `maskMap`
reads a channel of a texture — of an image, or of a painted lane a
renderer supplies; `maskSlope` and `maskHeight` read a tangent normal
dotted with an axis, or a value dotted with an axis, from whatever
texture the renderer supplies as the source. All of them then fit — `low` and `high` remap the
raw value onto 0..1 and clamp, and `invertMask` flips it — which is why
the slope and height factories take the range: without one those masks
mean nothing. `fitMask` moves the range on an existing mask, and both it
and `invertMask` reshape A MASK and nothing else: handed a material that
is not one they change nothing and say so, because a material with no
range to move looks, from the stack that reads it, exactly like a fit
that was wrong. Both mask
recipes carry a body in every language a renderer here speaks, because a
mask is an operand of a stack and a stack is only composable for a target
all three of its operands have a body for.

`kit::gold`, `kit::chrome` and `kit::glass` are recipes over two slots,
`normals` and `env` (glass adds `backdrop`, an image of what sits behind
the shape in the same device coordinates). Each parameter struct's fields
are the body's uniforms by name, with two exceptions the comments state:
`roughness` picks the environment level when the material is built, and
`envSize` is filled by the builder. Real reflection models sampled per
pixel: gold adds foil crinkle and glints, chrome the contrast curve and
brushed anisotropy, glass refracts the backdrop through the normal field
with a fresnel-weighted reflection on top.

**The grained surfaces are generated, never photographed.** `kit::stone`,
`kit::timber`, `kit::latten` and `kit::board` are recipes over no texture
at all, and all four are the same construction: a RAMP of the material's
own tones, a GRAIN of value noise folded into the colour as light rather
than as hue — which is what keeps a coloured surface from reading as
rainbow terrazzo — and a SPECKLE in some fraction of the cells of a
lattice. What differs is the ramp. `kit::StoneParameters` runs a bed of `hi`
and `lo` at `bedAngle` over `bedLength`, flecked in its own tones;
`kit::TimberParameters` is a planed board, a flat face between a narrow lit
arris and a narrow shadowed one across its `span`, with `flip` to light
the far edge and `along` to turn the piece down local y, so one recipe
boards a lattice's rails and its posts; `kit::LattenParameters` is sheet
brass, whose one colour and many lights are a three-tone LADDER — a
piece's `level` is where on it that face sits, and `sheen` drifts that
position along the run from `from` to `to`, which is how one light
crosses two hundred nodes of one instrument — and `kit::lattenTone`
reads that ladder on the CPU, at a position along the same run, for the
stroke or the gradient stop that takes a colour and cannot take a
material; `kit::BoardParameters` is a flat
`paint` under a fine tooth and a slow wear. Every length is in pixels
rather than in the box, because a tessera is cut from a slab and its
grain does not scale with the piece, and `seed` offsets every field, so
two pieces at two seeds are two pieces of one quarry. Each recipe carries
a body in both languages — the SkSL one reads pixels, the Slang one the
surface's uv — so a device renderer shades the same piece the 2D painter
does.

**A field of a thousand pieces banks its materials.** A paving whose
every sett differs cannot afford a material per sett — a material is a
program and a resolve — so `Bank` bounds them: `bank.get(recipe,
parameters, seed)` folds the seed into one of `buckets()` and answers the
instance for that (recipe, parameters, bucket) triple, minting it once. The
parameters' BYTES are their identity, which `schema<P>()` proves is sound by
refusing a struct that is not packed floats, so two pieces of one species
in one bucket are one material and a second tone is a second species; a
caller whose parameters belong to no C++ type hands those bytes
themselves, `bank.get(recipe, bytes, seed, make)`, and lands on the same
row a struct of them would. The
seeded form writes the bucket into a `seed` field and ignores whatever
seed the caller left there, so no caller can make the bank unbounded; the
form taking a maker banks whatever that maker builds per bucket — a
stack, a recipe over a jittered tone — so a blend is banked exactly as a
recipe is. Because the instance is held rather than re-minted per
describe, its identity is stable, which is what lets a consumer that
compares materials prune.

**Resolve is memoised on its inputs.** `resolve()` samples the bindings,
snaps and injects the frame values, and compares the resulting bytes plus
the target and variant against the previous call's; when they match, the
previous program and bytes come back with no cache lookup.

## Colour

The colour leaf is its own chapter: **[COLOUR.md](COLOUR.md)** — the
colour value and its packed spelling, the sRGB, OKLab, OKLCH and CIELAB
round trips with `fitToSrgb`, `Ramp` as one value with the palette
crossings both ways, the harmonies, the dither threshold, and the table
a run of pixels is made of. It links nothing of this project's and no
renderer: every value there is stated over colours and numbers, and
where one has to meet a picture the crossing lives with the renderer
that owns the picture.

## The primitives

**sdf.** `sdf::material(shape, style)` is shape, border, glow and soft
shadow in ONE pass over a signed distance — `roundBox`, `circle` or
`star` — with every style parameter a uniform, so a pulsing border is a
bound `uBorderW` and however many styles there are, three programs
compile. Distances are in pixel space over the resolution the frame
supplies, never uv, so borders stay even on a stretched box. The style's
outer treatments reserve `pad(style)` inside the box; size a box with
`minBoxFor(style, contentPx)` or the reserve eats the interior. A
`Style`'s colours are `Color`, which an `SkColor4f` converts to, so a
Skia caller writes one straight into the field. `star`'s `pointiness`
runs BLUNT TO SHARP: 2 is the regular polygon, and values toward the
point count narrow the arms until at the count itself they close to
nothing.

**pattern.** A `Tile` is one bake plus a mapping. The program draws one
seamless tile at a seed; the bake is memoised on shared state, `seed(n)`
and `program()` copy-on-write that state and drop it, and `scale`,
`rotate`, `offset` and `filter` act on the sampling matrix alone, so a
rotated repeat stays seamless with no rebake. The bake is the identity:
hold a Tile where assets are held. `texture()` is the bake repeating on
both axes through the mapping. The stock tiles — `halftone`, `stripes`,
`sequence`, `checker`, `gridLines`, `speckle` — are programs over it, and
each takes its colours as `Color`, which an `SkColor4f` converts to.
`sequence` takes the AXIS its runs travel along (`Axis::U` across,
`Axis::V` down) rather than leaving it to `rotate(90)`: rotating remaps
the sampling of a tile whose repeat is one period by an arbitrary eight
pixels, which reads right only while the other direction is constant.

**A woven cloth is two threadcounts and one interlacing.** `ThreadRun`
is a run of consecutive threads of one shade — "18 black" is one — and
`threadcount(runs, symmetry)` expands a sett into one shade index per
thread. The shades are INDICES into the cloth's own palette, because a
threadcount is the cloth's identity and the shade card is a variable:
the same count woven in two dyers' blues is the same cloth. `Symmetry`
is how the runs spell the repeat — `Asymmetric` takes them whole,
`Reflective` takes them as the half sett and follows it with its mirror,
which is what a register printing a pivot at half its width means — and
`pivots(threads)` reads the reflection boundaries back off a count, two
of them exactly half a repeat apart for a reflective sett and none for
an asymmetric one.

`Weave` is the interlacing: how many ends the warp floats over, how many
it passes under, and how far the pattern steps per pick. `Weave::plain()`
is the checkerboard; `Weave::twill(2, 2)` is the tartan twill, whose
step is what draws the rib on a diagonal, and the step's sign chooses
which diagonal. `warpUp(weave, end, pick)` is the whole rule, and
`Cloth::at(end, pick)` reads a crossing through it: the warp's shade
where the warp is up, the weft's where it is not, darkened by the cloth's
`rib` on the weft floats so the interlacement stays legible inside a
block of one colour. `clothRepeat` is the repeat in threads — a sett
whose length is not a multiple of the weave's period tiles wider than
the sett — `clothImage(cloth, origin, size)` bakes a window one pixel
per thread, and `clothTile(cloth, threadPx)` is the whole repeat as a
nearest-sampled `Tile`. A tartan is that generator at a reflective sett
under a 2/2 twill; gingham is a two-colour sett under a plain weave;
houndstooth is a four-and-four sett under the tartan's own twill.

A Tile is not a fill: what fills is the material over it, and a
consumer that takes one as a fill is expected to refuse it by name
rather than bake it per frame. The bake is the identity, so a tile
minted inside a describe is a fresh state with no bake in it and
re-renders every frame — hold the tile where assets are held and fill
with `tile.material()`.

**field.** `halftoneRamp` swells a staggered dot grid down the box and
reads the resolution; `noise` is Skia's Perlin generator behind a
pass-through recipe, so it fills a slot and compares by its parameters;
`grain` is value-noise fBm collapsed to one channel, one recipe per
octave count because the count is a constant in the body; `ripple`
resamples its `content` child through a sine displacement; `crtOverlay`
is the tube laid over a picture — in black, with the alpha carrying all
of it — and reads the resolution. Its darkening is a SUM, and each term
is absent at no strength, so one recipe covers a monitor across a room
and a plate shot close: a hard line at `uScanPitch`, the beam's own
profile at `uBeamPitch` and `uBeamFalloff`, the beat a composite signal
carries under it at `uBeatPitch`, `uGrain` moving how much light a cell
gives up, and the corner falloff. The positional `crtOverlay(scanPitch,
…)` is the hard line alone; `crtOverlay(CrtOverlayParameters)` is the whole
tube.

### CRT screens

A screen is THREE SUBJECTS, and `<sigilmaterial/field/Crt.h>` holds each
of them as a recipe of its own:

- `field::crtBeam` — what the tube draws, in the coordinates it draws it
  in: the picture in the `content` slot read line by line with the guns
  converging `uRgbShift` apart, rastered at `uScanPitch` by `uRaster`,
  swept sideways by `uJitter` and `uSync`, carrying the supply's
  `uFlicker` and the signal's `uNoise` at `uBrightness`. The scanlines
  alone, for a flat surface.
- `field::crtBloom` — the light that picture throws: the layer blurred
  at `uBloomRadius` and kept inside the bounds. The bloom source alone,
  as a layer to add over whatever threw it.
- `field::crtGlass` — the glass over both: the barrel at `uCurvature`,
  the light from the `bloom` slot at `uBloom`, the corner falloff at
  `uVignette`. The barrel alone, for any surface that wants a tube's
  curvature over it, and opaque wherever it stands, so a transparent
  region of what is under it reads as black glass. What stands under
  the glass is read ONCE, at the bent coordinate, because in one
  program a second reading is a second whole pass of whatever is under
  there — which is why the guns are the beam's and not the glass's.

`field::crt` is their composition and the whole screen: one program, in
which the glass reads the beam where it would read a bound picture, so
the coordinate is bent ONCE and the beam is drawn at the coordinate it
was bent to. Its `field::CrtParameters` are the three parameter sets
under one name, in local pixels and explicit seconds, so a still is
deterministic. Zero strengths preserve source RGB inside the bounds. The
screen is opaque black beneath transparent source content; outside its
rectangular bounds it is transparent. Burn-in needs frame history and is
not included.

The supply is the BEAM'S. `uBrightness` and `uFlicker` modulate the
picture the tube draws and nothing else, so the light over it holds the
strength `uBloom` names while the picture dims and stutters. And the
beam's picture is finished before the glass reads it — rastered,
grained and clamped to what a display can carry — so the light is added
to a highlight already standing at white, and `uVignette` darkens the
picture and the light over it together.

The tube's light is a SECOND slot, `bloom`, declared as one an executor
fills from the same layer blurred at `uBloomRadius` — a Gaussian sigma
in local pixels — and read once at the bent coordinate. So the bloom is
spread flat and sampled through the curvature, its cost follows its own
radius, and how far it reaches is not capped by a tap count. The blur is
taken over the whole layer, so a bright thing drawn outside the bounds
lights the glass near that edge; where the light lands is gated by the
bounds exactly as the picture is. An ordinary fill has no layer and
therefore no executor: the bodies of `crt`, `crtGlass` and `crtBloom`
spell that slot at every strength, so bind a source to `bloom` as well
whatever `uBloom` is, or the compiler refuses the material by name.

`kit::crt` in `<sigilmaterial/kit/Crt.h>` supplies a restrained colour CRT
preset. The material stays renderer-independent; fill its `content` and
`bloom` slots with textures for a material, or pass it to
`skia::Effect::recipe` for a rendered layer, which fills `bloom` itself.
Pass a conservative local sampling radius to the effect;
`field::crtSampleRadius` calculates it from a parameter struct as the sum
of its passes' own — `field::crtGlassSampleRadius`, the warp, and
`field::crtBeamSampleRadius`, the guns, the jitter and the sync — and
never the bloom, which asks for no reach of its own. When changing
uniforms on the preset, update that radius to cover them.

```cpp
#include <sigilmaterial/kit/Crt.h>
#include <sigilmaterial/skia/Effect.h>

const auto screen = sigil::material::kit::crt(SkRect::MakeWH(1440, 1052));
const auto effect = sigil::material::skia::Effect::recipe(screen, 40.0f);
```

The SkSL adaptation and its provenance are described in
[CRT-NOTICE.md](CRT-NOTICE.md); its license is GPL-3.0-or-later.

## The Skia paint

The paint and the effect are their own chapter:
**[PAINT.md](PAINT.md)** — `skia::Paint` as ONE `sk_sp<SkShader>` over a
tree of solids, ramps, images, buffers, SkSL effects and blend layers;
the three volatility tiers it declares by what it reads; the unit-square
ramps that need no box size written down; and `skia::Effect`, the
post-processing recipe over a layer a consumer has already rendered.

`skia::Effect::recipe(material)` captures the material's uniforms and child
slots when constructed — the layer slots with them, whose filters are
built from the amounts the material carries at that moment and for the
same reason. Static captures compare by material value and compiled
program, so describing the same effect again can reuse a retained layer.
Captures with live inputs compare by their built filter's identity: a binding
names its source rather than the value captured from it. Re-describe to sample
those inputs again. Surface-specific lowering to a colour filter retains that
filter's own identity.

## Warming every program

**One instance of every recipe, as a list.** `kit::everyRecipe()`,
`sdf::everyRecipe()` and `field::everyRecipe()` each answer a
`std::vector<Material>` holding one instance of every recipe that
feature ships, dressed the way its own builder dresses it and with a
stand-in image in any slot that needs one — because a recipe is only
half of what a backend compiles and a slot left empty generates a
different program. They are for a caller that has to reach every program
the library can ask a backend for without knowing what it holds: a
device renderer warming its pipeline cache, and the device sweep below.
A recipe added to one of those features belongs in its list.

Every body those instances carry is already in the archive, so building the
list opens nothing. A Skia host can prepare the entire catalogue with
`skia::warmup(stock::everyRecipe())`, or pass only the materials it uses.
This prepares the backend and compiles its distinct programs before the
first draw; omitting warm-up leaves compilation to first use.
`material::warmup(requests)` folds identical recipe,
target and variant keys and compiles distinct keys concurrently;
`material::warmup(materials, target, variant)` is the catalogue-shaped
spelling. A request arriving while the same key is compiling shares that
in-flight result, and the cache's synchronization remains an implementation
detail. A registered compiler can therefore receive concurrent calls for
different keys; a backend with thread-affine work must marshal that work at
its own executor seam.

**Every body an effect is built out of, as one list.**
`skia::everyEffectProgram()` in `<sigilmaterial/skia/Effect.h>` answers a
`std::span<const sk_sp<SkRuntimeEffect>>` holding the compiled SkSL an
`skia::Effect` runs — the bright pass and phosphor halo a bloom gathers,
the tap that lays that halo back, a light's deepening and whitening, and
a parametric blur's mix. The recipes a `skia::Paint` runs are not among
them; those are reached through the program cache, one per recipe.

They are the very objects the effects go on to use, not copies. A device
backend can be asked to give a runtime effect a name that outlives the run,
so a device program built over one can be written down and rebuilt at the
next launch instead of compiled again — and the name is given to the OBJECT,
so an effect compiled a second time from the same source is a stranger to
it. An effect's place in the list is part of its name, which is why the
order is fixed and why a body that would not compile is absent from the list
rather than null in it: the list is what is really there, and a machine that
loses a body offers a shorter one. Reach for this when declaring effects to
such a backend. Asking compiles all of them, which is a handful of small
programs beside the device programs they are inlined into.

## Where the stock shaders live

Every body this library ships is a `.sksl` or `.slang` file in the
`shaders/` directory beside the feature that owns it, so an editor and a
shader tool see the language, and `sigil_shader_sources()` compiles that
whole directory into the feature's archive as a table of
`std::string_view` keyed by file name. A feature reaches its own text
through the accessor the generated header declares —
`<sigilshaders/MaterialKit.h>` spells `kit::shaderSource("Stone.sksl")`
and `kit::shaderSources()`, the whole table — and no feature reaches
another's: text that two of them need is asked for by name from the one
that owns it, which is what `termsSource` is.

Adding a file to a `shaders/` directory is the whole of adding a body:
the glob picks it up on the next build, and a per-feature case fails if
the table and the directory ever disagree.

Nothing here reads a shader from disk at run time, so a binary carries
every body it can draw with wherever it is run from. A shader a CONSUMER
authored is the other thing entirely and arrives by URI through SigilIO,
from wherever that consumer keeps it.

## Boundaries

The core links no renderer; the texture feature links Skia because a
texture IS a Skia image with its sampling, and SigilImage because an
asset is a source. The Skia seam is therefore wider than
`sigilmaterial/skia/*`: every `texture/*.h`, `pattern/Tile.h` and
`kit/TextPaint.h` names a Skia type in its own signatures, because an
image, a baked tile and a text paint ARE Skia values. A header outside
those places that needed one would be the boundary moving. SigilIO owns resource access and SigilImage owns
image meaning, so this library decodes no pixels and opens no consumer asset
file — every door that needs pixels takes them or takes a decoder. Its own
shader files are compiled into its archives rather than read through SigilIO,
so no feature here links a resource hub and none of them can be run from a
directory that has no shaders in it.
SigilGeometry draws
the normals passes and outlines a surface is shaded over, and links
nothing here but the colour leaf, privately, for the OKLab interpolation
its path blend runs in; SigilWorld's renderer is one executor of the
surface the kit defines and adds no shading model of its own;
SigilCompose places what a material paints — it takes a `skia::Paint` as
a node's fill and routes it, and holds no paint model of its own.

## Building and testing

[docs/overview/testing.md](../../../docs/overview/testing.md) is the
contract every library here is built, tested and measured under: one
`material_test` over every feature's `test/` and one `material_bench`,
ctest one entry per CASE, what a case may pin, and what a label
promises. What is only true of SigilMaterial:

| suites | what they prove | label |
|---|---|---|
| `core/test/` | the value model, with no renderer in reach: parameters reflection — including that the schema IS the parameter struct's own layout, read off `offsetof` rather than off the numbers this compiler happened to choose, and that a field list packed at run time lays out the same way — recipe identity against definition equality, the program cache's keys, a compile held open until every concurrent request has arrived so the fold is asked without a clock, the field it names once when a compiled body never reads it, material equality, bindings and which field carries one, slots and tiers, what `over()` stacks, the bank keyed on the parameters' bytes whichever door they arrive through, and `UniformBlock` revisioning | — |
| `color/test/` | the four suites that hold to closed forms rather than to colours this code once answered: `Color` (the transfer function and the OKLab round trip against their own inverses, the three mixes separated by where their midpoint lands, a palette read exactly where a ramp is read between), `Ramp` (the decisions one at a time — the ends are the stops and outside them is flat, the domain is the caller's numbers, reverse and easing move the position and not the stops, each space walks its own path while both ends round-trip, two stops at one position are an edge with nothing across it, a table read at band centres comes back a ramp), `Harmony` (the polar round trip, a rotation giving up chroma alone, each scheme its own set of angles with the base first) and `Dither` (the ordered matrix holding every threshold once and averaging a half, the noise averaging the same with no period to find, a dithered ramp coming back at the value it was asked for). `Extract` holds the two methods apart by what each is for and pins the determinism and the stride | — |
| `sdf/`, `pattern/`, `field/test/` | the primitives beneath the leaf: the SDF surfaces, the tile mechanism and the stock generators over it, and the fields | — |
| `ocio/test/` | the bake: an exponent baked to a response row, that row lowered to a table an eight-bit surface admits, and the program held to what it paints across a whole ramp, while a float surface and a channel-mixing transform keep the program. A config that cannot be read failing soft is asked unconditionally, since that needs no OpenColorIO | `ocio` on `Ocio` |
| `texture/test/` | the image side: the sources and their identity across the erasure, the sampling dials, the environment map, the bevel producer, the atlas readers and packer, and the tools' file names — one row per name, so a failure says which tool's spelling moved rather than that a list changed | — |
| `mask/test/` | that a mask shapes what it reads, and that reshaping something that is not a mask changes nothing | — |
| `kit/test/` | every preset compiled and a fill staying inside its path, a surface dressed from a decoded set, a stack shaded at both ends of its mask, every shading term against its closed form, the named ramps asked for the properties they were chosen for rather than their stop lists, and the sampler budget: a stack asks a device for its operands' samplers and no more, and a tree over the limit is refused with the count and the limit named rather than drawn | — |
| `skia/test/` | the SkSL backend — a two-uniform recipe compiled through the cache shading a raster byte identical to the same SkSL compiled and filled by hand, the four parameter names a body may not redeclare and the three spellings that must still compile — and `SkiaPalette`, a picture's own colours coming back | — |
| `slang/test/` | the Slang backend, with no device | — |
| `stock/test/` | that the catalogue holds every feature catalogue, and that the warm-up compiles every program it gathered | — |
| `MaterialGpu` | every body this library ships, on a device | `gpu` |

The `MaterialGpu` suite belongs to the whole library rather than to a
feature: every other suite shades on a raster surface, where a body is
compiled as its own SkSL program, and a body can pass that and fail once
a GPU backend has inlined it into a pipeline. It stands Graphite up,
installs a shader-error handler through
`GraphiteContext::reportShaderErrorsTo`, and draws every material
`kit::everyRecipe()`, `sdf::everyRecipe()` and `field::everyRecipe()`
answer — plus a stack per blend, the whole terms text, and the ocio bake
where OpenColorIO is available — through the same `skia::Paint` a
consumer draws it through, demanding that not one reports an error. It
needs Metal, and it carries its own control: the collision the reserved
names exist to prevent, built as a raw runtime effect so it reaches the
device, must be reported — which is what proves the handler is wired to
anything at all.

```sh
ctest --test-dir build -C Release -R '^MaterialGpu\.'
```

**The fixtures more than one file needs live once, in `test/support/`**:
`Shade.h` holds the two ways of drawing a material — the shader over a
whole surface, which asks what a body computes at each point, and a fill
over a path, which asks what a caller painting a shape gets — beside the
readings taken off the result. A directory a case writes into is
`sigil::test::ScratchDir`, the tree's own.

The acceptance pieces are the `material_lab`, `material_atlas`,
`material_slots`, `stock_materials`, `text_paints`, `reflection_lab`,
`env_faces`, `env_lanes`, `shapeworks_lab` and `mesh_normal_bridge`
sketches under `src/sketch/sketches/`, whose surfaces are shaded here.
SigilCompose is the largest consumer: its `Material::recipe` resolves a
material through this library's cache with the frame built from its
paint context, and its patterns, SDF fills, layer styles and view
transforms are the primitives and presets here spelled as compose
values.
