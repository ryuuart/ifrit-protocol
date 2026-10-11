# SigilMaterial

SigilMaterial describes the appearance of a region or surface. A `Material`
is a comparable value: a color, gradient, image or shader, optionally built
up with layers, surface properties and effects. Compose, Draw and World
consume the same value. The library owns no window, render loop or asset
store.

Namespace: `sigil::material`. Start with a base and add what the appearance
needs. Ordinary authoring does not require a recipe, program cache or
renderer configuration.

## Build a material

```cpp
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>

namespace material = sigil::material;

material::Material panel() {
  return material::from(material::hexColor(0xB8BDC4))
      .layer(material::noise(0.4f),
             {.blend = material::BlendMode::Multiply, .opacity = 0.3f})
      .surface({.metallic = 1.0f, .roughness = 0.2f})
      .effects(material::Filter::shadow(material::hexColor(0x000000),
                                       {.blur = 8, .offset = {0, 4}}));
}
```

Pass the result to a consumer's fill, ink or stroke operation. A `Color`
converts to a `Material`; the gradient, image, noise and shader factories
already return materials. Application-specific looks belong to the
application and can be ordinary functions like `panel()`.

`material::noise(frequency, {.cellular = true, .jitter = 1})` is cellular
noise rather than fractal: one seed per cell of a square lattice `frequency`
cells per pixel, the distance to the nearest seed in red, to the second in
green, and their difference in blue — zero along the walls between cells —
so a crackle or a cell network is a fill read through
`material::channel(cells, ColorChannel::Blue)`. Its body exists for Skia and
for Slang, so the same value plays on a page and on a 3D body;
`material::field::cellular(frequency, jitter, seed)` is the same field.

The four parts have distinct jobs:

| Part | Meaning |
|---|---|
| Base | The initial appearance: a color, gradient, image, shader or another material. |
| `layer()` | Blend another material over the accumulation, with optional opacity and mask. |
| `surface()` | Describe the surface a renderer shades: roughness, metallic, normals and related channels. |
| `effects()` | Apply a filter chain to rendered coverage: shadows, glows, strokes or bevels. |

A layer mask takes a material source, a channel, a `[low, high]` range and
an optional inversion, stated in place in the layer's options:
`{.mask = {depth, material::ColorChannel::Luminance}}`, or
`{.mask = {depth, material::ColorChannel::Luminance, 0.2f, 0.8f, true}}`.
The range maps to coverage `[0, 1]`. Surface channels can be constants or
materials where their types allow it, and `material::remap()` turns one
channel of a map into the stretch of values a channel wants — a metalness
of one in a carving's hollows and zero on its face is
`material::remap(depth, {.to = {0, 1}})`, and `RemapOptions::from` says
which stretch of the map is read.

The designated form is useful when the parts are already data:

```cpp
material::Material panel = material::from({
    .base = material::hexColor(0xB8BDC4),
    .layers = {{material::noise(0.4f),
                {.blend = material::BlendMode::Multiply, .opacity = 0.3f}}},
    .surface = material::SurfaceOptions{.metallic = 1.0f, .roughness = 0.2f},
});
```

Use either form; both build the same value model.

## Gradients and images

```cpp
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/texture/Image.h>

material::Material gradient = material::linearGradient(
    {0, 0}, {0, 1},
    {material::hexColor(0x334455), material::hexColor(0x112233)});

material::Material picture = material::image(pixels);
```

`pixels` is a `sigil::media::PixelSource`, supplied by the caller.
SigilMedia decodes pixels; SigilIO resolves and reloads resources.
Gradient coordinates use box units by default: `{0, 0}` is one corner
and `{1, 1}` the opposite. `GradientOptions` selects pixel units, repeat
and the other gradient controls. `ImageOptions` controls image repeat.

A `Texture` combines a pixel source with sampling, tiling, placement and
an optional pixel region. Its `frameAt(time)` reads the source at the
requested time. Animated sources use the frame supplied by the renderer,
not a clock owned by the material. Device sources can expose their GPU
image to a compatible renderer; other consumers read a host-memory frame.
The region is cached per source image rather than copied every frame.

