---
kind: type
library: SigilGeometry
name: Ellipse
qualified: sigil::geometry::shapes::Ellipse
group: Silhouettes
status: stable
---

# Ellipse

The one conic inscribed in the box — a circle on a square box — and
everything cut from it: the open arc, the wedge, the chord, the ring,
the donut slice, the squircle. `shapes::ellipse(options)` makes one;
the short names a sketch reaches for are stock values over it, each one
line: `circle()`, `arc(from, sweep)`, `sector(from, sweep, inner)`,
`annulus(inner)`, `ring(thickness, dot)` and `squircle(exponent)`.

```cpp
box().shape(shapes::circle());                       // the inscribed circle
box().shape(shapes::sector(200, 250, 0.45f));        // a donut slice
box().shape(shapes::ellipse({.sweepDegrees = 120, .close = shapes::Close::Chord}));
```

## The winding decides which way glyphs face

Direction is not a detail on a text baseline. `textOnPath` orients to the
tangent, so a clockwise ring puts glyph-up radially OUTWARD and a
counter-clockwise one puts it INWARD. Both are uniform engraver's
conventions, and they are opposite in sign, so a ring inscription that
reads upside down wants
`sigil::geometry::shapes::EllipseOptions::winding` rather than a
hand-written `OutlineFunction`.

## Where the contour begins

`sigil::geometry::shapes::EllipseOptions::start` picks which of the
oval's four extreme points the contour begins at, which is what a text
path measures its arc-length fraction from. It defaults to 1, the oval's
due-east extreme, so `circle(path::Winding::OutersClockwise)` yields
exactly the outline `circle()` gives and the oriented overload is a
strict superset. Changing that default would silently move every label
placed by arc-length fraction.

## Standing clear of the box

`sigil::geometry::shapes::EllipseOptions::inset` pulls the figure
concentrically inside the box by that many px — the spelling for a ring
that must stand CLEAR of the box edge: a text baseline whose glyphs
straddle the circle and need room on both sides, a band drawn inside a
frame. Zero is the inscribed figure; negative pushes it outside the box,
which every consumer that clips at the box will truncate.

## The true circle on an oblong box

`sigil::geometry::shapes::EllipseOptions::uniform` makes the figure a
CIRCLE on a box that is not square — the largest one that fits, centred —
where the default is the box's own oval. A box a pixel or two out of
square gives an oval out of round by a pixel or two, which reads as a
mistake wherever the mark is small and meant to be round: an eye, a
pupil, a bullet, a dot on a dial. On a square box the two are the same
figure, so a caller that never leaves square boxes never has to think
about it. Exact conics either way, not a sampled polyline.

## The sweep and how it closes

`sigil::geometry::shapes::EllipseOptions::sweepDegrees` below a whole
turn cuts the figure, starting at `fromDegrees` (0° is +x, clockwise on
screen), and `close` says how: `Close::Open` is the arc, which strokes and
cannot fill; `Close::Chord` runs straight back across; `Close::Pie` runs
through the centre, the wedge a pie or polar-area chart is built of. A
sweep of a whole turn with a hole is a gauge's annular TRACK and is held
just short of the turn, since an arc swallows a full one and would draw
nothing at all.

## The ring, said two ways

`sigil::geometry::shapes::EllipseOptions::inner` is a concentric hole at
a fraction of the radius, even-odd so the figure fills as an annulus.
`sigil::geometry::shapes::EllipseOptions::thickness` is the same ring
stated as its own width in PIXELS, which is what a ring keeps when the
box it stands in does not: a reticle, a dial's rim, a glyph that must
read the same weight at two sizes. Nonzero, it decides the hole and
`inner` is not read. A thickness that eats the whole radius leaves a
disc, which is what a ring that thick is.

`sigil::geometry::shapes::EllipseOptions::dot` puts a concentric disc of
that pixel radius at the centre. Two marks are not always two things: a
ring around a point says something an arrow cannot — that what it names
is not in the picture plane at all — and as ONE outline the pair fills,
strokes and animates together.

## The superellipse

`sigil::geometry::shapes::EllipseOptions::exponent` is the `e` of
|x|^e + |y|^e = 1: 2 is the ellipse, 4–5 the app-icon squircle, and a
large value approaches the rectangle. A value other than 2 draws the
whole figure, sampled.

## See also

- `kit/Radial.h` — the header: `Ellipse`, `EllipseOptions`, `Close`,
  `Radial`, `Fitted`, `FitOptions`
- [Radial](value:sigil::geometry::shapes::Radial) — the other general
  shape over a box
