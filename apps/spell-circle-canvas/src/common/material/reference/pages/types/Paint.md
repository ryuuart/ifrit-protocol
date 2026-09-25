---
kind: type
library: SigilMaterial
name: Paint
qualified: sigil::material::Paint
group: The paint
status: stable
---

# Paint

WHAT A 2D REGION IS PAINTED WITH, as one comparable value: a colour, a
gradient, a recipe instance, a stack of paints blended over each other,
or a source a renderer supplied. It is the value a node's fill takes when
the fill is more than a colour, and it is the general paint value the rest
of the tree means by "a paint". Its header names no renderer; the Skia
executor (`skia/Paint.h`) turns it into ONE shader — layers through a
blend shader, never a stack of saved layers — or a plain solid colour.

It is not the raw Skia paint. A Skia `SkPaint` carries a style, a stroke
width and a blend mode for one draw; this carries what a surface is
shaded WITH, and compares by value so a consumer can prune on it. Python
spells the two with the same last word and a different module:
`sigil.skia.Paint` is Skia's own, `sigil.material.Paint` is this one.

## Anatomy

A paint sits in one of THREE VOLATILITY TIERS, and the tier is decided by
what it is made of rather than declared:

| Tier | What puts it there | What it costs |
| --- | --- | --- |
| static | a solid, a gradient ramp, an image or sprite, a blend of static layers, an SkSL effect with only constant uniforms | resolves once; answers its colour or its shader with no frame, so a consumer caches and prunes it like any other value |
| geometry | an effect declaring only `uResolution`, a stated `Paint::fit`, `Paint::worldSpace` | resolves when the node RECORDS and caches between layouts — it depends on the box, not on the clock |
| live | a bound uniform, an effect reading `uTime` or `uContentScale`, a live child | re-resolved every frame; its node is declared volatile and no cache can freeze it |

`Paint::isRunning` and `Paint::geometryDependent` are how the tier is
asked, and a blend or a slot INHERITS the tier of what it holds.

`Paint::isSolid` with `Paint::solidColor` is the short-circuit every
consumer asks first — a solid has no coordinates and nothing to resolve.
`Paint::isNone` is the empty paint, which draws nothing.
The Skia executor answers the shaders: `skia::staticShader` is the one a
non-live paint already holds; `skia::shader(paint, frame)` is the per-draw
one, built against a `FrameData`; `skia::shader(paint)` always produces a
shader, sampling live values at their current value as a snapshot.

## Make one

Every leaf is a static factory, and each names what it is made of.

| Spelling | Language | What it gives |
| --- | --- | --- |
| `Paint()` | C++ | nothing — fully transparent, draws nothing |
| `Paint::solid(colour)` | C++ | a flat colour |
| `Paint::linearGradient(start, end, stops)` | C++ | an n-stop gradient between two points, in the box's unit square unless `{.units = GradientUnits::Pixels}` |
| `Paint::radialGradient(centre, radius, stops)` | C++ | a gradient out of a centre; in box units a radius of 1 reaches the box's corners, or its sides with `{.extent = RadialExtent::ClosestSide}`; `.focus` moves the hot spot while the outer circle stays put |
| `Paint::conicGradient(centre, stops)` | C++ | a gradient swept around a centre; its window CLAMPS outside the circle rather than wrapping |
| `skia::image(image)` | C++ | an image or sprite as a fill, repeated or padded past its edges (`Repeat`) |
| `skia::buffer(source)` | C++ | a caller-owned raster the paint samples — a simulation, a decoded frame, a scrollback |
| `skia::sksl(effect, constants)` | C++ | a runtime effect as a paint |
| `skia::paint(shader)` | C++ | any raw Skia shader — the interop escape |
| `Paint::recipe(material)` | C++ | a `Material` instance as the paint |
| `Paint::blend(layers)` | C++ | layers painted bottom to top, each composited with its own blend mode, flattened into one shader |
| `material.Paint.solid(...)` and the rest | Python | the same factories under the same names |
| a `Material` | Python | implicitly, where a paint is taken |

