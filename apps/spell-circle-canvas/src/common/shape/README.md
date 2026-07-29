# SigilShape

Namespace `sigil::shape`, target `SigilShape`, headers
`include/sigilshape/`. The higher-level drawing vocabulary over Skia —
no compose kernel, no motion, no Qt — so SigilCompose and any product
can adapt downward while this library stays extractable.

Two type currencies, split by context: **glm** (`vec2/vec3/vec4/mat4`)
for everything directly 3D — Mesh, Curves, Points, Pop, Import, the
Camera — and **Skia** for everything genuinely 2D or draw-time —
SkPath outlines, paint/style colors, textures, the canvas. Space.h is
the declared bridge (`toSkM44()` is the seam; both stores are
column-major). SigilWorld consumes the glm side without touching Skia
except for `SkImage` textures. One glm trap to know: `vec3::length()`
is the COMPONENT COUNT (3), not the magnitude — always spell
`glm::length(v)`.

Twelve headers, one dependency direction (later headers may use earlier):

```
Geometry.h   SkPath -> Polyline (adaptive flatten, corners exact)
             SkPath -> Sampled  (N arc-length-uniform points)
             cyclic alignment, Catmull-Rom rebuild, lerp
Blend.h      the Illustrator blend tool over that currency
Ops.h        the Pathfinder panel (unite/subtract/intersect/exclude/
             simplify/offset over Skia pathops) and the Distort menu
             (Roughen/Zigzag/PuckerBloat/Twirl as parameter values;
             chain() composes non-destructive recipes)
Mesh.h       renderer-neutral Mesh + procedural generators, plus the
             PRIMITIVE attribute lanes (Mesh::prims — one float4 per
             TRIANGLE, named like the point lanes) and
             bakePrimColor()
Import.h     model files into that Mesh currency: OBJ (+MTL, via
             tinyobjloader), glTF 2.0 .gltf/.glb (cgltf; node
             transforms baked, base-color material + texture),
             ascii/binary STL, and PLY (ascii + binary LE; hand-
             rolled) — THE attribute carrier: every non-conventional
             vertex property becomes a named lane, faceless files are
             point clouds — and Alembic .abc (Ogawa; meshes + point
             clouds at a chosen nearest-sample time, arbGeomParams as
             lanes). glTF _NAME custom accessors (Blender/
             Houdini exports) land as lanes too; Part::asCloud() /
             Model::mergedCloud() pour attributes into shape::Cloud —
             scatter in Houdini, cook in pops, stamp with points::
             here. Bytes in, Model{Part…} out; external refs
             (.mtl/.bin/textures) pull through a caller Resolver
             (a directory, a SigilLoader Hub, anything). Textures stay
             ENCODED bytes — SigilImage decodes, same split as the
             loader. merged() bakes part colors into the color lane;
             fitTransform() puts any unit scale on the table
Curves.h     Spline3 (Catmull-Rom/Bezier/linear over glm knots) with
             arc-length sampling, parallel-transport frames, tube()/
             ribbon() sweeps, and project() to a 2D path
Space.h      Skia's 3D: the glm camera + painter-pipeline drawMesh,
             perspective drawPanel / drawImagePanel (toSkM44 seam)
Pop.h        POP combinators as VALUES: a Chain of operator values
             (SplineScatter generator; Jitter/Noise/Ramp/Vary/LookAt/
             Math filters, Promote for the prim class) with CPU
             executors — cook() -> Cloud,
             cookMesh(stamp) -> one Mesh, cookTube()/cookRibbon()
             sweep the cooked points as a path. The same Chain runs
             GPU-side in SigilWorld (addPoints); formulas match bit
             for bit
Points.h     Cloud = positions + named attribute lanes; generators
             (onSpline/grid/ring/scatterBox/onMesh), jitter/noise,
             instance()/panels() stamping, drawBillboards() particles
Save.h       the return leg of Import.h: save::ply(cloud|mesh) —
             ascii PLY by default (binary_little_endian via PlyOptions,
             smaller and bit-exact for big point dumps) with EVERY
             lane written ("normal" as nx/ny/nz,
             "tint" as uchar colors, customs as name / name_x.. /
             name_r..; the importer folds them back, so round trips
             are lossless). Loudest use: World::readPoints a
             GPU-cooked pop surface and hand the file to Houdini or
             Blender
Materials.h  literal materials: gold foil / chrome / glass SkSL
             over normal maps + equirect environments
Easel.h      the ARTIST surface: stock shapes (star/ngon/dot/pill/
             ring), fluent Shape recipes (.bloat().roughen().offset()
             .gold()), Blend chains (.colors().steps().along()),
             Wire (.through().closed().tube()/beads()), Particles
             (.on().count().drift().ramp().glow()) — loud defaults,
             one draw() at the end, made for the sketch host (see the
             easel_playground study)
```

## Blend (the Illustrator study)

`blend::make(keys, options)` expands two-or-more `Key`s (outline +
fill/stroke/opacity) into drawable `Step`s, faithful to Object > Blend:

