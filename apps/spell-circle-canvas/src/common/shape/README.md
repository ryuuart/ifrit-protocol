# SigilShape

A C++ library for 2D and 3D drawing on top of [Skia](https://skia.org).
It gives you path resampling, boolean and distortion operators over
`SkPath`, shape interpolation, a renderer-neutral triangle mesh with
procedural generators plus model import and export, splines with swept
geometry, point clouds carrying named attribute lanes and a point-operator
chain language, a software rasterizer that draws meshes and perspective
panels onto an ordinary `SkCanvas`, and material shaders written as Skia
runtime effects.

It links only Skia and [glm](https://github.com/g-truc/glm) publicly. There
is no windowing, no GPU device, no UI framework and no scene graph — you
hand it values, it hands you paths, meshes, clouds and pixels.

Namespace `sigil::shape`. Headers under `include/sigilshape/`.

## Using it

```cpp
#include <sigilshape/Easel.h>
#include <sigilshape/Pop.h>
#include <sigilshape/Space.h>

using namespace sigil::shape;

void paint(SkCanvas &canvas, SkSize viewport) {
  // 2D: a six-pointed star, bloated, roughened and filled. Every dial
  // stays editable — the recipe is not applied until draw().
  easel::shape(easel::star(6, 90))
      .bloat(0.3f)
      .roughen(3)
      .fill({1.0f, 0.6f, 0.2f, 1.0f})
      .draw(canvas, {320, 240});

  // 3D: scatter points along a window of a closed loop, drift them with
  // noise, smooth the kinks out, colour them along the loop, then sweep
  // a tube through the result.
  const Mesh comet =
      pop::on(std::vector<glm::vec3>{{-300, 0, -100},
                                     {0, 140, 120},
                                     {300, 0, -100}})
          .count(4000)
          .window(0.9f, 0.3f)
          .noise(18)
          .smooth()
          .fade({1.0f, 0.3f, 0.6f, 1.0f}, {0.2f, 0.9f, 1.0f, 1.0f})
          .tube(9);

  space::Camera camera;
  camera.eye = {0, 180, 640};

  space::MeshStyle style;
  style.backfaceCull = true;

  space::drawMesh(canvas, comet, space::place({0, 0, 0}), camera,
                  viewport, style);
}
```

Nothing above holds a device, a context or a frame. `Mesh` is a plain
struct of vectors; `pop::Chain` is a `std::vector` of variants; the easel
objects are values you can copy, tweak and re-cook.

## The mental model

**Two type currencies.** Anything genuinely three-dimensional — mesh
vertices, spline knots, camera vectors, cloud positions, transforms —
speaks glm (`vec2`, `vec3`, `vec4`, `mat4`). Anything genuinely
two-dimensional or draw-time speaks Skia: `SkPath` outlines, `SkColor4f`
paint, `SkImage` textures, `SkCanvas`. `Space.h` is the declared bridge
between them, and `space::toSkM44()` is the seam. Because glm's `mat4` and
Skia's `SkM44` are both column-major, that conversion is a straight memory
pour with no transpose.

**Resampling is the substrate.** `Geometry.h` reduces any path to one of
two forms: a `Polyline` (adaptive curve flattening that keeps corner
anchors exact) or a `Sampled` (exactly N points spaced uniformly by arc
length). Everything above stands on those two. Blending interpolates
`Sampled` pairs. Distortions displace resampled points and rebuild.
Extrusion walls sweep flattened contours. Swept geometry rides arc-length
samples of a spline.

**Values, not baked results.** Options structs, distortion structs,
operator values, splines, clouds and chains are all plain data you edit and
re-cook. `ops::PathOp` plus `ops::chain()` compose a non-destructive
recipe; `blend::Options`, `curves::TubeOptions` and `pop::Chain` behave the
same way. Nothing is committed until a draw call or an explicit cook asks
for it, so changing one dial and re-running is always available.

**Named attribute lanes, in two classes.** The *point* class lives on
`Cloud`: string-keyed lanes of scalars, vectors and colours, created on
first touch and sized to the point count. Generators write conventional
names — `"t"`, `"tangent"`, `"normal"`, `"binormal"`, `"size"`, `"tint"`,
`"uv"` — and consumers read them back by name, so your own cooked lane
slots in wherever a built-in one does. The *primitive* class lives on
`Mesh::prims`: `vec4` lanes sized to `triangleCount()`, because a primitive
here *is* one triangle. Its conventional names are `"Color"` (a flat
per-triangle tint) and `"Id"` (`.x` carries which piece the triangle
belongs to). `points::promoteToPrims()` and the `pop::Promote` operator
move values from the point class to the primitive class.

**`Mesh` is the shared currency.** The same `positions`/`normals`/`uvs`/
`colors`/`indices` buffers feed the CPU painter in `Space.h` and upload
directly to a GPU renderer downstream. Nothing renderer-shaped lives in the
struct.

**`pop::Chain` is a backend-neutral description.** It is a vector of
operator variants, not a program — and a value: every operator, `Mesh`
and `Cloud` compares by content with `==`, so a reconciler can ask
whether a chain changed. The CPU executor in `popops::cook()` is
the reference implementation; a GPU consumer can execute the identical
chain as compute dispatches, and the two are required to agree bit for bit
— which is what makes the hash helpers and the variant order load-bearing
(see below).

**`Easel.h` is the artist façade.** Stock outlines (`dot`, `ngon`, `star`,
`pill`, `ring`) and four fluent value types over everything underneath:
`Shape` (`offset`, `roughen`, `zigzag`, `bloat`, `pucker`, `twirl`,
`unite`, `cut`, `clip`, `step`, plus one look — `fill`, `stroke`, `gold`,
`chrome`, `glass` — and `path()`/`draw()`), `Blend` (`colors`, `steps`,
`every`, `smoothColor`, `along`, `turning`, `smooth`, `between`, `cook`,
`draw`), `Wire` (`through`, `closed`, `straight`, `spline`, `tube`,
`ribbon`, `beads`, `draw`) and `Particles` (`on`, `inBox`, `onSurface`,
`count`, `seed`, `drift`, `jitter`, `size`, `ramp`, `sprite`, `cook`,
`glow`). It adds no capability; it picks defaults and reads like a
sentence.

## The headers

They form a dependency chain — each header includes those it needs, so
including a later one pulls the earlier ones in.

Four headers stand alone and depend on nothing else in the library:

- **`Geometry.h`** — the resampling core. `Polyline` and `flatten()`,
  `Sampled` and `resample()`, `bestAlignment()`/`applyAlignment()` for
  matching two closed contours, `toPath()` to rebuild (optionally through
  Catmull-Rom cubics), and `lerp()`.
- **`Ops.h`** — path operators. Booleans over Skia's pathops (`unite`,
  `subtract`, `intersect`, `exclude`, `simplify`, and a stroke-expansion
  `offset`), and four distortions as parameter structs you apply on demand:
  `Roughen`, `Zigzag`, `PuckerBloat`, `Twirl`. `PathOp` and `chain()`
  compose them, `offsetBy()` adapts `offset` into a step.
- **`Materials.h`** — reflective materials as `SkRuntimeEffect` shaders.
  `bevelNormals()` derives a normal map from a path's coverage;
  `Environment` supplies what the surface reflects, either as a procedural
  bake (`studio()`, `sunset()`) or a loaded equirectangular panorama
  (`fromEquirect()`), with cached roughness blurs. `gold()`, `chrome()` and
  `glass()` return shaders; `drawGold()`, `drawChrome()` and `drawGlass()`
  run the whole pipeline for one path.
- **`Mesh.h`** — the mesh currency and its generators. The `Mesh` struct
  (positions, normals, uvs, colors, indices, and the `prims` lane map),
  `append()`/`transform()`/`computeNormals()`/`bounds()`, and the
  generators `extrude()`, `revolve()`, `grid()`, `torus()`,
  `superellipsoid()`, `cylinderPanel()`, `quad()`, plus
  `mesh::bakePrimColor()`.

The rest build on those:

- **`Blend.h`** needs `Geometry`. Shape interpolation modelled on
  Illustrator's blend tool: `Key`s expand into drawable `Step`s under
  `Options` controlling spacing (`Steps`, `Distance`, `SmoothColor`), an
  optional spine path, orientation, sample density and outline smoothing.
- **`Space.h`** needs `Mesh`. Skia's 3D put to work: a `Camera`,
  `drawMesh()` (a painter-order software rasterizer with per-vertex
  lighting and `SkVertices` batching), `drawPanel()`/`drawImagePanel()`
  (perspective-correct 2D content on a plane), and the transform helpers
  `place()` and `faceCamera()`. `toSkM44()` is the glm-to-Skia seam.
- **`Curves.h`** needs `Mesh` and `Space`. `Spline3` (linear, Catmull-Rom
  or Bezier, open or closed) with `position()`, `tangent()`, `length()`,
  `sample()` and `sampleArcLength()`; `curves::frames()` for
  parallel-transport `Frame3`s that do not flip at inflections; the swept
  generators `tube()`, `ribbon()` and `banner()`; and `project()` to draw
  the curve as a 2D path under a camera.
- **`Points.h`** needs `Curves`, `Mesh` and `Space`. `Cloud` and its lane
  accessors; the generators `onSpline()`, `grid()`, `ring()`,
  `scatterBox()` and `onMesh()`; the modifiers `jitter()` and
  `displaceNoise()`; the consumers `instance()` and `panels()` (stamp a
  mesh at every point into one merged mesh) and `drawBillboards()`
  (camera-facing sprites); and `promoteToPrims()`.
- **`Pop.h`** needs `Curves` and `Points`. The operator chain language and
  its CPU executor.
- **`Import.h`** and **`Save.h`** need `Mesh` and `Points`. Import reads
  OBJ (with MTL), glTF 2.0 as `.gltf` or `.glb`, ascii and binary STL,
  ascii and binary-little-endian PLY, Ogawa Alembic, and Houdini's JSON
  `.geo`, producing a `Model` of `Part`s; external references resolve
  through a caller-supplied `Resolver`. Save writes PLY back out —
  `save::ply()` over a `Cloud` or a `Mesh`, ascii by default or binary via
  `PlyOptions`.
- **`Easel.h`** needs `Blend`, `Curves`, `Materials`, `Mesh`, `Ops`,
  `Points` and `Space`. It does not pull in `Pop`, `Import` or `Save`.

### The operators

`pop::Op` is a variant over twenty operator values, and `pop::Chain` is
a vector of them. Generators seed a chain: `SplineScatter` (points along a
window of a closed loop), `MeshScatter` (points on a formed model's
faces) and `PointSet` (an existing `Cloud` — an import's `asCloud()`, a
previous cook — every lane riding in as an attribute, so a Houdini group
arrives as a mask under its own name). Filters rewrite attributes in place: `Jitter`, `Noise`, `Ramp`,
`Vary`, `LookAt`, `Math`, `Relax`, `Set`, `Atlas`, `Lookup`, `Transform`
(any `mat4` on a position or a direction lane), `Peak` (push along a
direction lane), `Deform` (twist, taper or bend about an axis) and `Mix`
(blend two lanes into a third by a constant or a lane). `Group` is the
selector: it writes a mask lane from a sphere or box region, feathered at
its edge and combined into what the lane already holds (replace, union,
intersect, subtract). `Promote` and `Sort` are the primitive-class and
permutation-class operators.

Every operator addresses attributes by name through `pop::AttrRef`, with
`"P"`, `"T"`, `"Dir"`, `"Scale"`, `"Color"` and `"Tex"` as the well-known
names and anything else creating a custom lane on first write.

**Every filter takes a mask.** Each per-point filter carries a `mask`
field naming a lane; that lane's `.x`, clamped to `[0, 1]`, is how much of
the operator's write each point receives — `old + (new - old) * mask`. An
empty name (the default) is every point in full; naming a lane nothing has
written selects nobody, the way an empty group is empty. `Group` is one
way to write such a lane; a `Lookup`, a `Math` on a custom lane, or an
importer's attribute serve just as well. Both executors apply the mask
with the same expression.

`pop::on()` — over a loop, a `Mesh`, a `Chain` or a `Cloud` — returns a
`Builder` whose chained verbs (`count`, `window`,
`spread`, `seed`, `jitter`, `noise`, `vary`, `fade`, `tint`, `lookAt`,
`move`, `set`, `atlas`, `rampBy`, `order`, `orderBy`, `promote`, `smooth`,
`select`, `masked`, `transform`, `orient`, `peak`, `twist`, `taper`,
`bend`, `mix`, `mixBy`, `copy`, `op`) append operators — `masked()` sets
the mask on the filter just added — and the builder converts to a
`Chain`, so you can reach into any operator afterwards and re-cook. Sinks
turn a chain into geometry: `cook()` to a `Cloud`, `cookMesh()` to one
mesh of stamps, and `cookTube()`/`cookRibbon()`/`cookSweep()` treating the
cooked points as a path to sweep along.

## Conventions that will bite you

These are properties of the code. Getting one wrong produces geometry that
is silently, plausibly wrong rather than obviously broken.

- **Mesh space is right-handed and y-up.** Skia's 2D space is y-down.
  `mesh::extrude()` therefore explicitly negates the incoming path's y and
  centres the result on the path's tight bounds — an extruded shape does
  not sit where the source path sat.
- **UV origin is the texture's top-left**, the image convention, in every
  generator and every consumer. Formats that use a bottom-left origin
  (OBJ, Alembic) have their v flipped at import; glTF already matches.
- **`Polyline::signedArea()` is positive for a clockwise contour**, because
  it is computed in Skia's y-down space. That is the opposite sign from the
  usual y-up convention, so a winding test copied from elsewhere will be
  inverted. Open polylines are treated as if closed.
- **`space::Camera` is right-handed and y-up, and `fovYDeg` is the
  *vertical* field of view.** `viewProjection()` carries normalized device
  coordinates through to viewport pixels and flips y back to Skia's y-down
  at that last step, so screen-space results are already in canvas
  coordinates.
- **glm and Skia matrices are both column-major**, which is why
  `space::toSkM44()` is a raw pour. Remember that glm indexes
  column-then-row: `m[0][1]` is column 0, row 1 — not the transpose you may
  expect from a row-major API.
- **`glm::vec3::length()` returns 3.** It is the component count, a static
  member of the vector type, not the magnitude. Always write
  `glm::length(v)`. This compiles cleanly and is one of the easiest ways to
  produce nonsense here.
- **`space::place()` composes as translate × rotate × scale, applied right
  to left** — scale first, then rotate, then translate. Yaw is about +Y,
  pitch about +X, roll about +Z.
- **`MeshStyle::Mode::Normals` writes device-space normals with +y down**,
  encoded as `rgb = n * 0.5 + 0.5` with the y component negated before
  encoding. This is deliberate: it matches the normal maps
  `materials::bevelNormals()` produces, so a G-buffer surface can be fed
  straight into a material shader.
- **`SkColor4f` values here are display-encoded sRGB, not linear.** Colour
  interpolation runs through OKLab, with an explicit sRGB decode on the way
  in and encode on the way out (`blend::detail::lerpOklab`). Interpolating
  the components directly is a different — and visibly worse — result.
- **`Mesh::append` pad rules are load-bearing, not cosmetic.** Consumers
  read "this lane is sized to `positions`" as the mesh's presence bit for
  that lane — `space::drawMesh` literally decides `hasNormals` that way —
  so a merge that left a lane undersized would turn lighting, texturing or
  tinting off for *both* halves, not just the half that lacked it. Every
  optional lane therefore comes out sized to the merge whenever either side
  authors it, and a lane neither side authors stays empty (append pads an
  existing lane, it never conjures one). The pads: colors white, uvs
  `(0, 0)`, and **normals `{0, 0, 1}` rather than zero** — a zero normal
  survives `Mesh::transform`'s normalization as zero, collapses every
  lighting term so the padded half renders black, and is undefined input to
  a shader's `normalize()`. Primitive lanes pad by name: `"Color"` white,
  everything else zeros. `Cloud::append` takes the same posture for the
  point class: scalar `"size"` pads 1 and other scalars 0, colour `"Tex"`
  pads the identity window `{0, 0, 1, 1}` and `"uv"` pads `{0, 0, 0, 0}`
  while other colours pad white, and vectors pad `{0, 0, 1}`. Call
  `computeNormals()` on the merge when you want the geometric truth instead
  of the pad.
- **The hash helpers in `detail/Hash.h` are ABI.** `pcgAdvance`, `pcgMix`
  and `pcgHash` are bit-matched to the GPU compute kernels that execute the
  same operator chains. The constants and the shift schedule are not tuning
  knobs — changing either desynchronizes the CPU reference from the GPU
  executor, and the failure appears as two renderers scattering points
  differently rather than as a build error.
- **The declaration order of `pop::Op`'s variant alternatives is ABI.** A
  GPU consumer maps each operator's variant *index* to a compute pipeline.
  New operators are appended; inserting one in the middle silently
  reassigns every operator after it to the wrong kernel.
- **`Deform` bends positions only.** `Dir` is left where it was, so a bent
  column's stamps still point the way the loop's tangent did; re-derive a
  direction afterwards (`LookAt`, `Transform` on `Dir`) when the stamps
  should follow the bend. Points outside the band `[low, high]` ride the
  arc's end tangents rigidly, so the geometry past the band keeps its
  shape rather than being stretched.
- **A `PointSet` lays its cloud out by name, and the layout is shared.**
  `popops::seedAttrs()` is the one function that maps a cloud onto the
  attribute store — positions to `P`, `"t"`/`"size"`/`"tint"` to
  `T`/`Scale`/`Color`, `"dir"` (or, failing that, `"normal"`) to `Dir`,
  `"Tex"` to `Tex`, everything else under its own name — and the GPU
  executor uploads exactly what it produces. `count()`, `window()`,
  `spread()` and `seed()` are inert on a point-set-led chain: the cloud
  is the count.
- **`Group` sizes are radii per axis in both shapes.** A box of `size`
  `{100, 20, 100}` spans 200 by 40 by 200; a sphere with unequal `size` is
  an ellipsoid. `feather` is a fraction of that extent, not a distance.
- **Mesh indices are 32-bit.** Skia's `SkVertices` 16-bit index limit is
  handled by chunking inside `space::drawMesh()`, not by the data — you do
  not need to split meshes yourself.
- **`drawPanel()` runs your callback in panel-local coordinates**: origin at
  the panel's centre, x right, **y down** like any Skia canvas, and one
  unit equals one world unit.
- **`mesh::quad()` and `mesh::cylinderPanel()` face +z**, and
  `space::faceCamera()` orients that +z face at the eye. `points::instance`
  orients a stamp's +z along the orient lane using the same basis
  construction, so a face-camera'd quad and an instanced facing lane agree.
- **Imported textures are not decoded.** `import::Part` carries the encoded
  bytes (or the unresolved URI); turning them into pixels is a separate
  concern. glTF's whole metallic-roughness material rides along the same
  way: `Part::textures` keys the normal, packed metallicRoughness
  (`"orm"`), occlusion and emissive images by usage word, beside the
  `metallic`/`roughness`/`emissive` factors — words SigilWorld's
  texture-set door reads directly. Likewise, `import::model()` never touches the filesystem for
  external references unless you gave it a `Resolver` or used the path
  overload.
- **Alembic support is Ogawa-only and nearest-sample.**
  `AlembicOptions::time` picks the closest stored sample; nothing is
  interpolated, and HDF5-cored archives return `nullopt`.
- **A `.geo` import is unwelded, and its groups are lanes.** Every polygon
  vertex becomes its own mesh vertex (so a vertex-class `uv` or `N`
  survives seams and hard edges; the vertex class outranks the point
  class for the conventional names), the `uv` v axis is flipped to the
  top-left convention, primitive `Cd` becomes the `"Color"` prim lane and
  every other primitive attribute a prim lane under its own name. Point
  and primitive *groups* arrive as 0/1 lanes named after the group — the
  shape a `pop` mask expects — so a Houdini group named `top` is
  `.masked("top")` downstream. Detail (global) attributes and string
  attributes have no lane to land in and are dropped; only the JSON
  `.geo` spelling is read, not `.bgeo` or the blosc-compressed `.sc`
  variants.

## Boundaries

Publicly the library links Skia and glm and nothing else. Privately it uses
tinyobjloader for OBJ, Alembic for `.abc`, and the header-only earcut (cap
triangulation) and cgltf (glTF); STL and PLY are parsed by hand.

It deliberately does not own a GPU device, a window, a Qt dependency, a
component or scene kernel, an animation timeline, an image decoder, a
resource-access layer, or text layout. Where one of those is needed —
decoding a texture an importer handed you, or fetching an asset over the
network — that is the caller's job, and the library is designed so the
caller can supply it (`import::Resolver` is the hook).

The relationship with **SigilWorld**, the GPU renderer that sits beside it,
is one-directional: SigilWorld links SigilShape and consumes its `Mesh`,
`Cloud`, `pop::Chain`, `Spline3` and `space::Camera` types. SigilShape does
not link SigilWorld, does not include its headers, and does not know it
exists. The consequence worth internalizing is that **the CPU
implementations here are the reference**: `space::drawMesh()` is the twin
of the GPU uploader, and `popops::cook()` is the definition a GPU chain
executor must reproduce. When the two disagree, this side is right.

## Build and test

Configure and build from `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
```

Targets: `SigilShape` (static library), `shape_test` (registered with
ctest), `shape_demo`, and `shape_bench` (Google Benchmark: the pop cook by
count and operator mix, the stamping sink, and the `.geo` reader — run
it from a Release build).

```sh
ctest --test-dir build -C Debug -R shape_test --output-on-failure
./build/bin/Debug/shape_demo [outdir] [assetdir]
```

Everything is CPU and raster Skia, so the tests need no GPU and run
anywhere.

`shape_demo` writes PNG panels into `outdir` (default `shape_demo_out`):
`blend_morph`, `blend_color`, `blend_spine`, `materials`,
`mesh_perspective`, `mesh_chrome`, `panels_space`, `pathfinder`,
`splines_particles`, `pop_models`, `pop_prims` and `yarn_marquee`. Two more
appear when `assetdir` (default `assets`) has been populated by the
optional `fetch_assets` build target: `materials_hdri`, which lights the
material swatches with a loaded HDRI panorama, and `imported_models`, which
renders whatever model files sit in `<assetdir>/models` through the import
path.
