# SigilMaterial

A **material** is what a region or a surface looks like, as one value
built up by composition: a **base** (a colour, a gradient, an image, a
noise, a program, or another material), a stack of **layers** each
blended over the ones beneath it through an opacity and an optional mask,
an optional lit **surface** response — shaded by a 3D renderer's lights,
and in 2D by the lighting a scene states — and an optional
**effects** stage — a filter chain over the painted layer's coverage.
The same value fills a Compose box, inks its text, strokes a boundary and
dresses a World body; the library ships the builder and its primitives,
and no named looks — a look belongs to the sketch that uses it.

### Tier 1 — the builder and its bases

```cpp
#include <sigilmaterial/core/Material.h>    // Material, from, LayerOptions, Mask, SurfaceOptions
#include <sigilmaterial/core/Lighting.h>    // studio, environment, Lighting
#include <sigilmaterial/field/Field.h>      // noise
#include <sigilmaterial/filter/Filter.h>    // Filter
#include <sigilmaterial/paint/Bases.h>      // linearGradient, radialGradient, conicGradient
#include <sigilmaterial/program/Shader.h>   // shader, placeholder
#include <sigilmaterial/texture/Image.h>    // image

namespace material = sigil::material;
using material::BlendMode, material::Filter;

// Bases: each IS a Material, and a Color converts implicitly.
material::Material steel = material::from(material::hexColor(0xB8BDC4))
    .layer(material::noise(0.4f, {.grain = true}), {.blend = BlendMode::Multiply, .opacity = 0.3f})
    .layer(material::linearGradient({0, 0}, {0, 1}, {white, clear}),
           {.blend = BlendMode::Screen, .mask = material::Mask{.source = lens}})
    .surface({.metallic = 1.0f, .roughness = 0.2f})
    .effects(Filter::shadow(black, {.blur = 8, .offset = {0, 4}})
                 .then(Filter::stroke(rim, {.width = 1})));

material::image(pixels, {.repeat = material::Repeat::Repeat});      // a media::PixelSource

// A shader: its source and a struct whose fields are its uniforms.
struct Ember { float heat = 0.5f; material::Color ink = {1, 0.4f, 0.1f, 1}; };
material::Material ember = material::shader(kEmberSkSL, Ember{0.6f});
ember.set("heat", 0.8f).bind("heat", flicker);                        // its fields, by name
material::shader(hub, "res://ember.sksl", Ember{});                   // the same body kept in a file

// The designated form, through from(): the same value in one pair of braces.
material::Material steelToo = material::from({
    .base = material::hexColor(0xB8BDC4),
    .layers = {{material::noise(0.4f), {.blend = BlendMode::Multiply}}},
    .surface = material::SurfaceOptions{.metallic = 1.0f},
    .effects = Filter::shadow(black, {.blur = 8, .offset = {0, 4}})});
```

Where it goes: `box().fill(steel)`, `text(…).ink(steel)`,
`box().stroke(steel, {.width = 2})` in Compose, where the effects dress
the node's own layer (shadows beneath the fill, strokes and bevels over
it, a hard shadow as an echo of the fill and the text);
a World element's `fill(steel)`, which reads the base and the
surface and ignores the effects; `material::skia::paint(steel)` for a raw
canvas. Two materials built the same way compare equal, so a re-described
node prunes.

**The effects are an image editor's layer styles.** Each reads the
coverage of what the material fills: `Filter::shadow(c, {.inside = true})`
is an inner shadow, cast along its offset and hugging the opposite inner
edge; `Filter::shadow(c, {.blur, .spread})` with no offset is an outer
glow; `Filter::bevel({.depth, .size, .angleDegrees, .highlight,
.shadow})` is the lit edge and the shaded one; `Filter::stroke` is a
keyline. No consumer draws these as marks of its own: a look that wants
one fills with a material that carries it.