- **Spacing**: `Steps` (exact count), `Distance` (px along the spine),
  `SmoothColor` (step count from color distance — the 254-step
  black-to-white rule, scaled).
- **Spine**: default is the straight line between key centroids
  (plain interpolation already follows it); set `Options::spine` to any
  path to ride a spiral, `reverseSpine` to flip direction,
  `Orientation::AlignToPath` to rotate steps with the tangent.
- **Correspondence**: both outlines are resampled to `samples`
  arc-length points per contour; closed contours get least-squares
  cyclic alignment (rotation + direction), the stable version of
  dragging between anchor points. Unmatched contours collapse toward
  the other key's centroid.
- **Color**: interpolation runs in OKLab (`detail::lerpOklab`), so
  red-to-blue passes through neither gray nor mud.

## Mesh (procedural geometry, shared with SigilWorld)

`Mesh {positions, normals, uvs, indices}` is deliberately
renderer-neutral: `space::drawMesh` consumes it through SkVertices and
`sigil::world::World::addSurface` uploads the same buffers to Diligent.
Generators: `extrude(path)` (earcut caps with holes, flat-shaded
walls), `revolve(profile)`, `grid(nu, nv, fn)`, and the presets
`torus`, `superellipsoid`, `cylinderPanel` (the curved diegetic
screen), `quad`.

Conventions: mesh space is y-UP right-handed (extrude flips the y-down
path upright and centers on its bounds); UVs are image-convention —
(0,0) samples the texture's top-left — in BOTH renderers.

### Append keeps every lane coherent (2026-07-28)

`Mesh::append` used to concatenate `normals` and `uvs` with bare
`insert` calls while `colors` (and later `prims`) got a padding dance.
That is not a cosmetic asymmetry: every consumer reads "lane sized to
positions" as the mesh's presence bit — `space::drawMesh` literally sets
`hasNormals = normals.size() == positions.size()` — so merging a
normal-less mesh into a normal-bearing one dropped lighting for the
WHOLE merge, not just the half that lacked normals. Same for `uvs` and
texturing. The everyday source is `points::instance` over a stamp
authored with positions and indices only, and `import`'s `merged()`
across model parts.

Both lanes now pad like `colors`: if EITHER side authors the lane, the
merged mesh carries it sized to `positions`; if neither does, the lane
stays empty (append pads an existing lane, it never conjures one).

The pads, and why: a missing **normal** pads `{0, 0, 1}`, not zero. A
zero normal is degenerate three ways — `Mesh::transform`'s
`normalized()` keeps it zero forever, every lighting term collapses so
the padded half renders BLACK (louder than the bug being fixed), and
shader-side `normalize()` of it is undefined. +Z is already the
library's answer for "no direction" (`detail::normalized`'s fallback,
`basisFor`'s axis), is unit length, and shades the padded half like a
flat card. Callers wanting the geometric truth call `computeNormals()`
on the merge. A missing **uv** pads `{0, 0}` — texel (0,0), the same
convention `Cloud::append` gives a missing `"uv"` lane.

Pinned by `Mesh.AppendKeepsNormalAndUvLanesSizedToPositions`. No
`shape_demo` panel moved (all 14 byte-identical): the catalog never
merges a mixed pair.

### Primitive attributes (2026-07-28)

The TouchDesigner/Houdini **prim class**, the point lanes' sibling. A
primitive here IS a **triangle** — one index triple — because Mesh is
the currency both renderers already consume; every other candidate
(a stamp instance, a swept ring, a cooked contour) is a GROUPING of
triangles and is expressible as a lane VALUE instead of a second
container, which is exactly what the reserved `"Id"` lane does.

`Mesh::prims` is a name -> `vector<vec4>` map sized to
`triangleCount()`, addressed by the same names `pop::AttrRef` uses:
`prim(name)` creates on touch, `primIf(name)` reads. Conventional
names are `"Color"` (flat per-primitive tint) and `"Id"` (`.x` = the
piece the triangle belongs to); anything else is a custom lane.
`Mesh::append` concatenates lanes and pads a missing side by name
("Color" pads white, everything else zeros) — the posture
`Cloud::append` takes for the point class.

Three ways in and three ways out:

- **In, by hand**: write `mesh.prim("Color")` on any formed model.
- **In, from the point class**: `points::promoteToPrims(mesh, cloud,
  cloudLane, primLane)` (Houdini's Attribute Promote) for anything
  `points::instance` stamped, and `pop::Promote` — the chain op,
  spelled `.promote(from, to)` — which `popops::cookMesh` honours.
- **Out, natively**: `space::MeshStyle::primColorLane` multiplies the
  lane into each triangle's colour with no vertex duplication (Lit
  mode only; Normals/Uv are buffers and stay unmodulated).
- **Out, portably**: `mesh::bakePrimColor(mesh, lane)` unwelds into
  per-vertex colours, so SigilWorld's Diligent pipelines — or any
  vertex-only renderer — show flat per-primitive colour unchanged.
- **Out, to the interchange world**: `save::ply` declares each lane on
  the PLY **face** element (`name_r/_g/_b/_a`, after the index list),
  which is how Houdini and Blender read per-face attributes.

