---
kind: type
library: SigilMaterial
name: Paint
qualified: sigil::material::skia::Paint
header: sigilmaterial/skia/Paint.h
group: The Skia paint
python: sigil.material.Paint
status: stable
---

# Paint

THIS LIBRARY'S PAINT MODEL AS A SKIA SHADER: a small tree of paint nodes
that compiles to ONE shader — layers through a blend shader, never a
stack of saved layers — or to a plain solid colour. It is the value a
node's fill takes when the fill is more than a colour, and it is the
general paint value the rest of the tree means by "a paint".

It is not the raw Skia paint. A Skia `SkPaint` carries a style, a stroke
width and a blend mode for one draw; this carries what a surface is
shaded WITH, and compares by value so a consumer can prune on it. In
Python they are two names one module apart: `sigil.skia.Paint` is Skia's
own, `sigil.material.Paint` is this one.

## Anatomy

A paint sits in one of THREE VOLATILITY TIERS, and the tier is decided by
what it is made of rather than declared:

| Tier | What puts it there | What it costs |
| --- | --- | --- |
| static | a solid, a gradient ramp, an image or sprite, a blend of static layers, an SkSL effect with only constant uniforms | resolves once; answers its colour or its shader with no frame, so a consumer caches and prunes it like any other value |
| geometry | an effect declaring only `uResolution`, a stated `Paint::fit`, `Paint::worldSpace` | resolves when the node RECORDS and caches between layouts — it depends on the box, not on the clock |
| live | a bound uniform, an effect reading `uTime` or `uContentScale`, a live child | re-resolved every frame; its node is declared volatile and no cache can freeze it |

`Paint::isAnimated` and `Paint::geometryDependent` are how the tier is
asked, and a blend or a slot INHERITS the tier of what it holds.

`Paint::isSolid` with `Paint::solidColor` is the short-circuit every
consumer asks first — a solid has no coordinates and nothing to resolve.
`Paint::isNone` is the empty paint, which draws nothing.
`Paint::staticShader` is the shader a non-live paint already holds;
`Paint::shaderFor` is the per-draw one, built against a `PaintFrame`;
`Paint::asShader` always produces a shader, sampling live values at their
current value as a snapshot.

## Make one

Every leaf is a static factory, and each names what it is made of.

| Spelling | Language | What it gives |
| --- | --- | --- |
| `Paint()` | C++ | nothing — fully transparent, draws nothing |
| `Paint::solid(colour)` | C++ | a flat colour |
| `Paint::linear(a, b, stops)` | C++ | an n-stop ramp between two points, in node-local px |
| `Paint::radial(centre, radius, stops)` | C++ | a circular ramp |
| `Paint::conical(focus, focusRadius, centre, radius, stops)` | C++ | an offset-focus radial: the outer circle stays put while the hot spot moves |
| `Paint::sweep(centre, stops)` | C++ | an angular sweep; angles outside the circle CLAMP rather than wrapping |
| `Paint::linearUnit(from01, to01, stops)` | C++ | the linear ramp in the node's UNIT SQUARE, for a box whose size the layout decides |
| `Paint::radialUnit(centre01, radius01, stops)` | C++ | the unit radial, whose radius is a fraction of the HALF-DIAGONAL — radius 1 reaches the corners |
| `Paint::glowUnit(centre01, radius01, stops)` | C++ | the soft round light that FILLS the box — radius 1 is the inscribed circle |
| `Paint::image(image)` | C++ | an image or sprite as a fill, tiled or clamped |
| `Paint::buffer(source)` | C++ | a caller-owned raster the paint samples — a simulation, a decoded frame, a scrollback |
| `Paint::sksl(effect, constants)` | C++ | a runtime effect as a shader |
| `Paint::shader(shader)` | C++ | any raw Skia shader — the interop escape |
| `Paint::recipe(material)` | C++ | a `Material` instance as the paint |
| `Paint::blend(layers)` | C++ | layers painted bottom to top, each composited with its own blend mode, flattened into one shader |
| `material.Paint.solid(...)` and the rest | Python | the same factories under the same names |
| a `Material` | Python | implicitly, where a paint is taken |

Then the modifiers, each of which copies on write: `Paint::uniform` sets
or binds a named uniform, `Paint::slot` fills a declared `uniform shader`
with a SECOND SOURCE, `Paint::amount` is the layer strength inside a
blend, `Paint::fit` says how an image meets the box, `Paint::offset`
binds a live pan, `Paint::worldSpace` anchors the coordinates to the
root, `Paint::bleed` reserves recording cull, and `Paint::quantizeTime`
steps the injected clock.

`Stop` is one gradient stop — `Stop::pos` and `Stop::color`. `Fit` is
how a source meets the box: `Fit::Native`, `Fit::Stretch`, `Fit::Cover`,
`Fit::Contain`. `PaintFrame` is what one draw supplies and no author
sets: `PaintFrame::size`, `PaintFrame::rootSize`, `PaintFrame::toRoot`,
`PaintFrame::seconds` and `PaintFrame::contentScale`.

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `Paint::slot` | member | SigilMaterial — a paint fills another paint's second source |
| `Paint::blend` | function | SigilMaterial — as one layer |
| `skia::Effect::slot` | member | SigilMaterial |

Outside this library a paint is what a node's fill, a stroke's paint, a
text fill and a coverage gate take. Those slots belong to the libraries
that own them; each states the paint vocabulary as this library's, and
none re-exports it.

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| every leaf factory above | function | SigilMaterial |
| `skia::unitRamp` | function | SigilMaterial — a stop list over the unit square, which is what a text fill and a mask take |

## Description

The vocabulary mirrors the solid / ramp / image / blend atoms a material
graph format would use, and nothing of the sort is linked: the backend
here is SkSL and Skia's shaders and nothing else. Colour management is
not part of a paint — a view transform belongs to the consumer's output
stage and applies to a whole composite.

TIER INHERITANCE IS LOAD-BEARING. A slot's source and a blend's layers
ride the prune signature, so two paints with different sources never
compare equal and two with identical ones prune. A slot left out of
equality would let a node prune while its second source had changed, and
it would sample the old texture indefinitely.

`Paint::uniform` and `Paint::slot` are meaningful on an effect-backed or
recipe-backed paint — the kinds that have named uniforms and declared
sockets. On a solid, a gradient or an image there is nothing to bind: the
call warns once and is ignored, never aborts, because one typo in a
sketch must not take a live-reload host down.

## See also

- `skia/Paint.h` — the header: `Paint`, `PaintFrame`, `Stop`, `Fit`
- [Material](Material.md) — the recipe instance a `Paint::recipe` holds
- [Effect](Effect.md) — the same idea over an already-rendered layer
- [Ramp](Ramp.md) — the stops as one value, for the gradients above
- [Colour, fill, paint and material](../../../../compose/reference/COLOURING.md)
  — the lattice whole, and which Paint is which
