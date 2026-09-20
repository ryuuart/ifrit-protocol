---
kind: type
library: SigilMaterial
name: Effect
qualified: sigil::material::skia::Effect
group: The Skia paint
status: stable
---

# Effect

POST-PROCESSING OVER A LAYER THAT IS ALREADY RENDERED, as a comparable
value. A paint shades a shape; an effect takes the picture a consumer has
drawn and runs a filter over it — a blur, a glow, a bloom, a colour map,
a displacement, or an SkSL program whose `content` slot IS that layer.

The one-sentence difference from a paint: a paint answers "what colour is
this pixel of this shape", an effect answers "what happens to the pixels
already there".

## Anatomy

Every effect is built by a static factory and then narrowed with the
modifiers. `Effect::isAnimated` is the volatility declaration — one word
across the whole library — and it is true while any uniform is bound or
any child material is live. `Effect::usesWorldSpace` asks whether any
child paint anchors to the root frame.

`Effect::imageFilter` is the built filter; `Effect::colorFilter` is the
colour lane, set only by the colour-filter factory and by a lowered
recipe, and the two are never both present. `Effect::resolvedImageFilter`
is the filter with bound uniforms resolved NOW, against the painting
node's frame — which is what the paint phase applies.

`Effect::then` chains: `next` runs after this one. `Effect::emit` makes a
LIGHT from the same input this effect reads and blends it over this
effect's own output, so lights stack rather than compound — a second
emit adds a light of the layer, never a light of the first light.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `Effect::filter(imageFilter)` | C++ | any Skia image filter — blur, displacement, lighting, compose chains |
| `Effect::filter(colorFilter)` | C++ | a per-pixel colour map with no neighbourhood, applied to the layer's paint as a blit rather than a pass |
| `Effect::recipe(material)` | C++ | a recipe as the effect: its program runs over the layer, which arrives in the slot named `content` |
| `Effect::recipe(material, sampleRadius)` | C++ | the same, bounding the largest local-coordinate source offset |
| `Effect::recipe(material, surface)` | C++ | the same recipe LOWERED for the surface it will land on — a channelwise recipe on an eight-bit surface becomes a table and no program at all |
| `Effect::shader(effect)` | C++ | an SkSL runtime effect whose content slot is the rendered layer |
| `Effect::glow(colour, sigma)` | C++ | the layer re-emitted blurred beneath itself, which keeps the content on top |
| `Effect::blur(sigma)` | C++ | the plain blur |
| `Effect::blur(sigmaMap, maxSigma)` | C++ | a blur whose radius is read from a paint, per pixel |
| `Effect::directionalBlur(sigma, angleDeg)` | C++ | the blur along one axis |
| `Effect::brightPass(threshold, knee)` | C++ | the layer with everything but its light taken out — the first half of a bloom |
| `Effect::phosphorBloom(radius, threshold)` | C++ | the whole bloom in one gather |
| `Effect::dilate(pixels)`, `Effect::deepen(amount)`, `Effect::whiten(amount)` | C++ | the small tonal passes |
| `material.skia.Effect.blur(...)` and the rest | Python | the same factories under the same names |

Then the modifiers: `Effect::uniform` sets or binds a named uniform —
including a `motion::Animatable<float>`, which makes the effect live —
and `Effect::slot` fills a declared `uniform shader` with a paint.

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `Effect::then` | member | SigilMaterial — as the next link of a chain |
| `Effect::emit` | member | SigilMaterial — as the light |

Outside this library an effect is what a node's own layer filter and its
backdrop filter take, and what a composer's view transform is stated in.
Those slots belong to the libraries that own them and each spells this
library's name for the value.

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| every factory above | function | SigilMaterial |
| `Effect::then`, `Effect::emit` | member | SigilMaterial |
| `skia::bloom` | function | SigilMaterial |

## Description

