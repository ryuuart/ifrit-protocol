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

It is **two tiers**, one per currency, and eight feature libraries under
them. The `path` tier is 2D: an outline resampled, addressed by
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
  curve/       SigilGeometryMeshCurve     sigil::geometry::mesh::curve
  pop/         SigilGeometryMeshPop       sigil::geometry::mesh::pop
  codec/       SigilGeometryMeshCodec     sigil::geometry::mesh::codec
device/        SigilGeometryDevice        sigil::geometry::device
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
#include <sigilgeometry/path/Ops.h>
#include <sigilgeometry/kit/Sections.h>
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
re-cook. `ops::PathOp` plus `ops::chain()` compose a non-destructive
recipe; `blend::Options`, `pop::SweepOptions` and `pop::Chain` behave the
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
per-point arithmetic, and `mesh/pop/kernels/Sweep.slang`, the swept
ring vertex. The build compiles each twice — to C++, which the executor
behind the built-in runtime calls, and to SPIR-V, which a runtime that
owns a device dispatches. Neither side re-derives a formula, which is
what lets two tiers be held to bit identity rather than to a tolerance.
`mesh/pop/Spirv.h` is the one thing both kernels' words go through:
`noContraction()` adds the decoration the emitter leaves out, in one
place rather than once per kernel. `kernel::has(op)` is the one
answer to whether an operator has a kernel, `kernel::describe()` packs
one into the argument block both ends read, `kernel::run()` is the host
call, and `kernel::opSpirv()` is the module a device runs.

**One namespace holds every kernel here.** `mesh::kernel` is where the
point operators' arithmetic, the swept ring's and the stamping's are all
declared, each naming its own subject — `OpArgs`/`OpDispatch`,
`SweepArgs`/`SweepDispatch`, `StampArgs`/`StampDispatch`, and
`opSpirv()`/`sweepSpirv()`/`stampSpirv()` — so no two of them answer to
one name and a reader looking for what a device dispatches finds all of
them together. `kernel::run()` is one overload set the dispatch type
decides.

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
`kernel::opSpirv()` because the emitter puts none there — without it a
driver fuses a multiply and the add after it and rounds once where the
source rounds twice.

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
feature directory, and a fixture several features' tests share sits at
the library root instead, in `test/support/`. Features nest by dependency — a feature links only
what sits above it in the tree — and each header includes what it needs,
so including a deeper one pulls the shallower ones in.

**`path`** — `SigilGeometryPath`, the leaf. Sixteen headers that depend on
nothing else in the library: Skia, glm, and SigilCoreCompute, whose
seeded mixers the value-noise field is built on.

- **`path/Polyline.h`** — the resampling core. `Polyline` (its points, its
  closure and its `lane`, one scalar riding each vertex) and `flatten()`,
  `sample()` to walk a parametric curve evenly by arc length, `Sampled`
  and `resample()`, `bestAlignment()`/`applyAlignment()` for matching two
  closed contours, `toPath()` to rebuild (optionally through Catmull-Rom
  cubics), `smoothThrough()` to rebuild as a curve the points STEER — one
  quadratic per interior point, through the midpoint of every edge and
  never outside the hull of the points, so a coastline given a dozen
  points or a brush centreline given four reads as one stroke rather
  than a chain of chords — and `lerp()`. Beside them the two resamplings
  keyed to a SPACING: `subdivide()`, every edge cut into equal steps no
  longer than the spacing with every source vertex kept, and
  `catmullRom()`, the same cut through the curve the controls lie on,
  blended toward the chords by an amount so a hand-placed chain does not
  bow further than the hand meant. Both carry the lane. And what a
  polyline answers about the area it bounds: `bounds()` and `bounds()`
  over a set, `contains()` (the even-odd ray test on one ring, which
  joins the ends whether or not the polyline says it is closed) with
  `containsEvenOdd()` over a set of them, and `edgeCrossings()`, where a
  segment crosses the edges, nearest its start first.
- **`path/Stride.h`** — the even-spacing walk for a curve that arrives one
  piece at a time. `Stride::advance(length, spacing, land)` answers the
  fractions of the piece the walk lands at and carries the distance still
  owed across pieces, so a stroke sampled as a device reports it and the
  same stroke sampled whole put their marks in the same places. It names
  no point: the caller owns the geometry and interpolates whatever rides
  on it.
- **`path/Lattice.h`** — the scanline fill. `lattice()` lays parallel lines
  at an `angle`, a `spacing` apart with an optional `taper` opening or
  crowding each successive gap, and cuts them to the even-odd interior of
  a set of rings; each `LatticeMark` is a centreline. A hatch, a plotter
  fill and a mass of strokes are the same construction, so there is one.
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
- **`path/Noise.h`** — `valueNoise()`, trilinear value noise over the
  integer lattice in [-1, 1], seeded: this library's own field, read at a
  POSITION rather than at an index. The per-index mixers it is built on —
  `hash(seed, i)` and the PCG family — are SigilCoreCompute's, included
  from `<sigilcore/compute/Noise.h>` and spelled `core::noise::`; a
  resource key and a text cache fold with the same arithmetic.
