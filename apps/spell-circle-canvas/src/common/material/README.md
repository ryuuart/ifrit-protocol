# SigilMaterial

Materials as recipe instances. A **recipe** is a material's definition: a
plain C++ struct of uniform-typed fields that is its ABI, one shader body
per shading language, the child slots it samples and the per-frame values
it reads. A **material** is one instance of a recipe: the field values,
mirrored as the bytes the shader will receive; live bindings that overwrite
fields every frame; other materials filling its child slots; and the
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
with a map per role — is a preset like any other: one params struct is
its ABI, one child slot per map, and the choice between the lit and the
unlit recipe is what the surface IS. Local variation on top of it is not
a bespoke recipe per pair but a composition: `over(base, top, mask)`
stacks two materials where a mask says.

Above those sit the PRIMITIVES — fully parameterised generators, one
feature each: **sdf** (shape,
border, glow and shadow in one pass over a signed distance), **pattern**
(a tile baked once with a mapping and an explicit reseed, and the stock
tiles over it), and **field** (the halftone ramp, Perlin noise, luminance
grain, the ripple, the CRT overlay). Under the core sits **colour**, the
leaf: the colour value a params struct holds and the OKLab round trip,
linking nothing; above the texture feature sits **ocio**, OpenColorIO's
view transforms baked to materials. The **kit** holds PRESETS — functions that fix
colours, proportions or a named style over the primitives: the
metallic-roughness surface and the masks that stack it; gold, chrome
and glass over a normal map and an environment; the girih panel and its
palettes; the gel and chrome colour tables; the six text paints and the
chrome-type ramps.

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

Namespace `sigil::material`. Ten feature libraries, one per directory,
each a static archive that links only what sits beneath it:

