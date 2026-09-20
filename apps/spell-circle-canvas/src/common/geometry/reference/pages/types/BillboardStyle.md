---
kind: type
library: SigilGeometry
name: BillboardStyle
qualified: sigil::geometry::mesh::points::BillboardStyle
group: Points
status: stable
---

# BillboardStyle

How `sigil::geometry::mesh::points::drawBillboards` splats its sprites — the
image and its size, the blend, the depth sort, and which of a cloud's
lanes each splat reads.

## The atlas window lane

`sigil::geometry::mesh::points::BillboardStyle::textureLane` names a colour lane
holding {uOffset, vOffset, uScale, vScale} per point, in the unit
square — which is exactly what a `pop::AtlasCell` operation writes into
"Tex". Each splat then draws THAT CELL of the sprite instead of the
whole image, so one sheet of sprites splats as a field of different
ones and a cloud carries which is which.

Named rather than assumed, because a cloud may carry "Tex" for the
stamping path while these splats are meant to be one sprite; say
`"Tex"` to read what the atlas operation wrote. A point whose window is
degenerate, or which the lane does not reach, takes the whole image.

## The sheet wants a gutter

One batch samples the whole sheet through one shader, so a cell is
taken half a texel inside its window, which keeps the linear filter off
the cell next door at the size the sheet is authored for: a sheet whose
sprites run to their cell edges loses that half texel.

Splats minified far below their cell read from a mip level of the WHOLE
sheet, whose texels already average over the cell boundaries, so no
gutter holds the neighbours out down there; a sheet meant to be seen
that small wants cells that stay legible when they blur together. A
cell narrower than the inset is not drawn at all.

## See also

- `mesh/pop/Points.h` — the header: `BillboardStyle`, `drawBillboards`,
  `Cloud`
- [Cloud](value:sigil::geometry::mesh::Cloud) — the lanes a splat reads
