---
kind: function
library: SigilGeometry
name: crossingPatch
qualified: sigil::geometry::path::crossingPatch
group: Crossings
status: stable
---

# crossingPatch

The region where two strands' MARKS actually overlap at one crossing:
the intersection of the two paths stroked to their own reach, reduced
to the component containing the crossing point and bounded by
`maxRadius` px around it.

## Description

Exact at any angle, which a disc is not — two marks meeting at 12°
overlap in a long lens whose extent along each strand goes as
reach/sin(theta), and a disc sized for the perpendicular case leaves
the under-strand showing straight across the over-strand's mark.

`maxRadius` is not a safety margin, it is REQUIRED for correctness on
any ordinary braid. Once reach/sin(theta) approaches the spacing
between knots, neighbouring lenses touch and path operations merge them
into ONE contour — at which point the first crossing's patch owns the
whole run and the weave degenerates to "one strand on top" for half its
knots. Pass half the arc distance to the adjacent crossing, so each
knot can only ever claim its own half.

Falls back to a disc when the intersection is empty, which is a
degenerate or non-overlapping input.

## See also

- `path/Crossings.h` — the header: `crossingPatch`, `CrossingRule`,
  `discoverCrossings`
- [CrossingRule](value:sigil::geometry::path::CrossingRule) — who
  passes over whom at that knot
