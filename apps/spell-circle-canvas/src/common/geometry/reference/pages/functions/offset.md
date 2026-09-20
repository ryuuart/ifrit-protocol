---
kind: function
library: SigilGeometry
name: offset
qualified: sigil::geometry::path::operations::offset
group: Path operators
status: stable
---

# offset

OFFSET: the mark a path becomes a distance to the side of itself.

ONE operator for what an outline offset, a concentric frame, a
parallel rail and a bolder silhouette all are. A positive distance
offsets to the LEFT of travel, which for a filled shape at the
default `position` is outward — the library-wide sign convention,
shared with `parallel` and the profile offset.

A band that STRADDLES the source encloses the source's own edge, so
what is answered there is the source with the band added (a positive
distance) or taken away (a negative one) — the grown or shrunk area,
which is what an outline offset means. A band that lies to one side
touches no interior and is answered as itself.

Implemented as stroke-expansion plus a boolean where the band
straddles, which is robust for UI-scale geometry, and as the
contour walk `parallel` elsewhere; a polygon-clipper backend can slot
in later for cartography-grade needs.

## The dials of one offset

`sigil::geometry::path::operations::OffsetOptions::position` is the one that makes
this a single operator rather than a family. It is CONTINUOUS: at 0 the
offset is the single curve a distance to the LEFT of travel, at 1 the
single curve the same distance to the right, and at 0.5 it is both at
once — the band that straddles the source, which for a filled shape is
that shape grown by the distance (or shrunk, at a negative one).
Everything between is the band slid across the source, its two rails at
`distance·(1 ± 2·position ∓ 1)`. A three-valued side enum would be
three operators wearing one name.

`sigil::geometry::path::operations::OffsetOptions::step` is the stride the sideways
walk takes where a walk is used — away from `position` 0.5, where
Skia's stroker answers instead.

## Moving the source's own nodes

`sigil::geometry::path::operations::OffsetOptions::keepCompatible` moves the
source's own nodes rather than forming a new outline: every node
travels along its corner bisector and every handle along its segment's
normal, so the answer has the nodes the source had, in the same order
and of the same kinds, and the two still interpolate. Which way "out"
is comes from the contour's own winding, so this is the one spelling
whose sign follows the drawing rather than the boolean. A needle-sharp
corner's mitre is capped by `miterLimit`, blunting the corner rather
than dropping the node.

It is the WHOLE offset when it is set: `position` and `step`, which
describe a band and a walk, say nothing about moving a node and are not
read.

## See also

- `path/Operations.h` — the header: `offset`, `OffsetOptions`,
  `parallel`, `roundCorners`, `chamferCorners`
- [Strip](value:sigil::geometry::path::operations::Strip) — the joinery in the same
  header