`image(texture)` carries that placement, region and sampling into a material.
Use it for a fill, ink or surface channel, including a placed normal map:

```cpp
material::Material face = material::from(material::hexColor(0xB8BDC4))
    .surface({.metallic = 1.0f, .roughness = 0.2f,
              .normal = material::image(normalTexture), .normalDirectX = true});
```

`normalTexture` is the caller's texture; its normal convention determines
`normalDirectX`.

For coarse relief with finer detail, `surface::blendNormals` combines two
normal materials by reorienting the detail into the coarse tangent frame.
Its options state each input's green-channel convention and the output's.
Compose evaluates composed normal materials; a device renderer evaluates a
material in a slot when it has a Slang body, through `material::inlined`
below.

`surface::normalFromHeight(height, options)` converts a grayscale material
into a normal channel. White is the stated height above black; negative
depth engraves. Depth and sample step are logical pixels, and `directX`
selects the green-channel convention. Transparent input contributes zero
height. Smooth hard edges in the input when a rounded transition is wanted.
Mapped glyph and word ink keeps those pixel units as its box scales or turns;
encoded normal images retain their authored slopes.
This Skia operation samples the input's colour stack; rendered effects must be
baked into that input first. A constant height or zero depth stays flat.

The texture feature also provides:

- `texture::classify`, `texture::discover` and `texture::fromFiles` for
  texture sets exported by tools. The caller supplies the pixel decoder.
- `EnvironmentMap` for equirectangular panoramas, cube faces or packed cube
  maps. Specular levels, diffuse irradiance and their shared caches are
  derived from the same panorama; calculations preserve floating-point
  radiance above one.
- `Atlas` for grid sheets, TexturePacker and Aseprite metadata, named regions
  and frame sequences.

These values describe images and sampling. The backend operations under
`sigilmaterial/skia/` convert them into Skia images and shaders.

### What a texture holds

A texture states whether its pixels are colour or data with
`Texture::colorSpace`, spelled with SigilMedia's `media::ColorSpace`:

- `media::ColorSpace::Srgb` is colour as a display stores it: decoded into
  light where it is lit, its alpha multiplied into it where it is
  composited, converted between colour spaces where a canvas asks.
- `media::ColorSpace::Data` is numbers — a normal, a roughness, a flow, a
  mask packed into a channel, a motion vector. A renderer reads them as
  stored: no decode, no alpha multiplied into the other channels (a texel
  whose alpha is zero keeps its red, green and blue), no colour
  conversion, and sixteen-bit and floating-point pixels at their own
  precision. Decode an image as data with `media::ImageOptions::colorSpace`,
  so its sixteen bits and its straight alpha survive the decode too.

Unstated, the place the texture fills decides: a surface's normal,
roughness, metalness, occlusion, opacity and clear-coat maps are data by
their role, its base colour and emission are colour, and anywhere else a
texture is colour. `surface::program(TextureMaps)` states data on the maps
a texture set imports, and reads its emissive, thickness and distortion
maps at the strength, depth and displacement the set says their white
stands for, and its transmission with the set's volume. `Texture::mipmaps` says whether a renderer builds
the smaller copies a shrunk texture is read from; unstated, colour has
them, a surface's data maps have them, and other data does not — a table
or a sheet of cells must not be averaged. `Texture::sampling` with
`Sampling::Nearest` reads texel by texel. `material::ImageOptions` carries
the colour space for `material::image(pixels, options)`. Skia reads a data
texture through a raw shader, unconverted.

### Playing what an effects tool hands over

```cpp
#include <sigilmaterial/texture/Channel.h>
#include <sigilmaterial/texture/Flipbook.h>
#include <sigilmaterial/texture/Flowmap.h>

material::Material fire = material::flipbook(
    sheet, {.columns = 8, .rows = 8, .framesPerSecond = 30,
            .blendFrames = true});
material::Material grain =
    material::channel(material::image(packed), material::ColorChannel::Green);
material::Material streaming = material::flowmap(
    grain, {.flow = directions, .strength = 0.15f, .speed = 0.5f});
```