- **`path/Numeric.h`** — `kPi`, `kTau`, the degree/radian factors,
  `bisect()` over a predicate and `wrap()` into a period.
- **`path/Arrange.h`** — namespace `arrange`. Where item i of n goes when
  a run of things is spread out: `Turn` (`Open` occupies both ends of an
  extent in n−1 steps, `Closed` takes n steps so the last stops short of
  the first), `step()` and `along()` over an extent in the caller's own
  unit — radians round a ring, arc length along a contour — and
  `onRing()`, item i's centre on the ellipse at a centre with a radius per
  axis. The grid half is `Cell`, `cellAt()` (row-major index to column and
  row), `moduleSize()` (the module that fits columns by rows of itself and
  the gaps between them exactly into a container) and `cellRect()` (the
  rect a block of cells covers, swallowing the gaps it crosses; nothing is
  clamped to a column or row count, because whether landing outside is an
  error or a bleed is the caller's to know). Here rather than in a
  catalog of placements because a catalog is where this arithmetic gets
  spelled a second time, and two spellings of one ring round apart.
- **`path/Skia.h`** — `toSk()` and `fromSk()` between `glm::vec2` and
  `SkPoint`, and `centre()` of an `SkRect`.
- **`path/Edges.h`** — narrowing an outline before something is drawn on
  it. `Edge` and `has()`, `edges()` (the sub-contours facing chosen box
  edges, classified against the bounds centre and cut by bisection at
  each run boundary) and `insetOutline()` (a mitred concentric copy;
  positive shrinks). Both take an outline and give an outline. Beside
  them `insetPolygon()` takes a polygon's vertices and gives them back
  moved inward ONE FOR ONE — every edge parallel to its source at the
  distance, inward read off the polygon's own winding — so a caller
  pairing each source corner with its moved one (a chamfer band, a lid
  on a plinth) keeps the correspondence an outline offset cannot give;
  a needle-sharp corner's mitre is capped at a stated number of
  distances, blunting the corner rather than dropping the vertex.
- **`path/Ops.h`** — path operators. Booleans over Skia's pathops (`unite`,
  `subtract`, `intersect`, `exclude`, `simplify`, and a stroke-expansion
  `offset`, and `roundCorners`), and four distortions as parameter structs
  you apply on demand: `Roughen`, `Zigzag`, `PuckerBloat`, `Twirl`.
  `PathOp` and `chain()` compose them, `offsetBy()` adapts `offset` into a
  step.
- **`path/Shaper.h`** — `Shaper`, the COMPARABLE `SkPath -> SkPath` value,
  over the `ShaperScheme` concept (`shape()`, equality, an optional
  `bleed()` declaring how far the deviation reaches). It bends one
  continuous mark — a wave, a zigzag, a jitter, an offset. Comparable is
  the point: a consumer that caches drawings proves two frames asked for
  the same deviation and keeps the recording it has, which `ops::PathOp`
  cannot answer.
- **`path/Profile.h`** — `Profile`, the comparable WIDTH LAW, over the
  `ProfileScheme` concept (`across(along)`, `max()`, equality). `max()`
  is what every cull and bleed is sized from; equality is required
  because a profile is read live. `PxKeyedProfileScheme` declares
  `alongIsPx` for a law keyed in px of arc length rather than in a
  fraction of it — which is what keeps a calligraphic pressure law from
  sliding along a mark as a reveal grows — and `acrossAt(along, lengthPx)`
  is the one call that converts. `profile::self()` and
  `profile::offset(px)` are the two presets every other profile is
  defined against.
- **`path/Band.h`** — `profileOffset()` walks one rail of a width law;
  `bandRegion()` walks both and closes them per contour, on
  `Formation::Centered`, `Outward` or `Inward`. A constant profile
  delegates to `parallel`, so corners get the real-vertex repair rather
  than the spur a sample-and-displace walk leaves inside every rectangle.
- **`path/Frame.h`** — the two coordinate systems a figure is measured in.
  `Frame` converts `(angle, radius)` into a point, a rect or an
  arc-length fraction IN THE DRAWING'S OWN CONVENTION: `Zero::North` or
  `East`, `Sense::CW` or `CCW`, plus an origin offset. That is the reason
  it is a value — written as a bare `polar()` helper the difference is a
  sign flip and a −90 that every call site repeats. `scaled`, `about` and
  `turned` derive a frame that keeps the convention it came from. `Grid`
  is the unit map: artefact units to canvas px through one scale, an
  origin and an optional snap, `constexpr` so a canvas constant can be
  declared in the artefact's units. `centred` is the rect both are read
  through.
