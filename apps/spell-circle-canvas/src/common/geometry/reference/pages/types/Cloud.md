---
kind: type
library: SigilGeometry
name: Cloud
qualified: sigil::geometry::mesh::Cloud
group: Points
status: stable
---

# Cloud

Positions plus NAMED ATTRIBUTE LANES — scalars, vectors, colors — and
the centre of a Houdini-flavoured miniature: generators put points
places (a spline, a ring, a grid, a mesh surface, a box), modifiers
perturb them, and two consumers turn them into pictures.

- `sigil::geometry::mesh::points::instance` and
  `sigil::geometry::mesh::points::quads` stamp a Mesh (or a quad) onto every
  point — scale, tint and orientation read from lanes — producing ONE
  merged Mesh for `render::drawMesh` or for a 3D set. "Instance planes
  across points" is `quads()` plus a normal lane, or leave normals off
  and let billboarding face the camera at draw time.
- `sigil::geometry::mesh::points::drawBillboards` is the UI-particle path:
  camera-facing sprites (an SkImage, or a soft procedural dot) with
  perspective size, depth sort, per-point size and tint lanes, additive
  or normal blend.

Everything is a value: clouds copy, lanes are plain vectors, and a
generator plus modifier stack re-runs whenever a parameter moves — the
non-destructive posture of the rest of the library.

Conventional lane names, with nothing enforcing them: `"t"` (0..1 along
a generator), `"normal"` (orientation), `"size"`, `"tint"`.

## How a stamp rides those lanes

`sigil::geometry::mesh::points::stampOptions` is that table, as one value.

The orient lane is "dir" where a chain produced one and "normal"
where a generator or an importer did, so a cloud from either source
stands its stamps up without the author naming a lane; "size" scales
and "tint" colours. A lane the cloud does not carry is left empty
rather than named, so nothing is looked for that is not there.

Every stamping path takes its options from here. Two tables would
mean one cloud standing its stamps up through one caller and lying
them flat through another, which is what a single convention is for.

## Point class to primitive class

`sigil::geometry::mesh::points::promoteToPrimitives` is the bridge — Houdini's
Attribute Promote — and the instancing companion: an instanced mesh
lays each point's stamp down as a consecutive run of triangles, so
triangle index / (triangles per stamp) IS the owning point. It fills
`Mesh::primitives` at the named lane from the cloud lane named —
scalars broadcast to all four components, vectors take w = 0,
colors copy — and the RESERVED source name "Id" writes the owning
point's index in .x instead of reading a lane.

It is a no-operation unless the mesh's triangle count divides evenly by
the cloud's point count, which is to say unless the mesh really is that
cloud instanced.

## See also

- `mesh/pop/Points.h` — the header: `Cloud`, `instance`, `quads`,
  `drawBillboards`, `stampOptions`, `promoteToPrimitives`,
  `InstanceOptions`, `BillboardStyle`
- [BillboardStyle](value:sigil::geometry::mesh::points::BillboardStyle) — how
  the billboard path reads a sheet of sprites