- `material::flipbook(sheet, FlipbookOptions)` plays a grid of frames read
  row by row from the top left. The sheet carries no word about its grid,
  so `columns`, `rows` and `frames` (zero is every cell) state it;
  `framesPerSecond`, `loop` and `startFrame` drive it from the frame's
  clock. A bound `frame` (in frames) or `progress` (zero to one over the
  run) wins over the clock. `blendFrames` mixes the two cells the clock
  stands between; `motionVectors` — a second sheet of the same grid whose
  red and green, encoded around one half in units of one cell, say how each
  pixel moves to the next frame — pushes each of the two reads along the
  motion first, scaled by `motionStrength`. Each cell covers the region it
  paints: a mesh's uv square, a node's box. Which frame stands is a pure
  function of the clock and these values. On a body drawn as instanced
  geometry, `frameAttribute` names a scalar each instance carries, read
  as its frame, so every copy of one stamp plays its own cell.
- `material::instanceAttributeKey(name)` is the number a device program
  asks an instance's named scalar for by — a program cannot hold a name.
  The program a body is compiled into answers `instanceColor()`,
  `instanceIndex()` and `instanceAttribute(key)` for the instance a pixel
  belongs to: white, zero and zero on a body that is not instanced.
- `material::flowmap(source, FlowmapOptions)` reads `source` through a
  texture of directions (`flow`, data) moving with the clock: two reads half
  a cycle apart slide `strength` along the flow each cycle, `speed` cycles a
  second, crossfaded so neither is seen when it jumps back.
- `material::channel(source, ColorChannel)` is one channel of any material
  as an opaque grey — red, green, blue, alpha or luminance — wherever a
  material is taken: a roughness, a mask, a layer, a base. `ColorChannel` is
  also what a layer's `Mask::channel` reads.

All three are materials with a body for Skia and one for Slang, so the same
value plays on a page and on a 3D body.

## Surfaces and light

A surface response is optional, and it describes a surface rather than
shading one. `SurfaceOptions` is the metallic-roughness description glTF
and three.js use: `metallic`, `roughness` and `occlusion` (each a number
or a material), a tangent-space `normal`, `emission` with its strength
and map, a clear coat — `clearcoat`, `clearcoatRoughness` and its own
`clearcoatNormal`. Six further lobes take glTF's names:

- a sheen — `sheenColor` (a colour or a material; black is none) and
  `sheenRoughness` — a soft reflection at the rim, as cloth shows;
- anisotropy — `anisotropy` (0 to 1), `anisotropyRotation` in degrees
  counter-clockwise from the way the surface's u runs, and an
  `anisotropyDirection` map (red and green the direction, blue a scale on
  the strength) — a highlight stretched along a brushing;
- iridescence — `iridescence` (a weight), `iridescenceIor`, and
  `iridescenceThickness` in nanometres, which with an
  `iridescenceThicknessMap` is the thickness at white over
  `iridescenceThicknessMinimum` at black — a thin film's interference;
- a dielectric's own `specular` strength and `specularColor`;
- transmission — `transmission` (a weight, a number or a material),
  `ior`, `thickness` in scene units (a number or a material; zero a thin
  wall), the volume's `attenuationColor` and `attenuationDistance` (light
  that has crossed the distance has become the colour; zero takes
  nothing), and `dispersion` (the index spread across the three colours,
  zero none) — glass, liquid, tinted plastic; a map is read by its red; its reflectance follows its index — nothing at an index of one;
- distortion — a two-channel DATA map `distortion` (red across, green up,
  half-grey still) and `distortionStrength` in scene units at the surface:
  what stands behind displaced, with no light involved, for heat haze and
  shockwaves; carried only with a map at a strength other than zero.