- **`path/Crossings.h`** — where a set of paths cross each other and who
  is on top there. `discoverCrossings()` finds every PROPER crossing —
  coincident paths and endpoint touches are meetings, not crossings —
  and numbers them along the boundary. `CrossingRule` is the comparable
  answer: list order by default, `crossing::alternate()`,
  `crossing::alternateAlong()`, `crossing::sequence()`,
  `crossing::pairs()` for dominance (cycles
  legal, which is the impossible braid), your own `CrossingScheme`, and
  `except(i, order)` pinning one knot POSITIONALLY. The two alternating
  rules are not the same rule: `alternate()` alternates by DISCOVERED
  ORDINAL, which is arc length along one strand, so every other strand
  meets that numbering in whatever order it happens to;
  `alternateAlong()` is the knot-theoretic weave — walk ANY strand and
  the crossings run over, under, over — and it answers by sorting the
  passes, two per crossing, by strand and then by arc length. It
  therefore needs the whole set before it can answer any of it, which is
  what `CrossingRule::prepare(all)` and the `PreparedCrossingScheme`
  concept are for: a holder that discovers crossings calls it once per
  discovery, and what it works out stays outside equality because it is
  a function of the geometry rather than of the author. A {7/2}
  heptagram is the smallest figure that tells the two apart. `crossingPatch()` is
  the region two marks actually overlap at one knot, bounded by a
  `maxRadius` that is required for correctness rather than a margin:
  without it neighbouring lenses merge and one strand owns half the
  braid.

**`path/blend`** — `SigilGeometryPathBlend`, needs `path`.

- **`path/blend/Blend.h`** — shape interpolation modelled on Illustrator's
  blend tool: `Key`s expand into drawable `Step`s under `Options`
  controlling spacing (`Steps`, `Distance`, `SmoothColor`), an optional
  spine path, orientation, sample density and outline smoothing.

**`mesh`** — `SigilGeometryMesh`, needs `path`. The 3D tier's root, and
the currency every feature under it speaks.

- **`mesh/Mesh.h`** — the mesh currency. The `Mesh` struct (positions,
  normals, uvs, colors, indices, and the `prims` lane map),
  `append()`/`transform()`/`computeNormals()`/`bounds()`, and
  `mesh::bakePrimColor()`. Two surfaces are here rather than on the kit's
  shelf because everything else is built through them: `grid()`, the
  parametric-sheet seam a caller hands its own formula to, and `quad()`,
  the flat panel a consumer needs to have a mesh at all. The struct's own
  methods are `Mesh.cpp`; the two surfaces and the bake are
  `Generators.cpp`.
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
- **`mesh/render/Shading.h`** — the arithmetic a lit draw is composed of,
  for a tier with no shading language: `Environment` (the prefiltered
  chain a reflection reads, the cosine convolution a diffuse term reads,
  the orientation, the dials, the backdrop and the ground sphere it is
  projected onto), `equirectUv` and the two polynomials under it,
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
  window of a closed loop whose across-vector is held world-vertical; the
  and `project()` to draw the curve as a 2D path under a camera.
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

- **`mesh/pop/Points.h`** — `Cloud` and its lane accessors (`Cloud.cpp`);
  the generators `onSpline()`, `grid()`, `ring()`, `scatterBox()` and
  `onMesh()` (`Generators.cpp`); the modifiers `jitter()` and
  `displaceNoise()`, the consumers `instance()` and `quads()` (stamp a
  mesh at every point into one merged mesh) and `promoteToPrims()`
  (`Modifiers.cpp`); and `drawBillboards()`, camera-facing sprites
  (`Billboards.cpp`). `BillboardStyle::texLane` names a colour lane of
  {uOffset, vOffset, uScale, vScale} windows — what a `pop::Atlas` op
  writes into `"Tex"` — and each splat then draws THAT CELL of the
  sprite, so one sheet splats as a field of different sprites. It is
  named rather than assumed, because a cloud may carry `"Tex"` for the
  stamping path while its splats are meant to be one sprite.

  **A modifier is its operator without a chain.** `jitter()` runs the
  `Jitter` operator's own kernel over the positions and
  `displaceNoise()` reads the field `Noise` displaces by
  (`pop::noiseField`), so a cloud perturbed with a chain and a cloud
  perturbed without one move by the same floats. One verb is one field:
  a second arithmetic under one name would mean nobody could say which
  of them a picture came from.
