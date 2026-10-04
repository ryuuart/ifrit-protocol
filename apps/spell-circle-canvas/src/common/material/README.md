# SigilMaterial

SigilMaterial describes the appearance of a region or surface. A `Material`
is a comparable value: a color, gradient, image or shader, optionally built
up with layers, lighting properties and effects. Compose, Draw and World
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

The four parts have distinct jobs:

| Part | Meaning |
|---|---|
| Base | The initial appearance: a color, gradient, image, shader or another material. |
| `layer()` | Blend another material over the accumulation, with optional opacity and mask. |
| `surface()` | State the lighting response: roughness, metallic, normals and related channels. |
| `effects()` | Apply a filter chain to rendered coverage: shadows, glows, strokes or bevels. |

A layer mask takes a material source, a channel, a `[low, high]` range and
an optional inversion. The range maps to coverage `[0, 1]`. Surface channels
can be constants or materials where their types allow it.

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
Compose evaluates composed normal materials; World's current surface binder
accepts direct `Texture` slots and does not evaluate general composed channels.

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

## Lighting

A surface response is optional. In Compose, put the lighting on a parent
and let its children inherit it:

```cpp
#include <sigilmaterial/core/Lighting.h>
#include <vector>

scene.lighting(material::studio({.direction = 30, .elevation = 35}));
```

`scene` is the caller's Compose element. `studio()` supplies a directional
light; `environment()` supplies reflected and ambient light from a
panorama. `Lighting::lights` holds the direct sources, alongside one
environment. A single light still converts to a lighting value; use a vector
for a rig:

```cpp
material::Lighting rig(std::vector<material::Light>{
    material::studio({.direction = 30, .elevation = 35, .ambient = 0.1f}),
    material::studio({.direction = 160, .elevation = 60, .ambient = 0.1f}),
});
scene.lighting(rig);
```

The direct contributions add. Their ambient shares also add; an environment
with no direct sources supplies a full ambient share. With direct sources
present, their summed `ambient` shares scale the environment's diffuse
light instead. An environment with no image is no environment. Environment reflection,
emission and coating attenuation run once over the sum. A material's own
`SurfaceOptions::lighting` overrides the inherited lighting.

The Skia executor also reads `LightKind::Point` and `LightKind::Spot`.
`Light::position` uses root-page logical pixels: X right, Y down, Z toward
the viewer. The page stays flat at Z=0; normals change response without
displacement or shadows. The authored range falloff is
`(1 - distance²/range²)²`, zero beyond range; spots also interpolate their cone
in cosine about the source-facing direction/elevation axis. Positioned lights
depend on affine node-to-root placement, with inverse-transpose XY normals;
perspective or degenerate transforms disable their direct light. Directional
lighting uses node-local normals under the default `LightingFrame::Surface`.
That frame's environment uses page normals when a source is positioned,
local normals otherwise. `LightingFrame::Scene` instead carries normals
through affine node-to-root placement for every source and the environment.
Positioned sources use root-page coordinates in either frame. Scene-frame
lighting therefore follows receiver placement even with only a directional
source or environment.

In 2D, the lighting pass reads the material's color stack and surface
channels. With no lighting, or with `unlit` set, it paints the colors flat.
In World, the renderer supplies its own lights. Effects require rendered
coverage and are ignored by a renderer that has none.

In Skia, a positive `SurfaceOptions::alphaCutoff` removes color-stack samples
whose alpha is below the threshold, with or without lighting. Samples at or
above the threshold keep their color and alpha; shape antialiasing still
applies afterward. Zero preserves ordinary blending.

The Skia 2D executor reads `SurfaceOptions::clearcoat` as a dielectric coating
weight, clamped to `[0, 1]`; nonfinite weights disable it. The coating has fixed
roughness 0.2 and index 1.5, shares the surface's shaded normal and adds a neutral
GGX highlight under direct light. Its environment reflection uses its own
roughness lobe, independent of the base roughness. Its Fresnel weight attenuates the
underlying lighting and emission. Environment reflection respects occlusion
and `reflectionWeight`. Zero coating preserves the uncoated response.

