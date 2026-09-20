# SigilGeometry — the mesh tier and its headers

The chapter on the 3D tier, header by header: `mesh` the currency,
`mesh/camera` the viewpoint, `mesh/render` the draw and its runtime,
`mesh/curve` the splines, `mesh/pop` the point clouds and the chain
language over them, and `mesh/codec` model interchange. `README.md`
beside the library is the front page; `PATH.md` is the 2D tier underneath this one, `POP.md` the
operator catalogue and `KIT.md` the shelf.

**`mesh`** — `SigilGeometryMesh`, needs `path`. The 3D tier's root, and
the currency every feature under it speaks.

- **`mesh/Mesh.h`** — the mesh currency. The `Mesh` struct (positions,
  normals, uvs, colors, indices, and the `primitives` lane map),
  `append()`/`transform()`/`computeNormals()`/`bounds()`, and
  `mesh::bakePrimitiveColor()`. `primitiveNames()` answers WHICH lanes the
  mesh carries, in the map's own order, so a reader that did not write
  them walks them without holding the map's own type. Two surfaces are
  here rather than on the kit's
  shelf because everything else is built through them: `grid()`, the
  parametric-sheet seam a caller hands its own formula to — answering the
  position, which is differenced for the normals, or the position AND its
  normal, which is taken at its word for one evaluation a vertex — and
  `quad()`,
  the flat panel a consumer needs to have a mesh at all. The struct's own
  methods are `Mesh.cpp`; the two surfaces and the bake are
  `Generators.cpp`.
- **`mesh/Vec.h`** — the two glm policies the library and its GPU twin
  share: `normalized()` with a fallback for a degenerate vector, and
  `basisFor()`, the orientation basis every stamp is placed with, so a
  cloud renders identically merged, instanced or GPU-drawn.
- **`mesh/Faces.h`** — a mesh read by its FACES rather than by its
  triangles, over the `"Id"` primitive lane: `faceCount()`,
  `faceNormal()`, `faceCentroid()`, `opposedFace()` (the face across from
  one, measured from the centroids) and `faceUp()`, the rotation that
  lands a chosen face's outward normal on a chosen axis. A mesh with no
  such lane has one face per triangle, so every reader works on any mesh,
  and a generator that fans a pentagon into three triangles is read as
  the one face it is. **A pose is how anything rides a curve, and
  `faceUp()` is the other kind**: it places a SOLID rather than following
  a spine, which is why it stands here and not beside `curve::poseAlong()`.

**`mesh/camera`** — `SigilGeometryMeshCamera`, needs `mesh`. Where a
viewpoint and the transforms that answer to it live; a camera grows its
own repertoire here rather than inside whatever draws through it.

- **`mesh/camera/Camera.h`** — a right-handed, y-up `Camera` with a
  vertical field of view, `view()`/`projection()`/`viewProjection()`,
  and the transform helpers `place()` and `faceCamera()`. `toSkM44()` is
  the glm-to-Skia seam. The view and projection are built with Skia's own
  matrix factories, so a point projected here lands where a canvas concat
  would put it.

  `clipProjection(extent)` is the same view for a DEVICE: the projection
  and the view composed without the viewport step `viewProjection()` ends
  with, and depth put where a device reads it — zero at the near plane
  and one at the far one, the other way about from the projection's own.
  Every executor that hands a camera to a shader takes it from here, so
  the arithmetic that turns a viewpoint into clip space is written once.

  `project(point, viewport)` carries one point the whole of that way —
  view, projection, viewport — so a mark placed by it sits ON the
  geometry the painter drew rather than near it, and answers nothing for
  a point at or behind the eye plane, where the canvas has no place for
  it. `extentAt(distance, aspect)` is the other direction: how wide and
  how tall the frustum is that far in front of the eye, which is the
  measurement a head-up overlay is built from — a quad that size, at that
  distance, oriented by `faceCamera()`, maps one texture pixel onto one
  plate pixel. It asks the projection rather than the field of view,
  because this projection's centre stands a unit behind the eye and its
  frame is therefore a shade wider than the angle alone would make it.

  `Orbit` is a viewpoint as a POINTER states it — yaw and pitch in
  degrees about the target, and the distance from it — with `orbitOf()`
  reading one off a camera and `cameraAt()` putting a camera back on one.
  The two are exact inverses, which is what lets a control take hold of a
  camera rather than replace it: reading a camera's orbit and moving it
  by nothing gives that camera back, keeping its aim, its up axis and its
  lens. A host that offers a drag over a 3D view spells these rather than
  deriving a viewpoint of its own.