- **`mesh/pop/Pop.h`** — the operator chain language and the runtime seam
  it executes through, both in the `pop` scope: `pop::on()` opens a chain,
  `pop::cook()` evaluates one on the `pop::Runtime` it is given,
  `pop::Executor` is what a runtime supplies (`pop::Runtime` is an erased
  value of SigilCoreComparable's shape) and `pop::opName()` names an
  operator. The field table behind `pop::setField()`/`getField()` is
  `Fields.cpp`; the built-in executor, the `Runtime::cpu()` value with
  its `Runtime::cpu(itemGrain)` spelling, and the
  `cook()` door that checks an executor's capability before dispatching
  are `Cook.cpp`; the mesh-forming sinks `pop::cookMesh()` and
  `cookSweep()` are `Sinks.cpp`.
- **`mesh/pop/Kernel.h`** — the seam between the two ends of one piece of
  arithmetic: `kernel::OpArgs` (the argument block, every member a
  four-component vector so its bytes stand at the same offsets in a
  uniform buffer), `kernel::OpDispatch` (which lane fills each binding
  role), `has()`, `describe()`, `run()` and `opSpirv()`. It also names
  the namespace every kernel here shares. `Kernel.cpp` packs
  and calls; `Spirv.cpp` decorates the module.
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
- **`mesh/pop/Spirv.h`** — `mesh::noContraction()`, a compiled module
  given one `NoContraction` decoration per arithmetic result. It stands
  here once rather than beside each kernel's own words, because a module
  that means one thing in one feature and another in the next is not a
  single source.
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
declares; `PlyEncode.cpp` and `GeoEncode.cpp` are the writers.

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

**`Geometry.h`** at the root of the include tree includes every public
header, for a consumer that takes the whole library rather than a tier
of it.

### The operators

`pop::Op` is a variant over twenty-two operator values, and `pop::Chain`
is a vector of them. Generators seed a chain: `SplineScatter` (points along a
window of a closed loop), `MeshScatter` (points on a formed model's
faces) and `PointSet` (an existing `Cloud` — an import's `asCloud()`, a
previous cook — every lane riding in as an attribute, so a Houdini group
arrives as a mask under its own name). Filters rewrite attributes in place: `Jitter`, `Noise`, `Ramp`,
`Vary`, `LookAt`, `Math`, `Relax`, `Fill`, `Atlas`, `Lookup`, `Affine`
(any `mat4` on a position or a direction lane), `Peak` (push along a
direction lane), `Deform` (twist, taper or bend about an axis), `Mix`
(blend two lanes into a third by a constant or a lane) and `Normal` (make
a direction lane unit-length and give every one of them the same sense,
outward from a centre or inward). `Select` is the selector: it writes a
mask lane from a sphere or box region, feathered at its edge and combined
into what the lane already holds (replace, union, intersect, subtract),
and `Delete` is its other half — it drops the points a mask names, which
is the one operator that changes the count. `Promote` and `Sort` are the
primitive-class and permutation-class operators.

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
`select`, `drop`, `keep`, `masked`, `affine`, `orient`, `peak`, `twist`,
`taper`, `bend`, `mix`, `mixBy`, `copy`, `normal`, `op`) append operators — `masked()` sets
the mask on the filter just added — and the builder converts to a
`Chain`, so you can reach into any operator afterwards and re-cook. Sinks
end a chain: `cook()` to a `Cloud`, `cookMesh()` to one mesh of stamps,
`cookSweep()` reading the cooked points as the path `pop::sweep()`
carries a profile along, and `cookBillboards()` splatting them onto a
canvas as camera-facing sprites — the one sink that forms no geometry,
because a billboard faces the eye and so is answered where the eye is
rather than in the world. The builder reaches all four as `cloud()`,
`stamps()`, `sweep()` and `billboards()`.


### The pop family, and where each member runs

Every member of the family is a described VALUE performed by an
EXECUTOR, and the two columns say which executors there are for it. A
"kernel" is one piece of Slang this feature compiles twice — to the C++
the host executor calls, and to the SPIR-V a device executor dispatches
— so the two tiers are held to bit identity rather than to a tolerance
wherever the device column says yes. A *host-only* entry is a stated
boundary and not a gap: the reason is in the operator's own doc comment
and repeated in one word here, and a device runtime DECLINES such an
operator by name rather than dropping it, so `pop::cook` stops with a
message naming both the operator and the runtime.

