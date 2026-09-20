---
kind: type
library: SigilGeometry
name: Chords
qualified: sigil::geometry::shapes::Chords
group: Divisions
status: stable
---

# Chords

The n vertices of a regular n-gon on a frame, as chord endpoints, wound
so that consecutive contours run the same way round.

## What it is for, and nothing else in the library does this

With `step = 1` and `closed = false` the sides come out as *n separate
open contours of one path*, and `TextPath` walks every contour of a
baseline in order as ONE arc-length coordinate. So side *k*'s midpoint
is at exactly `(k + 0.5) / n` of the whole run, and an inscription
around a polygon becomes one text node on one outline instead of n runs
a caller has to place. `shapes::polygon(n)` cannot do this: it emits one
CLOSED contour, so a per-side coordinate does not exist.

The winding decides which way glyphs on that baseline face — clockwise
on screen puts glyph-up radially outward, the engraver's convention,
the same choice `shapes::circle` documents. It comes from the frame's
`sense` rather than from an argument here.

## Anatomy

`sigil::geometry::shapes::Chords::step` is 1 for the polygon's sides, 2
for a {n/2} star polygon's chords, and so on. Coprime with `sides` it
gives one closed traversal; otherwise it gives `gcd(sides, step)`
separate rings, which is the correct {6/2} hexagram (two triangles)
rather than an error.

`sigil::geometry::shapes::Chords::inset` shortens each chord by that
many px at BOTH ends — the gap an engraver leaves at a vertex so the
corner ornament reads. A chord shorter than twice the inset is dropped
entirely. It reaches the OPEN form alone: a closed traversal joins the
chords into one contour, and a contour has no chord ends to trim.

`radius` is the rNorm of the vertices, `from` the frame's degrees of
vertex 0, and `closed` joins the chords into closed contours (a star
outline you can fill) rather than leaving each its own OPEN contour,
which is the addressable-per-side form TextPath wants.

## The shape value

`sigil::geometry::shapes::ChordsShape` takes its frame from the laid-out
box — same rule as `ticks`: centre at the box centre, radius half the
shorter side, and the `conventions` frame's own centre and radius
ignored. Fully comparable, since `Chords` has no callable member, so it
always prunes.

## See also

- `kit/Divisions.h` — the header: `Chords`, `ChordsShape`, `Ticks`,
  `Arcs`, `Span`
- [Ticks](value:sigil::geometry::shapes::Ticks) — the radial ladder, and
  where the one-path rule is stated
