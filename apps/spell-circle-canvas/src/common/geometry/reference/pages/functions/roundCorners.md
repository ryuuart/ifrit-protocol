---
kind: function
library: SigilGeometry
name: roundCorners
qualified: sigil::geometry::path::operations::roundCorners
group: Path operators
status: stable
---

# roundCorners

Round Corners: every sharp corner of the path replaced by an arc of the
radius given. A non-positive radius returns the path unchanged, and a
path the effect refuses comes back unchanged rather than empty.

WITH ANY OPTION SET this is a POLYLINE treatment: the selection and
the visual correction are read off the two straight legs meeting at a
corner, so a joint where either side is a curve passes through
untouched. Skia's corner effect, which the default
`sigil::geometry::path::operations::CornerOptions` use, has no such limit and no
such dials.

## The chamfer beside it

`sigil::geometry::path::operations::chamferCorners` cuts every line-line corner of
a path with a straight bevel of so many px along each leg — on an
orthogonal route's right angles that is the 45-degree face of the
game-UI and PCB corner convention, which `SkCornerPathEffect` cannot
spell because it only rounds. The cut clamps to half of each adjacent
leg, so short legs degenerate to a diagonal rather than crossing over.
Straight-through vertices are left alone; closed polyline contours
chamfer the closing vertex too, so a routed loop and a
`shapes::chamfered` panel agree.

THIS IS A POLYLINE TREATMENT. A contour containing ANY curve segment —
quad, conic or cubic — is copied through completely untouched, so a
chamfer over an arc, a rounded route, or anything already run through a
corner effect is a silent no-op on that contour.

## See also

- `path/Operations.h` — the header: `roundCorners`, `chamferCorners`,
  `CornerOptions`, `displaceSquare`
- [offset](page:SigilGeometry/functions/offset) — the other outline
  treatment in the same header