| Operator | Class | Host | Device | Why, where there is no kernel |
| --- | --- | --- | --- | --- |
| `SplineScatter` | generator | yes | seeded on the host, uploaded | a generator makes points rather than mapping over them |
| `MeshScatter` | generator | yes | seeded on the host, uploaded | as above |
| `PointSet` | generator | yes | seeded on the host, uploaded | as above |
| `Jitter` | filter | kernel | kernel | |
| `Noise` | filter | yes | declines | a field of library sines; a polynomial sine is a different function, not a rounding of one |
| `Ramp` | filter | kernel | kernel | |
| `Vary` | filter | kernel | kernel | |
| `LookAt` | filter | kernel | kernel | |
| `Math` | filter | kernel | kernel | |
| `Relax` | filter | yes | declines | a point reads two it does not own, so one lane cannot be both what is read and what is written |
| `Fill` | creator | kernel | kernel | |
| `Atlas` | filter | kernel | kernel | |
| `Promote` | primitive | yes | declines | addresses triangles a sink has not formed yet |
| `Lookup` | filter | kernel | kernel | |
| `Sort` | permutation | yes | declines | a permutation is a sorting network, not a per-point map |
| `Select` | selector | kernel | kernel | |
| `Affine` | filter | kernel | kernel | |
| `Peak` | filter | kernel | kernel | |
| `Deform` | filter | yes | declines | twist and bend turn on library trigonometry |
| `Mix` | filter | kernel | kernel | |
| `Delete` | set | yes | declines | the count is what it changes, and a per-point map cannot change it |
| `Normal` | filter | kernel | kernel | |

And the sinks, which stand on the cooked cloud:

| Sink | What it forms | Host | Device |
| --- | --- | --- | --- |
| `cook()` | the `Cloud` itself | yes | the chain dispatched, read back once |
| `cookMesh()` / `points::instance()` | the stamp placed at every point | yes | the vertices dispatched, read back once |
| `cookSweep()` / `pop::sweep()` | the profile carried along the cooked points | yes | the ring vertices dispatched, read back once |
| `cookBillboards()` / `points::drawBillboards()` | camera-facing sprites on a canvas | yes | — |

