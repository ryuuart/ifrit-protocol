---
kind: function
library: SigilGeometry
name: ply
qualified: sigil::geometry::mesh::codec::encode::ply
group: Interchange
status: stable
---

# ply

Geometry OUT to the interchange world, the return leg of the readers in
`mesh/codec/Decode.h`. PLY is the carrier, being the one widely read
format where arbitrary per-vertex attributes are first-class.

- a `sigil::geometry::mesh::Cloud` writes positions plus EVERY lane —
  "normal" as nx/ny/nz, "uv" as s/t, "tint" as uchar
  red/green/blue/alpha (so Blender shows vertex colors on import),
  scalar lanes under their own names, other vector lanes as
  name_x/_y/_z and other color lanes as name_r/_g/_b/_a. The PLY
  reader folds those suffixed triples and quads back into lanes, so a
  round trip is lossless, up to the uchar colour quantization.
- a `sigil::geometry::mesh::Mesh` writes vertices — positions,
  normals, uvs, colors — plus its triangles.

## Ascii or binary

Ascii is the default because the result is readable and diffable.
Choose binary through `sigil::geometry::mesh::codec::encode::PlyOptions`
when the file has to round-trip or has to be small: rows become raw
little-endian bytes, so floats survive exactly instead of through a
decimal spelling, the file carries no token text, and neither writer
nor reader formats or parses numbers.

## The other carrier

Houdini's JSON `.geo` is the second, and it is the one to reach for
when the destination IS Houdini: the same lanes travel, under the names
that side already knows them by, with no suffix folding to arrange
between the two spellings. It is the exact return leg of the `.geo`
reader in `Decode.h` — everything that reader understands, and nothing
it does not.

A typical use: read a GPU-cooked pop surface back off its chain and
encode it — compute-shader geometry, attributes and all, opened in
Houdini or Blender.

## See also

- `mesh/codec/Encode.h` — the header: `ply`, `geo`, `PlyOptions`
- [geo](page:SigilGeometry/functions/geo) — the Houdini side of the
  same leg