**A shader is one line.** `material::shader(source, Parameters{…})` is a
material whose base is the body `source` over the uniforms the struct
declares — its fields, by name, in declaration order — so it fills, inks,
strokes and layers like any other base. The source is SkSL unless
`.target` says Slang (drawn where a renderer compiles Slang); two shaders
of one source are one definition, so a re-described node prunes, and the
name a message calls it is derived from the source unless `.key` gives
one. Its fields are written and followed exactly as a Substance graph's
inputs are: `set(name, value)` writes one and `bind(name, animatable)`
follows a `motion::Animatable<float>` — the struct itself holds plain
values, because its memory IS the upload. The spelling takes the source
first and the struct second; `material::shader(source).parameters(P{…})`
is not it, because the struct's type decides the definition and a
builder would stand a material with no uniforms between the two calls,
and neither is `material::program<P>(…)`, because a program is the
compiled form a renderer holds, not what an author writes.

### A lit surface in 2D

```cpp
// The looks live in the sketch that wears them.
/** Gold leaf: a warm metal, burnished, with a beaten relief. */
inline material::Material goldLeaf(media::PixelSource beaten) {
  return material::from(material::hexColor(0xC9A45C))
      .surface({.metallic = 1.0f, .roughness = 0.3f,
                .normal = material::image(beaten, {.repeat = material::Repeat::Repeat})});
}
/** An embossed plate: a cool body whose stamped relief catches the light. */
inline material::Material embossedPlate(media::PixelSource stamp) {
  return material::from(material::linearGradient({0, 0}, {0, 1}, {plateTop, plateFoot}))
      .layer(material::noise(3.5f, {.octaves = 2, .contrast = 0.6f}),
             {.blend = BlendMode::Multiply, .opacity = 0.18f})
      .surface({.roughness = 0.45f, .normal = material::image(stamp)});
}

motion::Animatable<float> sun = motion::animatable(0.0f);   // the sketch turns it each frame
stack().lighting(material::studio({.direction = sun, .elevation = 35.0f}))
    .children({box().fill(embossedPlate(stamp)),
               text(u8"GILT").ink(goldLeaf(beaten)),
               box().stroke(goldLeaf(beaten), {.width = 6})});
```

A material whose `surface()` states a response is **lit in 2D** where it
fills a Compose box, inks a line of type or strokes a boundary: its colour
stack is lowered once, exactly as it paints flat, and a lighting pass over
it reads the normal map for relief and the roughness, metallic, occlusion
and emission channels for the rest. What lights it is the scene's
**`lighting`**, an inherited Compose property set once on a parent or the
page — `material::studio({.direction, .elevation, .color, .intensity,
.ambient})` for a directional key light (direction in degrees
counter-clockwise from three o'clock, where the light comes from;
elevation above the page), `material::environment(pixels, {.rotation})`
for a latitude-longitude picture the surface reflects and takes its
ambient colour from, or a `material::Lighting` holding both. A surface's
own `.surface({.lighting = …})` stands over the scene's. With no lighting
in force a lit surface is painted flat, as its colours. Every angle and
strength takes an `Animatable<float>`: a bound light re-runs only the
lighting pass each frame, so a relief turns while the colours beneath are
never painted again. The effects stage still reads coverage, and a World
mesh shades the same surface under its own lights and ignores the effects.
`material::skia::lit(material, lighting)` is the pass on a raw canvas.

### Writing a shader

```cpp
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/texture/Texture.h>   // Sampling

// The struct IS the uniform block: every field a float, a glm::vec2, a
// glm::vec4, a Color or a std::array<float, N>, declared to the body under
// its own name.
struct Ripple {
  material::Color ink = {0.2f, 0.5f, 0.9f, 1};
  float rings = 12;
  float speed = 1.5f;
};

constexpr std::string_view kRipple = R"(
half4 main(float2 p) {
  float2 uv = p / uResolution;
  float wave = 0.5 + 0.5 * sin(length(uv - 0.5) * rings * 6.2831 - uTime * speed);
  return half4(ink.rgb * wave * ink.a, ink.a);
}
)";

material::Material ripple = material::shader(kRipple, Ripple{});
ripple.bind("speed", tempo);                           // a motion::Animatable<float>
box().fill(material::from(ripple).layer(material::noise(0.6f),
                                        {.blend = BlendMode::Overlay, .opacity = 0.2f}));

// A file, live-coded: the last text that compiled paints while an edit
// is broken, the placeholder while none has, and what is wrong stands on
// hub.problems() under the URI.
material::Material aurora = material::shader(hub, "res://aurora.sksl", Ripple{});

// A picture the body samples, by the name it reads it as.
material::Material lens = material::shader(
    "half4 main(float2 p) { return photo.eval(p * 0.5); }",
    {.key = "lens", .sampling = material::Sampling::Nearest,
     .textures = {{"photo", pixels}}});
```