An effect is a value so that a consumer caching a filtered layer can
prove two frames asked for the same one. A static shader effect compares
by RECIPE — the runtime effect pointer plus its constant uniforms — so a
re-described effect prunes as long as the caller holds ONE runtime effect
and rebuilds only the wrapper around it. A filter effect compares by
filter pointer, because an already-built image filter carries no recipe
to compare. A live effect never compares equal, conservatively, like a
live material.

A recipe effect reads its material's bindings ONCE, at construction, so
animation happens by re-describing rather than by the material moving
underneath it. That is the difference from a recipe PAINT, which
re-resolves per frame, and it is why the two exist side by side.

Where an effect is applied is a consumer's decision: it is attached at a
stacking-context boundary, and where the consumer caches that layer an
expensive filter over static content is paid once.

### The two lanes

A COLOUR FILTER is a per-pixel colour map with no neighbourhood, so a
consumer applies it to the layer's paint rather than through the filter
graph and pays a blit instead of a pass. `Effect::colorFilter` is how
that consumer reads it back, and it is set by the colour-filter factory,
by the lowered recipe, and by the per-pixel stages —
`Effect::brightPass`, `Effect::deepen` and `Effect::whiten`. An effect
built that way carries no image filter of its own, and where one is
asked for — an `Effect::then` chain,
`Effect::resolvedImageFilter` — the colour filter is lifted into the
filter graph so the picture is right either way. The two are never both
present.

`Effect::resolvedImageFilter` is the filter with any bound uniforms
resolved NOW, which is what the paint phase applies; it is identical to
`Effect::imageFilter` for a static effect. Its paint frame is the
painting node's, which the slots' materials resolve against — its box,
its clock — exactly the context `Material::slot` hands its sources. Null
is the context-free form: static children keep their snapshot, and it is
what a caller holding an effect outside a paint can ask for.

### The stages, one by one

**`Effect::recipe` reads its material's bindings ONCE, at
construction**, so animate by re-describing. Its program runs over the
layer, which arrives in the slot named `content`; every other slot and
every uniform is bound from the material as it stands. A sample radius
bounds the largest local-coordinate source offset. A static material
compares by its value and compiled program; a material with live inputs
compares by the built filter's identity, because those inputs were
sampled once.

**Lowered for the surface it will land on.** A recipe that declares
itself channelwise maps each channel through one row of samples and
touches nothing else, which on a surface carrying eight bits per channel
is a 256-entry table per channel and no program at all: the surface
overload answers that table as a colour filter, which the consumer hangs
on the layer's paint. Every other case — a recipe that is not
channelwise, a row that is not 256 samples wide or cannot be read, a
surface with more precision than a table can carry, and the unknown
colour type, which is what a canvas backed by neither raster nor GPU
answers — falls back to the program, so the picture is the same either
way and only the cost differs. The surface argument is the colour type
of the surface the effect will be composited on, which a consumer reads
from its canvas at the moment it paints.

**`Effect::glow`** re-emits the layer blurred beneath itself in a
colour — a drop shadow at zero offset, which keeps the content on top.
Chain it for a tighter core over a wider halo.

**`Effect::brightPass` is the layer with everything but its light taken
out**: what is brighter than the threshold, faded in over the knee above
it, carrying that brightness as its own coverage. It is the first half
of a bloom on its own, so a consumer can spend a blur and an additive
composite where `Effect::phosphorBloom` would spend a gather — chain it
with a blur and lay the result back over the source.

The gate is read on the STRAIGHT colour and the coverage is rewritten
from it, because what comes back is a layer rather than light to add: a
pixel half covered by white is white, and gating it premultiplied would
call it grey and eat the edge of every source in the layer.
`Effect::phosphorBloom`'s own gate reads the premultiplied colour for
the opposite reason — it never emits a layer, it accumulates energy, and
there coverage IS part of how much light a pixel contributes.
Brightness is the peak channel, not luminance, so a saturated primary
blooms as readily as a white, which is what a phosphor and a lamp both
do and what a luminance gate would refuse a deep blue source.