| target | holds | links |
|--------|-------|-------|
| `SigilMaterialColor` | `Color`, `rgb()`, `hsv()`, `scale()` and `mixToward()`, `RampStop` and the OKLab round trip — the leaf, which the core's `Params.h` includes | nothing of this project's |
| `SigilMaterialCore` | the value model: `Target`, `Params`, `Recipe`, `Program` and the cache, `Material`, `Leaf`, `UniformBlock`, `FrameData`; `Bank`, the bounded seeded bank of a field's instances; `termsSource`, the shading terms a surface is composed of; and `over()`, the combinator that stacks one material on another through a mask | SigilMaterialColor, SigilMotionValues, glm, Boost.PFR, Boost.Container; Boost.Unordered privately |
| `SigilMaterialTexture` | `Texture` and its sources, `ShaderLeaf`, `texture::` (the tools' sets by role), `EnvironmentMap` and `bevelNormals`, `Atlas` | SigilMaterialCore, SigilImageAsset, Skia, Boost.Container; simdjson privately |
| `SigilMaterialMask` | the third operand of `over()`: `maskConstant`, `maskMap`, `maskVertexColor`, `maskSlope`, `maskHeight`, and `fitMask` / `invertMask`, which reshape a mask and nothing else | SigilMaterialTexture, glm |
| `SigilMaterialOcio` | `ocio::` — `available()`, and the OCIO `viewTransform`, `convert`, `exponent` as baked materials, over the 3D-LUT `lutRecipe()` and the per-channel `responseRecipe()` | SigilMaterialTexture; OpenColorIO privately, when found |
| `SigilMaterialSdf` | `sdf::` — `Shape`, `Style`, `pad`, `material`, `everyRecipe` | SigilMaterialCore, SigilMaterialColor |
| `SigilMaterialPattern` | `pattern::Tile` and the stock tiles | SigilMaterialTexture, SigilMaterialColor; SigilCoreCompute privately |
| `SigilMaterialField` | `field::` — `halftoneRamp`, `noise`, `grain`, `ripple`, `crtOverlay`, `everyRecipe` | SigilMaterialTexture, SigilMaterialColor |
| `SigilMaterialSkia` | the SkSL compiler and `SkiaProgram`, whose builder uploads resolved bytes; `skia::builder` and `skia::shader` binding leaves into slots; `skia::fill`; the colour bridge `skia::toColor` / `skia::toSkColor` / `skia::toColors`; `skia::verticalRamp` and `skia::unitRamp`, the two crossings a list of `RampStop`s reaches Skia through; `skia::Paint`, the model as ONE shader; and `skia::Effect`, the post-processing recipe over a rendered layer | SigilMaterialTexture, SigilMaterialColor, SigilMotionValues |
| `SigilMaterialSlang` | the Slang compiler: `slang::compileModule` to SPIR-V, `slang::Compiled` with the reflected `slang::UniformSlot` per uniform, `slang::SlangProgram`, and `slang::Uniforms`, the buffer one draw is written into; `Portable.slang`, the subset a host and a device answer alike, loaded into every session by name | SigilMaterialCore, Boost.Container; Slang privately |
| `SigilMaterialKit` | the presets: the metallic-roughness `kit::surface` and `kit::unlit`; `kit::gold`, `kit::chrome`, `kit::glass`; the grained `kit::stone`, `kit::timber`, `kit::latten` and `kit::board`; `kit::girih8` and its palettes; the gel and chrome tables with `kit::contourRing`; the text paints and chrome-type ramps; `kit::studioEnvironment` and `kit::sunsetEnvironment`, the two named skies; and `kit::everyRecipe`, one instance of each of the above | SigilMaterialPattern, SigilMaterialColor, SigilMaterialMask, Boost.Container |

`SigilMaterial` is the umbrella, an interface over all ten. Headers live
under `include/sigilmaterial/<feature>/` and are spelled that way —
`<sigilmaterial/core/Recipe.h>`, `<sigilmaterial/texture/Texture.h>`,
`<sigilmaterial/kit/Surfaces.h>` — and `<sigilmaterial/Material.h>`
includes the whole core.

## Using it

```cpp
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/Material.h>
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
// declarations — the uniforms above, then uTime, then the child slot.
auto glow = std::make_shared<const Recipe>(
    Recipe::of<Glow>("glow")
        .frame(FrameInput::Time)
        .child("uSrc")
        .body(Target::SkSL, std::move(*glowSource)));

// An instance: values now, a bound clock and a live table later.
Material m(glow, Glow{1.0f, {1, 0.8f, 0.2f, 1}, {}});
m.bind("uScale", &scaleOutput);        // a choreograph::Output<float>
m.bind("uBars", spectrumBlock);        // a shared_ptr<UniformBlock>, 8 floats
m.child("uSrc", Material(gradientRecipe, GradientParams{...}));

// A renderer, once:
skia::install();                        // registers the SkSL compiler

// A renderer, per frame:
FrameData frame{.seconds = clock.now(), .resolution = {w, h}};
sk_sp<SkShader> shader = skia::shader(m, frame);
```

`skia::shader` is the whole Skia path: it resolves the material, builds
over the program's effect with every uniform set from the resolved bytes,
binds each child slot — a material child resolved recursively, a texture
leaf as its image shader — and makes the shader. A renderer that needs
the pieces takes them apart the same way — `m.resolve(Target::SkSL,
frame)` returns the `Program` and the bytes, and
`program->as<skia::SkiaProgram>()->upload(builder, bytes)` fills a builder
the renderer made over `program->effect()`. `skia::fill(canvas, path, m)`
is the one-call draw: clip to the path, paint the shader across it.

A surface from the kit reads the same way, its slots filled with textures:

```cpp
#include <sigilmaterial/kit/Environments.h>
#include <sigilmaterial/kit/Surfaces.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/texture/Surface.h>

const EnvironmentMap studio = kit::studioEnvironment();
kit::ChromeParams steel;
steel.brushed = 0.6f;
steel.roughness = 0.2f;
// bevelNormals() places its map at the outline's bounds, so the recipe
// reads the normal under the pixel it shades.
const Material badge = kit::chrome(bevelNormals(outline, 12), studio, steel);
skia::fill(canvas, outline, badge);   // per frame; the program is cached
```

## Mental model

**A params struct is the ABI, and the bytes are the upload.** Every field
type is some count of floats with float alignment — `float`, `glm::vec2`,
`glm::vec4`, `std::array<float, N>`, `Color` — so a struct of them has no
padding and its memory image is exactly the uniform data in declaration
order. `schema<P>()` proves this at compile time and refuses a struct with
any other field type or with padding. The same walk emits the uniform
declarations (`declare<P>(target)`), so the names in the shader are the
names in the struct and cannot drift. A struct with NO fields is legal and
is a recipe with no ABI of its own — a body over child slots and frame
inputs alone.

**Writing to a field no body reads is reported at the write.** A dial
that does nothing looks, from the call site, exactly like a dial set to
the wrong value: the bytes go up and the picture does not change. So
`Material::set(name, …)` asks the recipe — `Recipe::readsField(name)`,
which is whether any body of it SPELLS the name as a whole identifier —
and names the recipe and the field on stderr once per pair, beside the
reports for an unknown field and a wrong float count. The value is still
written; the report is about the picture, not the bytes.

It is asked at the WRITE and not at the compile because a params struct
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
params layout, so the values, bindings and children carry over and the two
definitions compile and cache apart. Hold the specializations, one per
distinct constant, or the cache fills with a definition per draw.

**One body per target, and asking for a missing one is an error once.**
`Recipe::body(Target, source)` stores the body for a language;
`Recipe::source(target)` is the generated declarations followed by it.
The two targets ask a body for the same thing in their own words:

| target | what a body is | how it reads a child slot |
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
than scrolling past every frame. A body that compiles but leaves a params
field unread is reported the same way.

**One program cache.** `ProgramCache::shared()` holds every compiled
program in the process, keyed by (recipe identity, target, variant). A
backend registers its compiler with `registerCompiler(Target, Compiler)`
and the cache compiles on first use. `SigilMaterialSkia` registers the
SkSL one with `skia::install()`; a device renderer registers the Slang
one, since only the renderer knows the scaffold a body is appended to. `Variant` is a small ordered key the
backend owns the meaning of — a premultiplied build, a debug view — and
the default variant is the plain build.

**Bindings are live, and equality is by identity.** `bind(name, Output*)`
makes a float field read the output's current value at every resolve;
`bind(name, shared_ptr<UniformBlock>)` does the same for an array field
and a caller-owned table. A bound material `isAnimated()`. Two materials
bound to the same output or block compare equal; bound to different ones,
unequal; the values behind them never enter the comparison. A `UniformBlock`
carries a revision (`commit()` advances it) so a caller can tell an edited
frame from an untouched one, and its values are read live whether or not
they were committed.

**Frame inputs are declared, then injected.** `Recipe::frame(FrameInput)`
declares that the body reads `uTime`, `uResolution`, `uContentScale` or
`uWorld`; the declaration adds the uniform after the params and
`resolve()` fills it from the `FrameData`. Time and content scale make a
material `isAnimated()`; resolution and the world transform make it
`geometryDependent()`. `quantizeTime(hz)` snaps the time a material sees
to a step, so a material that need not move every frame resolves only
when the snapped clock advances.

**Children ride everything.** A recipe declares slots (`child("uSrc")`,
exposed to SkSL as `uniform shader uSrc`); a material fills them with other
materials or with leaves. A live child makes the parent live, a
geometry-dependent child makes it geometry-dependent, and a different
child makes it unequal — which is required, not incidental: a child left
out of equality would let a node prune while its second source had
changed.

**A leaf is a child no recipe computes.** `Leaf` is the core's seam for
an image with its sampling, a rendered frame, anything a backend binds
into a slot directly: it compares by value (same dynamic type, then the
type's own equality) and says whether it moves between frames.
`ShaderLeaf` is the Skia-facing refinement — a leaf that yields the
`SkShader` to bind — and `Texture` is one such leaf; a renderer's own
native sources (a gradient it built, a Perlin generator) are others. The
Skia backend binds any `ShaderLeaf`. A slot holds a material or a leaf,
never both, and `Material::child(name)` and `Material::leaf(name)` each
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
u = azimuth, v = 0 at the zenith, with `equirectUv` and
`equirectDirection` as the one convention every consumer shares. Sources
resolve into that single form while the value is built: `baked()` runs a
radiance function over the panorama (the kit's `studioEnvironment()` and
`sunsetEnvironment()` are two written against it, and need no assets),
`fromEquirect()` wraps a loaded
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
become its children, so the stack compares, animates and resolves as one
value, and applying `over` again builds a taller one. The MASK is any
material whose red channel is read as a scalar; `blend` is `Mix`, `Add`
or `Multiply`, one recipe each so a body carries no branch; `amount` is
how strongly the top shows where the mask is fully on, which is the
stack's own strength rather than a second answer about where it applies.
It is a parameter of the call because a COMPOSED stack has no params
struct to write afterwards — its ABI is its operands' fields — so a
caller who did not know to write the field by name would get a stack at
full strength and read it as a wrong mask. `under(m)`
is the material a stack stands on — one step down, so walking it reaches
the bottom — and `stackDepth(m)` counts the steps. A consumer that can
only express one material (`UsdPreviewSurface`, say) writes the bottom
and records the depth.

**Two kinds of target read a stack, and only one of them can reach the
operands.** A target whose child slot is a SHADER — SkSL's is — samples
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
as children, the same walk down, and the same recipe NAME — which is what
says a material is a stack, since a composed one carries a recipe built
for its own operands rather than the shared one. The operands' values and
their sampled slots are copied in at the moment of the call, so a later
edit to an operand is not seen and a live binding on one does not reach
the composed body; the operand still rides every query as a child, so the
stack still reports itself animated. The composition costs one recipe and
one program per distinct triple of definitions and buys nothing for a
target that samples its operands, so it is built only where a compiler
that needs it is installed. `Target::Slang` is the one such target,
and `stackName(blend)` is the name every stack of a blend carries.

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
is the {8/2} khatam and whose outlines are the interlace. The gel and
chrome tables — `aquaBodyRamp`, `aquaGlowRamp`, `chromeRamp`,
`contourRing` — are `RampStop` lists and alpha ladders a renderer turns
into its own gradient, and nothing else: which highlight a bundle shows
and how deep its bevel cuts are knobs on that renderer's decorations, so
its option sets are its own. The text paints
— `water`, `meshGradient`, `sparkle`, `starNest`, `clouds`, `tunnel` —
share the `TextPaintParams` ABI of a run's origin and extent, the clock
and a slow motion vector; `sunsetChromeText()` and `silverChromeText()`
are the chrome-type ramps in unit space.

**A surface is composed of TERMS.** `termsSource(target)` is one
text holding each piece of shading arithmetic as a function with a closed
form — `lambert`, `blinn`, `fresnel` and `fresnelRough`,
`specularColor`, `environmentBrdf` and `environmentSpecular` (the split
sum), `environmentReflection` (the additive one), `refraction`,
`attenuate` (Beer-Lambert, not called `absorption` because a surface's
own absorption is a uniform of that name and a term compiled beside one
would be an ambiguous reference), `emission`, `occlusion` — beside the display transform
every lit sum ends at, `luminance` and `toneMap`, and the panorama's own
geometry, `equirectUv`, `equirectDirection` and `roughnessLevel`. No
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
equirect lookup that disagreed between them would put a seam down the
middle of a reflection.

**The metallic-roughness surface** is `kit::SurfaceParams` — base
colour, metallic, roughness, emission, the normal convention, the channel
each packed map is read from, the cutout threshold and the glass terms,
which are transmission, index of refraction, thickness and the
Beer-Lambert absorption a medium takes out of what passes through it —
under two recipes over the same ABI: `kit::surface()` takes light,
`kit::unlit()` is its own light. `SurfaceParams::chrome()`, `gold()`,
`metal(tint, roughness)`, `dielectric(colour, roughness)` and `glass()`
are the compositions the kit ships. `Reflection` is how the environment
reaches a lit surface — `SplitSum`, where the surface's own reflectance
and its Fresnel decide, or `Additive` at `reflectionWeight`, with
neither — and it is one recipe each, so no body carries a branch. Seven child slots, one per role
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

A renderer that HAS the surface attributes reads the same params and
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
reads a channel of a texture; `maskVertexColor`, `maskSlope`
and `maskHeight` read a channel, a tangent normal dotted with an
axis, or a value dotted with an axis, from whatever texture the renderer
supplies as the source. All of them then fit — `low` and `high` remap the
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
the shape in the same device coordinates). Each params struct's fields
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
lattice. What differs is the ramp. `kit::StoneParams` runs a bed of `hi`
and `lo` at `bedAngle` over `bedLength`, flecked in its own tones;
`kit::TimberParams` is a planed board, a flat face between a narrow lit
arris and a narrow shadowed one across its `span`, with `flip` to light
the far edge and `along` to turn the piece down local y, so one recipe
boards a lattice's rails and its posts; `kit::LattenParams` is sheet
brass, whose one colour and many lights are a three-tone LADDER — a
piece's `level` is where on it that face sits, and `sheen` drifts that
position along the run from `from` to `to`, which is how one light
crosses two hundred nodes of one instrument; `kit::BoardParams` is a flat
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
params, seed)` folds the seed into one of `buckets()` and answers the
instance for that (recipe, params, bucket) triple, minting it once. The
params' BYTES are their identity, which `schema<P>()` proves is sound by
refusing a struct that is not packed floats, so two pieces of one species
in one bucket are one material and a second tone is a second species. The
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

`Color` is four straight (not premultiplied) sRGB floats, uploaded as one
float4; `rgb(0xRRGGBB)` is its packed spelling. `Color.h` also holds the
sRGB transfer function both ways and the OKLab round trip — `toOklab`,
`fromOklab`, `lerpOklab` — which every perceptual interpolation in the
codebase runs through.

**Two ways to name a colour, for two different jobs.** `rgb()` is how an
authored palette is typed in; `hsv(hueDegrees, saturation, value)` is how
a palette is WALKED — a wheel, a run of chips on a golden-angle step, one
hue's tone ladder read off saturation and value together. The hue wraps
and the other two clamp, and both folds are in the verb rather than at
the call site because the sextant ladder underneath answers magenta for
any hue it does not recognise, which is exactly what an unwrapped angle
hands it. HSV is not a perceptual space and must not be used as one:
`value` is the largest channel and nothing more, so a full-value yellow
and a full-value blue are nowhere near the same brightness. Anything that
INTERPOLATES goes through `lerpOklab`.

**A Skia colour crosses at one place, and it is `Color` itself.**
`SkColor4f` holds the same four straight sRGB floats in the same order,
so the crossing is a field-for-field copy — no transfer function, no
premultiply, no clamp, so a channel above 1 survives. `Color` is
IMPLICITLY CONSTRUCTIBLE from one, matched by shape rather than by name
(`FourFloatColor`: four float members `fR`, `fG`, `fB`, `fA`), so the
leaf that every params struct includes still names no renderer:

```cpp
pattern::stripes(6, 6, kInk);              // kInk is an SkColor4f
sdf::Style style{.fill = kInk, .borderColor = kEdge};
```

`skia::toSkColor` is the way BACK, which a colour cannot carry without
naming Skia, and `skia::toColors` converts a palette in one call;
`skia::toColor` is the same conversion under a name, for a call that
wants to say so (`<sigilmaterial/skia/Color.h>`). The mapping is written
once because a copy of it spelled at a call site is a place where a
channel order or an alpha convention drifts silently.

**A view transform is a baked material with one open slot.**
OpenColorIO's GPU codegen never emits SkSL, so `ocio::viewTransform(
config, display, view)`, `ocio::convert(config, src, dst)` and
`ocio::exponent(gamma)` each build a CPU processor, bake it once (F16,
because F32 textures are not linearly filterable on Apple GPUs), hold
the bake as a texture in the `lut` slot, and apply it through a recipe
whose `content` slot is the layer being transformed and is left to the
renderer. A bad config fails soft to a material with an empty `lut` slot
and the error reported. In a build that found no OpenColorIO the feature
still links: `ocio::available()` is false and every factory answers that
empty material, and `SIGILMATERIAL_ENABLE_OCIO` says which build this is.

**Which recipe depends on whether the transform mixes channels.** A
transform whose channels are INDEPENDENT — an exponent, a gamma, a
contrast, a per-channel display curve — carries no more information than
one response curve per channel, so it bakes to one row of 256 samples
and applies through `responseRecipe()`; a transform that mixes channels
needs the volume and applies through the trilinear `lutRecipe()`, its
slices laid side by side in one image. Independence is ESTABLISHED, not
assumed from the transform's type: the bake reads the three responses off
the grey ramp, then requires a lattice of mixed colours to equal those
three responses composed, to within half an eight-bit code. So `lutSize`
means nothing to a transform that bakes to a row.

**A channelwise recipe does not have to run as a program.**
`Recipe::channelwise(slot)` is the declaration that every output channel
depends on the same input channel and nothing else, with `slot` holding
the response row — and `responseRecipe()` makes it. `skia::Effect::recipe(
material, surface)` is where it is spent: on a surface carrying eight
bits per channel it answers the row as `SkColorFilters::TableARGB`, which
a consumer hangs on a paint and pays a blit for, and on anything else —
a float surface, or `kUnknown_SkColorType`, which is what a canvas backed
by neither raster nor GPU answers — it falls to `recipe(material)` and
the program. The picture is the same either way; only the cost differs,
which is why the surface is a parameter rather than something the effect
guesses. `skia::Effect::filter` takes an `sk_sp<SkColorFilter>` as well
as an `sk_sp<SkImageFilter>`, and `colorFilter()` reads that lane back.

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

A Tile is not a fill and neither is a compose `Pattern`: `Element::fill`
deletes both overloads so the error names the rule. The bake is the
identity, so a Pattern minted inside a describe is a fresh state with no
bake in it and re-renders its tile every frame — hold the Pattern where
assets are held and fill with `pattern.material()`.

**field.** `halftoneRamp` swells a staggered dot grid down the box and
reads the resolution; `noise` is Skia's Perlin generator behind a
pass-through recipe, so it fills a slot and compares by its parameters;
`grain` is value-noise fBm collapsed to one channel, one recipe per
octave count because the count is a constant in the body; `ripple`
resamples its `content` child through a sine displacement; `crtOverlay`
is the tube laid over a picture — hard scanlines and a corner falloff, in
black, with the alpha carrying both — and reads the resolution.

## The Skia paint

`skia::Paint` is the model as ONE `sk_sp<SkShader>`. A small tree of
paint nodes — a solid, an N-stop `linear`/`radial`/`conical`/`sweep`
ramp, an `image` or a caller-owned `buffer`, a raw `sksl` effect, a
`blend` stack, or `recipe` over a `Material` instance — that compiles to
a single shader through nested `SkShaders::Blend`, never a stack of
saveLayers. Its children nest and still compile to one shader.

**A paint declares its own volatility, and the declaration is what it
READS.** Three tiers, and nothing chooses between them by hand:

- STATIC — a solid, a ramp, an image, a blend of those, or an `sksl`
  effect with only constant uniforms. It resolves eagerly, so
  `isSolid()`/`solidColor()` or `staticShader()` answer with no frame at
  all and a consumer caches and prunes it like any other value.
- GEOMETRY — an effect declaring `uResolution`, or a `worldSpace()` flag.
  It depends on the box, not on the clock: `geometryDependent()` is true,
  and `shaderFor(frame)` answers against the box the frame names.
- LIVE — an effect with a uniform bound to an `Output`, or one reading
  `uTime` or `uContentScale`. `isAnimated()` is true and the paint is
  rebuilt every draw; a live CHILD or blend layer makes its parent live,
  which is what stops a cache from freezing the parameter.

**A TABLE AND A SECOND SOURCE ARE BOTH DOORS ON `sksl`.**
`child(name, Paint)` fills a `uniform shader NAME` slot with another whole
paint — an index texture, a mask, a noise field, a second gradient — and
`uniform(name, std::vector<float>)` fills a declared array, matched
against its TOTAL float count, so 1024 floats fill `float4 uPal[256]` and
a count that is not the declaration's is refused whole rather than
written partly. `uniform(name, shared_ptr<const UniformBlock>)` is the
live form of the same array, re-read every paint. Together they are what
a FIXED PALETTE needs: the picture is one channel of indices and the
table is one uniform array, or — when the lookup is dynamic, which is the
usual case, since the index is a pixel value — one 256 x 1 child image
sampled nearest at the texel centre. Neither door asks for a variant
baked per palette. A child rides the volatility tier and the prune
signature: a live child makes the parent live, and two paints with
different children never compare equal.

**A PASS body is not a shader of its own.** A material handed to a text
runtime's pass is written against declarations that runtime prepends once
it knows the track's unit count — `uContent`, `uUnitRect[N]`,
`uUnitPhase[N]`, `kUnitCount` — so compiling it standalone names four
things that do not exist yet and reports one error per mention, about a
compile nobody asked for. `Paint::recipe` recognises such a body by those
names (`skia::detail::isPassBody`) and builds no static shader for it:
the picture comes from `resolvePass`, and used as an ordinary fill the
material draws nothing rather than failing loudly at load.

`PaintFrame` is what one draw supplies and no author sets: the box, the
root's size, the box→root matrix, the clock and the device scale. A
`worldSpace()` paint anchors to that matrix — the field is authored once
against the root and every flagged box samples it where it actually sits,
through its own transform — and with an identity matrix it degrades to
box-local rather than answering wrongly.

**Equality is the RECIPE, and it is load-bearing.** Two paints built from
the same values compare equal though each minted a fresh `SkShader`,
which is what lets a consumer prune across rebuilds; children and blend
layers ride the signature, because a child left out of it would let a
holder prune while its second source had changed. An `sksl` paint
compares by EFFECT POINTER, so a helper that compiles a fresh
`SkRuntimeEffect` per call never compares equal to itself — compile once
and hold the paint. Every mutation is copy-on-write, so binding on a copy
never reaches the value it was copied from.

**Post-processing is the other half of the same frame.** `skia::Effect`
takes the layer a consumer has already rendered and runs a filter over
it: `filter()` wraps any `SkImageFilter`, `shader()` an SkSL program
whose `content` child IS that layer, `recipe()` a `Material` in the same
position, and `blur()`/`directionalBlur()`/`glow()` are the three named
spatial ones. `phosphorBloom()` is the display post-process: a bright pass
feeds three radii whose RGB channels have different reach, so the feather
changes hue while the sharp source remains on top. `then()` chains effects,
and the same tier rules hold — a bound
uniform or a live child makes the effect live, and a static chain
precomposes once. It resolves against the same `PaintFrame` a paint
does, so a consumer builds one frame per draw and hands it to both.

**FOUR NAMES A BODY MAY NOT DECLARE: `pos`, `inColor`, `destColor`,
`primitiveColor`.** A GPU backend does not compile a runtime effect as a
program of its own — it inlines the body into the pipeline's fragment
shader as a helper whose parameters it names itself, those four, and
discards the names the body's own `main` declared, rewriting references
to them as those. So a body declaring anything else by one of those
names redeclares a parameter. `SkRuntimeEffect::MakeForShader` cannot
see it, because there the body IS the whole program and the name is
free, and every raster suite compiles that way; on a device it is a
pipeline that never builds, a draw that paints nothing and a compiler's
complaint per frame. The compile refuses those declarations up front,
naming the recipe, and `main`'s own parameter is the one place the name
is allowed — it is the declaration the backend replaces.

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
list opens nothing. `material::warmup(requests)` folds identical recipe,
target and variant keys and compiles distinct keys concurrently;
`material::warmup(materials, target, variant)` is the catalogue-shaped
spelling. A request arriving while the same key is compiling shares that
in-flight result, and the cache's synchronization remains an implementation
detail. A registered compiler can therefore receive concurrent calls for
different keys; a backend with thread-affine work must marshal that work at
its own executor seam.

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
asset is a source. SigilIO owns resource access and SigilImage owns
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

```sh
ctest --test-dir build -C Release -R material
python3 scripts/bench_ledger.py --benches material_color_bench \
    material_core_bench material_texture_bench material_ocio_bench \
    material_sdf_bench material_pattern_bench material_field_bench \
    material_skia_bench material_kit_bench material_slang_bench