The body is written and the declarations are generated. In SkSL a body is
`half4 main(float2 p)` returning PREMULTIPLIED colour at the point `p`, in
the pixels of the box it fills; in Slang it is `float4 surface(float2 uv)`
returning straight colour, for a renderer that compiles Slang. Before the
body stand one uniform per field of the struct, one sampled texture per
name in `.textures` (`uniform shader NAME` in SkSL, read as
`NAME.eval(p)`), and each frame value the body spells: `uTime` (seconds,
which makes the material live), `uResolution` (the box's pixels),
`uContentScale` and `uWorld` (the box's placement in the root). A field of
the struct named like one of those is the author's own and nothing is
added for it.

A shader is a material like any other: a base, a `layer()` source, a mask,
a surface channel. Its fields are written and followed through the
Material by name — `set("rings", 16.0f)`, `bind("speed", tempo)` for a
`motion::Animatable<float>`, `bind("bars", block)` for a caller's table —
so a bound field re-uploads its bytes each frame and nothing compiles
again, while a layer over it that does not move stays one lowered paint.
One source is one definition however often it is described: a sketch may
call `material::shader` in every describe, and two calls with equal
values compare equal and prune. `material::shader(hub, uri, Parameters{…})`
is the same over a file read through SigilIO, its language told by the
extension; an edited file compiles anew once the hub's poll has seen the
edit and the caller describes again. A SkSL text pass that reads the runtime's
unit uniforms is validated with one unit; the text executor compiles the
same body with the count of units it draws. A file holds three promises:

- **A broken edit never blanks the picture.** A text that does not
  compile does not replace one that did: the newest text that compiled
  keeps painting, and the material compares equal to the one before, so
  the node over it prunes.
- **A file with no program says so on the canvas.** While no text of the
  file has compiled — the file is missing, its first text is broken, its
  extension names no language — the material is `material::placeholder()`,
  a magenta and black checker sixteen pixels a cell. It is a diagnostic
  standing where the program would paint, the one stock material this
  library owns, and never a look.
- **The failure reaches the host.** What is wrong stands on the hub's
  `problems()` under the URI — the compiler's message and the body's own
  line when the compiler names one — and is taken back when a text
  compiles. A host that shows `problems()` where it shows a failed build
  shows a broken shader there; the program cache still writes the
  compiler's message once to the diagnostic stream.

A text is judged when a compiler for its language is registered — the
Skia backend registers on its first use, a device renderer when it
starts — and before that it is drawn as it stands.

What one source cannot say — a body in two languages at once, a slot an
executor fills from the rendered layer through a filter, a channelwise
claim, a bank of seeded instances — is the program model, in
[ADVANCED.md](ADVANCED.md).

### Tier 2 — the options

`GradientOptions` (units, extent, repeat, focus, the conic window),
`ImageOptions` (`repeat`, `repeatY`), `NoiseOptions` (`octaves`, `seed`,
`turbulence`, `grain`, `contrast`, `stretch`), `ShaderOptions` (`key`,
`target`, `sampling`, and the `textures` a body samples by name, each a
`ShaderTexture`), `LayerOptions` (`blend`, `opacity`, `mask`), `Mask`
(`source`, `channel`, `low`, `high`, `invert`), `SurfaceOptions` (every
channel a number or a material: `metallic`, `roughness`, `occlusion`,
`normal`, `emission`, `clearcoat`, `transmission`, `unlit` …), and the
filter options `ShadowOptions` (`blur`, `offset`, `spread`, `inside`),
`StrokeOptions` (`width`, `position`), `BevelOptions`, `BloomOptions`;
and the light: `Light` (`direction`, `elevation`, `color`, `intensity`,
`ambient`, and for a set in three dimensions `kind` — a `LightKind` —
`position`, `range`, `innerAngle`, `outerAngle`), `EnvironmentOptions` (`rotation`, `intensity`, `size`),
`Lighting`.
The pixel sources a layer reads — the `pattern::` tiles, the sdf shapes
as masks — are tier 2 as well.

### Tier 3 — control

A program base is a **recipe** instance. A **recipe** is a material's
definition: a plain C++ struct of uniform-typed fields that is its ABI,
one shader body per shading language, the slots it samples and the
per-frame values it reads. The instance holds the field values, mirrored
as the bytes the shader will receive; live bindings that overwrite fields
every frame; other materials filling its slots; and the settings a
renderer reads off the instance. A renderer asks such a material to
**resolve** against a frame and receives the compiled **program** for its
shading language plus the bytes to upload — the same answer, memoised,
until an input changes. `shader()` builds exactly this: one recipe per
source, with its frame inputs read off the names the body spells. The
program model — `Recipe`, `Program` and the cache, `Leaf`, `Bank`,
`termsSource`, `over()`, `UniformBlock`, `FrameData` — lives under
`<sigilmaterial/advanced/…>`, and neither `core/Material.h` nor
`program/Shader.h` includes any of it: a consumer that defines a recipe
by hand, writes a leaf or runs a renderer includes the advanced header by
name. The compilers and the executors (`skia/`: `material::skia::paint`,
`material::skia::base`, the `Paint` a material lowers to on a Skia
canvas; `slang/`) are tier 3 beside it.

Beside the recipe model sits the image side: a **texture** is an image
and how it is sampled, a comparable value that fills a recipe's child
slot as a **leaf** — bound by the backend rather than compiled. The
texture feature also knows the folders material tools export (a texture
set by role), and cuts an atlas into regions and frame sequences. The
**surface** feature is the metallic-roughness program a 3D renderer runs
for a material's surface response.

The PRIMITIVES are fully parameterised generators, one feature each:
**sdf** (shape, border, glow and shadow in one pass over a signed
distance), **pattern** (a tile baked once with a mapping and an explicit
reseed, the stock tiles over it, and the woven cloth a sett and a weave
make), and **field** (the halftone ramp, Perlin noise, luminance grain,
the ripple). Under the core sits **colour**, the leaf: the colour value a
parameter struct holds, the OKLab, OKLCH and CIELAB round trips, the ramp
as one value, the harmonies read around a hue, the dither threshold a
pixel is rounded against and the table a run of pixels is made of — all
of it linking nothing; above the texture feature sits **ocio**,
OpenColorIO's view transforms baked to materials.

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
| `SigilMaterialColor` | `Color`, `hexColor()`, `hsv()`, the three mixes and `luminance()`, `ColorStop` with `sampleRamp()`, the OKLab, OKLCH and CIELAB round trips with `fitToSrgb`, `Ramp` (the ramp as one value) with `palette()` both ways, `harmony()` and `rotateHue()`, `Dither`, and `palette(pixels)` with `closestEntry()` — the leaf, which the core's `Parameters.h` includes | SigilCoreCompute |
| `SigilMaterialCore` | the value model: `Target`, `Parameters`, `Recipe`, `Program` and the cache, `Material`, `Leaf`, `UniformBlock`, `FrameData`; the light a lit surface is shaded under — `Light`, `studio()`, `Environment`, `environment()` over any material, `Lighting`; `ColorStops` and `GradientOptions`, what a gradient is told in box units or pixels; `Bank`, the bounded seeded bank of a field's instances; `termsSource`, the shading terms a surface is composed of; and `over()`, the combinator that stacks one material on another through a mask | SigilMaterialColor, SigilMotionValues, glm, Boost.PFR, Boost.Container; Boost.Unordered privately |
| `SigilMaterialTexture` | `Texture` and its sources, with `Sampling` and `PixelRect`, a frame standing on a device bound for the recorder that draws it (`Texture::frameAt(time, recorder)`); the image base `image()` and `environment()` over a pixel source; `texture::` (the tools' sets by role), `EnvironmentMap`, `Atlas` — no Skia type in any header | SigilMaterialCore, SigilMediaCore, Boost.Container; Skia and simdjson privately |
| `SigilMaterialMask` | the third operand of `over()`: `maskConstant`, `maskMap`, `maskSlope`, `maskHeight`, and `fitMask` / `invertMask`, which reshape a mask and nothing else | SigilMaterialTexture, glm |
| `SigilMaterialOcio` | `ocio::` — `available()`, and the OCIO `viewTransform`, `convert`, `exponent` as baked materials, applied through a private 3D-LUT recipe or a per-channel response recipe | SigilMaterialTexture; OpenColorIO privately, when found |
| `SigilMaterialSdf` | `sdf::` — `Shape`, `Style`, `pad`, `material` | SigilMaterialCore, SigilMaterialColor |
| `SigilMaterialPattern` | `pattern::Tile` and the stock tiles; the lattice sources `pattern::scanlines` and `pattern::stipple` with `pattern::ditherBits`; `pattern::Cloth`, the woven cloth, with `threadcount`, `pivots`, `Weave` and `warpUp` under it | SigilMaterialTexture, SigilMaterialColor; SigilCoreCompute and SigilMaterialSkia privately |
| `SigilMaterialField` | `field::` — `halftoneRamp`, `noise`, `grain`, `ripple` | SigilMaterialTexture, SigilMaterialColor; SigilMaterialSkia privately |
| `SigilMaterialProgram` | `shader()` and `ShaderOptions`, a shader from its source or from a file read through the hub, with the textures it samples placed in its slots | SigilMaterialCore, SigilMediaCore; SigilMaterialTexture and SigilIOHub privately |
| `SigilMaterialSkia` | the SkSL compiler and `SkiaProgram`, whose builder uploads resolved bytes; `skia::builder` and `skia::shader` binding leaves into slots; `skia::ShaderLeaf`, the leaf that yields its own Skia shader; a texture through Skia — `skia::image` and `skia::shader` over a `Texture`, `skia::toSkFilterMode`, `skia::toSkIRect` and `skia::toPixelRect`; `skia::painted`, a tile program painted into a canvas; `skia::bevelNormals`; `skia::fill`; a lit surface in 2D — `skia::isLit`, `skia::lightingFor`, `skia::lit`; the colour bridge `skia::toColor` / `skia::toSkColor`; `skia::paletteImage` and `skia::paletteLookup`, the palette's two crossings; `skia::palette`, the picture read down to the table it is made of; `Paint`, the model as ONE shader, with its three gradients `linearGradient`, `radialGradient` and `conicGradient` over `ColorStops`, with `skia::PassInputs` for a pass over a layer; and `Filter`, the post-processing recipe over a rendered layer | SigilMaterialTexture, SigilMaterialColor, SigilMotionValues |
| `SigilMaterialSlang` | the Slang compiler: `slang::compileModule` to SPIR-V, `slang::Compiled` with the reflected `slang::UniformSlot` per uniform, `slang::SlangProgram`, and `slang::Uniforms`, the buffer one draw is written into; `Portable.slang`, the subset a host and a device answer alike, loaded into every session by name | SigilMaterialCore, Boost.Container; Slang privately |
| `SigilMaterialSurface` | `surface::` — the metallic-roughness program a lit renderer shades with: `SurfaceParameters`, `Reflection`, `surfaceRecipe`, `program` and `unlit`, `isSurface` and `isUnlit`, `map` and the seven slot names, the dressing of a decoded texture set, and `lower`, which turns a material's stated `surface({…})` response into the program | SigilMaterialTexture, SigilMaterialColor; SigilMaterialSkia privately |

`SigilMaterial` is the umbrella, an interface over all twelve. Headers live
under `include/sigilmaterial/<feature>/` and are spelled that way —
`<sigilmaterial/core/Material.h>`, `<sigilmaterial/texture/Texture.h>`,
`<sigilmaterial/surface/Surface.h>` — and the program model the core
feature builds stands under `include/sigilmaterial/advanced/`
(`<sigilmaterial/advanced/Recipe.h>`). The library holds no catalogue of
looks: a named look — a grained stone, a chrome, a CRT tube, a text
paint — is a material a sketch builds and keeps beside itself.

## Textures

**A texture is a source plus sampling, and both enter equality.** The
source is SigilMedia's `media::PixelSource`, the one seam pixels cross:
a picture (`Texture(image)`, equal when it is the same image object), a
`media::Image` or a video read on the material's clock (animated
when the document is), a producer that bakes an image on first use
(`Texture(media::PixelSource::produce(key, producer))` — the key IS the
identity, so it must name the picture and every parameter that shaped it), frames
another application publishes, a rendered scene. The frame drawn is the
one the source answers at the material's `FrameData::seconds`, and one
standing on a device is read back unless a renderer on that device takes
`Texture::deviceImage` instead. Sampling is the tiling per axis
(`tile(Repeat)`, the same `Repeat` a gradient is told), the uv matrix
placing texture space in the sampled space (a `glm::mat3`; `at(origin)`
is the translation), a region of the image to read (a `PixelRect`), and
how it is read between pixels (`sampling(Sampling::Nearest)` or
`Sampling::Linear`). `Texture::frameAt(time)` is the frame sampled, in
host memory and cut to the region; the region is cut once per source
image and kept, so a texture sampled every frame does not copy its
pixels every frame. No texture header names a Skia type: `skia::image`
and `skia::shader` in `<sigilmaterial/skia/Texture.h>` are where a
texture becomes one.

**A source MAY say that its pixels already stand on a GPU.** One
optional member, `deviceImage()`, answers a `DeviceImage`: the device
that owns the texture and the texture itself, as the graphics API's own
object bridged to opaque values. This library reads none of it and
compares none of it — the source's own equality is still what says
whether two textures are the same picture. It is carried, unexamined,
from a source that painted on a device to a renderer standing on the
SAME device, which binds those pixels instead of uploading a copy of
the frame; a renderer holding another device, or none, finds a device it
does not know and reads `frameAt()` like any other source's. Every source
that has no device omits the member and is written exactly as it was.

**Texture sets are the tools' folders.** `texture::classify` reads a
file name into a `Role` (base colour, normal, roughness, metallic,
occlusion, emissive, packed occlusion-roughness-metallic, height,
opacity, specular), the set it belongs to and whether a normal map is
DirectX-convention; `discover` groups a directory into `TextureSet`s;
`fromFiles(set, decoder)` and `fromUsageMap(images)` decode into
`TextureMaps`, one repeating texture per role. The library opens no file:
a `Decoder` returns pixels (a `media::PixelSource`) for a path, and the
caller supplies it. What
a set MEANS to a renderer — which channel of a packed image feeds which
slot — is the renderer's rule, not this library's.

**A surface is shaded from two textures.** `EnvironmentMap` is the
panorama a surface sees when it looks past the lights — equirectangular,
u = azimuth, v = 0 at the zenith, with `equirectangularUv` and
`equirectangularDirection` as the one convention every consumer shares. Sources
resolve into that single form while the value is built: `baked()` runs a
radiance function over the panorama (a named sky a sketch keeps is one
written against it, and needs no assets),
`fromEquirectangular()` wraps a loaded
lat-long panorama, `fromFaces()` resamples six cube faces and
`fromCubeMap()` unpacks one sheet — a 4:3 or 3:4 cross, a 6:1 row or a
1:6 column — into the same. A cube map arrives as an ordinary image
because that is what SigilMedia decodes, and the two containers that
hold six faces in one file arrive as the 1:6 column: a KTX 1 or 2
through SigilMedia's own reader (uncompressed texels), a DDS through
its OpenImageIO backend.

Two readings hang off the panorama, cached with it and shared by every
copy of the value, and each is a `Texture`. The SPECULAR side is
`texture(roughness)` — nine wrap-aware blurs a reflection picks by how
rough the surface is, as the level a recipe's environment slot takes,
repeating in azimuth and clamped at the poles — and `chain()` as the same
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

`skia::bevelNormals(path, bevelPx)`, in the Skia executor because the
outline arrives as a Skia path, blurs the outline's coverage into a height
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

## Surfaces, masks and banks

The library ships primitives and no named looks. A look — a grained
stone, a chrome over bevel normals, a globe, a CRT tube, a text paint, a
colormap — is a material a sketch composes from these and keeps in its
own directory, so what follows is the machinery every such look is made
from: the shading terms, the surface program, masks, banks and resolve.

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
and a body at once. Everything else has to be spelled the same in
both, and a source written for this crossing accepts that in exchange for
being one source: no texture sampling, no construct one language has and
the other does not.

**The metallic-roughness surface** is `surface::SurfaceParameters` — base
colour, metallic, roughness, emission, the normal convention, the channel
each packed map is read from, the cutout threshold and the glass terms,
which are transmission, index of refraction, thickness and the
Beer-Lambert absorption a medium takes out of what passes through it —
under two recipes over the same ABI: `surface::program()` takes light,
`surface::unlit()` is its own light. Its colours are FACTORS on the maps in
their slots, and neither body transforms either side of that multiply or
the product it hands on, so a colour is held exactly as it is typed and
is in the encoding the images beside it are in: over the white fill a
surface is dressed with, `unlit()` paints the number that was written
into it. `Reflection` is how the environment
reaches a lit surface — `SplitSum`, where the surface's own reflectance
and its Fresnel decide, or `Additive` at `reflectionWeight`, with
neither — and it is one recipe each, so no body carries a branch. Seven slots, one per role
(`kBaseColorSlot`, `kNormalSlot`, `kRoughnessSlot`, `kMetallicSlot`,
`kOcclusionSlot`, `kEmissiveSlot`, `kOpacitySlot`), each dressed with a
neutral one-pixel fill when it is built so no body ever evaluates an
unbound child; `surface::map(m, slot)` answers the texture a caller placed
there and null for a fill. `surface::program(TextureMaps)` dresses one from a
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
and the glass terms have no effect on either body. `program()` shades the
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

A MATERIAL STATES ITS RESPONSE with `Material::surface(SurfaceOptions)`
and `surface::lower(material)` turns that statement into the program: the
base is the base colour — a colour its factor, an image's texture the
base-colour map, any other base or a base under layers the map slot's
material — each channel a number or a material placed in its map slot,
`.unlit` the unlit program, and a material that states no response its
base, unlit. A bare program lowers to itself. A surface has no coverage,
so an effects stage on a lowered material is dropped and said once.

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

## The program model

What a shader material is made of is its own chapter:
**[ADVANCED.md](ADVANCED.md)** — a recipe defined by hand, with a body per
language, the slots it samples and the frame values it reads; the one
program cache and the compilers registered into it; resolving an instance
against a frame; the leaf a backend binds; stacking one material over
another and the composed stack a one-body target needs; the Slang
backend's reflected layout; and warming every program before the first
draw. Every header it names is under `<sigilmaterial/advanced/…>`.

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

**pattern.** A `Tile` is one bake plus a mapping. The program bakes one
seamless tile at a seed into pixels — `skia::painted(painter)` makes one
from a painter that draws into a Skia canvas; the bake is memoised on
shared state, `seed(n)`
and `program()` copy-on-write that state and drop it, and `scale`,
`rotate`, `offset` and `sampling` act on the sampling alone, so a
rotated repeat stays seamless with no rebake. The bake is the identity:
hold a Tile where assets are held. `texture()` is the bake repeating on
both axes through the mapping. The stock tiles — `halftone`, `stripes`,
`sequence`, `checker`, `gridLines`, `speckle` — are programs over it, and
each takes its colours as `Color`, which an `SkColor4f` converts to.
`sequence` takes the AXIS its runs travel along (`Axis::U` across,
`Axis::V` down) rather than leaving it to `rotate(90)`: rotating remaps
the sampling of a tile whose repeat is one period by an arbitrary eight
pixels, which reads right only while the other direction is constant.

Beside the tiles stand two **lattice sources**, read per pixel rather than
baked, so each compares by its numbers and one described afresh prunes:
`pattern::scanlines(ScanlineOptions)` — a row `on` px tall every `period`
px, slid by `phase`, clear between — and `pattern::stipple(StippleOptions)`
— one colour through a repeating 1-bit mask of up to eight cells a side,
bit `y * size + x` the cell at (x, y), `cell` px each —, with
`pattern::ditherBits(on, size)` the mask of one ordered-dither tone. Each
is a material a `layer()` blends over a base through its own `blend`,
`opacity` and `mask`: the phosphor scanline is a `Plus` layer, the
printed one a low black alpha through `Normal`.

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
per thread as a nearest-read `Texture`, and `clothTile(cloth, threadPx)` is the whole repeat as a
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
octave count because the count is a constant in the body; and `ripple`
resamples its `content` child through a sine displacement.

## The Skia paint

The paint and the effect are their own chapter:
**[PAINT.md](PAINT.md)** — `Paint` as ONE `sk_sp<SkShader>` over a
tree of solids, ramps, images, buffers, SkSL effects and blend layers;
the three volatility tiers it declares by what it reads; the unit-square
ramps that need no box size written down; and `Filter`, the
post-processing recipe over a layer a consumer has already rendered.

`Filter::of(material)` captures the material's uniforms and child
slots when constructed — the layer slots with them, whose filters are
built from the amounts the material carries at that moment and for the
same reason. Static captures compare by material value and compiled
program, so describing the same effect again can reuse a retained layer.
Captures with live inputs compare by their built filter's identity: a binding
names its source rather than the value captured from it. Re-describe to sample
those inputs again. Surface-specific lowering to a colour filter retains that
filter's own identity.

## Where the stock shaders live

Every body this library ships is a `.sksl` or `.slang` file in the
`shaders/` directory beside the feature that owns it, so an editor and a
shader tool see the language, and `sigil_shader_sources()` compiles that
whole directory into the feature's archive as a table of
`std::string_view` keyed by file name. A feature reaches its own text
through the accessor the generated header declares —
`<sigilshaders/MaterialSurface.h>` spells
`surface::shaderSource("Surface.sksl")` and `surface::shaderSources()`,
the whole table — and no feature reaches
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

The core links no renderer; the texture feature links SigilMedia
because an asset is a source, and Skia only privately. The Skia seam is
`sigilmaterial/skia/*`: no `texture/` or `pattern/` header names a Skia
type, and a texture, an environment map, an atlas and a baked tile meet
Skia in the executor — `skia::image`, `skia::shader`, `skia::painted`,
`skia::bevelNormals`, `skia::ShaderLeaf`. A header outside it that
needed one would be the boundary moving. SigilIO owns resource access and SigilMedia owns
image meaning, so this library decodes no pixels and opens no consumer asset
file — every door that needs pixels takes them or takes a decoder. Its own
shader files are compiled into its archives rather than read through SigilIO,
so no feature here links a resource hub and none of them can be run from a
directory that has no shaders in it.
SigilGeometry draws
the normals passes and outlines a surface is shaded over, and links
nothing here but the colour leaf, privately, for the OKLab interpolation
its path blend runs in; SigilWorld's renderer is one executor of the
surface program this library defines and adds no shading model of its own;
SigilCompose places what a material paints — it takes a `Paint` as
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
| `surface/test/` | both surface programs compiled, an authored colour and a map texel one number, a program dressed from a decoded set, a stated response lowered with its numbers and maps in place, a stack shaded at both ends of its mask, every shading term against its closed form, and the sampler budget: a stack asks a device for its operands' samplers and no more, and a tree over the limit is refused with the count and the limit named rather than drawn | — |
| `program/test/` | a shader as a material: it paints what its body returns from the struct's values, one source is one definition while a differing one is another, a bound field makes its own pass live and compiles nothing, a texture is sampled by the name the body reads, a file is read through a hub, where a broken edit keeps the last program that compiled, a file with none paints the placeholder until one does, and the compiler's message and the body's line stand on the hub's problems — and `MaterialTier`, that neither `core/Material.h` nor `program/Shader.h` reaches a header under `advanced/` | — |
| `skia/test/` | the SkSL backend — a two-uniform recipe compiled through the cache shading a raster byte identical to the same SkSL compiled and filled by hand, the four parameter names a body may not redeclare and the three spellings that must still compile — and `SkiaPalette`, a picture's own colours coming back | — |
| `slang/test/` | the Slang backend, with no device | — |
| `MaterialGpu` | every body this library ships, on a device | `gpu` |

The `MaterialGpu` suite belongs to the whole library rather than to a
feature: every other suite shades on a raster surface, where a body is
compiled as its own SkSL program, and a body can pass that and fail once
a GPU backend has inlined it into a pipeline. It stands Graphite up,
installs a shader-error handler through
`GraphiteContext::reportShaderErrorsTo`, and draws one instance of every
recipe the surface, sdf and field features ship — plus a stack per blend, the whole terms text, and the ocio bake
where OpenColorIO is available — through the same `Paint` a
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