**`mesh/render`** — `SigilGeometryMeshRender`, needs `mesh` and
`mesh/camera`.

- **`mesh/render/device/Painter.h`** — `render::deviceRuntime(device)`,
  the device executor of that seam, beside `Runtime::cpu()`. It is its
  own target so the feature above stays free of a device: a consumer
  that draws meshes on the host links no renderer's dependencies. What
  differs from the host is not a shading disagreement — the host sorts
  triangles back to front and antialiases their edges, this depth-tests
  them and does not — and a panel is the same BYTES on either, because a
  panel's content is Skia's to rasterise whichever executor holds it.
  One readback per mesh draw: a canvas does not name the texture behind
  it, so there is nothing to compare against the device to decide the
  pixels could stay where they are.

- **`mesh/render/Runtime.h`** — the seam a draw executes through, as a
  value. `Executor` is what a runtime supplies (the mesh draw and the
  panel draw); `Runtime` holds one and compares like the model it holds;
  `Runtime::cpu()` is the built-in executor, an erased value of
  SigilCoreComparable's shape. glm, std and that leaf only — the header
  names no device, so a GPU executor arrives from a feature that owns one
  without this target learning about it.
- **`mesh/render/Painter.h`** — the draws themselves: `drawMesh()`
  (transform, per-vertex lighting, back-to-front sort, emission),
  `drawPanel()`/`drawImagePanel()` (perspective-correct 2D content on a
  plane), and `MeshStyle` with `Light` — the shading mode, colour,
  texture, lights and the `Runtime` that performs the work. `Painter.cpp`
  is the doors; `Runtime.cpp` is the built-in executor behind them. Two
  fields of the style are the surface speaking for itself rather than the
  scene: `lit`, off for a surface that is its own light, whose colour and
  tint are then the whole of what it shows; and `filter`, nearest for a
  map whose texel edges must stay hard, which takes no mip level with it
  because blending two levels is the same bleed by the other door.
- **`mesh/render/Shading.h`** — the arithmetic a lit draw is composed of,
  for a tier with no shading language: `Environment` (the prefiltered
  chain a reflection reads, the cosine convolution a diffuse term reads,
  the orientation, the dials, the backdrop and the ground sphere it is
  projected onto), `equirectangularUv` and the two polynomials under it,
  `specularColor`, `fresnelRough`, `environmentBrdf` and
  `environmentSpecular`, `attenuate`, `refraction`, and
  `luminance`/`toneMap`, the display transform every lit sum ends at.
  Beside them `samplePanorama`, `environmentRadiance`,
  `environmentIrradiance`, `backdropRay` — the direction a sky pixel
  reads, which is the eye's ray at infinity and the exit of that ray
  from the ground sphere where `groundRadius` is past zero — and
  `drawBackdrop`, which paints the sky itself for the eye the view
  matrix places. These are the SAME closed forms a device shader is composed of,
  transcribed: two spellings of one arithmetic is what a host tier costs,
  and each is pinned by its own test while the two tiers' pictures are
  compared within a stated per-channel ceiling. Shading here is per
  VERTEX, so a coarse mesh under a bright sky reads as facets where a
  device reads as a curve.

**`mesh/curve`** — `SigilGeometryMeshCurve`, needs `mesh` and
`mesh/camera`. The spline and the rails read off it; what a rail
CARRIES is a point operator and lives in `mesh/pop`.

- **`mesh/curve/Frame.h`** — `Frame3`, the moving frame every rail is a
  sequence of. Its own header because both the sweep and the pose stand
  on it and neither stands on the other.
- **`mesh/curve/Curve.h`** — `Spline3` (linear, Catmull-Rom or Bezier, open
  or closed) with `position()`, `tangent()`, `length()`, `sample()` and
  `sampleArcLength()`; the two rails — `curve::frames()`, parallel-transport
  `Frame3`s that do not flip at inflections, and `curve::hangFrames()`, a
  window of a closed loop whose across-vector is held world-vertical; and
  `project()` to draw the curve as a 2D path under a camera.
