---
kind: type
library: SigilGeometry
name: Strip
qualified: sigil::geometry::path::operations::Strip
group: Path operators
status: stable
---

# Strip

STRIP JOINERY over a set of pieces of stock — a segment cut to a
width: the outline of each piece mitred to the joints it stands in, the
whole figure those outlines unite into, and the half-laps where two
pieces cross rather than meet. Where a lattice, a trellis, a window bar
or a Voronoi cage is joined up.

## The outlines

`sigil::geometry::path::operations::stripOutlines` answers THE OUTLINE OF EACH
PIECE, ITS ENDS CUT TO THE JOINTS IT STANDS IN — one closed contour per
piece, in the order the pieces were given, and an empty path for a
piece of no length.

A node is wherever ends meet: the ends there are put in order round
it, and the seam between each neighbouring pair is the bisector of
their two directions, so every piece is planed to the same face as
the piece beside it. Two ends meeting give the corner mitre a picture
frame is cut to; three or more give each piece a wedge, which is what
a lattice node actually is; an end that meets nothing is cut square
across. Nothing here reads which piece is on top — a lattice is one
layer of stock at a time, and `sigil::geometry::path::operations::stripLaps` is
where the layers cross.

## The cut at a node

`sigil::geometry::path::operations::StripOptions::join` is the cut. `Miter` planes
each end back to the seams it shares with its neighbours round the
node, so the pieces fill the node with no gap and no overlap — real
mitred joinery, and the one join a wood or metal lattice is actually
cut to. `Bevel` stops each end a half-width from the node instead,
blunting the point. `Round` finishes each end with an arc of its own
half-width about the node, which at a lone end is a round cap and at a
joint a rounded one.

The mitre limit is how many half-widths a mitred point may stand from
its node before it is cut back: the sharper the angle, the further a
true mitre reaches, and a needle-thin one reaches off the page.

## See also

- `path/Operations.h` — the header: `Strip`, `StripOptions`,
  `stripOutlines`, `stripLaps`
- [offset](page:SigilGeometry/functions/offset) — the outline offset in
  the same header
