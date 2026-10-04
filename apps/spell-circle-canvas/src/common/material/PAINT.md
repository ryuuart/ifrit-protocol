# SigilMaterial — the Skia paint

This chapter covers direct Skia drawing and filter execution. Most callers
build a `Material` and pass it to their host. A renderer uses
`material::skia::paint` to lower its base and layers into a `Paint`, or
`material::skia::base` to wrap a paint as a material. `Filter` operates on
rendered coverage or pixels. The remaining sections describe their frame,
equality and cache contracts.

## The Skia paint

`Paint` is the model as ONE `sk_sp<SkShader>`. A small tree of
paint nodes — a solid, an N-stop `linearGradient`/`radialGradient`/
`conicGradient` over `ColorStops`, an `image` or a caller-owned `buffer`, a raw `sksl` effect, a
`blend` stack, or `recipe` over a `Material` instance — that compiles to
a single shader through nested `SkShaders::Blend`, never a stack of
saveLayers. Its slots nest and still compile to one shader.

**A paint declares its own volatility, and the declaration is what it
READS.** Three tiers, and nothing chooses between them by hand:

- STATIC — a solid, a ramp, an image, a blend of those, or an `sksl`
  effect with only constant uniforms. It resolves eagerly, so
  `isSolid()`/`solidColor()` or `staticShader()` answer with no frame at
  all and a consumer caches and prunes it like any other value.
- GEOMETRY — an effect declaring `uResolution`, `uWorld`,
  `uLocalToSample` or `uContentScale`, a `worldSpace()` flag, or an image
  or buffer carrying a `fit()`. It depends on the box and the destination's scale, not on the
  clock: `geometryDependent()` is true, and `shaderFor(frame)` answers
  against the box and the scale the frame names.
- LIVE — an effect with a uniform bound to a live value, or one reading
  `uTime`. `isRunning()` is true and the paint is
  rebuilt every draw; a live CHILD or blend layer makes its parent live,
  which is what stops a cache from freezing the parameter.

**A TABLE AND A SECOND SOURCE ARE BOTH DOORS ON `sksl`.**
`slot(name, Paint)` fills a `uniform shader NAME` slot with another whole
paint — an index texture, a mask, a noise field, a second gradient — and
`set(name, std::vector<float>)` fills a complete floating declaration,
matched against its TOTAL float count, so 1024 floats fill
`float4 uPal[256]` and a count that is not the declaration's is refused whole
rather than written partly. `bind(name, shared_ptr<const UniformBlock>)`
reads the uniform's published values at each paint. Both flat uploads also support
floating scalars, vectors and matrices; integer declarations are refused.
Typed scalar, float2 and float4 setters require the matching non-array
declaration. The last accepted constant replaces earlier typed or packet
constants with the same name; live bindings still override constants.
A rejected upload retains the previous input. The block
starts with published zeros; `values()` edits a draft and `commit()`
publishes the whole array without re-description. Scalar, clock and
geometry changes retain the last committed array. Together they are what
a FIXED PALETTE needs: the picture is one channel of indices and the
table is one uniform array, or — when the lookup is dynamic, which is the
usual case, since the index is a pixel value — one 256 x 1 image
sampled nearest at the texel centre. Neither door asks for a variant
baked per palette. A slot rides the volatility tier and the prune
signature: a live source makes the parent live, and two paints with
different sources never compare equal.

**Lit surfaces shade a material's colour stack.**
`material::skia::lit(material, lighting)` combines that stack with its normal,
roughness, metallic, occlusion and emission maps. Live light bindings and
surface channels resolve when drawn. With no lighting, or a surface stated
`unlit`, the result is the colour stack. `material::skia::lightingFor` selects
the surface's own lighting when stated, otherwise the inherited lighting.
Direct contributions and ambient shares add; environment, emission and
coating attenuation run once per pixel. Shader variants use the exact source
kinds and count, with bounded reuse and no fixed light-count ceiling.