It reads its own pixel and no neighbour, so it is a COLOUR MAP and
`Effect::colorFilter` answers it rather than `Effect::imageFilter`. That
is what keeps `Effect::emit` honest: a filter graph holding a program
over coordinates is evaluated in the LAYER's pixels and resampled onto a
scaled canvas, so a light made with one would soften the sharp layer it
is laid back over even where the light is wholly transparent. A colour
map carries no such constraint and the layer keeps the device's own
pixels.

**`Effect::dilate` is the rounded spread**: every edge grown outward by
some pixels, so the layer's colour carries past it as a body before
anything feathers it, as a shadow's spread does. A blur at one and a
half times the distance leaves a quarter of an edge's coverage one
distance out, and quadrupling coverage restores it there, so corners
stay round and the gaps between letters stay open until the spread
reaches them — where a square morphological kernel would fill them as
plates. The straight colour is kept. It grows COVERAGE, so over an
opaque ground it is only a blur: spread a light — a bright pass, which
carries brightness as coverage, or a layer drawn on transparency.

**`Effect::deepen` lets faint light lose its weaker channels first**, as
a tone curve's toe drops them: the straight colour, normalised to its
peak, is raised to one plus the amount times the missing coverage, so a
dense layer keeps its colour and a thin one sinks toward its strongest
channel — orange toward red, yellow toward orange, a blue-leaning cyan
toward blue — with its brightest channel held. Over a blurred light, the
halo deepens as it fades. **`Effect::whiten` is the other end of that
curve**: where the straight colour's peak is above the threshold, faded
in over the knee, it moves the given fraction of the way toward white at
that peak, as an overexposed core does, so a lit shape reads lighter
than the deeper light around it. Colour below the threshold is
untouched.

**`Effect::phosphorBloom` is display bloom over the completed layer.**
Pixels above the threshold feed three concentric kernels; their red,
green and blue channels are recombined with progressively different
reach — red the widest, blue the tightest, as a phosphor's own spread
is — so the feather changes hue instead of behaving like a same-colour
software blur. The radius is the outer kernel radius in pixels, the
intensity its additive energy, and the chroma blends from an achromatic
falloff at zero to full spectral separation at one. The sharp source is
retained on top.

The hue drift is the turn, in degrees, the halo's hue has made at the
outer radius: each kernel turns in proportion to its reach, and only
where the pixel is lit by a halo rather than by a source of its own, so
the source keeps its colour and its decay tail drifts. A NEGATIVE turn
is the direction a phosphor decays — a warm source's halo goes through
orange toward red, a cool source's through cyan toward green. The tail
is extra energy on the outermost kernel beyond the three-kernel
falloff, so a stronger glow reaches further rather than only brighter.
Both default to zero, which is exactly the falloff without them.

THE HALO IS GATHERED COARSE AND LAID BACK OVER THE SHARP SOURCE.
Twenty-four samples of the layer per pixel — three radii of eight
headings — is what a gather costs, so it is not spent at the layer's own
resolution: the bright pass, the rings, the drift and the tail run over
a layer reduced until the INNERMOST ring is about a pixel across, and
the result is resampled up and added to the untouched source, which is
one tap of each. A halo is a low-frequency picture and survives that;
the reduction is why the effect costs near a bright pass rather than
twenty-four times one. What it changes is the halo's fine structure — a
hard-edged source hands its step to a resample — and never the source
itself, which is composited at full resolution and to the bit. A reach
small enough to be blurred away by the reduction is gathered whole
instead. The layer this runs over should still be bounded: put the glow
sources on their own node and let the host bake that node to a texture,
and the bloom is baked with them once.

**`Effect::shader`** takes float uniforms set by name on the SkSL
effect; the layer arrives as the slot named `content`. A name the effect
does not declare as a float uniform — a typo, or a float2, float4 or
array, none of which that door can fill — is warned about once and
IGNORED, never a debug abort: one typo in a live-reloaded sketch must
not take the host process down.

