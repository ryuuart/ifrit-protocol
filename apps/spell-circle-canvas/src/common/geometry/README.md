# SigilGeometry

A C++ library for 2D and 3D drawing on top of [Skia](https://skia.org).
It gives you path resampling, boolean and distortion operators over
`SkPath`, shape interpolation, a renderer-neutral triangle mesh with
procedural generators plus model import and export, splines with swept
geometry, point clouds carrying named attribute lanes and a point-operator
chain language, and a runtime that draws meshes and perspective panels
onto an ordinary `SkCanvas`.

It links Skia, [glm](https://github.com/g-truc/glm) and two SigilCore
leaves publicly, and SigilMaterial's colour leaf privately in the one
feature that interpolates colour. There is no windowing, no UI framework
and no scene graph — you hand it values, it hands you paths, meshes,
clouds and pixels — and the one feature that owns a GPU device is named
`device`, linked only by what wants one.

It is **two tiers**, one per currency, and twelve feature libraries
across them. The `path` tier is 2D: an outline resampled, addressed by
distance, operated on, and interpolated. The `mesh` tier is 3D: the
triangle mesh, the camera that looks at it, the runtime that draws it,
splines, point clouds and model interchange. Each feature is a static
archive that links only what sits above it in the tree — so a text
engine or a drawable component library walks an outline through
**`SigilGeometryPath`** without linking meshes or importers, and a
renderer takes **`SigilGeometryMeshPop`** without the codec.
**`SigilGeometry`** is the umbrella, an interface over every one of
them, so a consumer of the whole library names only that.

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
    device/    SigilGeometryMeshRenderDevice
                                          (the same ::render namespace:
                                           a device executor stands in
                                           the scope of the seam it
                                           serves, as pop's do)
  curve/       SigilGeometryMeshCurve     sigil::geometry::mesh::curve
  pop/         SigilGeometryMeshPop       sigil::geometry::mesh::pop
  codec/       SigilGeometryMeshCodec     sigil::geometry::mesh::codec
device/        SigilGeometryDevice        sigil::geometry::device
  residency/   SigilGeometryDeviceResidency
                                          (the same ::device namespace:
                                           what is resident stands in
                                           the scope of the device it
                                           is resident on)
kit/           SigilGeometryKit           sigil::geometry::shapes
                                          (and ::shapers, ::sections, ::mesh)
```

`device/` is the GPU device itself, and it is here for one reason:
**Diligent creates the Vulkan device and cannot attach to one that
already exists**, so the single point where a device is made has to sit
at or below every consumer of one. A point operator's device executor, a
mesh painter's device draw and a frame runtime's passes all stand on the
device this feature created and the hardware device adopted, and none of
them can create it for the others. It is absent from a build without
Skia's Graphite on the same device, since being one device for both APIs
is the whole of what it is for.

`device/residency/` is what a mesh, a map and a compiled program BECOME
on that device, and it is a sibling of the device rather than part of it
because of what it has to link. Buffers are made out of the mesh
currency, textures out of a material's, pipelines out of a material's
Slang backend — and the feature that CREATES a device sits under every
consumer of one, the point operators' device executors among them. A
point operator dispatches a kernel and forms no picture, so its link line
has no business reaching a material; standing residency beside the device
is what keeps that true. A consumer takes the device, or the device and
what is resident on it.

`kit/` is the one directory that does not sit in that dependency tree:
it is the SHELF over the tiers, holding the stock values anybody could
have written, and nothing beneath it may reach back up into it. A
consumer that brings its own generators links a tier and not the kit.

Every signature in the library speaks glm — `glm::vec2` for a point on a
path as much as `glm::vec3` for a vertex — and Skia types appear only
where the object *is* a Skia path, image, canvas or paint. `path/Skia.h`
holds the two conversions, `toSk()` and `fromSk()`, so a caller drawing
a result never spells the swizzle itself.

## Using it

```cpp
#include <sigilgeometry/path/Operations.h>
#include <sigilgeometry/kit/Sections.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/render/Painter.h>

using namespace sigil::geometry::path;
using namespace sigil::geometry::mesh;

void paint(SkCanvas &canvas, SkSize viewport, const SkPath &star) {
  // 2D: an outline bloated, roughened and offset. A recipe is a chain of
  // operators, a value: hold it, apply it to any path, apply it again.
  const operations::PathOperation recipe = operations::chain({
      operations::PuckerBloat{0.3f},
      operations::Roughen{3},
      operations::offsetBy(4),
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
          .sweep(sections::circle(), false,
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
`PathOperation` is a callable you can copy, compose and re-apply.

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

**A resampling is keyed to a COUNT or to a SPACING, and which is held
fixed is the whole difference.** `resample` fixes the count and moves
every point; `subdivide` fixes the longest step and keeps every source
vertex; `catmullRom` does the same through a smooth curve rather than
along the chords. Anything that lays a mark every so many pixels — a
brush stamping dabs, a plotter drawing a fill, a dashed rule — asks for
the spacing, and `Stride` is that same walk for a curve that does not
exist yet: a stylus reports the next piece only when the hand moves, so
the walk carries the distance it still owes across pieces and lands where
one walk over the joined pieces would.

**A polyline may CARRY something.** `Polyline::lane` is one scalar per
vertex — pressure along a centreline, a width along a rail — and every
resampling here interpolates it with the positions, so a caller reading a
resampled curve never re-derives what the value there was. It is the same
word a point cloud's attributes use, in the one class a 2D outline
has room for.

**A contour is addressed by distance.** Where a `Polyline` is the outline
as vertices, a `Contour` is the outline as a length: position and unit
tangent at a distance (`at()` clamps, `around()` wraps a closed contour
past its seam), the piece between two distances as its own path, and the
corners along the way. Anything placed *along* an outline — text on a
path, a marching dash, a stroke's ornaments — reads it this way, so there
is one definition of "distance along" and one of "closed wraps around".

**An AREA is a set of rings under the even-odd rule.** `Polyline::contains`
is the ray test on one ring and `containsEvenOdd` the rule over a set of
them — inside an odd number is inside — which is the rule a path filled
with `SkPathFillType::kEvenOdd` is drawn by, so a point tested and a pixel
painted agree. `path::lattice` fills such an interior with parallel lines
cut to it, and what it answers with are CENTRELINES: a mark that can be
walked, drawn along with a tool, split or joined to the next, which is
what separates it from clipping a line pattern to an outline.

**Noise is seeded and bit-exact.** Everything random in the library draws
from `noise::` — a per-index hash, a PCG stream, and the trilinear value
noise built on them — so a scattered stamp, a roughened outline or a
drifted cloud re-rolls identically on every platform and every run. The
mixers under it live one library down, in SigilCoreCompute, so a shader's
CPU twin and a cache key fold with the same bodies rather than with
copies of them.

**Values, not baked results.** Options structs, distortion structs,
operator values, splines, clouds and chains are all plain data you edit and
re-cook. `operations::PathOperation` plus `operations::chain()` compose a non-destructive
recipe; `blend::Options`, `pop::SweepOptions` and `pop::Chain` behave the
same way. Nothing is committed until a draw call or an explicit cook asks
for it, so changing one dial and re-running is always available.

**Named attribute lanes, in two classes.** The *point* class lives on
`Cloud`: string-keyed lanes of scalars, vectors and colours, created on
first touch and sized to the point count. Generators write conventional
names — `"t"`, `"tangent"`, `"normal"`, `"binormal"`, `"size"`, `"tint"`,
`"uv"` — and consumers read them back by name, so your own cooked lane
slots in wherever a built-in one does. The *primitive* class lives on
`Mesh::primitives`: `vec4` lanes sized to `triangleCount()`, because a primitive
here *is* one triangle. Its conventional names are `"Color"` (a flat
per-triangle tint) and `"Id"` (`.x` carries which piece the triangle
belongs to). `points::promoteToPrimitives()` and the `pop::Promote` operator
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
([reference/TRAPS.md](reference/TRAPS.md)).

**The arithmetic two tiers must agree about is written once, in Slang.**
Two kernels are: `mesh/pop/kernels/Pop.slang`, the operators that are
per-point arithmetic, and `mesh/pop/kernels/Sweep.slang`, the swept
ring vertex. The build compiles each twice — to C++, which the executor
behind the built-in runtime calls, and to SPIR-V, which a runtime that
owns a device dispatches. Neither side re-derives a formula, which is
what lets two tiers be held to bit identity rather than to a tolerance.
`kernel::has(operation)` is the one answer to whether an operator has a kernel,
`kernel::describe()` packs one into the argument block both ends read,
`kernel::run()` is the host call, and `kernel::operationSpirv()` is the module a
device runs.

**One namespace holds every kernel here.** `mesh::kernel` is where the
point operators' arithmetic, the swept ring's and the stamping's are all
declared, each naming its own subject — `OperationArguments`/`OperationDispatch`,
`SweepArgs`/`SweepDispatch`, `StampArgs`/`StampDispatch`, and
`operationSpirv()`/`sweepSpirv()`/`stampSpirv()` — so no two of them answer to
one name and a reader looking for what a device dispatches finds all of
them together. `kernel::run()` is one overload set the dispatch type
decides.

Not every operator has one, and each absence is a boundary rather than a
gap: a generator makes the points rather than mapping over them (every
executor seeds through `pop::seedLanes()` and exports through
`pop::exportLanes()`, so the two ends of a cook are one definition);
`Smooth`, `Relax`, `Cluster` and `Transfer` read points they do not own;
`Sort` is a permutation; `Promote`
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
build and a Release one produce the same bits; and the SPIR-V is compiled
under `-fp-mode precise`, which puts a `NoContraction` decoration on
every float arithmetic result — without it a driver fuses a multiply and
the add after it and rounds once where the source rounds twice. Both
flags are set in one place, `sigil_slang_module`'s single-source kernel
lane, so a third kernel gets them by being one.

**A SWEEP runs on one as well, and its executor's whole contract is the
RING VERTICES.** `SweepOptions::runtime` carries a `pop::SweepRuntime`,
defaulting to `SweepRuntime::cpu()`, and a sweep is two things of which
only one is arithmetic: the ring vertices are a pure function of one
frame, one profile point and the size the profile scales to there, and
the topology around them — which vertices a quad joins, the fan that
closes an end, the averaging that forms a geometric normal — is integer
or a reduction over triangles that do not exist until the vertices do.
The topology is the same wherever the vertices were formed, so it is
written once and the seam is narrow: an executor is handed a
`kernel::SweepDispatch` and fills two lanes — a position carrying u
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
`pop::sweep()` over a rail, not the sink. An executor also declares,
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

**Operator dials are addressable by name.** `pop::setField(operation,
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
feature directory, and a fixture several features' tests share sits at
the library root instead, in `test/support/`. Features nest by dependency — a feature links only
what sits above it in the tree — and each header includes what it needs,
so including a deeper one pulls the shallower ones in.

The catalogue is four chapters beside this one, one per tier and one
per shelf: **[reference/PATH.md](reference/PATH.md)** the 2D tier's
headers, **[reference/MESH.md](reference/MESH.md)** the 3D tier's,
**[reference/POP.md](reference/POP.md)** the point operators and where
each member runs, and **[reference/KIT.md](reference/KIT.md)** the
stock values over both.

## The device

The one GPU device both APIs stand on is its own chapter:
**[reference/DEVICE.md](reference/DEVICE.md)** — bringing it up, the
rule the shared queue is submitted under, what a mesh and a map become
on it, the device executors beside their CPU ones, and the Vulkan
loader.

## Conventions that will bite you

The properties of the code that produce geometry which is silently,
plausibly wrong rather than obviously broken are their own chapter:
**[reference/TRAPS.md](reference/TRAPS.md)** — the handedness and the
origins, the sign conventions, the pads and the lanes, the numberings
that are ABI, and what each importer does and does not carry.

## Boundaries

Publicly the library links Skia, glm, Boost.Container and Boost.Unordered, and
the two SigilCore leaves —
SigilCoreCompute for the seeded mixers, SigilCoreComparable for the
erased value the mesh and point-operator runtimes are — and nothing
else; every feature links only the features above it in the tree. Linking the
leaves acquires no kernel, no device and nothing that draws.
Privately `path/blend` links `SigilMaterialColor`, the colour value and
the OKLab round trip its colour interpolation runs in — the one edge
from this library into SigilMaterial, and no header spells it. Privately
`mesh` uses the header-only earcut for cap triangulation, and
`mesh/codec` uses
tinyobjloader for OBJ, Alembic for `.abc` and the header-only cgltf for
glTF, and simdjson for the JSON a `.geo` is; STL and PLY are parsed by
hand. None of those reaches
another feature, and none of them reaches a public header.

`device` is the exception to all of that, and the one feature that
brings a renderer's dependencies with it: Diligent Engine, SigilCore's
hardware device and SigilSkia's Graphite. What links it is the device
executors alone — `mesh/pop/device` and `mesh/render/device`, each its
own target beside the CPU executor of the seam it serves — so a consumer
that cooks a chain or draws a mesh on the host acquires none of the
three, and no other feature here reaches down into it.
`device/residency` is where the mesh currency and SigilMaterial's texture
and Slang backend are named — it is the one place a geometry target names
a material one other than `path/blend`'s private colour link, and it
reads a texture and an environment map for one thing only: putting their
pixels on the device.

**Nothing here draws, and nothing here has a colour.** A scatter answers
points, a triangulation answers indices, a hull and a Voronoi cell and a
streamline answer rings and lines, a symmetry answers matrices, and a
cell sheet answers whatever the caller put in it. What is stamped on a
point, what a cell is filled with, how thick a flow line is drawn and in
what ink are all the caller's, and each of these answers in a type
something else already knows how to paint. That is what lets one scatter
serve a stipple, a packing and the seeds of a flock without any of the
three being spelled here — and it is why a "voronoi" that came back as a
picture would be a worse primitive than one that comes back as rings.

It deliberately does not own a window, a Qt dependency, a
component or scene kernel, an animation timeline, an image decoder, a
resource-access layer, or text layout. Where one of those is needed —
decoding a texture an importer handed you, or fetching an asset over the
network — that is the caller's job, and the library is designed so the
caller can supply it (`decode::Resolver` is the hook).

Surface shading is **SigilMaterial**'s: its bevel normal maps and its
gold, chrome and glass recipes take a normals pass this library draws,
handed over as pixels. No material target links a geometry one, and the
one edge the other way is `path/blend`'s private link to the colour leaf.

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
python3 scripts/sigil.py setup --config Release
cmake --build build --config Release
```

Targets: one static library per feature — `SigilGeometryPath`,
`SigilGeometryPathBlend`, `SigilGeometryMesh`, `SigilGeometryMeshCamera`,
`SigilGeometryMeshRender`, `SigilGeometryMeshCurve`,
`SigilGeometryMeshPop`, `SigilGeometryMeshCodec`,
`SigilGeometryMeshRenderDevice`, `SigilGeometryDevice`,
`SigilGeometryDeviceResidency`, `SigilGeometryKit` — the `SigilGeometry` umbrella over all of them, the tests, and one Google Benchmark binary,
`geometry_bench`, built by the `benches` target into
`bin/<config>/benches/` and run from a Release build through
`scripts/sigil.py bench`. Its arms sit in each feature's `bench/`:

| Arms | Measure |
| --- | --- |
| `path/bench/` | flattening and resampling by point count, corner detection and the parallel and displaced constructions by contour length, the noise hashes per call, the pose read over one contour and over many, a conic sampled by step count — whole, and held to a reach that drops most of the sweep — beside one point and one direction read on their own, and each projection scheme by the star read out and home with the closed-form image of a circle and the turn of a whole sky beside them |
| `path/blend/bench/` | a two-key blend by step count and by sample density, and the same blend threaded onto a spine |
| `mesh/bench/` | the parametric sheet by vertex count, and the two whole-mesh rewrites: appending and unwelding a primitive colour lane |
| `mesh/camera/bench/` | the per-frame transform builds: view, view-projection, the matrix seam, and the two placement helpers |
| `mesh/render/bench/` | the built-in runtime by triangle count and by shading mode, the cost of the cull and the sort, and the panel concat |
| `mesh/curve/bench/` | arc-length sampling and parallel-transport frames by count, and the pose read over a held rail and over the spline that builds one |
| `mesh/pop/bench/` | the cook per operator over a thousand points, whole chains by count and operator mix, the runtime seam's dispatch against the same cook reached directly, and the swept operator by tessellation for a circle profile, a line profile and a line on a hung rail — with the ring seam measured on its own |
| `mesh/codec/bench/` | OBJ, GLB and `.geo` decoded from bytes in memory, per triangle or point |
| `device/bench/` | the way in, less the driver: the Vulkan handles read off Diligent's interfaces and adopted, with Graphite stood up on what comes back |
| `kit/bench/` | one silhouette generated from a value — analytic, sampled by density, seeded, wrapped — against the comparison a caching consumer prunes with; and the solids by output size, an extrusion against the outline it lifts, a lathe against the profile it turns, and each regular solid gathered from its own corner table |

A test asserts ONE behaviour this library promises through its public
headers to a caller who has read only this document, and its name is that
promise written as a sentence, so a failure line reads as the claim that
broke. It pins only what editing this library alone could falsify: a
closed form, a comparison two values are held to, a lane sized to its
vertices, two executors of one kernel agreeing bit for bit. It never pins
an anti-aliased byte, a fitted tolerance, a golden float read out of a
mixer, a byte layout the compiler chose or elapsed time — a picture
compared byte for byte is the plate ledger's to judge and a duration is
the bench ledger's. A claim made N times with one thing varying is one
`TEST_P` whose rows are named, so the failure line still reads as a
promise. One file per subject, named for what it asserts.

The library has ONE test binary, `geometry_test`, built from every
feature's `test/` directory and landing in `bin/<config>/tests/`. ctest
discovers one entry per CASE out of it, so a suite or a case is selected
by name — `ctest -R '^PopChains\.'` — with no target behind it. A
suite's file sits in the feature it covers.

| Files | Proves |
| --- | --- |
| `path/test/` — `ContoursTest`, `PolylinesTest`, `MarksTest`, `SegmentsTest`, `NodesTest`, `NeighboursTest`, `ScatterTest`, `TriangulateTest`, `FieldsTest`, `OpsTest`, `SeamsTest`, `CrossingsTest`, `FramesTest`, `ConicsTest`, `ProjectionsTest`, `BlendTest` | the 2D leaf and the shape interpolation over it: where a distance along a contour lands (held against an independent walk of the same contours), what a polyline flattens and resamples to, where marks land inside a shape, an outline read verb for verb and rewritten to start elsewhere or run the other way, the node arithmetic (nodes put where a curve turns, nodes taken away where they say nothing, a run of points fitted as few cubics, the exact in-between of a pair that pairs), the uniform grid judged against the brute-force answer, what each rate and each spread of a scatter guarantees, a triangulation on sets whose answer is known by hand with the dual cells and the outline at a tightness beside it, the three things a field is walked, repeated or stepped by, what each path operator names of two outlines, the two comparable seams a mark is deviated and widened through, who goes over at a crossing, the two coordinate systems a figure is measured in, which of the four curves a conic is at each eccentricity with where its focus stands against the figure it draws, every map coming back from the plane it lands on with the middle of it the same size whichever scheme was chosen — a stereographic carrying circles to circles and saying so rather than approximating where it cannot, the plate an astrolabe is with its pole and its horizon, the two cylindrical forms' parallels spaced by their own rule, and the three-angle turn that carries a star from one epoch to the next — and how many steps a blend makes |
| `mesh/test/` — `MeshTest`, `FacesTest`, `CameraTest` | the mesh currency, its faces and the camera that places it: the sheet's coherent lanes, transform and append with every lane kept sized to its elements, the primitive bake, a fanned polygon read as one face with one plane and one centroid, the face-up rotation landing the face it names on the axis it is given, and the view-projection and billboard transforms carried through to viewport pixels |
| `kit/test/` — `SilhouettesTest`, `ShapersTest`, `HatchesTest`, `DivisionsTest`, `SolidsTest` | the shelves: every silhouette inscribed in its box and equal values drawing equal paths (the contract a caching consumer prunes on), every shaper answering the deviation seam and moving the mark, the hatch door taking an outline and giving one back with the lattice and the offset behind it, a tick ladder and a chord fan as one multi-contour path at their frame's convention, and a path lifted with its hole intact, a profile lathed, the named surfaces closed and unit-normalled, and each regular solid counted by its own faces, closed on itself (V - E + F = 2), equal-edged and stood on a chosen face |
| `mesh/curve/test/CurveTest` | splines, the two rails, the pose read along them, and the projection to a 2D path |
| `mesh/render/test/` — `PainterTest`, `RuntimeTest`, `ShadingTest` | the mesh draw's pixels and the normals G-buffer's encoding; the draw's runtime seam; and each shading term against the closed form a device shader's own spelling of it is held to |
| `mesh/pop/test/` — `PointsTest`, `PopChainsTest`, `PopFiltersTest`, `PopLanesTest`, `PopNeighboursTest`, `PopSelectionTest`, `PopSinksTest`, `PopFieldsTest`, `RuntimeTest`, `SweepTest`, `SweptShapesTest` | point clouds and the chains over them: the generators' conventional lanes, the modifiers that move points exactly as the operators of the same name do, the splat as one canvas draw however many points and however many cells the cloud carries — with that draw keeping the back-to-front order, each point's own tint, size and atlas cell with no neighbouring cell bleeding into it, the square splat a cell of any aspect draws, the requested blend where two splats overlap, and every splat across the batch's chunk boundary, the lanes a chain carries and the dials that address them by name, the operators that read points they do not own — a relaxation pushing a scatter apart and stopping, a clustering grouping it in the metric its weights name, a transfer carrying a lane over from another cloud, and the connection sink answering the pairs near enough to join — each declined by name on a device runtime, naming a subset and acting on it, the sinks a chain reaches by its own verb, the cook's and the sweep's runtime seams, and what a profile carried along a rail forms. Links the codec to seed chains from an imported model |
| `mesh/codec/test/` — `ObjTest`, `GltfTest`, `StlTest`, `PlyTest`, `AlembicTest`, `GeoTest`, `ModelTest`, `EncodeTest` | one file per format, plus the Model operations over whatever reader made it and both writers' return leg. The only binary linking Alembic |
| `device/test/DeviceTest` | one device end to end: Graphite draws on the very queue Diligent submits through, the adopted device names every Vulkan handle, and Diligent still drives it afterwards |
| `device/residency/test/ResidencyTest` | what the device keeps between draws: a named mesh crossing once and drawn from after, a nameless one written through the streaming pair, the depth of an uploaded map's chain, and the letting go that keeps a scene from holding everything it ever cooked |
| `mesh/pop/device/test/` — `DeviceCookTest`, `DeviceStampTest`, `DeviceSweepTest` | the CONFORMANCE of the device executors: every chain, stamping and sweep they say they can do compared with the host's bit for bit, the operators they decline by name, and a cook that reads back and cooks again with the backend's diagnostics collected |
| `mesh/render/device/test/PainterTest` | the mesh painter's device executor: the runtime as a value, the style's own answers read the same way on either executor, and a panel as the same BYTES on both — which it is because a panel's content is Skia's to rasterise whichever executor holds it. How far two rasterisers stand apart on everything else is a picture, judged against a committed baseline rather than here |

The device suites carry the `gpu` label: every case in them brings a
Vulkan device up and skips, naming what is missing, when the machine has
none. A machine without one runs `ctest -LE gpu` and checks the whole
host tier. Nothing else here needs a device, a font or a network.

| Label | On | Means |
| --- | --- | --- |
| `gpu` | `AdoptedDevice`, `AdoptedGraphite`, `Device`, `MapUpload`, `MeshResidency`, `TextureResidency`, `DevicePop`, `DeviceStamp`, `DeviceSweep`, `Painter` | needs a Vulkan runtime (on macOS: `brew install molten-vk vulkan-loader`); skips with the reason without one |

Fixtures live in one place per audience. `test/support/` at the library
root holds what more than one binary reads: `GeometrySupport.h` (the OBJ
cube with its material, a quad with a known winding, the bytes of a piece
of text), `Paths.h` (a square and a rectangle) and `RuntimeSeam.h` — the
typed suite every runtime seam in this library is held to, instantiated
once per seam with a traits type. `device/test/support/OnDevice.h` is the
one device a test process brings up, and the reason there is none; any
binary with device cases puts that directory on its include path.
`mesh/pop/test/support/Loops.h` holds the ring every pop chain is
scattered along. A helper one file uses stays in that file.

```sh
ctest --test-dir build -C Release --output-on-failure
```

**Looking at any of it** goes through SigilSketch, in `src/sketch/`: one
file per renderable thing, in one registry, drawn by one application.
The studies over this library are `blend_options`, `path_booleans`,
`crossing_rule`, `exact_tangent`, `curve_shelf`, `shape_tour`,
`corner_notched`, `svg_silhouette`, `contour_poses`, `mesh_generators`,
`mesh_normal_bridge`, `floating_panels`, `painter_gpu`, `pop_stamps`,
`pop_prims`, `pop_deform`, `pop_math`, `pop_order`, `pop_billboards`,
`formation_bands`, `over_under`, `routes_probe`, `geo_groups`,
`yarn_marquee` and `shapeworks_lab`; `codec_roundtrip` takes a mesh out
through the codec and back, and `scattered_model` brings a file in
through it and stands it in a lit room. Each is addressed by
its own stem:

```sh
build/bin/<config>/Sketchbook.app/Contents/MacOS/Sketchbook --sketch pop_stamps
```