**The unit a material is painted into** can be a source of its surface,
resolved by whatever paints it: `Material::relief(ReliefOptions)` raises
(positive `depth`) or impresses the unit's coverage, softened over the
`shoulder` in logical pixels, and combines that relief under the stated
normal; `Material::brushed(BrushedOptions)` states an anisotropy of
`strength` that runs along the unit, turned by `rotation` degrees — the
`SurfaceOptions::relief` and `SurfaceOptions::anisotropyAlongUnit` fields.
`readsUnit(material)` says whether a material reads one. A material holds
no unit: a 2D page's painter knows the glyph, word, box or stroke it paints
and answers the sources with `surface::resolveUnit`, cutting the relief with
`skia::reliefNormals` and the direction with `surface::unitDirection`; a 3D
body wearing the material with no page behind it has no unit and is drawn
without them, said by name.

What a renderer makes of the last two is its own: SigilWorld reads a copy
of the scene behind the surface, one layer deep. A page's surface maps
carry both, per pixel: the transmission's weight, its thickness over the
deepest the page states and its distortion over the strongest, with the
volume — index, attenuation, dispersion — of the first transmitting
material the page paints standing for the whole page.

Emission above white is `emissionStrength` past one: the colour decoded
into light and multiplied, and a page's emissive map's white stands for the
brightest emission the page states, or for the strength a page states as
its range from its first paint. Colours in it are
encoded as an author types them; occlusion darkens only light arriving
from everywhere. A surface carries only the lobes it states
(`material::lobes(SurfaceOptions)`, `material::Lobes`): the surface program
lowered from one that states no sheen has no sheen in its body and samples
none of the sheen's maps, and the program's own recipe says which it
carries (`surface::lobes(Material)`). SigilMaterial shades nothing: a renderer that shades a scene
shades a material's surface under the scene's own lights, and a 2D painter
paints the material's colour stack flat. `material::skia::paint` lowers
that colour stack whether or not the material states a surface.

The lights a surface is shaded under are the renderer's: a scene's are
SigilWorld's, which names them itself.

The shading terms a renderer composes a lit surface from — the GGX
lobe, Fresnel, the falloffs, the tone curves and the colour encoding —
are one text, `material::termsSource`, with the split sum's table beside
it (`material::environmentBrdfTable`) and the sheen's directional albedo
over the same square (`material::sheenAlbedoTable`).

A renderer that brings its own lights reads a surface UNSHADED, one surface
map at a time: `material::skia::asMap(material, role)` answers the base
colour, normal, roughness, metallic, occlusion or emissive map, the three
scalars packed in glTF's order (`texture::Role::Packed`), the clear coat's
weight and roughness (`texture::Role::Clearcoat`), or one of the optional
lobes' — the sheen's colour (`Sheen`), the sheen's roughness with the
specular strength (`SheenSpecular`), the specular colour
(`SpecularColor`), the brushing's direction and strength (`Anisotropy`),
the film's weight and thickness (`Iridescence`), the transmission's weight
(`Transmission`), its thickness in scene units (`Thickness`) and the
distortion in scene units, red across and green up around one half
(`Distortion`) — and the coat's own normal (`ClearcoatNormal`), keyed by
`texture::Role`, as the texture of that role encodes it. An emission past
one comes back as light encoded past white, and a thickness or a
distortion past what white holds comes back past it, for a reader that
scales the map back into range.
`material::skia::LobeListener` hears which lobes the surfaces read as maps
on its thread state, and how far they reach past a map's white
(`material::skia::MapReach`: the brightest emission, the deepest volume, the
strongest distortion, the first volume), which is how a page learns which
of those maps it needs and what each one's white stands for. The normal comes
back as `(n + 1) / 2` with green up the picture whatever convention the
material stated, including a height-derived or blended normal, and its lean is
written in the root frame the node is drawn into — turned by the inverse
transpose of the node's placement with its steepness kept, so a node turned a
quarter writes a normal leaning a quarter round, while a material anchored to
the root frame writes its normal as sampled; a scalar
comes back grey, its number times its map; a role a material states
nothing for reads the stock `SurfaceOptions` number. Every map carries the
colour stack's alpha as its coverage. A material with no surface reads as
the stock `SurfaceOptions`: its colour stack is its base colour and it
takes the renderer's light. An `unlit` one is its own colour: emissive is
the colour stack, the base is black, the normal faces the viewer,
roughness and occlusion are one and metallic is zero. `material::skia::SurfaceMaps` prepares a material's colour
stack and channels once and answers every map from them; its `asMap()`
holds one pass per role, and `SurfaceMaps::mapShader` places a map for one
draw whose unit square maps into the node. Construct another when the
material changes. `reflectionWeight` has no map. A material anchored to the
root frame (`Material::worldSpace`) anchors every map there, so its normal
and numbers stand where its colours do.

