---
kind: type
library: SigilGeometry
name: Circle
qualified: sigil::geometry::shapes::Circle
group: Silhouettes
status: stable
---

# Circle

The circle (ellipse, on a non-square box) inscribed in the box, with a
chosen WINDING and start point. It is the OUTLINE a path-following
consumer takes — a baseline, a mask gate, a decoration; a consumer
that also has an element form for a disc keeps that separately.

## The winding decides which way glyphs face

Direction is not a detail on a text baseline. `onPath` orients to the
tangent, so a clockwise ring puts glyph-up radially OUTWARD and a
counter-clockwise one puts it INWARD. Both are uniform engraver's
conventions, and they are opposite in sign, so a ring inscription that
reads upside down wants this argument rather than a hand-written
`OutlineFunction`.

## Where the contour begins

`sigil::geometry::shapes::Circle::startIndex` picks which of the oval's
four extreme points the contour begins at, which is what a text path
measures its arc-length fraction from. It defaults to 1 to match Skia's
own `addOval(rect, dir)`, so `circle(kCW)` yields byte-for-byte the
path `circle()` gives and the oriented overload is a strict superset.
Changing that default would silently move every label placed by
arc-length fraction.

## Standing clear of the box

`sigil::geometry::shapes::Circle::inset` pulls the circle concentrically
inside the box by that many px — the spelling for a ring that must
stand CLEAR of the box edge: a text baseline whose glyphs straddle the
circle and need room on both sides, a band drawn inside a frame. Zero
is the inscribed circle; negative pushes it outside the box, which
every consumer that clips at the box will truncate.

## The true circle on an oblong box

`sigil::geometry::shapes::Circle::uniform` makes the figure a CIRCLE on
a box that is not square — the largest one that fits, centred — where
the default is the box's own oval. A box a pixel or two out of square
gives an oval out of round by a pixel or two, which reads as a mistake
wherever the mark is small and meant to be round: an eye, a pupil, a
bullet, a dot on a dial. On a square box the two are the same figure,
so a caller that never leaves square boxes never has to think about it.

Exact conics either way — this is `addOval`, not a sampled polyline.

## The shape vocabulary it belongs to

Closed silhouettes, curve families, corner treatments, tick and chord
divisions and hatch fills, each a comparable value that answers a path
for a box rather than a path. Because a generator is a value, two of
them can be compared, so a node whose shape did not change is a node
nothing has to redraw — which a hand-rolled callable can never prove.
Everything is a unit shape over the box it is asked for, so one
generator serves at any size.

A shape is ONE value and a lowercase factory spelling that value's
fields as arguments. The value carries the documentation, parameter by
parameter, and the factory carries none: a second copy of it can only
repeat the first or disagree with it.

## See also

- `kit/Generators.h` — the header: `Circle`, `Annulus`, `Chevron`,
  `Star`, `Polygon`
- [Annulus](value:sigil::geometry::shapes::Annulus) — the ring built on
  the same inscribed circle