**Against TouchDesigner's POP set**, the operators above answer Noise,
Transform, Math, Attribute Create, Attribute (blend and copy), Lookup and
Ramp, Group, Delete, Normal, Sort, Attribute Promote, Twist/Bend/Taper,
Smooth, Peak, Look At, Randomise and the texture cell pick; the generators answer Point
Generator, Scatter, SOP to POP; the sinks answer Copy/Instance and
Skin/Sweep. **Not present**: Ray (project points onto a surface along a
direction), Limit (clamp a lane to a range — a `Lookup` with a flat table
is the workaround), Trail (a point's history as a curve), Particle
(integrate velocity and force per frame) and Texture Sampler (read an
image at a point's uv into a lane; `Atlas` picks a cell, it does not
sample).

**`kit`** — `SigilGeometryKit`, the shelf. Stock values over the tiers
beneath, in `sigil::geometry::shapes`.

- **`kit/Generators.h`** — the closed silhouettes: `svg()` (an SVG path-d
  string parsed once, its bounds mapped onto the box), `polygon()`,
  `star()` (with `waist`, which bows each arm edge inward the way an
  engraved star narrows), `circle()` (winding, start point and a
  concentric `inset`), `annulus()`, `squircle()`, `blob()` (seeded, so the
  same seed is the same blob every run), `arc()` (open), `sector()`
  (closed and fillable, with an inner radius for the donut slice),
  `parallelogram()` and `arrow()`.
- **`kit/Curves.h`** — the open silhouettes, evaluated in a unit frame and
  scaled onto the box's half-extents so a curve keeps its proportions when
  the box changes: `parametric()` raw and keyed, `lissajous()`,
  `harmonograph()` (a Lissajous whose amplitudes decay, with precession),
  `rose()`, `spiral()` (Archimedean or logarithmic) and `trochoid()`.
- **`kit/Corners.h`** — `rounded()`, the wrapper that rounds any
  silhouette's sharp corners, and the two shapes a frame is cut to:
  `chamfered()` and `notched()`, both taking a per-`Corner` mask because a
  cut on one diagonal is the common case and no single radius says it. A
  treatment of ZERO is a square corner, not a cut of no length: the two
  vertices it would otherwise emit stand on top of each other, and
  `rounded()` over that path finds no corner to round there.
- **`kit/Silhouettes.h`** — the 2D shelf, including all three.
- **`kit/Shapers.h`** — `shapers::`, the stock over the deviation seam:
  `Wave` (also the braid primitive — strands that oscillate trade sides,
  and where they trade sides they cross), `Zigzag`, `Square`, `Jitter`,
  `Offset`, `Rounded` and `Chamfer`, with a factory each. Beside them, in
  the seam's OWN namespace one directory down, `path::profile::wave` —
  the oscillating width law, which is ZERO-MEAN and therefore a strand
  centreline rather than a band width.
- **`kit/Sections.h`** — `sections::`, the two unit cross-sections a
  sweep carries: `circle()` (open, its seam point duplicated so the swept
  u reaches 1) and `line()` (a unit-width segment, a flat band once
  swept). `SweepOptions::scale` sizes both, so neither takes a radius or
  a width, and an outline that is not one of these reaches a sweep
  through the sweep's own `profile::fromPath()`.
- **`kit/Divisions.h`** — a figure's divisions as ONE multi-contour path:
  `ticks()` walks a division count around a `Frame` (with a longer mark
  every N), `chords()` walks a polygon's sides. One path rather than N
  drawn things, because a divider ladder is static geometry with one
  style — the exception is per-mark animation, which needs its own
  keyed items. Each has a comparable `Silhouette` form (`TicksShape`,
  `ChordsShape`) for a consumer that shapes a box with it.
- **`kit/Solids.h`** — the 3D shelf, in `sigil::geometry::mesh` because
  what it makes is a `Mesh`. Two of them LIFT another currency:
  `extrude()` raises a filled path into a solid (caps earcut-triangulated
  with holes intact, walls swept from the flattened contours) and
  `revolve()` lathes a profile polyline around +y. The rest are the named
  surfaces — `torus()`, `superellipsoid()`, `cylinderPanel()` — each one
  `mesh::grid()` evaluated through a formula anyone could have written,
  which is why they are a shelf and not the currency.

Every value here has `path(SkSize)`, `operator==` and `operator()`, and
that is the whole contract: a consumer that caches drawings prunes on the
equality, and a consumer that wants a plain path-over-size function gets
one from the call operator. Your own generator written the same way has
the same standing — the kit is stock, never privileged, and equal values
must draw identical paths at every size.

**One table for the lane convention, and one for the stamp.**
`pop::attrFor` and `pop::cloudLaneFor` are the whole of the mapping
between a Cloud's lane names and the chain's attribute names —
`t`↔`T`, `size`↔`Scale`, `dir`↔`Dir`, `tint`↔`Color`, with `normal` also
seeding `Dir` because that is what a generator or an importer writes —
so a cloud seeded into a chain and exported back comes home to the lanes
it left from. `points::stampOptions(cloud)` is the whole of how a stamp
rides those lanes: "dir" where a chain produced one, "normal" where a
generator did, "size" scaling and "tint" colouring, each only where the
cloud carries it. Every stamping path takes its options from there,
because two tables mean one cloud standing its stamps up through one
caller and lying them flat through another.

## The device

`device::Device::create(config, &error)` brings the one GPU device up:
Diligent creates a Vulkan device and its immediate context, and
SigilCore's hardware device adopts the Vulkan device, queue and loader
entry points it made, with Graphite recording onto that same queue. A
texture named on `gpu()` is then an image both APIs reach — 2D drawing
through `graphite()` lands in it and a 3D pass samples it, with no copy
in either direction and one handle table naming both.

```cpp
#include <sigilgeometry/device/Device.h>

using namespace sigil;

geometry::device::DeviceConfig config;
std::string error;
std::unique_ptr<geometry::device::Device> device =
    geometry::device::Device::create(config, &error);
if (!device) return;  // no Vulkan runtime, for instance; `error` says why

core::hardware::GpuDevice& gpu = *device->gpu();
core::hardware::TextureDesc desc;
desc.width = desc.height = 512;
const core::hardware::TextureHandle texture = gpu.createTexture(desc);
const core::hardware::FenceHandle fence = gpu.createFence();

// Paint 2D into a texture a 3D pass will sample. Everything that submits
// on the shared queue happens under the lock.
geometry::device::Device::QueueLock lock(*device);
skia::OffscreenSurface surface(*device->graphite(), gpu, texture);
surface.canvas()->clear(SK_ColorBLUE);
surface.submit(gpu, fence);
```

`renderDevice()` and `context()` are the Diligent side and are never
null on a device that was created. `gpu()` and `graphite()` are the
adopted side and are null together when the adoption failed — a driver
without timeline semaphores, for instance, since that is what a hardware
fence is. A failed adoption costs the shared 2D path and nothing else.

**The queue is shared, and sharing has a rule.** Graphite's submissions
and Diligent's passes go into one queue in submission order, which is
what lets a submit stay asynchronous and still be correct — but only
while the two streams never interleave. Every Graphite submit, and every
fence signal or wait on `gpu()`, is made under a `QueueLock`. Diligent
takes the same lock from inside its own submissions, which is why the
lock does not nest: no Diligent call may be made while one is held.

**The device executors of this library's own seams stand beside their CPU
ones**, in `mesh/pop/device/`: `pop::deviceRuntime(device)` cooks a chain
by dispatching the kernel this build compiled,
`pop::sweepDeviceRuntime(device)` forms a sweep's rings by dispatching
theirs, and `points::deviceRuntime(device)` forms a stamping's vertices
by dispatching the third. Neither computes an arithmetic of its own — the kernel is one
Slang source compiled twice, to the C++ the host executor calls and to
the SPIR-V dispatched here — which is what lets the two tiers be held to
bit identity rather than to a tolerance. All three are absent from a build with no device feature, and
`mesh/pop/test/DeviceCookTest.cpp`, `DeviceStampTest.cpp` and
`DeviceSweepTest.cpp` are the conformance: every chain, every stamping
and every sweep the device runtimes say they can do, done both ways and
compared bit for bit — and, beside that, that the backend says nothing
while they do, because a wrong barrier is reported and then the right
picture is drawn anyway, so comparing answers cannot see one.

`device::Resources` is what every executor on that device stands on,
made once and shared: the buffer a draw's uniforms go into, the samplers
a map is read through (linear and nearest, clamped and tiled, plus the
one a panorama needs — periodic in azimuth, clamped at the poles and
linear ACROSS the prefiltered levels), the one white texel an unfilled
sampled slot reads, and the staging copy `read()` brings a texture's
pixels home through. None of it is a frame's; a frame's targets, meshes
and pipelines belong to whatever draws frames.

Four of those five samplers ARE the engine's own named states, and the
uniform buffer is made by its own one-call constructor, so nothing here
respells what the engine already spells; the readback's row copy is its
stride-aware subresource copy, which is what the two differing strides
need. The panorama's sampler is the one that stays written out, because
it is the one no named state covers: two different wraps on its two axes,
and a level range a roughness reads across.

**The Vulkan loader is opened once**, by the volk shim in
`device/VolkShim.c`, and the `vkGetInstanceProcAddr` it resolves is
handed to the hardware device — which refuses an adoption without one,
because dispatching through a second copy of the same library is what
makes two APIs stop being one device. `SIGILGEOMETRY_VULKAN_LIBRARY` names a
Vulkan library to open ahead of the built-in candidates. There is no
Metal path, because Diligent has no Metal backend: `create` fails on a
machine with no Vulkan runtime and says so, and on macOS the runtime is
`brew install molten-vk vulkan-loader`.

`geometry_device_test` is therefore where the hardware device's Vulkan
backend is exercised at all — the formats it maps, the import and export
round trip, the timeline fence, and Graphite over a texture the device
named — because a Vulkan device exists here and nowhere below.

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
- **A verb spelled two ways is one field.** `points::jitter` and
  `pop::Jitter`, `points::displaceNoise` and `pop::Noise` are the same
  operator reached with and without a chain, and the pre-chain spelling
  is written as a call into the operator's own arithmetic rather than as
  a second copy of it. `geometry_mesh_pop_test` compares the two paths
  bit for bit.
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
- **`Delete` reads an empty mask the other way round.** Every filter
  takes an empty mask name as "every point in full"; `Delete` takes it as
  "no point at all", because an operator that emptied the set by omission
  is not one anyone wants. It is also the only operator whose output has
  a different count from its input, so a chain that deletes and then
  addresses a point by index is addressing the compacted set.
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
  construction — `mesh::basisFor`'s policy, written out a second time in
  `kernels/Stamp.slang` because a kernel cannot call it, and held to it
  by the device conformance — so a face-camera'd quad and an instanced
  facing lane agree, and a cloud stamps identically on either tier.
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
hardware device and SigilSkia's Graphite. Nothing above it links it
unless it wants a device, and no other feature here reaches down into
it.

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
python3 scripts/setup.py --config Release
cmake --build build --config Release
```

Targets: one static library per feature — `SigilGeometryPath`,
`SigilGeometryPathBlend`, `SigilGeometryMesh`, `SigilGeometryMeshCamera`,
`SigilGeometryMeshRender`, `SigilGeometryMeshCurve`,
`SigilGeometryMeshPop`, `SigilGeometryMeshCodec`, `SigilGeometryDevice`,
`SigilGeometryKit` — the `SigilGeometry` umbrella over all of them, the tests, and one Google Benchmark binary
per feature, built by the `benches` target and run from a Release build
through `scripts/bench_ledger.py`:

| Binary | Measures |
| --- | --- |
| `geometry_path_bench` | flattening and resampling by point count, corner detection and the parallel and displaced constructions by contour length, the noise hashes per call, and the pose read over one contour and over many |
| `geometry_path_blend_bench` | a two-key blend by step count and by sample density, and the same blend threaded onto a spine |
| `geometry_mesh_bench` | the parametric sheet by vertex count, and the two whole-mesh rewrites: appending and unwelding a primitive colour lane |
| `geometry_mesh_camera_bench` | the per-frame transform builds: view, view-projection, the matrix seam, and the two placement helpers |
| `geometry_mesh_render_bench` | the built-in runtime by triangle count and by shading mode, the cost of the cull and the sort, and the panel concat |
| `geometry_mesh_curve_bench` | arc-length sampling and parallel-transport frames by count, and the pose read over a held rail and over the spline that builds one |
| `geometry_mesh_pop_bench` | the cook per operator over a thousand points, whole chains by count and operator mix, the runtime seam's dispatch against the same cook reached directly, and the swept operator by tessellation for a circle profile, a line profile and a line on a hung rail — with the ring seam measured on its own |
| `geometry_mesh_codec_bench` | OBJ, GLB and `.geo` decoded from bytes in memory, per triangle or point |
| `geometry_device_bench` | the way in, less the driver: the Vulkan handles read off Diligent's interfaces and adopted, with Graphite stood up on what comes back |
| `geometry_kit_bench` | one silhouette generated from a value — analytic, sampled by density, seeded, wrapped — against the comparison a caching consumer prunes with; and the solids by output size, an extrusion against the outline it lifts and a lathe against the profile it turns |

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

A binary exists only where it links a strictly smaller set of targets than
its neighbours AND that boundary is a promise someone could read. That is
why the blend feature's cases run in the path binary and the camera's in
the mesh binary: nothing links either without the tier beneath it, so
neither boundary is one a caller reads.

| Binary | Files | Proves |
| --- | --- | --- |
| `geometry_path_test` | `path/test/` — `ContoursTest`, `PolylinesTest`, `MarksTest`, `OpsTest`, `SeamsTest`, `CrossingsTest`, `FramesTest`, `BlendTest` | the 2D leaf and the shape interpolation over it: where a distance along a contour lands (held against an independent walk of the same contours), what a polyline flattens and resamples to, where marks land inside a shape, what each path operator names of two outlines, the two comparable seams a mark is deviated and widened through, who goes over at a crossing, the two coordinate systems a figure is measured in, and how many steps a blend makes |
| `geometry_mesh_test` | `mesh/test/` — `MeshTest`, `CameraTest` | the mesh currency and the camera that places it: the sheet's coherent lanes, transform and append with every lane kept sized to its elements, the primitive bake, and the view-projection and billboard transforms carried through to viewport pixels |
| `geometry_kit_test` | `kit/test/` — `SilhouettesTest`, `ShapersTest`, `DivisionsTest`, `SolidsTest` | the four shelves: every silhouette inscribed in its box and equal values drawing equal paths (the contract a caching consumer prunes on), every shaper answering the deviation seam and moving the mark, a tick ladder and a chord fan as one multi-contour path at their frame's convention, and a path lifted with its hole intact, a profile lathed, the named surfaces closed and unit-normalled |
| `geometry_mesh_curve_test` | `mesh/curve/test/CurveTest` | splines, the two rails, the pose read along them, and the projection to a 2D path |
| `geometry_mesh_render_test` | `mesh/render/test/` — `PainterTest`, `RuntimeTest`, `ShadingTest` | the mesh draw's pixels and the normals G-buffer's encoding; the draw's runtime seam; and each shading term against the closed form a device shader's own spelling of it is held to |
| `geometry_mesh_pop_test` | `mesh/pop/test/` — `PointsTest`, `PopChainsTest`, `PopFiltersTest`, `PopLanesTest`, `PopSelectionTest`, `PopSinksTest`, `PopFieldsTest`, `RuntimeTest`, `SweepTest`, `SweptShapesTest` | point clouds and the chains over them: the generators' conventional lanes, the modifiers that move points exactly as the operators of the same name do, the lanes a chain carries and the dials that address them by name, naming a subset and acting on it, the sinks a chain reaches by its own verb, the cook's and the sweep's runtime seams, and what a profile carried along a rail forms. Links the codec to seed chains from an imported model |
| `geometry_mesh_codec_test` | `mesh/codec/test/` — `ObjTest`, `GltfTest`, `StlTest`, `PlyTest`, `AlembicTest`, `GeoTest`, `ModelTest`, `EncodeTest` | one file per format, plus the Model operations over whatever reader made it and both writers' return leg. The only binary linking Alembic |
| `geometry_device_test` (`gpu`) | `device/test/DeviceTest` | one device end to end: Graphite draws on the very queue Diligent submits through, the adopted device names every Vulkan handle, and Diligent still drives it afterwards |
| `geometry_mesh_pop_device_test` (`gpu`) | `mesh/pop/test/` — `DeviceCookTest`, `DeviceStampTest`, `DeviceSweepTest` | the CONFORMANCE of the device executors: every chain, stamping and sweep they say they can do compared with the host's bit for bit, the operators they decline by name, and a cook that reads back and cooks again with the backend's diagnostics collected |

Two binaries carry the `gpu` label: every case in them brings a Vulkan
device up and skips, naming what is missing, when the machine has none. A
machine without one runs `ctest -LE gpu` and checks the whole host tier.
Nothing else here needs a device, a font or a network.

| Label | On | Means |
| --- | --- | --- |
| `gpu` | `geometry_device_test`, `geometry_mesh_pop_device_test` | needs a Vulkan runtime (on macOS: `brew install molten-vk vulkan-loader`); skips with the reason without one |

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
ctest --test-dir build -C Release -R geometry --output-on-failure
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
