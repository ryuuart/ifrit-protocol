---
kind: function
library: SigilGeometry
name: geo
qualified: sigil::geometry::mesh::codec::encode::geo
group: Interchange
status: stable
---

# geo

The geometry as Houdini's JSON `.geo`.

## A cloud

The points, `P`, and every lane — `normal` as N, `uv` as uv with its v
axis flipped back to the file's convention, `tint` as a
four-component Cd so the alpha rides in it, and every other lane under
its own name at the width that brings it back as the same kind of lane.
Empty geometry declines with an empty string, as the PLY writer does.

A GROUP IS NOT WRITTEN AS A GROUP. The reader turns a `.geo` group
into a 0/1 scalar lane and nothing here can tell such a lane from any
other scalar, so a lane that arrived as a group leaves as the
attribute it became — which is what round-trips, and what a mask reads
as either way.

## A mesh

A `.geo` of closed polygons, one per triangle, with the vertex
attributes on the points and every `Mesh::primitives` lane as a
four-component primitive attribute. A mesh with no faces is a point
cloud and is written as one.

IT COMES BACK UNWELDED, and that is the format rather than the writer:
a `.geo` addresses a polygon's corners through a vertex list, and the
reader gives every corner its own mesh vertex so that a per-corner uv
or normal survives a seam. A cube written with 8 shared positions
returns with 36. The positions, the winding and every attribute value
are the same; the vertex COUNT is not.

## See also

- `mesh/codec/Encode.h` — the header: `geo`, `ply`, `PlyOptions`
- [ply](page:SigilGeometry/functions/ply) — the other carrier, and the
  lane spellings it uses
