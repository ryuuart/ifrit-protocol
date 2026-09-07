# SigilMaterial — the Skia paint

The chapter on `skia::Paint` and `skia::Effect`: the model as ONE Skia
shader, the three volatility tiers a paint declares by what it reads,
the blend stack, the unit-square ramps, the buffer a caller writes into,
and the post-processing recipe over a layer that is already rendered.
`README.md` beside this file is the library; `COLOUR.md` is the colour
leaf underneath it.

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
- GEOMETRY — an effect declaring `uResolution`, a `worldSpace()` flag, or
  an image or buffer carrying a `fit()`. It depends on the box, not on the
  clock: `geometryDependent()` is true, and `shaderFor(frame)` answers
  against the box the frame names.
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
spatial ones. `brightPass()` is the layer with everything but its light
taken out — what is over a threshold, faded in across a knee, carried at
its own coverage — which is the first half of a bloom on its own: chain
it with a blur and lay the result back over the source with `kPlus`, and
the whole cost is one tap and a separable Gaussian. It gates on the
STRAIGHT colour and rewrites the coverage from it, because what it emits
is a layer: a pixel half covered by white is white, and a gate on the
premultiplied colour would call it grey and eat the edge of every source
there is.

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
sources — so a wide bloom is not the wide gather it looks like. The
layer is still worth bounding: put the glow sources on a node of their
own and let the host bake that node to a texture, and the bloom is baked
with them once rather than gathered over a whole canvas every frame. `then()` chains effects,
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