**`Effect::directionalBlur` smears ALONG one direction**: the sigma
along the axis at the given angle (degrees, screen sense — 0 smears
horizontally, 90 vertically, 45 down-right), and the across value
perpendicular to it, zero by default for a pure streak. It is a spatial
filter, not motion blur: it knows nothing about how the node moved.

It is built entirely from existing filters, with no new SkSL. At an
axis-aligned angle it IS Skia's two-axis blur, bit-identical; at any
other angle it is a rotate → blur → unrotate sandwich, three nodes the
filter graph composes. Unlike a raw filter it carries a comparable
RECIPE, so a re-described equal directional blur PRUNES where a filter
— which can only compare its already-built filter by pointer — does
not. The named parameters `sigma`, `angle` and `across` accept a bound
uniform, so an animated smear angle rides the live channel instead of
re-describing per frame.

**The parametric blur's SIGMA VARIES ACROSS THE NODE** — a
depth-of-field falloff, a lens edge, a tube's curvature. The sigma map
is a paint read as a NUMBER rather than as paint: its RED channel at a
pixel, times the declared maximum, is the blur radius there. The natural
authoring is therefore a unit-space ramp — `Paint::linearUnit` from
black at the left edge to white at the right is "sharp at the left,
softest at the right" over whatever box the layout decides — and any
`Paint::sksl` paint is an arbitrary field.

It is written as its own effect rather than left to `Effect::shader`
because a hand-written SkSL kernel would have to pay the WORST sigma at
every pixel: SkSL has no cheap dynamic loop bound, so the kernel must be
sized for the largest radius anywhere in the node, and a Gaussian stops
being separable once sigma varies. This spends a fixed number of passes
instead, so cost grows far more slowly with the declared maximum. How it
spends them is the library's business and deliberately absent from the
signature: the author says "blur varying by this map".

It rides the same rails as the directional blur — a comparable recipe,
in which the sigma map's paint participates; the named parameter
`maxSigma` accepts a bound uniform; and a LIVE sigma map makes the whole
effect animated by tier inheritance, so a bake can never sample the map
once and freeze it. Filling the slot named `sigma` re-aims the map on an
existing blur.

THE DECLARED VALUE IS THE RANGE A BOUND SIGMA RIDES INSIDE. The passes
are built once from the declared maximum and held; a bound `maxSigma`
re-wraps only the final mix with a scale, so a sigma that breathes every
frame costs the same fixed passes over the same held inputs and Skia's
filter cache keeps hitting. The result is exact at the pass sigmas and
linear in sigma between them, and a bound value above the declared range
clamps to it. Declare the LARGEST sigma the binding will reach: a
declared zero declares no range, and a bound value then rebuilds every
pass at every paint, which is the full cost the range exists to avoid.

### The slot and the uniforms

`Effect::slot` is `Material::slot` on the effect seam: same name, same
shape, same semantics. The effect declares `uniform shader NAME;` and
this fills it with a paint, so the SkSL can read a source the node has
NOT painted — a parameter field, a mask channel, a gradient, a second
texture. `Effect::shader` fills exactly one slot itself, `content`, the
node's own rendered layer, and this is how any further declared
`uniform shader` gets a source. The paint resolves against THIS NODE's
box, so unit-space authoring works here as it does on a fill.

TIER INHERITANCE is the load-bearing half, and it calls the material's
own recursion rather than repeating its rule: a live source makes the
effect animated, so the node is declared volatile and no cache can
freeze the parameter; the slots also ride the prune signature, so two
effects with different sources never compare equal.

The guardrails match. A name the effect does not declare as
`uniform shader` warns and is IGNORED. On an effect kind with no slot to
fill — a wrapped image filter, or a bare directional blur — the call is
a no-op with a warning, exactly as a uniform is there. On a parametric
blur the one fillable name is `sigma`, its sigma map.