Retain `material::skia::LitSurface` while the material stays the same. Copies
share its prepared inputs and most recent lighting setup. Construction
retains sources without reading pixels; each draw resolves live inputs and
binds sources to its destination. Changing a light reuses preparation, but
the resulting shader still evaluates its inputs at each painted pixel.
`material::skia::LitSurface::under` returns an ordinary paint with a frameless
snapshot, including source reads. A different material needs a different
prepared surface. The current environment's lowered image is retained
independently; rotation, intensity and size reuse that source, while a
different image replaces it.
For a unit-mapped input, `material::skia::LitSurface::shader` takes the
node's frame and a unit-square-to-node matrix. It maps color and surface
channels independently from lighting: positioned sources use the actual
node coordinates, normal slopes follow the node's placement, and
root-anchored inputs stay in root coordinates. Copies share the prepared
inputs; the resulting shader samples node coordinates for this draw.
Height-derived normals take logical-pixel steps through the mapping;
encoded normals keep their slopes. Projective mappings flatten derived
height relief because a constant sampling metric cannot describe them.
Framed draws retain one raster and one device atlas for the resolved environment,
its extent and recorder. Roughness selects and interpolates spherical reflection lobes;
coating uses its own lobe, and ambient light uses a cosine-weighted hemisphere.
Changing rotation or intensity reuses the atlas. A changed source, extent or
recorder replaces the affected atlas. Framed draws blend the two prepared
bands nearest a pixel's roughness; frameless snapshots convolve the
environment at the exact roughness instead, without preparing a raster atlas.

**A TEXTURE STANDING ON A DEVICE is bound where it stands** when the frame
a paint resolves against names the recorder drawing it
(`FrameData::recorder`) — a GPU Substance cook, frames another application
publishes — and read back into host memory once where it does not: a
static snapshot, a raster canvas, a picture recorded to replay.

**A PASS body is not a shader of its own.** A material handed to a text
runtime's pass is written against declarations that runtime prepends once
it knows the track's unit count — `uContent`, `uUnitRect[N]`,
`uUnitPhase[N]`, `kUnitCount` — so compiling it standalone names four
things that do not exist yet and reports one error per mention, about a
compile nobody asked for. `Paint::recipe` recognises such a body by those
names and builds no static shader for it:
the picture comes from `resolvePass`, and used as an ordinary fill the
material draws nothing rather than failing loudly at load.

`skia::PassInputs` is the public input to `skia::resolvePass`: the layer
shader, a unit count, four floats per unit for its rectangle, and two for
its progress and stable seed. The arrays are borrowed only for that call.
The paint owns specialization and program reuse; repeated calls with the
same recipe and unit count share one compiled program.

`FrameData` is what one draw supplies and no author sets: the box, the
root's size, the box→root matrix, the clock and the device scale. A
`worldSpace()` paint anchors to that matrix — the field is authored once
against the root and every flagged box samples it where it actually sits,
through its own transform — and with an identity matrix it degrades to
box-local rather than answering wrongly.
The same flag on a `Material` survives paint lowering, including a layer stack;
its root resolution is the canvas extent. A flagged child in a local material
keeps its root placement. `skia::usesWorldSpace` inspects these dependencies
without fetching image sources.

A raw `sksl` effect can declare `uniform float4x4 uWorld` to read the
node-local → root transform while keeping its input coordinates node-local.
The frame injects a column-major homogeneous matrix; multiply it by
`float4(p, 0, 1)` to obtain the root position. It is geometry-dependent and
placement-sensitive, and unchanged placement reuses the shader. This does not
set `worldSpace()` or anchor the shader a second time. Recipe-backed materials
receive their declared `float3x3 uWorld` from the same frame transform.

An explicit `set("uWorld", std::vector<float>{…})` with sixteen column-major
floats, or a sixteen-float `UniformBlock` binding, owns the raw matrix and
disables frame injection. A block reads only committed values. Other raw
`uWorld` types and arrays are not automatically injected. No transform enters
the raw shader's resolve key when the matrix is caller-supplied, unless the
paint separately requests `worldSpace()` anchoring.

**Equality is the RECIPE, and it is load-bearing.** Two paints built from
the same values compare equal though each minted a fresh `SkShader`,
which is what lets a consumer prune across rebuilds; slots and blend
layers ride the signature, because a slot left out of it would let a
holder prune while its second source had changed. An `sksl` paint
compares by EFFECT POINTER, so a helper that compiles a fresh
`SkRuntimeEffect` per call never compares equal to itself — compile once
and hold the paint. Every mutation is copy-on-write, so binding on a copy
never reaches the value it was copied from.