- **`mesh/curve/Pose.h`** — the rail addressed by DISTANCE rather than by
  index: `curve::poseAlong()` answers the `Frame3` at an arc length, over
  a rail you hold or over a spline that builds one, under the same
  `path::Wrap` policy the 2D side uses. A pose IS a rail frame — the same
  type, measured a different way — so a camera flying a curve and a ring
  of a sweep speak one vocabulary.

**`mesh/pop`** — `SigilGeometryMeshPop`, needs `mesh/curve` (and through
it `mesh` and `mesh/camera`); its generators draw from `path`'s noise.
Point operators are the subject; the point cloud is what they operate
on, so the cloud vocabulary lives in this feature beside the chain
language — and so does every piece of GPU-focused mesh work the library
has: the swept operator with its own kernel, both device executors, and
the decoration a compiled module needs before a driver may be handed it.
The built-in CPU executor divides its passes and its kernel dispatches
through SigilCore's schedule, and `Runtime::cpu(itemGrain)` is the same
executor dividing at a grain the caller names — the cloud is the same
either way, bit for bit. The device executors remain separate
implementations of the same dispatch seams.

- **`mesh/pop/Points.h`** — `Cloud` and its lane accessors, including
  `scalarNames()`, `vectorNames()` and `colorNames()`, which answer which
  lanes of each width the cloud carries (`Cloud.cpp`);
  the generators `onSpline()`, `grid()`, `ring()`, `scatterBox()` and
  `onMesh()` (`Generators.cpp`); the modifiers `jitter()` and
  `displaceNoise()`, `stampOptions()` and `promoteToPrimitives()`
  (`Modifiers.cpp`); the consumers `instance()` and `quads()`, which
  stamp a mesh at every point into one merged mesh (`Stamp.cpp`); and
  `drawBillboards()`, camera-facing sprites
  (`Billboards.cpp`). `BillboardStyle::textureLane` names a colour lane of
  {uOffset, vOffset, uScale, vScale} windows — what a `pop::AtlasCell` operation
  writes into `"Tex"` — and each splat then draws THAT CELL of the
  sprite, so one sheet splats as a field of different sprites. It is
  named rather than assumed, because a cloud may carry `"Tex"` for the
  stamping path while its splats are meant to be one sprite.

  **A cloud is ONE DRAW.** The splats of one call share a sheet and a
  blend mode, so they go down as a single sprite-atlas batch in depth
  order: the cell, the tint and the size are per sprite rather than per
  draw, and a dense cloud costs the canvas one draw instead of one per
  point — which is what a backend that must break its render pass at
  every additive draw charges for. The splat stays square whatever the
  aspect of the cell it takes. Because one shader samples the whole
  sheet, a cell's coordinates are pulled half a texel in so a linear
  filter cannot reach the cell next door, which holds while a splat is
  near the size of its cell: a sheet whose cells are meant to be sampled
  to their very edge wants a one-texel gutter, and a sheet seen far
  smaller than its cells is read from a mip level of the whole sheet,
  where the boundaries are averaged over whatever the gutter.

  **A modifier is its operator without a chain.** `jitter()` runs the
  `Jitter` operator's own kernel over the positions and
  `displaceNoise()` reads the field `Noise` displaces by
  (`pop::noiseField`), so a cloud perturbed with a chain and a cloud
  perturbed without one move by the same floats. One verb is one field:
  a second arithmetic under one name would mean nobody could say which
  of them a picture came from.
