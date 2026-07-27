# SigilShape

Namespace `sigil::shape`, target `SigilShape`, headers
`include/sigilshape/`. The higher-level drawing vocabulary over Skia —
Skia-only by contract (no compose kernel, no motion, no Qt), so
SigilCompose and any product can adapt downward while this library
stays extractable.

Eight headers, one dependency direction (later headers may use earlier):

```
Geometry.h   SkPath -> Polyline (adaptive flatten, corners exact)
             SkPath -> Sampled  (N arc-length-uniform points)
             cyclic alignment, Catmull-Rom rebuild, lerp
Blend.h      the Illustrator blend tool over that currency
Ops.h        the Pathfinder panel (unite/subtract/intersect/exclude/
             simplify/offset over Skia pathops) and the Distort menu
             (Roughen/Zigzag/PuckerBloat/Twirl as parameter values;
             chain() composes non-destructive recipes)
Mesh.h       renderer-neutral Mesh + procedural generators
Curves.h     Spline3 (Catmull-Rom/Bezier/linear over SkV3 knots) with
             arc-length sampling, parallel-transport frames, tube()/
             ribbon() sweeps, and project() to a 2D path
Space.h      Skia's 3D: SkM44 camera, painter-pipeline drawMesh,
             perspective drawPanel / drawImagePanel
Points.h     Cloud = positions + named attribute lanes; generators
             (onSpline/grid/ring/scatterBox/onMesh), jitter/noise,
             instance()/panels() stamping, drawBillboards() particles
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

## Demo and tests

```
./build/bin/Debug/shape_demo [outdir] [assetdir]   # 9 PNG panels, +1 with assets
./build/bin/Debug/shape_test
```

Panels: blend_morph, blend_color, blend_spine, materials,
mesh_perspective, mesh_chrome, panels_space, pathfinder,
splines_particles — and materials_hdri when `fetch_assets` has
populated the asset dir (the Poly Haven studio HDRI through
SigilLoader/OIIO). `shape_test` (32 tests) covers resampling
invariants, blend endpoint/spacing/OKLab rules, pathfinder booleans and
offset, distort sanity, spline interpolation/arc-length/frame
orthonormality, tube/ribbon well-formedness, cloud generators and
lanes, instancing, billboard coverage, extrude caps, grid/torus
normals, camera projection, and material shader compilation.