Then the modifiers, each of which copies on write: `Paint::set` sets and
`Paint::bind` binds a named parameter, `Paint::slot` fills a declared `uniform shader`
with a SECOND SOURCE, `Paint::amount` is the layer strength inside a
blend, `Paint::fit` says how an image meets the box, `Paint::offset`
binds a live pan, `Paint::worldSpace` anchors the coordinates to the
root, `Paint::bleed` reserves recording cull, and `Paint::quantizeTime`
steps the injected clock.

The three gradients take their stops as `ColorStops` — a list of
`ColorStop` (`ColorStop::offset` and `ColorStop::color`), plain colours
spaced evenly, or a `Ramp` — and share one `GradientOptions`:
`GradientOptions::units`, `GradientOptions::extent`,
`GradientOptions::repeat` (`Repeat::Pad`, `Repeat::Repeat`,
`Repeat::Mirror`, `Repeat::None`), `GradientOptions::focus` with
`GradientOptions::focusRadius`, and `GradientOptions::startDegrees` with
`GradientOptions::endDegrees`. `Fit` is
how a source meets the box: `Fit::Native`, `Fit::Stretch`, `Fit::Cover`,
`Fit::Contain`. `BlendMode` is how a layer meets the ones beneath it —
`BlendMode::Normal`, `BlendMode::Multiply`, `BlendMode::Screen`,
`BlendMode::SoftLight`, `BlendMode::PlusLighter` and the rest of CSS's
and Canvas's list. `FrameData` is what one draw supplies and no author
sets: `FrameData::resolution`, `FrameData::rootResolution`,
`FrameData::world`, `FrameData::seconds` and `FrameData::contentScale`.

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `Paint::slot` | member | SigilMaterial — a paint fills another paint's second source |
| `Paint::blend` | function | SigilMaterial — as one layer |
| `Filter::slot` | member | SigilMaterial |

Outside this library a paint is what a node's fill, a stroke's paint, a
text fill and a coverage gate take. Those slots belong to the libraries
that own them; each states the paint vocabulary as this library's, and
none re-exports it.

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| every leaf factory above | function | SigilMaterial |

## Description

The vocabulary mirrors the solid / ramp / image / blend atoms a material
graph format would use, and nothing of the sort is linked: the backend
here is SkSL and Skia's shaders and nothing else. Colour management is
not part of a paint — a view transform belongs to the consumer's output
stage and applies to a whole composite.

### The leaves, one by one

**A focus is what moving a radial's centre cannot do.** With
`GradientOptions::focus` the stops run from the circle (focus,
focusRadius) to the circle (centre, radius), so a highlight displaced off
a sphere's centre is `radialGradient(centre, R, stops, {.focus = hot})`.
Moving the centre instead couples the falloff to the displacement — the
entire ramp slides, including its outer edge — where here the outer
circle stays put and only the hot spot moves.

**`Paint::conicGradient` starts at 3 o'clock, so 12 o'clock is -90°, and
its window CLAMPS rather than wrapping.** A window from 90 to 450 — the
obvious way to start a hue wheel at red — paints the quarter before 90°
in the first stop's flat colour, because no canvas angle ever reaches
past 360. Rotate the STOPS into [0, 360) instead; the factory warns once
when a window leaves the circle.

**`skia::image` takes a local matrix that maps source px into the
node's space**, which is where a sprite's atlas sub-rect goes as a
translate and a scale.

**`skia::buffer` is content that changes without re-describing** — a
simulation, a decoded video frame, a paint surface, a scrollback. Own
the `PixelBuffer`, draw into it, commit. The recipe compares by (source,
revision), so an identical re-describe between commits PRUNES and the
first describe after a commit patches exactly once. That is the whole
point: the node keeps its picture caching and its decorations, where the
alternative — a custom leaf at no caching — gives up both.