- **`mesh/pop/Pop.h`** — the operator chain language and the runtime seam
  it executes through, both in the `pop` scope, in one include over four
  headers: `Operations.h` carries the attribute reference a filter addresses,
  the twenty-five operator descriptions and the `Operation` variant they form,
  with `pop::operationName()` naming one; `Runtime.h` the seam — `pop::Executor`
  is what a runtime supplies, `pop::Runtime` is an erased value of
  SigilCoreComparable's shape, `pop::cook()` evaluates a chain on the one
  it is given — together with the helpers every executor shares
  (`laneFill()`, `seedLanes()`, `seedAttributes()`, `exportLanes()`,
  `setField()`/`getField()`, `noiseField()`, `attributeFor()`/`cloudLaneFor()`);
  `Sinks.h` the sinks a cooked chain is spent into; and `Builder.h` the
  artist's spelling, where `pop::on()` opens a chain. The field table
  behind `pop::setField()`/`getField()` is `Fields.cpp`; the lane fill,
  the two ends of every cook (`seedLanes()`, `seedAttributes()`,
  `exportLanes()`) and the name table are `Lanes.cpp`; the built-in
  executor, the `Runtime::cpu()` value with its `Runtime::cpu(itemGrain)`
  spelling, and the `cook()` door that checks an executor's capability
  before dispatching are `Cook.cpp`, with the four operators that read
  points they do not own in `Neighbourhood.cpp`; the mesh-forming sinks
  `pop::cookMesh()` and `cookSweep()` are `Sinks.cpp`.
- **`mesh/pop/Kernel.h`** — the seam between the two ends of one piece of
  arithmetic: `kernel::OperationArguments` (the argument block, every member a
  four-component vector so its bytes stand at the same offsets in a
  uniform buffer), `kernel::OperationDispatch` (which lane fills each binding
  role), `has()`, `describe()`, `run()` and `operationSpirv()`. It also names
  the namespace every kernel here shares. `Kernel.cpp` packs
  and calls.