In Skia, a positive `SurfaceOptions::alphaCutoff` removes color-stack samples
whose alpha is below the threshold, in a flat paint and in every map.
Samples at or above the threshold keep their color and alpha; shape
antialiasing still applies afterward. Zero preserves ordinary blending.
Effects require rendered coverage and are ignored by a renderer
that has none.

**How a surface composites** is part of its description, as three.js and
glTF put it on the material: `SurfaceOptions::blend` is a `BlendMode` —
`Normal` laid over, `PlusLighter` added, `Multiply`, `Screen`, `Modulate`
— `depthWrite` whether it hides what is drawn after it and behind it
(unstated: an opaque surface laid over normally does, any other does not),
and `depthFade` the distance in scene units over which it fades out where
it meets the opaque surface behind it. The surface program carries the
three as `blend`, `depthWrite` and `depthFade` in `SurfaceParameters`, and
`surface::compositing(material)` reads them back as a `Compositing` for the
renderer that acts on them. The 2D painter composites a node by the node's
own blend mode and ignores them.

**A material in a slot, on a device.** A device compiler is handed one body
per material, so `material::inlined(material)` writes every child material
in a slot its Slang body samples into that body: the slot's reads become
calls of the child's `surface`, and the child's values, live bindings and
leaves are carried under the slot's name as a prefix. A flipbook in a
surface's base-colour or emission slot plays on a 3D body this way.

## Backdrop glass

`Filter::glass` refracts an already rendered layer using a normal material.
Attach it through the host's backdrop operation to bend the content behind
a pane. Use a Surface for reflections and coating; compose `Filter::blur`
for diffusion.

```cpp
material::Filter glass = material::Filter::glass({
    .ior = 1.5f, .thickness = 12, .sampleRadius = 32,
    .normal = normalField, .normalDirectX = true,
});
```

`normalField` is the caller's opaque RGB material encoding `(normal + 1) / 2`.
Normals face the viewer and are normalized before use. Green points down
for DirectX input and up otherwise. Only the color stack is sampled.
The normal resolves at the current node's size and clock; a live normal or
`bind("ior", value)` / `bind("thickness", value)` keeps the filter live.

Thickness and the sampling radius use node-local logical pixels. The radius
is a finite ceiling on each offset component, fixed when constructing the
filter; it cannot be set or bound afterward. Refraction preserves alpha at
the original pixel and borrows color from the displaced lookup. Transparent
or invalid lookups retain the original color. Index one or less, thickness
zero or less, and radius zero are identity; negative or nonfinite radii are
reported and return no filter. This is an orthographic single-interface
warp, with no dispersion or transport through a closed glass body.

## Layer styles

The coverage effects are an image editor's layer styles, drawn across the
outline a consumer paints — a box, the contours of a word — and they
read no light of any scene: each carries its own direction and colours.
`Filter::shadow` is a drop shadow, or with `.inside` an inner shadow, or
with a `.spread` and no offset an outer glow; `Filter::stroke` a keyline;
`Filter::bevel` a bevel and emboss lit from `angleDegrees`, with its
`highlight` and `shadow` colours. A bevel that states a gloss contour
(`BevelOptions::gloss`), a picture to reflect (`BevelOptions::reflection`)
or a relief of its face (`BevelOptions::normal`) is shaded from its
shoulder's normals under its own highlight direction and altitude
(`BevelOptions::altitudeDegrees`): chrome is a bevel whose face reflects a
picture laid across the box, bent where the face leans. `horizonReflection()`
is the stock picture, a sky over a hard horizon over a lit ground.

