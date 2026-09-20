---
kind: type
library: SigilGeometry
name: Arcs
qualified: sigil::geometry::shapes::Arcs
group: Divisions
status: stable
---

# Arcs

A ring of CLOSED ARC SEGMENTS: N wedges of the annulus between two
radii, each `spanDeg` wide, dealt round the frame the way
`sigil::geometry::shapes::ticks` deals its marks.

## The curved sibling of a tick with a width

This is what a `sigil::geometry::shapes::Ticks` carrying a `markPx` is
not, and the two answer different pictures. A tick's mark is a straight
bar of one width — a node, dealt round a ring, reading as marks of one
size. An arc's mark follows the ring, so its edges are the ring's own
arcs and it fattens with radius the way a segment of a dial does. A
segmented progress ring, a fan of sectors, a broken annulus: each is one
path here rather than N drawn things.

Every angle is in the FRAME's units, exactly as `ticks()` reads them,
and `sigil::geometry::shapes::Arcs::spanDeg` is a WIDTH, so it takes the
frame's sign but not its origin. A span wider than the pitch makes
neighbours overlap, which a fill with a non-zero winding closes into a
solid ring — say the pitch, not more, unless that is the drawing.

## Anatomy

`divisions` is how many segments, dealt as `ticks()` deals marks: with a
full sweep and `closed = false`, segment N would coincide with segment 0
and is not emitted. `from` is the frame's degrees of the first segment's
CENTRE, `sweep` the total span the divisions are dealt over, and
`closed` emits `divisions + 1` segments — the closed ladder a scale
wants and a full ring does not.

`mark` is how far in and out one segment reaches, in normalised radius.
Equal radii emit nothing: a segment with no thickness is not a figure,
and the degenerate contour would fill as nothing and stroke as a doubled
arc.

## The shape value

`sigil::geometry::shapes::ArcsShape` takes its frame from the laid-out
box — the same rule as `ticks` and `chords`: centre at the box centre,
radius half the shorter side, and the `conventions` frame's own centre
and radius ignored. Fully comparable, since `Arcs` has no callable
member.

## See also

- `kit/Divisions.h` — the header: `Arcs`, `ArcsShape`, `Ticks`,
  `Chords`, `Span`
- [Ticks](value:sigil::geometry::shapes::Ticks) — the straight sibling,
  and where the one-path rule is stated