- **`mesh/pop/Sweep.h`** — the swept operator as a subject: the
  door from an arbitrary outline, `pop::profile::fromPath()` (the two
  unit cross-sections a sweep is usually given are the kit's
  `sections::circle()` and `sections::line()`); `SweepOptions` with
  `SweepNormals`; the
  two swept formers `pop::sweep()`, over a rail you built and over the
  spline that builds one; and the seam a device replaces —
  `pop::SweepExecutor` (one call, `rings()`), `pop::SweepRuntime`
  holding one, `pop::describe()` turning a rail and a profile into a
  `kernel::SweepDispatch`, and `kernel::run()` and `kernel::sweepSpirv()`
  as the two ends of the one arithmetic. `Sweep.cpp` holds the profiles, the
  packing, the topology and the built-in executor; `mesh/pop/device/Sweep.cpp`
  the device one.

  **There is one sweep, and the shape is a parameter.** `sweep()` carries
  a 2D `path::Polyline` along a rail: every ring is that contour placed on
  one `Frame3` — x along the binormal, y against the normal, Skia's y-down
  convention, the one `mesh::extrude()` uses — sized by `scale` times
  `taper` at the frame's `t`, with u across the profile and v the frame's
  `t`. A circle profile forms a tube, the two-point line profile forms a
  flat band (on a hung rail, a gravity-rigged banner), and anything
  `fromPath()` flattens forms an extrusion that follows the curve. The two
  built-in profiles are UNIT shapes, which is why neither takes a radius
  or a width: `SweepOptions::scale` is the size, and a profile that
  carries its own — a flattened outline — leaves it at 1.
  `SweepOptions::normals` picks where a vertex normal comes from:
  `Radial` (the profile's offset itself, what a round profile wants),
  `Frame` (the rail's normal, what a flat band wants) or `Geometric`
  (averaged from the formed triangles, which any profile can take).
  `caps` closes an open rail's two ends with a fan to the ring's centre
  and assumes a convex profile; the spline overload drops it for a closed
  spline, which has no ends. A CLOSED profile wraps back onto its first
  point, so only an OPEN one lets u reach 1 — which is why
  `sections::circle()` duplicates its seam point and comes back open.
  There are two overloads: one over a rail you built, which is where a
  GPU executor forms the same rings, and one over a `Spline3`, which
  builds a transported rail of `segments` frames first. Both run on
  `SweepOptions::runtime`.

- **`mesh/pop/Stamp.h`** — the stamping operator as a subject —
  TouchDesigner's Copy, Houdini's copy-to-points — and the seam a device
  replaces: `points::StampExecutor` (one call, `vertices()`),
  `points::StampRuntime` holding one, `kernel::StampDispatch` (the
  stamp's lanes and the points', each four floats wide), and
  `kernel::run()` and `kernel::stampSpirv()` as the two ends of the one
  arithmetic. `points::describe()` and `points::instance()` are in
  `Points.h`, because they are where a Cloud and a Mesh become one;
  `Stamp.cpp` is the packing, the index runs and the built-in executor,
  and `mesh/pop/device/Stamp.cpp` is the device one. Nothing here names a Cloud, a
  Mesh or a device, so the seam is declarable before either of them.

  **The stamp rides the point, and the arithmetic is written once.** A
  point's origin, its size from the `size` lane, its direction from
  `dir` (or `normal`) and its tint from `tint` place the stamp's every
  vertex, and the cloud's `Tex` window remaps that vertex's uv as it is
  formed rather than in a second pass afterwards. Which of the optional
  lanes the result carries is the STAMP's answer and not the kernel's: a
  stamp with no normals forms none, because a lane is present on a mesh
  when it is sized to the positions and every consumer reads that as the
  presence bit.
- **`mesh/pop/kernels/Pop.slang`** — the point operators themselves, one
  entry point with the operator chosen by a uniform: one dispatch runs one
  operator over every point, so the branch is uniform across it, and one
  entry point is one pipeline and one generated function.
  **`mesh/pop/kernels/Sweep.slang`** is the ring vertex and
  **`mesh/pop/kernels/Stamp.slang`** the stamped vertex, written the same
  way. `src/common/material/cmake/Slang.cmake` compiles all three (`sigil_slang_module` with
  `CPP_VAR` and `SPIRV_VAR`, and `sigil_slang_kernel_flags` to pin the
  float model).

**`mesh/codec`** — `SigilGeometryMeshCodec`, needs `mesh` and
`mesh/pop`. Its parsers
are private to the feature: tinyobjloader, cgltf, Alembic and simdjson
(the JSON a `.geo` is), with STL and PLY parsed by hand. One reader per translation unit —
`Obj.cpp`, `Gltf.cpp`, `Stl.cpp`, `PlyDecode.cpp`, `Geo.cpp`, `Alembic.cpp` —
behind the dispatcher in `Model.cpp`, sharing only what `Internal.h`
declares; the .geo reader's own attribute-class decoding is
`GeoLanes.cpp` beside it, over the tree `GeoInternal.h` holds;
`PlyEncode.cpp` and `GeoEncode.cpp` are the writers.

- **`mesh/codec/Model.h`** — what every reader produces: `Part` (one draw
  unit: a mesh in model space, its material factors and texture
  references, its custom attributes as named lanes, `asCloud()`), `Model`
  (the parts, and `merged()`, `mergedCloud()`, `bounds()`,
  `fitTransform()`, `materialSlotCount()` across them) and the `Resolver`
  a reader consults for external references.
- **`mesh/codec/Decode.h`** — the doors in: `decode::model()` from bytes with
  a path hint, or from a file with its siblings resolved; and
  `decode::alembic()` with `AlembicOptions` choosing the time. OBJ (with
  MTL), glTF 2.0 as `.gltf` or `.glb`, ascii and binary STL, ascii and
  binary-little-endian PLY, Ogawa Alembic, and Houdini's JSON `.geo`. A
  hint carrying no useful extension — a blob off a wire, out of a cache,
  or from a URL path ending in nothing — falls through to a sniff, and the
  sniff covers every one of those but OBJ, which has no signature to be
  known by.
- **`mesh/codec/Encode.h`** — the doors out: `encode::ply()` over a `Cloud` or a
  `Mesh`, ascii by default or binary via `PlyOptions`; and `encode::geo()`
  over the same two, the exact return leg of the `.geo` reader.

`encode::geo` is the one to reach for when the destination IS Houdini:
the same lanes travel under the names that side already knows them by,
with no suffix folding to arrange between two spellings, and the v axis
of a `uv` is flipped back to the file's convention on the way out. It
writes everything the reader understands and nothing it does not — which
is why a group leaves as a scalar lane rather than as a group: the reader
turns a group INTO a 0/1 scalar, and nothing on this side can tell such a
lane from any other scalar. A mesh comes back UNWELDED, and that is the
format rather than the writer: a `.geo` addresses a polygon's corners
through a vertex list, and the reader gives every corner its own mesh
vertex so a per-corner uv or normal survives a seam, so a cube written
with 8 shared positions returns with 36 — same positions, same winding,
same attribute values, a different vertex count.
