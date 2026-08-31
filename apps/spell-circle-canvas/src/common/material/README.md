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
feature each: **colour** (the colour value, the OKLab round trip, and
OpenColorIO view transforms baked to LUT materials), **sdf** (shape,
border, glow and shadow in one pass over a signed distance), **pattern**
(a tile baked once with a mapping and an explicit reseed, and the stock
tiles over it), and **field** (the halftone ramp, Perlin noise, luminance
grain, the ripple). The **kit** holds PRESETS — functions that fix
colours, proportions or a named style over the primitives: the
metallic-roughness surface and the masks that stack it; gold, chrome
and glass over a normal map and an environment; the girih panel and its
palettes; the gel and chrome colour tables; the six text paints and the
chrome-type ramps.

The core links glm (for the vector types a struct may hold), choreograph
(for the animation output a field may bind to) and Boost.PFR (for the
reflection that reads a struct's field names off the type). The core has
no renderer in it: compilers arrive from backend features. One ships in
this library, for Skia's SkSL; `Target::Slang` is compiled by whichever
3D renderer speaks it, which registers itself the same way.

Namespace `sigil::material`. Eight feature libraries, one per directory,
each a static archive that links only what sits beneath it:

| target | holds | links |
|--------|-------|-------|
| `SigilMaterialCore` | the value model: `Target`, `Params`, `Recipe`, `Program` and the cache, `Material`, `Leaf`, `UniformBlock`, `FrameData`; and `over()`, the combinator that stacks one material on another through a mask | SigilGeometryPath, SigilMotionBind, Boost::pfr |
| `SigilMaterialTexture` | `Texture` and its sources, `ShaderLeaf`, `textures::` (the tools' sets by role), `Environment` and `bevelNormals`, `Atlas` | SigilMaterialCore, SigilImageAsset, Skia; simdjson and stb privately |
| `SigilMaterialColor` | `Color` (header-only, which the core's `Params.h` includes) and `color::` — the OCIO `viewTransform`, `convert`, `exponent` as LUT materials | SigilMaterialTexture; OpenColorIO privately, when found |
| `SigilMaterialSdf` | `sdf::` — `Shape`, `Style`, `pad`, `material` | SigilMaterialCore |
| `SigilMaterialPattern` | `pattern::Tile` and the stock tiles | SigilMaterialTexture |
| `SigilMaterialField` | `field::` — `halftoneRamp`, `noise`, `grain`, `ripple` | SigilMaterialTexture |
| `SigilMaterialSkia` | the SkSL compiler and `SkiaProgram`, whose builder uploads resolved bytes; `skia::builder` and `skia::shader` binding leaves into slots; `skia::fill` | SigilMaterialTexture |
| `SigilMaterialKit` | the presets: the metallic-roughness `kit::surface` and `kit::unlit` and the masks that stack them; `kit::gold`, `kit::chrome`, `kit::glass`; `kit::girih8` and its palettes; the gel and chrome tables; the text paints and chrome-type ramps | SigilMaterialPattern, SigilMaterialColor |

`SigilMaterial` is the umbrella, an interface over all eight. Headers live
under `include/sigilmaterial/<feature>/` and are spelled that way —
`<sigilmaterial/core/Recipe.h>`, `<sigilmaterial/texture/Texture.h>`,
`<sigilmaterial/kit/Surfaces.h>` — and `<sigilmaterial/Material.h>`
includes the whole core.

## Using it

```cpp
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

// The definition, made once and shared. The body is what follows the
// generated declarations — the uniforms above, then uTime, then the
// child slot.
auto glow = std::make_shared<const Recipe>(
    Recipe::of<Glow>("glow")
        .frame(FrameInput::Time)
        .child("uSrc")
        .body(Target::SkSL, R"(
          half4 main(float2 p) {
            half4 src = uSrc.eval(p);
            return src * half4(uTint) * half(uScale + 0.5 * sin(uTime));
          })"));

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
#include <sigilmaterial/kit/Surfaces.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/texture/Surface.h>

const Environment studio = Environment::studio();
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

**A field no body reads is not in the ABI, and the cache says so.** A
shader compiler discards a uniform its body never mentions, and the upload
then skips that field: every value the material writes there — a constant,
a bound output, a whole table — reaches nothing. The program cache compares
the params against the program it just compiled and names the recipe and
each unread field on stderr, once per (recipe, target), beside the reports
for a missing body and a failed compile. `Program::keeps(name)` is what it
asks; a backend whose compiler drops nothing leaves it at yes. A field a
body reads on one target and not on another is named for the target that
drops it, which is the honest answer: on that target the field is dead.

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
 A
material resolved for a target its recipe has no body for — or one no
compiler is registered for, or one whose body fails to compile — yields a
null program, and the cache reports it to stderr exactly once per (recipe,
target), naming both, so the mistake surfaces at the first describe rather
than scrolling past every frame. A body that compiles but leaves a params
field unread is reported the same way.

**One program cache.** `ProgramCache::shared()` holds every compiled
program in the process, keyed by (recipe identity, target, variant). A
backend registers its compiler with `registerCompiler(Target, Compiler)`
and the cache compiles on first use. `Variant` is a small ordered key the
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

**Texture sets are the tools' folders.** `textures::classify` reads a
file name into a `Role` (base colour, normal, roughness, metallic,
occlusion, emissive, packed occlusion-roughness-metallic, height,
opacity, specular), the set it belongs to and whether a normal map is
DirectX-convention; `discover` groups a directory into `TextureSet`s;
`fromFiles(set, decoder)` and `fromUsageMap(images)` decode into
`TextureMaps`, one repeating texture per role. The library opens no file:
a `Decoder` returns an image for a path, and the caller supplies it. What
a set MEANS to a renderer — which channel of a packed image feeds which
slot — is the renderer's rule, not this library's.

**A surface is shaded from two textures.** `Environment` is an
equirectangular panorama (u = azimuth, v = 0 at the zenith) with
roughness blurs cached per bucket; `studio()` and `sunset()` bake one
with no assets, `fromEquirect()` wraps a loaded panorama, and
`texture(roughness)` is the level a recipe's environment slot takes,
repeating in azimuth and clamped at the poles. `bevelNormals(path,
bevelPx)` blurs the outline's coverage into a height ramp, differentiates
it, and encodes device-space normals (+y down, +z toward the viewer) as
rgb = n * 0.5 + 0.5, flat across the interior and tilted along the rim —
placed at the outline's bounds so device xy reads the normal beneath it.
A normals pass a 3D painter rasterizes uses the same encoding and feeds
the same slot.

**An atlas is a sheet, its regions and its sequences.** `Atlas::grid`
cuts equal cells; `fromTexturePacker` and `fromAseprite` read the JSON
those tools write (hash or array form; trimmed sprites keep their source
size and offset), deriving a sequence per name stem for TexturePacker
(`walk_01`, `walk_02` become "walk") and per frame tag for Aseprite;
`pack(images)` lays loose images into one power-of-two sheet.
`region(name)` is the sheet texture cut to that region; `frame(sequence,
index)` wraps past the end.

## Stacking

**`over(base, top, mask, blend)` is a material.** The three operands
become its children, so the stack compares, animates and resolves as one
value, and applying `over` again builds a taller one. The MASK is any
material whose red channel is read as a scalar; `blend` is `Mix`, `Add`
or `Multiply`, one recipe each so a body carries no branch. `under(m)`
is the material a stack stands on — one step down, so walking it reaches
the bottom — and `stackDepth(m)` counts the steps. A consumer that can
only express one material (`UsdPreviewSurface`, say) writes the bottom
and records the depth.

## The kit

The kit is presets: functions that fix a colour, a proportion or a named
style over the primitives. `kit::girih8` is the 8-fold star-and-cross
panel as a `Tile`, with `fezPalette()` and `nasridPalette()`. The gel and
chrome tables — `aquaBodyRamp`, `aquaGlowRamp`, `chromeRamp`, the
`AquaGelOptions` and `ChromeOptions` a renderer's bundles read — are
`RampStop` lists a renderer turns into its own gradient. The text paints
— `water`, `meshGradient`, `sparkle`, `starNest`, `clouds`, `tunnel` —
share the `TextPaintParams` ABI of a run's origin and extent, the clock
and a slow motion vector; `sunsetChromeText()` and `silverChromeText()`
are the chrome-type ramps in unit space.

**The metallic-roughness surface** is `kit::SurfaceParams` — base
colour, metallic, roughness, emission, the normal convention, the channel
each packed map is read from, the cutout threshold and the glass terms —
under two recipes over the same ABI: `kit::surface()` takes light,
`kit::unlit()` is its own light. Seven child slots, one per role
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
evaluation of the model — and `unlit()` shades the albedo alone. A
renderer that HAS the surface attributes reads the same params and slots:
the 3D one puts its own lighting around what these return, so the
difference between the two recipes there is whether the emitters reach
the result at all.

A Slang body writes out the intrinsics whose two targets are two
different pieces of code — a `lerp`, a `dot`, a `smoothstep` — because an
intrinsic is where one source stops producing one answer.

**Masks say where.** `kit::maskConstant` is a number; `kit::maskMap`
reads a channel of a texture; `kit::maskVertexColor`, `kit::maskSlope`
and `kit::maskHeight` read a channel, a tangent normal dotted with an
axis, or a value dotted with an axis, from whatever texture the renderer
supplies as the source. All of them then fit — `low` and `high` remap the
raw value onto 0..1 and clamp, and `kit::invert` flips it — which is why
the slope and height factories take the range: without one those masks
mean nothing. `kit::fit` moves the range on an existing mask.

`kit::gold`, `kit::chrome` and `kit::glass` are recipes over two slots,
`normals` and `env` (glass adds `backdrop`, an image of what sits behind
the shape in the same device coordinates). Each params struct's fields
are the body's uniforms by name, with two exceptions the comments state:
`roughness` picks the environment level when the material is built, and
`envSize` is filled by the builder. Real reflection models sampled per
pixel: gold adds foil crinkle and glints, chrome the contrast curve and
brushed anisotropy, glass refracts the backdrop through the normal field
with a fresnel-weighted reflection on top.

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

**A view transform is a LUT material with one open slot.** OpenColorIO's
GPU codegen never emits SkSL, so `color::viewTransform(config, display,
view)`, `color::convert(config, src, dst)` and `color::exponent(gamma)`
each build a CPU processor, bake it into a 3D LUT once (F16, because F32
textures are not linearly filterable on Apple GPUs), hold the LUT as a
texture in the `lut` slot, and apply it through the trilinear
`lutRecipe()`. The `content` slot is the layer being transformed and is
left to the renderer. A bad config fails soft to a material with an empty
LUT slot and the error reported. Compiled only under
`SIGILMATERIAL_ENABLE_OCIO`.

## The primitives

**sdf.** `sdf::material(shape, style)` is shape, border, glow and soft
shadow in ONE pass over a signed distance — `roundBox`, `circle` or
`star` — with every style parameter a uniform, so a pulsing border is a
bound `uBorderW` and however many styles there are, three programs
compile. Distances are in pixel space over the resolution the frame
supplies, never uv, so borders stay even on a stretched box. The style's
outer treatments reserve `pad(style)` inside the box; size a box with
`minBoxFor(style, contentPx)` or the reserve eats the interior.

**pattern.** A `Tile` is one bake plus a mapping. The program draws one
seamless tile at a seed; the bake is memoised on shared state, `seed(n)`
and `program()` copy-on-write that state and drop it, and `scale`,
`rotate`, `offset` and `filter` act on the sampling matrix alone, so a
rotated repeat stays seamless with no rebake. The bake is the identity:
hold a Tile where assets are held. `texture()` is the bake repeating on
both axes through the mapping. The stock tiles — `halftone`, `stripes`,
`sequence`, `checker`, `gridLines`, `speckle` — are programs over it.

**field.** `halftoneRamp` swells a staggered dot grid down the box and
reads the resolution; `noise` is Skia's Perlin generator behind a
pass-through recipe, so it fills a slot and compares by its parameters;
`grain` is value-noise fBm collapsed to one channel, one recipe per
octave count because the count is a constant in the body; `ripple`
resamples its `content` child through a sine displacement.

## Boundaries

The core links no renderer; the texture feature links Skia because a
texture IS a Skia image with its sampling, and SigilImage because an
asset is a source. SigilLoader owns resource access and SigilImage owns
image meaning, so this library decodes nothing and opens no file — every
door that needs pixels takes them or takes a decoder. SigilGeometry draws
the normals passes and outlines a surface is shaded over and links
nothing here; SigilWorld's renderer is one executor of the surface the
kit defines and adds no shading model of its own; SigilCompose places
what a material paints.

## Building and testing

Eight tests and eight benchmarks, one pair per feature:

```sh
ctest --test-dir build -C Debug -R material
python3 scripts/bench_ledger.py --benches material_core_bench \
    material_texture_bench material_color_bench material_sdf_bench \
    material_pattern_bench material_field_bench material_skia_bench \
    material_kit_bench
```

`material_core_test` links the core alone, so a link edge that pulled a
renderer into the model would fail there. `material_texture_test` covers
the sources, the sampling dials, the tools' names, the environment and
bevel producers and the atlas readers and packer. `material_skia_test`
compiles a two-uniform recipe through the cache and checks the raster it
shades is byte-identical to the same SkSL compiled and filled by hand.
`material_sdf_test`, `material_pattern_test`, `material_field_test` and
`material_color_test` cover the primitives; `material_kit_test` compiles
every preset and checks a fill stays inside its path, dresses a surface
from a decoded set and pins the packed channels it wires, and shades a
stack at both ends of its mask. `material_core_test` pins what `over()`
builds: the operands as children, a blend per recipe, and the walk down
to the bottom of a stack. Three programs are the acceptance pieces: the
`shapeworks_lab` and `easel_playground` sketches under
`compose/sketch/sketches/`, and `geometry_demo`, whose
surface panels are shaded here. SigilCompose is the largest consumer: its
`Material::recipe` resolves a material through this library's cache with
the frame built from its paint context, and its patterns, SDF fills,
layer styles and view transforms are the primitives and presets here
spelled as compose values.