**`skia::sksl` decides its own tier by what the body reads.**
`constants` set named float uniforms once; bind live ones with
`Paint::bind` and fill declared `uniform shader` slots with
`Paint::slot`. Declaring `uTime` or `uContentScale` takes the LIVE path,
re-resolved each frame — the clock ticks and the host's zoom changes
independently of the node, so reading them IS the volatility
declaration. Declaring only `uResolution` takes the cheaper GEOMETRY
tier, resolved when the node records and cached between layouts.

**`Paint::recipe` is a `Material` instance as the paint.** The recipe's
declared frame inputs set the tier exactly as an SkSL effect's uniforms
do — time or content scale is LIVE, the resolution is GEOMETRY — and
its bindings make it live. `Paint::set`, `Paint::bind` and `Paint::slot` reach the
instance's fields and slots; equality is the instance's, so two paints
built from equal instances prune. `Paint::recipeMaterial` hands the
instance back, or null.

It is also the ONLY form a text runtime's pass takes. A pass body is
written against declarations the RUNTIME supplies —
`uniform shader uContent` (the addressed units' rendered layer),
`uniform float4 uUnitRect[N]`, `uniform float2 uUnitPhase[N]` and
`const int kUnitCount = N` — and N is the track's unit count, known only
at paint, so the runtime holds a specialization of the recipe per
distinct count. Do not declare those four names in the recipe, and read
them only from a material handed to a pass: used as an ordinary fill,
the recipe compiles without them and a body that mentions them does not
compile at all.

**`Paint::blend` layers into ONE flattened shader**, bottom to top, each
composited over the accumulation with its blend mode. The first layer IS
the accumulation, so both of its layer properties are ignored: its blend
mode, having nothing beneath to composite with, and its
[`amount`](../verbs/amount.md), having nothing to mix back toward. It is
nested blend shaders — one draw, fully picture-cacheable, no saved
layer. A blend whose layers are all static flattens eagerly; one
containing a LIVE or geometry-dependent layer DEFERS the flatten to
resolve time, per frame or per record respectively, so bound uniforms
and distance-field layers contribute their correct current form. The
blend simply inherits its layers' volatility tier.

### Box units and pixels

A gradient in PIXELS is workable for a box whose size you wrote down and
impossible for one the layout decides — a card as tall as its copy, a
button that grows with its label. Box units, the default, author the
same gradient in the node's UNIT SQUARE: (0,0) is the box's top-left,
(1,1) its bottom-right, whatever the box turns out to be. There is no
number to guess. A box-unit gradient rides the GEOMETRY tier through
`uResolution`, so it costs nothing per frame, and takes any number of
stops; a pixel one is static. A gradient placed against a canvas whose
size the sketch fixes says `{.units = GradientUnits::Pixels}`.

**A box-unit radial's radius is a multiple of its extent.** At the
default, `RadialExtent::FarthestCorner`, a gradient centred at
{0.5, 0.5} with radius 1 reaches the CORNERS of any box — which is a trap
for a soft round light: it still has alpha left where the INSCRIBED
circle is, so a node that also carries a circular shape cuts the ramp off
mid-falloff with a visible hard rim. `RadialExtent::ClosestSide` is the
one that means "a glow filling this node": radius 1 reaches the middle
of each side, which is the inscribed circle. The trap cuts the other way
too: a ramp authored past 1 — a planet terminator at radius 1.28 — puts
its far end entirely outside the inscribed disc, so on a circle-shaped
node the shading silently disappears.

Both extents are measured from the BOX's centre, so a gradient moved off
the centre keeps its size, and both work in the unit square, so on a
non-square box the falloff is elliptical: it fills the box rather than
staying circular. That is what you want for a panel wash and not for a
lamp; for a true circle, put it on a square node or place it in pixels.
A conic in box units is the exception: its centre is a fraction of the
box, and its angles stay true angles.

### Reading a finished paint

`skia::shader(paint)` always produces a shader — a solid becomes a colour
shader — which is what `Paint::blend` composes. For a live paint it
builds a fresh shader sampling bound values at their CURRENT readings, a
snapshot rather than a binding; a blend with a live LAYER folds its
layers per call for the same reason. `skia::shader(paint, frame)` is the per-draw
path: for a live paint, rebuilt from the bound values and the frame's
`uTime` / `uResolution` / `uContentScale`; for a geometry-dependent one,
built against the frame's box; for a static one, exactly
`skia::staticShader`. Both answer null for a solid and for nothing, so
ask `Paint::isSolid` and `Paint::isNone` first.

`skia::resolvePass` is what a text runtime calls for a pass track's
material, once per draw: the recipe specialized to the pass's unit count
(one definition per count, compiled once), the instance's values,
bindings and slots resolved exactly as an ordinary resolve resolves
them, and the runtime's own slots — the content, the unit rects, the
unit phases — filled from the inputs. It is null when the material is
not recipe-backed or its specialization does not compile; the caller
draws the units plainly then, so a broken pass shows resting letters
rather than nothing.

### What equality compares

`Paint::operator==` is the prune signature. Two paints compare equal
when they were built from the same recipe: solids by colour; gradients
by geometry, stops and repeat; images by image pointer, tile modes,
matrix and sampling; a static SkSL paint by effect pointer, constant
values and CHILD paints; blend stacks recursively by layer recipes and
modes. So re-running the same describe code yields EQUAL paints even
though each run minted a fresh shader, which is what lets a
paint-filled node prune across renders. Raw shader wrappers compare by
pointer, and bound paints compare by recipe identity only — they are
volatile and never prune regardless.

**An SkSL paint compares by EFFECT POINTER, so a helper that compiles a
fresh runtime effect on every call never compares equal to itself.** Its
node re-patches on every describe, and every memo above it misses.
Compile the effect once — a function-local static, or a cache keyed on
whatever varies — and hold the resulting paint rather than re-minting
it.

TIER INHERITANCE IS LOAD-BEARING. A slot's source and a blend's layers
ride the prune signature, so two paints with different sources never
compare equal and two with identical ones prune. A slot left out of
equality would let a node prune while its second source had changed, and
it would sample the old texture indefinitely.

`Paint::set`, `Paint::bind` and `Paint::slot` are meaningful on an effect-backed or
recipe-backed paint — the kinds that have named uniforms and declared
sockets. On a solid, a gradient or an image there is nothing to bind: the
call warns once and is ignored, never aborts, because one typo in a
sketch must not take a live-reload host down.

## See also

- `paint/Paint.h` — the header: `Paint`, `Fit`; `core/BlendMode.h` —
  `BlendMode`; `skia/Paint.h` — the Skia executor and the sources only
  Skia can supply
- `core/Gradient.h` — what a gradient is told: `ColorStops`,
  `GradientOptions`, `GradientUnits`, `RadialExtent`, `Repeat`
- The verbs on this value: [`bind`](../verbs/bind.md),
  [`slot`](../verbs/slot.md), [`amount`](../verbs/amount.md),
  [`fit`](../verbs/fit.md), [`offset`](../verbs/offset.md),
  [`worldSpace`](../verbs/worldSpace.md), [`bleed`](../verbs/bleed.md)
  and [`quantizeTime`](../verbs/quantizeTime.md)
- [Material](value:sigil::material::Material) — the recipe instance a
  `Paint::recipe` holds
- [Filter](value:sigil::material::Filter) — the same idea over an
  already-rendered layer
- [Ramp](value:sigil::material::Ramp) — the stops as one value, for the
  gradients above
- [Fill](value:sigil::compose::Fill) — the colouring value a paint
  converts to, beside the cascade's references
- The colour chapter on the [SigilCompose](doxygen:SigilCompose) site —
  the lattice whole, and which Paint is which