Environment reflections spread with scalar or mapped roughness. The executor
prepares spherical GGX samples at eight roughness levels and interpolates them
per pixel; ambient light uses a cosine-weighted hemisphere. This is a finite-sample
approximation, so very narrow sources can alias. A fully rough surface still
reflects its environment. Direct base lighting retains its approximate model.
This 2D model has no separate coating roughness or normal channel, refraction
or scattering between layers. World
surface lowering does not implement clearcoat and reports a nonzero request.
Without lighting, and for an unlit surface, the coating does not alter colors.

Light direction is in degrees counter-clockwise from three o'clock;
elevation is above the page. Light color, strength and applicable angles
can be bound to motion values. Point lights ignore direction and elevation.
Ambient, kind, position, range and cone angles are plain settings.
An environment's rotation can be bound; its intensity and extent are plain
settings. A material has no motion clock: a described tween reads its resting
value here. Run it into a live cell on the host's motion engine to animate it.
Advancing a bound value updates the lighting pass while retaining the color
stack below it:

```cpp
#include <sigilmotion/values/Animatable.h>

auto tint = sigil::motion::animatable(material::Color{1, 0.8f, 0.6f, 1});
material::Light key{.color = tint};
scene.lighting(key);
tint = material::Color{0.6f, 0.8f, 1, 1};
```

Skia hosts that replace the lighting value each frame retain a
`material::skia::LitSurface` for the material and call its `under()` with
the lighting in force. It shares the lowered colour stack and surface maps
across changes to source position, kinds, count or frame. Construct another
when the material changes. A shared one-entry cache retains the current
environment image across rotation, intensity and size changes. Framed draws
also retain one raster and one device reflection atlas for the resolved image,
extent and recorder. Rotation and intensity change sampling; a changed image,
extent or recorder replaces the affected atlas. Framed draws blend the two
prepared roughness bands nearest a pixel's roughness; frameless shaders
convolve the environment at that exact roughness instead, without
preparing an atlas, so the two agree closely but not exactly.
`material::skia::lit` remains the one-call spelling.

A renderer that brings its own lights reads the same prepared inputs
UNSHADED, one surface map at a time: `LitSurface::asMap(role)` and the
one-call `material::skia::asMap(material, role)` answer the base colour,
normal, roughness, metallic, occlusion or emissive map, keyed by
`texture::Role`, as the texture of that role encodes it. The normal comes
back as `(n + 1) / 2` with green up the picture whatever convention the
material stated, including a height-derived or blended normal; a scalar
comes back grey, its number times its map; a role a material states
nothing for reads the stock `SurfaceOptions` number. Every map carries the
colour stack's alpha as its coverage. A material with no surface, or an
`unlit` one, is its own colour: emissive is the colour stack, the base is
black, the normal faces the viewer, roughness and occlusion are one and
metallic is zero. `LitSurface::mapShader` places the maps for one draw the
way `shader()` places the lit pass.

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

## Features and dependencies

Include the originating header and link the feature used by the consumer.
`SigilMaterial` is an umbrella target for applications that use the whole
library; smaller consumers can select these targets:

| Target | Purpose |
|---|---|
| `SigilMaterialColor` | Color values, conversions, ramps, palettes and dithering. |
| `SigilMaterialCore` | Material values, layers, surface declarations, lighting and parameter reflection. |
| `SigilMaterialTexture` | Pixel sources, sampling, texture sets, environment maps and atlases. |
| `SigilMaterialProgram` | Shader source and resource factories. |
| `SigilMaterialSkia` | Skia paints, gradients, filters, shader compilation and lighting execution. |
| `SigilMaterialSlang` | Slang compilation and reflected uniform layouts. |
| `SigilMaterialSurface` | Surface programs and lowering for lit renderers. |
| `SigilMaterialMask` | Mask generators and transformations. |
| `SigilMaterialSdf` | Signed-distance shapes and their appearance. |
| `SigilMaterialPattern` | Baked tiles and cloth generators. |
| `SigilMaterialField` | Noise, grain, halftone and ripple fields. |
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
