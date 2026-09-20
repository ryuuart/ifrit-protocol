---
kind: type
library: SigilGeometry
name: Annulus
qualified: sigil::geometry::shapes::Annulus
group: Silhouettes
status: stable
---

# Annulus

A ring: the inscribed circle with a concentric hole at a fraction of
the radius. Even-odd, so it fills as an annulus.

## The ring said the other way about

`sigil::geometry::shapes::Annulus::thickness` is the same ring stated
as its own width in PIXELS, which is what a ring keeps when the box it
stands in does not: a reticle, a dial's rim, a glyph that must read the
same weight at two sizes. Nonzero, it decides the hole and
`innerRatio` is not read. A thickness that eats the whole radius leaves
a disc, which is what a ring that thick is.

## The dot at the centre

`sigil::geometry::shapes::Annulus::dot` puts a concentric disc of that
pixel radius at the centre. Two marks are not always two things: a ring
around a point says something an arrow cannot — that what it names is
not in the picture plane at all — and as ONE outline the pair fills,
strokes and animates together.

## The chevron beside it

`sigil::geometry::shapes::Chevron` is a wide flat V pointing down the
box, drawn as an outline of its own thickness, with an optional pair of
outrigger bars level with its shoulders — the level indicator, the rank
mark, the "you are here" on a gauge.

It is a V and not an arrowhead: `spread` takes the shoulders out to a
fraction of the box's width, `drop` takes the point down a fraction of
its height, and the shoulders stand a third of the drop ABOVE centre,
which is what keeps the two arms shallow. `thickness` is the mark's own
width as a fraction of the height, and `bars` is how far the outriggers
run in from each edge; zero leaves the V on its own.

## See also

- `kit/Generators.h` — the header: `Annulus`, `Chevron`, `Circle`
- [Circle](value:sigil::geometry::shapes::Circle) — the inscribed
  circle this is cut from, and where the winding rule is stated