`Effect::uniform` has the same four shapes a material's does. A LIVE
float is read from the bound value at every paint, and the node repaints
every frame while the effect is attached: a bound uniform declares
volatility exactly as a live material does, which is what lets a ripple
phase or a bloom threshold animate without re-describing. It is
meaningful on a shader effect (any declared float uniform), on a
directional blur (`sigma`, `angle`, `across`) or on a parametric blur
(`maxSigma`). A wrapped image filter has no uniform to receive the
value: the binding warns and is ignored there, and no volatility is
declared, so nothing animates. Every other rejection behaves the same
way — a name a shader effect does not declare as a float uniform, an
unknown recipe name on the other kinds: warned about once, not recorded,
and no volatility declared for it, because a binding nothing reads must
not cost a repaint per frame forever. It takes an animatable, so a
shaped binding chain drives the uniform directly; an effect holds no
instance, so a value carrying its own TRANSITION has nothing to run it
and reads as its target.

The CONSTANT shapes are for the sizes the shader constructor list cannot
carry. The float form is that list's late spelling; the float2 and
float4 forms fill `uniform float2` and `uniform float4` declarations;
the vector form fills a declared ARRAY, matched by TOTAL float count, so
12 floats fill `float4 uRect[3]` and `float uWeights[12]` alike. They
are meaningful on a shader effect only — the other kinds have no named
declarations to fill — and an undeclared name, or one whose declared
size is not the value's, warns once and is IGNORED. Constants
participate in equality, so a re-described equal effect prunes.

A LIVE ARRAY is a `UniformBlock` the caller owns, writes and commits,
read at every paint. It declares volatility exactly as a bound scalar
does: the node paints live while the effect is attached, and no cache
can freeze the table. The binding compares by block identity; the values
belong to the system and never prune. It is size-checked at store time
against the declared array's total float count, because the builder
refuses a partial array write.

### Chaining and emitting

`Effect::then` applies the next effect AFTER this one — a blur followed
by a colourising shader is the glass formula. Static chains precompose
once; a chain with a live side re-composes at each paint.

`Effect::emit` is the layer and a light made from it: the light runs
over the same input this effect does, and its result is blended over
this effect's own output with the given mode. So an empty effect that
emits a light is the layer with its light screened over it, and a
whitened effect that emits one is a whitened core under a light drawn
from the untouched layer. Each emit reads that same input, so lights
stack rather than compound: a second emit adds a light of the layer,
never a light of the first light. Static sides blend once; a live side
re-blends at each paint.

### Every program an effect is built out of

`skia::everyEffectProgram` is every SkSL body an effect is built out of,
as one list in a fixed order: the bright pass and the phosphor halo a
bloom gathers, the tap that lays that halo back over the source, the
deepening and whitening of a light, and the mix a parametric blur
interpolates through. The recipes a paint runs are not there — those are
reached through the program cache, one per recipe.

These are the very objects the effects go on to use, not copies of them.
A device backend can be asked to give a runtime effect a name that
survives the run, so a program built over one can be written down and
rebuilt at the next launch instead of compiled again — and it names the
OBJECT, so a second effect compiled from the same source is a stranger
to it. An effect's place in the list is part of its name, which is why
the order is fixed and why a body that would not compile is absent
rather than null: the list is what is really there. Asking compiles all
of them, which is a handful of small SkSL programs beside the device
programs they are inlined into.

## See also

- `skia/Effect.h` — the header: `Effect`
- `skia/Bloom.h` — the header: `bloom`, `BloomParameters`
- [Paint](value:sigil::material::skia::Paint) — what shades a shape, as
  against what filters a layer
- [Material](value:sigil::material::Material) — the recipe an effect can
  run
- The colour chapter on the [SigilCompose](doxygen:SigilCompose) site —
  the lattice whole
