---
kind: type
library: SigilGeometry
name: Ticks
qualified: sigil::geometry::shapes::Ticks
group: Divisions
status: stable
---

# Ticks

A radial division ladder, emitted as ONE path with N contours.

```cpp
// 72 divisions, every sixth reaching further in. One shape value,
// stroked once, instead of a loop of nodes.
shapes::ticks({.divisions = 72,
               .mark = {.inner = 0.96f, .outer = 1.0f},
               .longEvery = 6,
               .longMark = {.inner = 0.91f, .outer = 1.0f}})
```

Every angle is in the FRAME's units — `.from = 0` on a North/CW frame
is 12 o'clock, on an East/CW frame it is 3 o'clock. That is the whole
reason a `sigil::geometry::path::PolarFrame` exists: it carries where
zero is and which way the angles run, so a ladder need not restate
either.

## Why one path and not N of them

A divider ladder is static geometry with one style. As N drawn things it
costs N of everything a consumer spends per item, for a drawing that
never changes — and a radial mark's box is usually the full diameter of
the figure, so those are N items whose bounds are the whole plate. As
one path it is one recording, one stroke, and one arc-length coordinate
over all the marks.

**The one case that must stay N nodes is per-mark animation.** Marks
that fade or move individually need their own keyed nodes, because one
path has one style and one reveal window. If every mark shares those,
use this; if each mark has its own phase, do not.

## What this is not

Not a linear tick ladder along an edge: a consumer that stamps marks at
a pitch along any path already has the straight case. `Ticks`,
`sigil::geometry::shapes::Arcs` and `sigil::geometry::shapes::Chords`
are the radial and polygonal cases, where the mark positions come from
an angle convention rather than an arc length.

## The classifier

`sigil::geometry::shapes::Ticks::classify` is the escape hatch, for a
ladder with more than two length classes — three alternating lengths,
say, which no long/short pair expresses. It is asked for the span of
mark *i*; its second argument is what the fields above would have
given, so a classifier can defer to them.

**Return a degenerate span (`inner == outer`) to SKIP a mark.** That
is the only way to skip one, and it is what a ladder drawn in two
passes at two stroke weights needs: the light pass must leave a hole
where the heavy pass goes, or both passes stack on the same mark and
it prints darker than either weight.

Null (the default) means "use the fields". A `std::function` here has
no reconciler consequence through the SkPath overload — that path is
built immediately and never stored. Through the SHAPE overload it is
the one member equality cannot see, so a Ticks carrying a classifier
compares unequal to EVERYTHING, including a copy of itself: its node
never prunes and re-records on every describe. That is why
`Ticks::operator==` is conservative about it — any classify present
means "not provably the same ladder".

## The width of one mark

`sigil::geometry::shapes::Ticks::markPx` is how wide one mark is, in px
across the radius. Zero — the default — emits the open radial LINE a
ladder is stroked from; anything else emits a CLOSED rectangle standing
on the same radius, which is the node, the lozenge and the bar of a ring
that is filled rather than stroked.

The difference is not a stroke width by another name. A stroked line
is a mark the paint decides the weight of; a closed mark is
GEOMETRY, so it fills, it takes a gradient across its own width, it
unions with its neighbours, and the ring it belongs to can be
clipped, offset or measured as the shape it is. Px rather than
degrees because a node ring reads as marks of one size, not as
wedges that fatten with radius — the wedge is
`sigil::geometry::shapes::arcs`.

## The shape value

`sigil::geometry::shapes::TicksShape` takes the frame from the node's
own laid-out box: centre at the box centre, radius = half the SHORTER
side. Its `conventions` therefore supplies ONLY `zero`, `sense` and
`originDeg`; the frame's own `centre` and `radius` are overwritten, so a
frame passed there does not place the ladder — the node's box does.

Half the shorter side, not half the width, so a ladder on a non-square
box stays a circle instead of silently becoming an ellipse whose
`PolarFrame::fraction()` no longer matches. Give it a square box
(`PolarFrame::box()`, or a disc) and the question does not arise.

Comparable, so the node prunes — unless the Ticks carries a `classify`
callable, which equality cannot see and which therefore makes the whole
value compare unequal to everything.

`sigil::geometry::shapes::Span` is how far in and out one mark reaches,
in normalised radius.

## See also

- `kit/Divisions.h` — the header: `Ticks`, `TicksShape`, `Arcs`,
  `ArcsShape`, `Chords`, `ChordsShape`, `Span`
- [Arcs](value:sigil::geometry::shapes::Arcs) — the curved sibling
- [Chords](value:sigil::geometry::shapes::Chords) — the polygonal one
