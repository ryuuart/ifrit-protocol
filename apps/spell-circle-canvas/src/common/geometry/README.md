# SigilGeometry

A C++ library for 2D and 3D drawing on top of [Skia](https://skia.org).
It gives you path resampling, boolean and distortion operators over
`SkPath`, shape interpolation, a renderer-neutral triangle mesh with
procedural generators plus model import and export, splines with swept
geometry, point clouds carrying named attribute lanes and a point-operator
chain language, and a runtime that draws meshes and perspective panels
onto an ordinary `SkCanvas`.

It links only Skia and [glm](https://github.com/g-truc/glm) publicly. There
is no windowing, no GPU device, no UI framework and no scene graph — you
hand it values, it hands you paths, meshes, clouds and pixels.

It is **two tiers**, one per currency, and eight feature libraries under
them. The `path` tier is 2D: an outline resampled, addressed by
distance, operated on, and interpolated. The `mesh` tier is 3D: the
triangle mesh, the camera that looks at it, the runtime that draws it,
splines, point clouds and model interchange. Each feature is a static
archive that links only what sits above it in the tree — so a text
engine or a drawable component library walks an outline through
**`SigilGeometryPath`** without linking meshes or importers, and a
renderer takes **`SigilGeometryMeshPop`** without the codec.
**`SigilGeometry`** is the umbrella, an interface over all eight, so a
consumer of the whole library names only that.

**Directories, targets, headers and namespaces are the same outline.**
A feature at `mesh/curve/` is target `SigilGeometryMeshCurve`, headers
under `include/sigilgeometry/mesh/curve/`, namespace
`sigil::geometry::mesh::curve` — so a name tells you where its code is
and what to link for it.

```
path/          SigilGeometryPath          sigil::geometry::path
  blend/       SigilGeometryPathBlend     sigil::geometry::path::blend
mesh/          SigilGeometryMesh          sigil::geometry::mesh
  camera/      SigilGeometryMeshCamera    sigil::geometry::mesh::camera
  render/      SigilGeometryMeshRender    sigil::geometry::mesh::render
  curve/       SigilGeometryMeshCurve     sigil::geometry::mesh::curve
  pop/         SigilGeometryMeshPop       sigil::geometry::mesh::pop
  codec/       SigilGeometryMeshCodec     sigil::geometry::mesh::codec
```

Every signature in the library speaks glm — `glm::vec2` for a point on a
path as much as `glm::vec3` for a vertex — and Skia types appear only
where the object *is* a Skia path, image, canvas or paint. `path/Skia.h`
holds the two conversions, `toSk()` and `fromSk()`, so a caller drawing
a result never spells the swizzle itself.

## Using it

```cpp
#include <sigilgeometry/path/Ops.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/render/Painter.h>

using namespace sigil::geometry::path;
using namespace sigil::geometry::mesh;

void paint(SkCanvas &canvas, SkSize viewport, const SkPath &star) {
  // 2D: an outline bloated, roughened and offset. A recipe is a chain of
  // operators, a value: hold it, apply it to any path, apply it again.
  const ops::PathOp recipe = ops::chain({
      ops::PuckerBloat{0.3f},
      ops::Roughen{3},
      ops::offsetBy(4),
  });
  SkPaint fill;
  fill.setAntiAlias(true);
  fill.setColor4f({1.0f, 0.6f, 0.2f, 1.0f});
  canvas.drawPath(recipe(star), fill);

  // 3D: scatter points along a window of a closed loop, drift them with
  // noise, smooth the kinks out, colour them along the loop, then sweep
  // a round profile through the result.
  const Mesh comet =
      pop::on(std::vector<glm::vec3>{{-300, 0, -100},
                                     {0, 140, 120},
                                     {300, 0, -100}})
          .count(4000)
          .window(0.9f, 0.3f)
          .noise(18)
          .smooth()
          .fade({1.0f, 0.3f, 0.6f, 1.0f}, {0.2f, 0.9f, 1.0f, 1.0f})
          .sweep(curve::profile::circle(), false,
                 {.segments = 160, .scale = 9});

  camera::Camera cam;
  cam.eye = {0, 180, 640};

  render::MeshStyle style;
  style.backfaceCull = true;

  render::drawMesh(canvas, comet, camera::place({0, 0, 0}), cam, viewport,
                   style);
}
```

Nothing above holds a device, a context or a frame. `Mesh` is a plain
struct of vectors; `pop::Chain` is a `std::vector` of variants; a
`PathOp` is a callable you can copy, compose and re-apply.

## The mental model

**One numeric currency, one drawing currency.** Every vector, point and
matrix — mesh vertices, spline knots, camera vectors, cloud positions,
flattened path points, transforms — is glm (`vec2`, `vec3`, `vec4`,
`mat4`). What is drawn or drawn from speaks Skia: `SkPath` outlines,
`SkColor4f` paint, `SkImage` textures, `SkCanvas`. `path/Skia.h` converts a
point (`toSk()`, `fromSk()`); `mesh/camera/Camera.h` is the declared
bridge for matrices, and `camera::toSkM44()` is the seam. Because glm's `mat4` and
Skia's `SkM44` are both column-major, that conversion is a straight memory
pour with no transpose.

**Resampling is the substrate.** `path/Polyline.h` reduces any path to one of
two forms: a `Polyline` (adaptive curve flattening that keeps corner
anchors exact) or a `Sampled` (exactly N points spaced uniformly by arc
length). Everything above stands on those two. Blending interpolates
`Sampled` pairs. Distortions displace resampled points and rebuild.
Extrusion walls sweep flattened contours. Swept geometry rides arc-length
samples of a spline.

**A contour is addressed by distance.** Where a `Polyline` is the outline
as vertices, a `Contour` is the outline as a length: position and unit
tangent at a distance (`at()` clamps, `around()` wraps a closed contour
past its seam), the piece between two distances as its own path, and the
corners along the way. Anything placed *along* an outline — text on a
path, a marching dash, a stroke's ornaments — reads it this way, so there
is one definition of "distance along" and one of "closed wraps around".

**Noise is seeded and bit-exact.** Everything random in the library draws
from `noise::` — a per-index hash, a PCG stream, and the trilinear value
noise built on them — so a scattered stamp, a roughened outline or a
drifted cloud re-rolls identically on every platform and every run. The
mixers under it live one library down, in SigilCoreCompute, so a shader's
CPU twin and a cache key fold with the same bodies rather than with
copies of them.

**Values, not baked results.** Options structs, distortion structs,
operator values, splines, clouds and chains are all plain data you edit and
re-cook. `ops::PathOp` plus `ops::chain()` compose a non-destructive
recipe; `blend::Options`, `curve::SweepOptions` and `pop::Chain` behave the
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
`colors`/`indices` buffers feed the draw in `mesh/render/Painter.h` and
upload directly to a GPU renderer downstream. Nothing renderer-shaped
lives in the struct.

**A draw runs on a `Runtime`, and the runtime is a value.**
`render::MeshStyle` carries one, defaulting to `render::Runtime::cpu()`
— the built-in executor that transforms, shades, sorts and emits on the
CPU. A feature that owns a GPU device supplies its own executor as a
value and assigns it to the style; the call, the geometry and the
vocabulary do not change, and nothing here learns what a device is. Two
runtimes compare equal when they hold the same model with the same
value, so a reconciler can ask whether a description's runtime changed.

**`pop::Chain` is a backend-neutral description.** It is a vector of
operator variants, not a program — and a value: every operator, `Mesh`
and `Cloud` compares by content with `==`, so a reconciler can ask
whether a chain changed. A device consumer executes the identical chain
as compute dispatches, and the two are required to agree bit for bit —
which is what makes the hash helpers and the variant order load-bearing
(see below).

**The arithmetic two tiers must agree about is written once, in Slang.**
Two kernels are: `mesh/pop/kernels/Pop.slang`, the operators that are
per-point arithmetic, and `mesh/curve/kernels/Sweep.slang`, the swept
ring vertex. The build compiles each twice — to C++, which the executor
behind the built-in runtime calls, and to SPIR-V, which a runtime that
owns a device dispatches. Neither side re-derives a formula, which is
what lets two tiers be held to bit identity rather than to a tolerance.
`mesh/Spirv.h` is the one thing both kernels' words go through:
`noContraction()` adds the decoration the emitter leaves out, in one
place rather than once per kernel. `kernel::has(op)` is the one
answer to whether an operator has a kernel, `kernel::describe()` packs
one into the argument block both ends read, `kernel::run()` is the host
call, and `kernel::spirv()` is the module a device runs.

Not every operator has one, and each absence is a boundary rather than a
gap: a generator makes the points rather than mapping over them (every
executor seeds through `pop::seedLanes()` and exports through
`pop::exportLanes()`, so the two ends of a cook are one definition);
`Relax` reads points it does not own; `Sort` is a permutation; `Promote`
addresses primitives no sink has formed yet; and `Noise` and `Deform` are
defined in terms of a library sine, which is a different function from
the polynomial a portable kernel would have to use — a kernel for either
would change what the operator MEANS rather than where it runs.

(The paragraph below and the two after it are about the pop kernel in
particular; the sweep kernel's subset is narrower still — plain
arithmetic and one integer-to-float conversion, with no intrinsic at
all.)

**The portable subset is what one source can be compiled twice from and
still answer once**: arithmetic plus the operations IEEE 754 pins
exactly, with a lerp, a dot, a length and a smoothstep written out
because a library intrinsic is two different pieces of code on two
targets. Two things outside the source decide the rest. The generated C++
is compiled with `-ffp-contract=off`, which is also what makes a Debug
build and a Release one produce the same bits; and the SPIR-V carries one
`NoContraction` decoration per arithmetic result, added by
`kernel::spirv()` because the emitter puts none there — without it a
driver fuses a multiply and the add after it and rounds once where the
source rounds twice.

**A SWEEP runs on one as well, and its executor's whole contract is the
RING VERTICES.** `SweepOptions::runtime` carries a `curve::SweepRuntime`,
defaulting to `SweepRuntime::cpu()`, and a sweep is two things of which
only one is arithmetic: the ring vertices are a pure function of one
frame, one profile point and the size the profile scales to there, and
the topology around them — which vertices a quad joins, the fan that
closes an end, the averaging that forms a geometric normal — is integer
or a reduction over triangles that do not exist until the vertices do.
The topology is the same wherever the vertices were formed, so it is
written once and the seam is narrow: an executor is handed a
`curve::kernel::Dispatch` and fills two lanes — a position carrying u
in its fourth float and a normal carrying v in its, because both of
those floats were spare and a lane of its own for two numbers is a
third of everything this seam moves. The TAPER never crosses
it — an arbitrary function of t is evaluated once per ring on the host
and arrives as the number that ring scales by — which is why a runtime
that owns a device can hold a sweep to bit identity rather than to a
tolerance.

**A cook runs on a `Runtime` too, and it is the same kind of value.**
`pop::cook()`, `cookMesh()` and `cookSweep()` take one, defaulting to
`pop::Runtime::cpu()`. The executor's whole contract is cooking a chain
into a `Cloud`: the two mesh-forming sinks stand on the cooked cloud and
hand back a `Mesh`, so a device-side former would have to read its own
result back to answer them — the place a device replaces ring forming is
`curve::sweep()` over a rail, not the sink. An executor also declares,
per operator, whether it runs it, and `cook()` asks before it dispatches:
an operator a runtime lacks stops the cook with a message naming the
operator and the runtime, because a chain quietly missing an operator
cooks a plausible cloud that is not the described one.

**A pose is how anything rides a curve.** `path::poseAlong()` answers
where a 2D contour is at a distance, which way it heads and which way is
sideways; `curve::poseAlong()` answers the `Frame3` at a distance along a
spline. One `path::Wrap` policy governs both — a closed curve comes
round, an open one parks — so a mark travelling a 2D outline and a camera
flying a 3D spline agree about what "past the end" means without either
of them spelling it.

**Operator dials are addressable by name.** `pop::setField(op,
"amount", v)` and `getField` reach every numeric field of every operator
— vector components dotted (`"center.x"`, `"add.w"`, `"to.g"`), enums and
bools as numbers, ints truncated — so a control surface, a preset file
or an animation lane can drive a chain without knowing the operator's
type. Strings, lane names, meshes, clouds and matrices are descriptions,
not dials, and stay out of it.

## The features and their headers

Each feature is a directory holding its sources, its `CMakeLists.txt`,
its `test/` and its `bench/`; its public headers sit under the matching
directory of `include/sigilgeometry/`. Internal headers never leave the
feature directory. Features nest by dependency — a feature links only
what sits above it in the tree — and each header includes what it needs,
so including a deeper one pulls the shallower ones in.

**`path`** — `SigilGeometryPath`, the leaf. Seven headers that depend on
nothing else in the library: Skia, glm, and SigilCoreCompute for the
seeded mixers `noise::` names.

- **`path/Polyline.h`** — the resampling core. `Polyline` and `flatten()`,
  `sample()` to walk a parametric curve evenly by arc length, `Sampled`
  and `resample()`, `bestAlignment()`/`applyAlignment()` for matching two
  closed contours, `toPath()` to rebuild (optionally through Catmull-Rom
  cubics), and `lerp()`.
- **`path/Contour.h`** — a path's sub-paths by arc length. `Contour::of()`
  splits a path (skipping zero-length contours); `length()`, `closed()`,
  `at()`, `around()`, `segment()`/`appendSegment()`, and `corners()`, which
  walks the contour in strides and bisects to each turn sharper than a
  threshold. Three constructions walk every contour of a path:
  `parallel()` (a curve a constant distance to the side, round outer
  joins and mitred inner ones), `displace()` (a sinusoidal or zigzag
  sideways wave, fitted to a whole number of cycles so both ends stay on
  the source curve) and `cornerWindows()` (the pieces within a radius of
  each corner, or everything but them).
- **`path/Pose.h`** — a contour addressed by distance, with the sideways
  direction and the end policy written down. `Pose` (position, unit
  tangent, the normal that tangent turned toward +y, and the distance the
  policy resolved to), `Wrap` (`Clamp` parks at the nearer end, `Around`
  comes round closed geometry), `poseAlong()` over one contour or over a
  list of them walked as ONE arc-length coordinate, and `totalLength()`
  and `closedThroughout()` over such a list.
- **`path/Noise.h`** — namespace `noise`. `hash(seed, i)` to [-1, 1] for
  per-index jitter; the PCG family `pcgAdvance`, `pcgMix`, `pcgHash`,
  `pcgNext` (a stream over a carried state) and `pcgUnit` (either squeezed
  to [0, 1)); and `value3()`, trilinear value noise over the integer
  lattice. The mixers are SigilCoreCompute's bodies, named here because a
  resource key and a text cache fold with the same arithmetic; `value3()`
  is this library's, because it is read at a POSITION rather than at an
  index.
- **`path/Numeric.h`** — `kPi`, `kTau`, the degree/radian factors,
  `bisect()` over a predicate and `wrap()` into a period.
- **`path/Skia.h`** — `toSk()` and `fromSk()` between `glm::vec2` and
  `SkPoint`, and `centre()` of an `SkRect`.
- **`path/Ops.h`** — path operators. Booleans over Skia's pathops (`unite`,
  `subtract`, `intersect`, `exclude`, `simplify`, and a stroke-expansion
  `offset`), and four distortions as parameter structs you apply on demand:
  `Roughen`, `Zigzag`, `PuckerBloat`, `Twirl`. `PathOp` and `chain()`
  compose them, `offsetBy()` adapts `offset` into a step.

**`path/blend`** — `SigilGeometryPathBlend`, needs `path`.

- **`path/blend/Blend.h`** — shape interpolation modelled on Illustrator's
  blend tool: `Key`s expand into drawable `Step`s under `Options`
  controlling spacing (`Steps`, `Distance`, `SmoothColor`), an optional
  spine path, orientation, sample density and outline smoothing.

**`mesh`** — `SigilGeometryMesh`, needs `path`. The 3D tier's root, and
the currency every feature under it speaks.

- **`mesh/Mesh.h`** — the mesh currency and its generators. The `Mesh`
  struct (positions, normals, uvs, colors, indices, and the `prims` lane
  map), `append()`/`transform()`/`computeNormals()`/`bounds()`, and the
  generators `extrude()`, `revolve()`, `grid()`, `torus()`,
  `superellipsoid()`, `cylinderPanel()`, `quad()`, plus
  `mesh::bakePrimColor()`. The struct's own methods are `Mesh.cpp`; the
  generators are `Generators.cpp`.
- **`mesh/Vec.h`** — the two glm policies the library and its GPU twin
  share: `normalized()` with a fallback for a degenerate vector, and
  `basisFor()`, the orientation basis every stamp is placed with, so a
  cloud renders identically merged, instanced or GPU-drawn.

**`mesh/camera`** — `SigilGeometryMeshCamera`, needs `mesh`. Where a
viewpoint and the transforms that answer to it live; a camera grows its
own repertoire here rather than inside whatever draws through it.

- **`mesh/camera/Camera.h`** — a right-handed, y-up `Camera` with a
  vertical field of view, `view()`/`projection()`/`viewProjection()`,
  and the transform helpers `place()` and `faceCamera()`. `toSkM44()` is
  the glm-to-Skia seam. The view and projection are built with Skia's own
  matrix factories, so a point projected here lands where a canvas concat
  would put it.

**`mesh/render`** — `SigilGeometryMeshRender`, needs `mesh` and
`mesh/camera`.

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

**`mesh/curve`** — `SigilGeometryMeshCurve`, needs `mesh` and
`mesh/camera`.

- **`mesh/curve/Frame.h`** — `Frame3`, the moving frame every rail is a
  sequence of. Its own header because both the sweep and the pose stand
  on it and neither stands on the other.
- **`mesh/curve/Curve.h`** — `Spline3` (linear, Catmull-Rom or Bezier, open
  or closed) with `position()`, `tangent()`, `length()`, `sample()` and
  `sampleArcLength()`; the two rails — `curve::frames()`, parallel-transport
  `Frame3`s that do not flip at inflections, and `curve::hangFrames()`, a
  window of a closed loop whose across-vector is held world-vertical; the
  `curve::sweep()` overload over a spline; and `project()` to draw the
  curve as a 2D path under a camera. It includes `Sweep.h`, so a
  consumer that spells only this one reaches everything a sweep needs.
- **`mesh/curve/Sweep.h`** — the swept primitive as a subject: the
  cross-sections `curve::profile::circle()`, `curve::profile::line()` and
  `curve::profile::fromPath()`; `SweepOptions` with `SweepNormals`; the
  one swept former `curve::sweep()` over a rail; and the seam a device
  replaces — `curve::SweepExecutor` (one call, `rings()`),
  `curve::SweepRuntime` holding one, `curve::describe()` turning a rail
  and a profile into a `curve::kernel::Dispatch`, and `kernel::run()` and
  `kernel::spirv()` as the two ends of the one arithmetic.

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
  `profile::circle()` duplicates its seam point and comes back open.
  There are two overloads: one over a rail you built, which is where a
  GPU executor forms the same rings, and one over a `Spline3`, which
  builds a transported rail of `segments` frames first. Both run on
  `SweepOptions::runtime`.

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
language.

- **`mesh/pop/Points.h`** — `Cloud` and its lane accessors (`Cloud.cpp`);
  the generators `onSpline()`, `grid()`, `ring()`, `scatterBox()` and
  `onMesh()` (`Generators.cpp`); the modifiers `jitter()` and
  `displaceNoise()`, the consumers `instance()` and `quads()` (stamp a
  mesh at every point into one merged mesh) and `promoteToPrims()`
  (`Modifiers.cpp`); and `drawBillboards()`, camera-facing sprites
  (`Billboards.cpp`).
- **`mesh/pop/Pop.h`** — the operator chain language and the runtime seam
  it executes through, both in the `pop` scope: `pop::on()` opens a chain,
  `pop::cook()` evaluates one on the `pop::Runtime` it is given,
  `pop::Executor` is what a runtime supplies (`pop::Runtime` is an erased
  value of SigilCoreComparable's shape) and `pop::opName()` names an
  operator. The field table behind `pop::setField()`/`getField()` is
  `Fields.cpp`; the built-in executor, the `Runtime::cpu()` value and the
  `cook()` door that checks an executor's capability before dispatching
  are `Cook.cpp`; the mesh-forming sinks `pop::cookMesh()` and
  `cookSweep()` are `Sinks.cpp`.
- **`mesh/pop/Kernel.h`** — the seam between the two ends of one piece of
  arithmetic: `kernel::Args` (the argument block, every member a
  four-component vector so its bytes stand at the same offsets in a
  uniform buffer), `kernel::Dispatch` (which lane fills each binding
  role), `has()`, `describe()`, `run()` and `spirv()`. `Kernel.cpp` packs
  and calls; `Spirv.cpp` decorates the module.
- **`mesh/pop/kernels/Pop.slang`** — the operators themselves, one entry
  point with the operator chosen by a uniform: one dispatch runs one
  operator over every point, so the branch is uniform across it, and one
  entry point is one pipeline and one generated function. `cmake/Slang.cmake`
  compiles it (`sigil_slang_module` with `CPP_VAR` and `SPIRV_VAR`, and
  `sigil_slang_kernel_flags` to pin the float model).

**`mesh/codec`** — `SigilGeometryMeshCodec`, needs `mesh` and
`mesh/pop`. Its parsers
are private to the feature: tinyobjloader, cgltf and Alembic, with STL,
PLY and `.geo` parsed by hand. One reader per translation unit —
`Obj.cpp`, `Gltf.cpp`, `Stl.cpp`, `PlyDecode.cpp`, `Geo.cpp`, `Alembic.cpp` —
behind the dispatcher in `Model.cpp`, sharing only what `Internal.h`
declares; `PlyEncode.cpp` is the writer.

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
  binary-little-endian PLY, Ogawa Alembic, and Houdini's JSON `.geo`.
- **`mesh/codec/Encode.h`** — the door out: `encode::ply()` over a `Cloud` or a
  `Mesh`, ascii by default or binary via `PlyOptions`.

**`Geometry.h`** at the root of the include tree includes every public
header, for a consumer that takes the whole library rather than a tier
of it.

### The operators

`pop::Op` is a variant over twenty operator values, and `pop::Chain` is
a vector of them. Generators seed a chain: `SplineScatter` (points along a
window of a closed loop), `MeshScatter` (points on a formed model's
faces) and `PointSet` (an existing `Cloud` — an import's `asCloud()`, a
previous cook — every lane riding in as an attribute, so a Houdini group
arrives as a mask under its own name). Filters rewrite attributes in place: `Jitter`, `Noise`, `Ramp`,
`Vary`, `LookAt`, `Math`, `Relax`, `Fill`, `Atlas`, `Lookup`, `Affine`
(any `mat4` on a position or a direction lane), `Peak` (push along a
direction lane), `Deform` (twist, taper or bend about an axis) and `Mix`
(blend two lanes into a third by a constant or a lane). `Select` is the
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
written selects nobody, the way an empty group is empty. `Select` is one
way to write such a lane; a `Lookup`, a `Math` on a custom lane, or an
importer's attribute serve just as well. Both executors apply the mask
with the same expression.

`pop::on()` — over a loop, a `Mesh`, a `Chain` or a `Cloud` — returns a
`Builder` whose chained verbs (`count`, `window`,
`spread`, `seed`, `jitter`, `noise`, `vary`, `fade`, `tint`, `lookAt`,
`move`, `fill`, `atlas`, `rampBy`, `order`, `orderBy`, `promote`, `smooth`,
`select`, `masked`, `affine`, `orient`, `peak`, `twist`, `taper`,
`bend`, `mix`, `mixBy`, `copy`, `op`) append operators — `masked()` sets
the mask on the filter just added — and the builder converts to a
`Chain`, so you can reach into any operator afterwards and re-cook. Sinks
turn a chain into geometry: `cook()` to a `Cloud`, `cookMesh()` to one
mesh of stamps, and `cookSweep()` reading the cooked points as the path
`curve::sweep()` carries a profile along.

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
- **`camera::Camera` is right-handed and y-up, and `fovYDeg` is the
  *vertical* field of view.** `viewProjection()` carries normalized device
  coordinates through to viewport pixels and flips y back to Skia's y-down
  at that last step, so screen-space results are already in canvas
  coordinates.
- **glm and Skia matrices are both column-major**, which is why
  `camera::toSkM44()` is a raw pour. Remember that glm indexes
  column-then-row: `m[0][1]` is column 0, row 1 — not the transpose you may
  expect from a row-major API.
- **`glm::vec3::length()` returns 3.** It is the component count, a static
  member of the vector type, not the magnitude. Always write
  `glm::length(v)`. This compiles cleanly and is one of the easiest ways to
  produce nonsense here.
- **`camera::place()` composes as translate × rotate × scale, applied right
  to left** — scale first, then rotate, then translate. Yaw is about +Y,
  pitch about +X, roll about +Z.
- **`MeshStyle::Mode::Normals` writes device-space normals with +y down**,
  encoded as `rgb = n * 0.5 + 0.5` with the y component negated before
  encoding. This is deliberate: it is the encoding SigilMaterial's bevel
  normal maps use, so a normals pass can be fed straight into one of its
  surface recipes as the normal map.
- **`SkColor4f` values here are display-encoded sRGB, not linear.** Colour
  interpolation runs through OKLab, with an explicit sRGB decode on the way
  in and encode on the way out (`blend::detail::lerpOklab`). Interpolating
  the components directly is a different — and visibly worse — result.
- **`Mesh::append` pad rules are load-bearing, not cosmetic.** Consumers
  read "this lane is sized to `positions`" as the mesh's presence bit for
  that lane — `render::drawMesh` literally decides `hasNormals` that way —
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
- **The PCG helpers `path/Noise.h` names are ABI.** `noise::pcgAdvance`,
  `noise::pcgMix` and `noise::pcgHash` are bit-matched to the GPU compute
  kernels that execute the same operator chains. Their bodies are
  SigilCoreCompute's, and its tests pin the exact words they answer. The
  constants and the shift schedule are not tuning knobs — changing either
  desynchronizes the CPU reference from the GPU executor, and the failure
  appears as two renderers scattering points differently rather than as a
  build error.
- **The declaration order of `pop::Op`'s variant alternatives is ABI.**
  The variant *index* IS the operator number the kernel switches on, so
  one numbering serves the host and the device. New operators are
  appended; inserting one in the middle silently sends every operator
  after it to the wrong kernel branch.
- **`Deform` bends positions only.** `Dir` is left where it was, so a bent
  column's stamps still point the way the loop's tangent did; re-derive a
  direction afterwards (`LookAt`, `Affine` on `Dir`) when the stamps
  should follow the bend. Points outside the band `[low, high]` ride the
  arc's end tangents rigidly, so the geometry past the band keeps its
  shape rather than being stretched.
- **A `PointSet` lays its cloud out by name, and the layout is shared.**
  `pop::seedAttrs()` is the one function that maps a cloud onto the
  attribute store — positions to `P`, `"t"`/`"size"`/`"tint"` to
  `T`/`Scale`/`Color`, `"dir"` (or, failing that, `"normal"`) to `Dir`,
  `"Tex"` to `Tex`, everything else under its own name — and the GPU
  executor uploads exactly what it produces. `count()`, `window()`,
  `spread()` and `seed()` are inert on a point-set-led chain: the cloud
  is the count.
- **`Select` sizes are radii per axis in both shapes.** A box of `size`
  `{100, 20, 100}` spans 200 by 40 by 200; a sphere with unequal `size` is
  an ellipsoid. `feather` is a fraction of that extent, not a distance.
- **Mesh indices are 32-bit.** Skia's `SkVertices` 16-bit index limit is
  handled by chunking inside `render::drawMesh()`, not by the data — you do
  not need to split meshes yourself.
- **`drawPanel()` runs your callback in panel-local coordinates**: origin at
  the panel's centre, x right, **y down** like any Skia canvas, and one
  unit equals one world unit.
- **`mesh::quad()` and `mesh::cylinderPanel()` face +z**, and
  `camera::faceCamera()` orients that +z face at the eye. `points::instance`
  orients a stamp's +z along the orient lane using the same basis
  construction, so a face-camera'd quad and an instanced facing lane agree.
- **Imported textures are not decoded.** `decode::Part` carries the encoded
  bytes (or the unresolved URI); turning them into pixels is a separate
  concern. glTF's whole metallic-roughness material rides along the same
  way: `Part::textures` keys the normal, packed metallicRoughness
  (`"orm"`), occlusion and emissive images by usage word, beside the
  `metallic`/`roughness`/`emissive` factors, the transmission and ior
  extensions and the alpha mode — words SigilWorld's texture-set door
  reads directly. A part's material SLOT (`materialIndex`, glTF's material
  index; a `.geo`'s `shop_materialpath` string index) is also written
  across its `mesh.prims["Material"]` lane, so `Model::merged()` keeps
  per-triangle materials and `materialSlotCount()` says how many.
  Likewise, `decode::model()` never touches the filesystem for
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

Publicly the library links Skia, glm and the two SigilCore leaves —
SigilCoreCompute for the seeded mixers, SigilCoreComparable for the
erased value the mesh and point-operator runtimes are — and nothing
else; every feature links only the features above it in the tree. The
leaves are the standard library (and Boost.PFR) behind a name, so
linking them acquires no kernel, no device and nothing that draws.
Privately `mesh` uses the header-only earcut for cap triangulation, and
`mesh/codec` uses
tinyobjloader for OBJ, Alembic for `.abc` and the header-only cgltf for
glTF; STL, PLY and `.geo` are parsed by hand. None of those reaches
another feature, and none of them reaches a public header.

It deliberately does not own a GPU device, a window, a Qt dependency, a
component or scene kernel, an animation timeline, an image decoder, a
resource-access layer, or text layout. Where one of those is needed —
decoding a texture an importer handed you, or fetching an asset over the
network — that is the caller's job, and the library is designed so the
caller can supply it (`decode::Resolver` is the hook).

Surface shading is **SigilMaterial**'s: its bevel normal maps and its
gold, chrome and glass recipes take a normals pass this library draws,
and it links `SigilGeometryPath`, never the reverse.

The relationship with **SigilWorld**, the GPU renderer that sits beside it,
is one-directional: SigilWorld links SigilGeometry and consumes its `Mesh`,
`Cloud`, `pop::Chain`, `Spline3` and `camera::Camera` types. SigilGeometry does
not link SigilWorld, does not include its headers, and does not know it
exists. The consequence worth internalizing is that **the CPU
implementations here are the reference**: `render::drawMesh()` is the twin
of the GPU uploader, and `pop::cook()` is the definition a GPU chain
executor must reproduce. When the two disagree, this side is right — and
for the operators that have a kernel they cannot disagree about a
formula, because there is one formula and this side compiled it.

## Build and test

Configure and build from `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
```

Targets: one static library per feature — `SigilGeometryPath`,
`SigilGeometryPathBlend`, `SigilGeometryMesh`, `SigilGeometryMeshCamera`,
`SigilGeometryMeshRender`, `SigilGeometryMeshCurve`,
`SigilGeometryMeshPop`, `SigilGeometryMeshCodec` — the `SigilGeometry`
umbrella over all of them, the tests, `geometry_demo`, and one Google
Benchmark binary per feature, built by the `benches` target and run from
a Release build through `scripts/bench_ledger.py`:

| Binary | Measures |
| --- | --- |
| `geometry_path_bench` | flattening and resampling by point count, corner detection and the parallel and displaced constructions by contour length, the noise hashes per call, and the pose read over one contour and over many |
| `geometry_path_blend_bench` | a two-key blend by step count and by sample density, and the same blend threaded onto a spine |
| `geometry_mesh_bench` | extrude, revolve and the grid presets by vertex count |
| `geometry_mesh_camera_bench` | the per-frame transform builds: view, view-projection, the matrix seam, and the two placement helpers |
| `geometry_mesh_render_bench` | the built-in runtime by triangle count and by shading mode, the cost of the cull and the sort, and the panel concat |
| `geometry_mesh_curve_bench` | arc-length sampling and parallel-transport frames by count, the pose read over a held rail and over the spline that builds one, and the sweep by tessellation for a circle profile, a line profile and a line on a hung rail |
| `geometry_mesh_pop_bench` | the pop cook per operator over a thousand points, whole chains by count and operator mix, and the runtime seam's dispatch against the same cook reached directly |
| `geometry_mesh_codec_bench` | OBJ, GLB and `.geo` decoded from bytes in memory, per triangle or point |

The tests are one binary per feature, named for the feature's path, each
linking only that feature's library (and the features above it), so a
test cannot reach past the code it exercises and an edit to one feature
recompiles one small file. All are registered with ctest and answer to
`-R geometry`:

| Binary | Source | Covers |
| --- | --- | --- |
| `geometry_path_test` | `path/test/PathTest.cpp` | the leaf alone: polylines, contours, poses along them (held against an independent walk of the same contours), the path operators, noise, numerics |
| `geometry_path_blend_test` | `path/blend/test/BlendTest.cpp` | shape interpolation |
| `geometry_mesh_test` | `mesh/test/MeshTest.cpp` | the mesh currency and its generators |
| `geometry_mesh_camera_test` | `mesh/camera/test/CameraTest.cpp` | the view-projection carried through to viewport pixels, and the two placement transforms |
| `geometry_mesh_render_test` | `mesh/render/test/PainterTest.cpp`, `mesh/render/test/RuntimeTest.cpp` | the mesh draw's pixels, the normals G-buffer's encoding and the primitive tint; and the runtime seam — the built-in value, comparison by model, and a substituted executor receiving the draw |
| `geometry_mesh_curve_test` | `mesh/curve/test/CurveTest.cpp`, `mesh/curve/test/SweepTest.cpp` | splines, the two rails, the pose along them, and the sweep held vertex for vertex against independent reference bodies for a tube, a ribbon and a banner; and the ring seam — what a rail and a profile become as a dispatch, the taper resolved on the host, comparison by model, and a substituted executor forming the vertices |
| `geometry_mesh_pop_test` | `mesh/pop/test/PointsTest.cpp`, `mesh/pop/test/PopTest.cpp`, `mesh/pop/test/RuntimeTest.cpp` | point clouds, instancing, the agreement between an instanced facing lane and `faceCamera()`, and pop chains with their operators; and the cook's runtime seam — the built-in value, comparison by model, a substituted executor receiving the cook, and the message an unsupported operator produces. Links the codec to seed chains from an imported model |
| `geometry_mesh_codec_test` | `mesh/codec/test/DecodeTest.cpp`, `mesh/codec/test/EncodeTest.cpp` | every reader, and the PLY writer's round trips; the only one linking Alembic |

Helpers that more than one binary reads (`kCubeObj`, `splitQuad`) live in
`test/support/GeometrySupport.h` at the library root — the one shared
test location, beside `examples/geometry_demo.cpp`, which is the one
program over the whole library; a helper one binary uses stays in that
binary's file.

```sh
ctest --test-dir build -C Debug -R geometry --output-on-failure
./build/bin/Debug/geometry_demo [outdir] [assetdir]
```

Everything is CPU and raster Skia, so the tests need no GPU and run
anywhere.

`geometry_demo` writes PNG panels into `outdir` (default `geometry_demo_out`):
`blend_morph`, `blend_color`, `blend_spine`, `materials`,
`mesh_perspective`, `mesh_chrome`, `panels_space`, `pathfinder`,
`splines_particles`, `pop_models`, `pop_prims` and `yarn_marquee`. Two more
appear when `assetdir` (default `assets`) has been populated by the
optional `fetch_assets` build target: `materials_hdri`, which lights the
surface swatches with a loaded HDRI panorama, and `imported_models`, which
renders whatever model files sit in `<assetdir>/models` through the import
path.