**Post-processing is the other half of the same frame.** `Filter`
takes the layer a consumer has already rendered and runs a filter over
it: `filter()` wraps any `SkImageFilter`, `shader()` an SkSL program
whose `content` slot IS that layer, `recipe()` a `Material` in the same
position — and any slot that material's recipe declared as one an
executor fills, filled here from that same layer through the filter it
named — and `blur()`/`directionalBlur()`/`glow()` are the three named
spatial ones. `brightPass()` is the layer with everything but its light
taken out — what is over a threshold, faded in across a knee, carried at
its own coverage — which is the first half of a bloom on its own: chain
it with a blur and lay the result back over the source with `kPlus`, and
the whole cost is one tap and a separable Gaussian. It gates on the
STRAIGHT colour and rewrites the coverage from it, because what it emits
is a layer: a pixel half covered by white is white, and a gate on the
premultiplied colour would call it grey and eat the edge of every source
there is.

It reads its own pixel and no neighbour, so it is a COLOUR MAP —
`colorFilter()` answers it and `imageFilter()` does not — and that is
what keeps `emit()` honest. A filter graph holding a program over
COORDINATES is evaluated in the layer's own coordinates and resampled
onto a scaled canvas, so a light built from one softens the sharp layer
it is laid back over even where that light is wholly transparent; a
colour map carries no such constraint and the layer keeps the device's
own pixels. `deepen()` and `whiten()` are colour maps for the same
reason, and all three compare by their numbers, so a re-described equal
stage prunes.

A glow is made of stages, each an ordinary effect chained with `then()`:
`Filter::blur(sigma)` spreads light; `Filter::dilate(pixels)` grows its
coverage outward first, so the colour carries past the source as a rounded
body — a blur at one and a half times the distance with its coverage
quadrupled, which restores an edge's full coverage one distance out, so it
spreads a light (a bright pass, or a layer on transparency) and is only a
blur over an opaque ground; `Filter::deepen(amount)` lets faint light lose
its weaker channels first, raising the straight colour to one plus the
amount times the missing coverage, so a halo sinks toward its strongest
channel as it thins — orange toward red, yellow toward orange, a
blue-leaning cyan toward blue; and `Filter::whiten(amount, threshold,
knee)` moves a lit core toward white at its own peak, the other end of the
same tone curve. `emit(light, mode)` is the one join `then()` cannot say:
the light runs over the same input the effect does and is blended over its
output, so `Filter().emit(light)` is the layer with its light screened over
it, and a second `emit` stacks another light of the layer rather than a
light of the first.

`Filter::bloom` from `<sigilmaterial/skia/Filter.h>` is those stages composed:
the bright pass, dilated, blurred at a near and a broad radius, each
deepened and weighted, added together under an opacity ceiling that keeps
dark lettering visible inside luminous panels, and emitted over the source
after that source's own softness blur and whitening. Its
`BloomOptions` name each stage's amount. Hold the effect so its
filter graph is shared across descriptions.

`phosphorBloom()` is the display post-process: the same bright pass,
gated premultiplied because it emits light to add rather than a layer,
feeding three radii whose RGB channels have different reach, so the
feather changes hue while the sharp source remains on top. A gather is
what it costs — twenty-four samples per pixel, three radii of eight
headings, where `brightPass()` takes ONE and hands the spreading to
Skia's own separable blur — so the gather is not spent at the layer's own
resolution. THE HALO IS GATHERED COARSE: the bright pass, the rings, the
hue drift and the tail run over a layer reduced until the innermost ring
is about a pixel across, and the result is resampled up and added to the
untouched source, which is one tap of each. A halo is a low-frequency
picture and survives that; what moves is the halo's fine structure at a
hard-edged source, never the source itself, which is composited at full
resolution and to the bit. A reach too small for the reduction to leave
anything behind is gathered whole, and a wider reach is gathered coarser
— down to a quarter, past which the bright pass would alias on its own
sources — so a wide bloom is not the wide gather it looks like. Painted
in a box, the bloom declares its reach — the box grown by the radius and
one pixel of the reduced layer — and every stage of it runs over that and
no more, so a bloom on a caption line costs a caption line and not the
canvas it stands on; without a box, Skia gives its runtime shaders the
whole clip. A glow that does not move is still worth baking: put the
glow sources on a node of their own and let the host bake that node to a
texture, and the bloom is gathered once rather than every frame. `then()` chains effects,
and the same tier rules hold — a bound
uniform or a live slot makes the effect live, and a static chain
precomposes once. It resolves against the same `FrameData` a paint
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