```cpp
material::Material chrome = material::from(material::hexColor(0xD8DEE6))
    .effects(material::Filter::bevel({
        .depth = 2, .size = 3, .altitudeDegrees = 50,
        .gloss = {0, 0.9f, 0.4f, 1},
        .reflection = material::horizonReflection()}));
```

## Write a shader

```cpp
#include <sigilmaterial/program/Shader.h>

struct Ripple {
  material::Color ink = {0.2f, 0.5f, 0.9f, 1};
  float rings = 12;
  float speed = 1.5f;
};

constexpr std::string_view rippleSource = R"(
half4 main(float2 p) {
  float2 uv = p / uResolution;
  float wave = 0.5 + 0.5 * sin(length(uv - 0.5) * rings * 6.2831 - uTime * speed);
  return half4(ink.rgb * wave * ink.a, ink.a);
}
)";

material::Material ripple = material::shader(rippleSource, Ripple{});
ripple.set("rings", 16.0f);
```

The struct fields become uniforms with the same names. Supported fields
are `float`, `glm::vec2`, `glm::vec4`, `Color` and
`std::array<float, N>`, with no padding; `schema<P>()` checks the layout.
Write the body and helpers, without repeating those uniform declarations.

SkSL uses `half4 main(float2 p)` and returns premultiplied color in box
pixels. Slang uses `float4 surface(float2 uv)` and returns straight color;
select it with `ShaderOptions::target` and use a renderer that compiles
Slang. `ShaderOptions::textures` names sampled textures; in SkSL each is a
shader slot read with `name.eval(p)`.

For a texture you paint yourself, hold a `material::skia::PixelBuffer`,
draw into its `canvas()` and publish the edits with `commit()`. A fresh
buffer paint supplies the shader slot:

```cpp
#include <sigilmaterial/skia/Paint.h>

// The shader declares "painted" in ShaderOptions::textures.
pixels->commit();
effect.slot("painted", material::skia::base(material::skia::buffer(pixels)));
```

Pass the updated material to its consumer after publishing. A held buffer
paint keeps its snapshot; recreating it between commits keeps the same
value. This raster source stores premultiplied N32 pixels. Use textures
for spatial data such as painted height or wetness, and uniforms for
parameters such as brush radius or material strength.

Frame uniforms are generated when the body uses them:

| Uniform | Value |
|---|---|
| `uTime` | Seconds from the renderer's frame; makes the material live. |
| `uResolution` | Box size in pixels. |
| `uContentScale` | Device scale. |
| `uWorld` | Placement in the root frame. |
| `uLocalToSample` | Logical node offsets to sampling offsets, for pixel-sized taps. |

A struct field with one of these names takes precedence over the generated
frame uniform. `Material::set` writes a field; `Material::bind` follows a
scalar or color motion value, or a uniform block. Color bindings upload all
four components without clamping. A material owns no motion engine, so the
host must advance the source of a live binding. An animatable carrying only
its own transition reads as its target here.

A bound `UniformBlock` starts with published zeros. Edit its draft through
`values()`, then call `commit()` to publish the whole array; no re-description
is needed. Renderers read `committedValues()`, so changing a scalar, time or
box size cannot expose draft edits. Keep the fixed-size block with your model.

Equal shader source, parameter layout, options and values reuse the same
definition and compare equal. Describing a shader again does not require
compiling it again. Unknown fields and incompatible values are reported
once and ignored.

A shader can also come from a resource:

```cpp
material::Material ripple = material::shader(hub, "res://ripple.sksl", Ripple{});
```

