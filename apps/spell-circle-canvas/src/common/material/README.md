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
and frame sequences. The **kit** holds the stock recipes: gold, chrome and
glass over a normal map and an environment.

The core links glm (for the vector types a struct may hold), choreograph
(for the animation output a field may bind to) and Boost.PFR (for the
reflection that reads a struct's field names off the type). The core has
no renderer in it: compilers arrive from backend features, and there is
one, for Skia's SkSL.

Namespace `sigil::material`. Four feature libraries, one per directory,
each a static archive that links only what sits beneath it:

| target | holds | links |
|--------|-------|-------|
| `SigilMaterialCore` | the value model: `Target`, `Params`, `Recipe`, `Program` and the cache, `Material`, `Leaf`, `UniformBlock`, `FrameData`, `Color` | SigilGeometryPath, SigilMotionBind, Boost::pfr |
| `SigilMaterialTexture` | `Texture` and its sources, `textures::` (the tools' sets by role), `Environment` and `bevelNormals`, `Atlas` | SigilMaterialCore, SigilImageAsset, Skia; simdjson and stb privately |
| `SigilMaterialSkia` | the SkSL compiler and `SkiaProgram`, whose builder uploads resolved bytes; `skia::shader` binding textures into slots; `skia::fill` | SigilMaterialTexture |
| `SigilMaterialKit` | the stock recipes: `kit::gold`, `kit::chrome`, `kit::glass` and their params | SigilMaterialTexture |

`SigilMaterial` is the umbrella, an interface over all four. Headers live
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
names in the struct and cannot drift.

**A recipe's identity is the object.** Two recipes built from the same
text are two definitions with two sets of programs; `operator==` compares
definitions and is for tests, while the program cache and a material's
equality use the pointer. Define a recipe once and hold it in a
`shared_ptr<const Recipe>` beside the code that owns it.

**One body per target, and asking for a missing one is an error once.**
`Recipe::body(Target, source)` stores the body for a language;
`Recipe::source(target)` is the generated declarations followed by it. A
material resolved for a target its recipe has no body for — or one no
compiler is registered for, or one whose body fails to compile — yields a
null program, and the cache reports it to stderr exactly once per (recipe,
target), naming both, so the mistake surfaces at the first describe rather
than scrolling past every frame.

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
type's own equality) and says whether it moves between frames. `Texture`
is the one leaf type; the Skia backend recognises it and binds its image
shader. A slot holds a material or a leaf, never both, and
`Material::child(name)` and `Material::leaf(name)` each answer null for
the other kind.

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

## The kit

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
float4. `Color.h` also holds the sRGB transfer function both ways and the
OKLab round trip — `toOklab`, `fromOklab`, `lerpOklab` — which every
perceptual interpolation in the codebase runs through.

## Boundaries

The core links no renderer; the texture feature links Skia because a
texture IS a Skia image with its sampling, and SigilImage because an
asset is a source. SigilLoader owns resource access and SigilImage owns
image meaning, so this library decodes nothing and opens no file — every
door that needs pixels takes them or takes a decoder. SigilGeometry draws
the normals passes and outlines a surface is shaded over and links
nothing here; SigilWorld consumes the texture-set vocabulary and keeps
its own slot rules; SigilCompose places what a material paints.

## Building and testing

Four tests and four benchmarks, one pair per feature:

```sh
ctest --test-dir build -C Debug -R material
python3 scripts/bench_ledger.py --benches material_core_bench \
    material_texture_bench material_skia_bench material_kit_bench
```

`material_core_test` links the core alone, so a link edge that pulled a
renderer into the model would fail there. `material_texture_test` covers
the sources, the sampling dials, the tools' names, the environment and
bevel producers and the atlas readers and packer. `material_skia_test`
compiles a two-uniform recipe through the cache and checks the raster it
shades is byte-identical to the same SkSL compiled and filled by hand.
`material_kit_test` compiles every stock surface and checks a fill stays
inside its path. Three programs are the acceptance pieces: the
`shapeworks_lab` and `easel_playground` sketches under
`compose/sketch/sketches/`, and `geometry_demo`, whose surface panels are
shaded here.