```

A case here asserts one thing this library promises through its public
headers and is named that promise as a sentence, so a failure line reads
as the claim that broke. It pins only what editing this library could
falsify — a reflected layout against the struct's own `offsetof`, a
compile count, a closed form, one material shaded two ways — never a
byte layout the compiler chose, an anti-aliased pixel, a fitted tolerance
or elapsed time: pixel identity is the plate ledger's to judge and timing
is the bench ledger's. A claim made N times with one thing varying is one
`TEST_P` whose parameter is that thing, with a name per row — the shading
terms against their closed forms, the reserved parameter names a body may
not redeclare, and the file names the texture tools write.

**A binary exists where it links a strictly smaller set of targets than
its neighbours and that boundary is a promise somebody could read**; two
binaries over one closure are one binary. That is why there are six here
and not eleven: the colour leaf, the signed-distance surfaces, the tiles,
the fields and the baked view transforms are five features, and a test of
any of them shades through the Skia backend, so `material_test` is one
binary over the five. The features themselves are still five archives —
a consumer of the colour leaf links the colour leaf alone, and that is
what the library promises.

| binary | what it proves | label |
|---|---|---|
| `material_core_test` | the value model, with no renderer in reach | — |
| `material_test` | the primitives, the colour leaf, the view transforms | `ocio` |
| `material_texture_test` | the image side | — |
| `material_kit_test` | the presets and the shading terms | — |
| `material_skia_test` | the SkSL backend | — |
| `material_slang_test` | the Slang backend, with no device | — |
| `material_gpu_test` | every body this library ships, on a device | `gpu` |

`material_core_test` links the core alone, so a link edge that pulled a
renderer into the model would fail there. It covers params reflection —
including that the schema IS the params struct's own layout, read off
`offsetof` rather than off the numbers this compiler happened to choose —
recipe identity against definition equality, the program cache's keys, a
compile held open until every concurrent request has arrived so the fold
is asked without a clock, the field it names once when a compiled body
never reads it, material equality, bindings, children and tiers, what
`over()` stacks, and `UniformBlock` revisioning.

`material_test` covers the primitives and the leaf beneath them: the
colour value's transfer function and OKLab round trips and its
perceptual midpoint, the SDF surfaces, the tile mechanism and the stock
generators over it, the fields, and the OpenColorIO bake — an exponent
baked to a response row, that row lowered to a table an eight-bit
surface admits, holding it to what the program paints across a whole
ramp, while a float surface and a channel-mixing transform keep the
program. The view-transform cases skip where the transforms are
unavailable, which is what the `ocio` label says; a config that cannot
be read failing soft is asked unconditionally, because that needs no
OpenColorIO to ask.

`material_texture_test` covers the image side: the sources and their
identity across the erasure, the sampling dials, the environment map, the
bevel producer, the atlas readers and packer, and the tools' file names —
one row per name, so a failure says which tool's spelling moved rather
than that a list changed. `material_skia_test` compiles a two-uniform
recipe through the cache and checks the raster it shades is byte
identical to the same SkSL compiled and filled by hand, and states the
four parameter names a body may not redeclare together with the three
spellings that must still compile. `material_kit_test` compiles every
preset and checks a fill stays inside its path, dresses a surface from a
decoded set, shades a stack at both ends of its mask, and holds every
shading term to its closed form.

`material_gpu_test` belongs to the whole library rather than to a
feature: every other suite shades on a raster surface, where a body is
compiled as its own SkSL program, and a body can pass that and fail once
a GPU backend has inlined it into a pipeline. It stands Graphite up,
installs a shader-error handler through
`GraphiteContext::reportShaderErrorsTo`, and draws every material
`kit::everyRecipe()`, `sdf::everyRecipe()` and `field::everyRecipe()`
answer — plus a stack per blend, the whole terms text, and the ocio bake
where OpenColorIO is available — through the same `skia::Paint` a
consumer draws it through, demanding that not one reports an error. It
is labelled `gpu` and needs Metal, and it carries its own control: the
collision the reserved names exist to prevent, built as a raw runtime
effect so it reaches the device, must be reported — which is what proves
the handler is wired to anything at all. Run it with

```sh
ctest --test-dir build -C Release -R material_gpu_test
```

One file per subject, named for what it asserts. **The fixtures more than
one file needs live once, in `test/support/`**: `Shade.h` holds the two
ways of drawing a material — the shader over a whole surface, which asks
what a body computes at each point, and a fill over a path, which asks
what a caller painting a shape gets — beside the readings taken off the
result. A directory a case writes into is `sigil::test::ScratchDir` from
the tree-wide `src/test/`, keyed by process id and emptied both ways, so
two runs side by side never read each other's files.

The acceptance pieces are the
`material_lab`, `material_atlas`, `material_child`, `stock_materials`,
`text_paints`, `reflection_lab`, `env_faces`, `env_lanes`, `env_theme`,
`shapeworks_lab` and `mesh_normal_bridge` sketches under
`src/sketch/sketches/`, whose surfaces are shaded here. SigilCompose is the largest consumer: its
`Material::recipe` resolves a material through this library's cache with
the frame built from its paint context, and its patterns, SDF fills,
layer styles and view transforms are the primitives and presets here
spelled as compose values.