Executor boundary, stated: prim lanes are **CPU-only** in this pass.
The GPU executor cooks the point class into lane arenas and has no
Mesh value to promote onto, so `World::addPoints`/`addPointsOn`/
`setPoints` **decline** any chain holding `pop::Promote` (return 0)
rather than dropping it silently — the same graceful boundary
`MeshScatter` gets.

Deferred on purpose: the swept sinks (`cookTube`/`cookRibbon`/
`cookSweep`) promote nothing — their triangles ride RESAMPLED
cross-sections, so there is no owning point; the PLY *importer* skips
face properties, so prim lanes are export-only today; and no edge
class exists (nothing in the library addresses half-edges).

## Space (Skia's 3D)

One `Camera` (eye/target/up/fovY) drives two devices:

- `drawPanel` / `drawImagePanel`: concat a full perspective SkM44 and
  let Skia rasterize — perspective-correct 2D content on planes, the
  zero-copy diegetic-panel path.
- `drawMesh`: CPU transform + per-vertex Blinn lighting + backface
  cull + painter sort, chunked under the 16-bit SkVertices limit.
  `MeshStyle::Mode::Normals` renders a DEVICE-space normal G-buffer
  (+y down, rgb = n*0.5+0.5) instead — feed that surface to a material
  shader and per-pixel chrome lands on true 3D geometry (see
  shape_demo's mesh_chrome panel).

## Materials (the literal ones)

A two-channel deferred pass in miniature: **normals** (where the
surface points) × **environment** (what it reflects), combined per
pixel by an SkRuntimeEffect.

- `bevelNormals(path, bounds, bevelPx)`: coverage → blur →
  smoothstep shoulder → Sobel; flat interior, rounded rim.
- `Environment::studio()` / `::sunset()`: procedural equirect bakes
  (F32, HDR-ish softboxes; sunset is the y2k chrome horizon).
  `Environment::fromEquirect(image)` wraps a loaded panorama — an
  OIIO-decoded .hdr from SigilLoader drops straight in.
  `image(roughness)` returns cached blurs (CPU box blur with proper
  horizontal wrap — equirect u is periodic).
- `gold` (F0-tinted reflection, fbm foil crinkle, hash glints),
  `chrome` (contrast-crushed env, brushed anisotropic smear,
  `exposure` gain for dim real HDRIs), `glass` (backdrop child
  refracted through the normal field, fresnel-weighted reflection,
  edge glow). `drawGold/drawChrome/drawGlass` run the whole pipeline
  for one path.

## Non-destructive posture

Everything upstream of a pixel is a VALUE with editable parameters:
blend Keys/Options, distort structs and `ops::chain` recipes, Spline3
control points, Cloud lanes, mesh generator arguments. Nothing bakes
until a draw call asks; re-run any stage after touching any dial. The
attribute vocabulary is deliberately Houdini-ish: generators write
conventional lanes ("t", "tangent"/"normal"/"binormal", "size",
"tint"), consumers read them by name, and cooked lanes (write your own
vector per point) slot in anywhere a built-in one does.

### One scatter hash (2026-07-28)

`detail/Hash.h` holds the single PCG the library scatters with —
`pcgAdvance` / `pcgMix` / `pcgHash`. `Pop.cpp`'s `hash1` (bit-matched to
the Slang pop kernels, so this is ABI) and `Points.cpp`'s `pcg` PRNG were
two copies of the same arithmetic; only `Pop.cpp`'s was covered by the
GPU-parity exemption. They were verified bit-identical over 2^26 inputs
before merging, and the merged definition reproduces both streams
exactly (`comet_points.ply` from `world_demo` is byte-identical across
the change). `Pop.SharedPcgHashKeepsBothConsumersBitStable` pins goldens
taken from the two originals through both public surfaces. The scatter
BASIS stays literal at the `Pop.cpp` site, as `VecMath.h` already notes.

## Demo and tests

```
./build/bin/Debug/shape_demo [outdir] [assetdir]   # 12 PNG panels, +2 with assets
./build/bin/Debug/shape_test
```

Panels: blend_morph, blend_color, blend_spine, materials,
mesh_perspective, mesh_chrome, panels_space, pathfinder,
splines_particles, pop_models, pop_prims, yarn_marquee — and
materials_hdri / imported_models when `fetch_assets` has
populated the asset dir (the Poly Haven studio HDRI through
SigilLoader/OIIO, the Khronos Avocado glTF). `shape_test` (75 tests)
covers resampling
invariants, blend endpoint/spacing/OKLab rules, pathfinder booleans and
offset, distort sanity, spline interpolation/arc-length/frame
orthonormality, tube/ribbon well-formedness, cloud generators and
lanes, instancing, billboard coverage, extrude caps, grid/torus
normals, camera projection, material shader compilation, the import
formats (OBJ/glTF/GLB/STL/PLY/Alembic) and PLY save round trips, the
pop chains and their sinks, and the primitive attribute layer
(lane sizing/append padding, promote, flat draw, bakePrimColor, PLY
face properties).