Include `sigilio/hub/Hub.h` when creating the hub. After the hub observes an
edit, describe the shader again. Once a compiler is registered, an invalid
edit keeps the last program that compiled. With no accepted program, the
material paints `placeholder()`, a magenta and black checker. Errors appear
under the URI in the hub's `problems()` and clear after a successful compile.
The Skia backend registers its compiler on first use; a device renderer
registers its compiler when it starts. Before registration, source cannot
be validated by that backend.

## Values, lifetime and caching

- Materials compare by their definitions, parameters, children, settings
  and binding identities. A live binding compares by its source, not its
  current number. Copies retain independent value semantics.
- Renderers derive whether a material is static, geometry-dependent or
  live. Ordinary callers do not choose a cache tier or resolve programs.
- Images compare by source identity. A producer key must identify the image
  and every parameter that affects it; matching keys promise matching pixels.
- Keep a `pattern::Tile` with the assets it produces, then use
  `Tile::texture()` to supply its repeat to a renderer. Constructing a new
  tile during each description also constructs a new bake state.
- `Filter::of` captures uniforms and child slots at construction. Describe
  it again to sample live inputs again; static captures can compare by value.
- A manually constructed `Recipe` has object identity. Define it once and
  share it; building equal recipe objects repeatedly creates distinct program
  cache entries. The ordinary `shader()` factory handles definition reuse.
- A renderer that must not wait for a compile asks
  `ProgramCache::prepare(recipe, target, variant)` first: true when the
  program can be had at once, otherwise false with the compile started on
  a worker of its own, which a later ask finds held. `compiling()` counts
  the compiles under way; the process waits for them when it exits.

## Features and dependencies

Include the originating header and link the feature used by the consumer.
`SigilMaterial` is an umbrella target for applications that use the whole
library; smaller consumers can select these targets:

| Target | Purpose |
|---|---|
| `SigilMaterialColor` | Color values, conversions, ramps, palettes and dithering. |
| `SigilMaterialCore` | Material values, layers, surface declarations, the emitter description and parameter reflection. |
| `SigilMaterialTexture` | Pixel sources, sampling, texture sets, environment maps and atlases. |
| `SigilMaterialProgram` | Shader source and resource factories. |
| `SigilMaterialSkia` | Skia paints, gradients, filters, shader compilation and surface maps. |
| `SigilMaterialSlang` | Slang compilation and reflected uniform layouts. |
| `SigilMaterialSurface` | Surface programs and lowering for lit renderers. |
| `SigilMaterialMask` | Mask generators and transformations. |
| `SigilMaterialSdf` | Signed-distance shapes and their appearance. |
| `SigilMaterialPattern` | Baked tiles and cloth generators. |
| `SigilMaterialField` | Noise, grain, cellular noise, halftone and ripple fields. |
| `SigilMaterialOcio` | OpenColorIO transforms baked into materials. |

The core has no renderer. Skia types belong to the Skia executor headers;
texture and pattern headers use media values. Resource access belongs to
SigilIO, image decoding to SigilMedia, and geometry to SigilGeometry. The
program feature uses the hub for consumer-authored shader files. Shaders
shipped by the library are embedded from each feature's `shaders/` directory.

## Further reference

- [Color](COLOUR.md): color spaces, ramps, harmonies, palette extraction and dithering.
- [Skia paint and effects](PAINT.md): direct canvas use and filter execution.
- [Program model](ADVANCED.md): recipes, renderer resolution, slots, banks and compilers.
- [Substance](SUBSTANCE.md): procedural archives through the separately selected
  `SigilMaterialSubstance` target.
- [Values](reference/VALUES.md): the public value catalog.

## Build and test

```sh
cmake --build build --config Release --target material_test material_bench
build/bin/Release/tests/material_test
```

`material_test` contains all feature cases; `material_bench` measures their
CPU work. The `MaterialGpu` suite requires Metal and checks shipped shader
bodies through Graphite. The `Ocio` suite carries the `ocio` label for its
OpenColorIO requirements. The README and linked chapters compile against
the public headers through `material_api_doc_probes`.
