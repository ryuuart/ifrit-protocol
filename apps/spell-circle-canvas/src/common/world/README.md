# SigilWorld

SigilWorld describes a 3D scene as comparable values, turns those values
into a frame — a scene, an ordered list of passes and the readbacks the
caller asked for — and executes that frame. It owns three things and
nothing else: the 3D scene description, the frame graph that orders
passes from their declared inputs and outputs, and the execution of that
graph. It holds no window, no swapchain, no clock, and no second
copy of anything a library beneath it already defines: meshes, point
operators, splines, cameras and the CPU mesh executor are SigilGeometry's;
materials, recipes and programs are SigilMaterial's; the reconciler, its
phases and the caching proof are SigilCore's, as are the erased value a
`Generator` and a `PassBody` take, the field pin every hand-written
comparator here sits under, and the fold a geometry signature accumulates
with; the device and its handles are SigilCoreHardware's, and Graphite is
SigilSkia's; animation is SigilMotion's; counters and timers are
SigilMeasure's.

Namespace `sigil::world`, headers under `include/sigilworld/`. Each
feature is its own static archive with its own tests and benchmark, and
links only the features beneath it; **`SigilWorld`** is the umbrella over
them, so a consumer of the whole library names only that. This page says
which features are built and which are not, rather than describing a
library that is not here.

## What is here

| directory | target | namespace | holds |
|---|---|---|---|
| `element/` | `SigilWorldElement` | `sigil::world` | `Element` and its verbs, the transform lanes, the geometry slot, tags, `Selector` and the `Generator` seam. No device, no retained state. |
| `frame/` | `SigilWorldFrame` | `sigil::world` | `Frame`, `Pass`, `Readback`, the `Targets` a frame's passes write, the `View` they read, and the `Runtime`/`Executor` seam with its CPU executor. No device, no retained state. |
| `graph/` | `SigilWorldGraph` | `sigil::world::graph` | the `Plan`: the order the passes run in, the surfaces they share, the barriers between them, and how each selection is realised. It reads declarations and draws nothing. |
| `scene/` | `SigilWorldScene` | `sigil::world` | the retained side: the reconcile host, the entity store, the content-keyed resource store, the declared phases, the execution of a frame's passes, and the draw. |
| `light/` | `SigilWorldLight` | `sigil::world::light` | emitters as plain comparable values over glm: a sun, a point light, a spot, their falloffs and the per-frame budget. |
| `kit/` | `SigilWorldKit` | `sigil::world::kit` | presets that compose elements: a three-point rig, a turntable, and the lit set both make over a ground plane; and the rails a body rides — the turntable's ring, a loop that rises and falls, a winding round a shell. Nothing here decides a look. |
| `diligent/` | `SigilWorldDiligent` | `sigil::world::diligent` | the programs this backend draws with — the scaffold, the sky, the mesh painter and the post stages, compiled through SigilMaterial's Slang backend — and the two seam values that stand on that device: the `Runtime` that performs a frame's passes and the `geometry::mesh::render::Runtime` that draws a mesh onto a canvas — plus `importNative`, the door a foreign texture reaches a material slot by. The chain cook and the swept rings are SigilGeometry's own device executors, beside the CPU ones of the same seams. |
| — | `SigilWorld` | — | the umbrella: an interface target over every feature above, and `<sigilworld/World.h>`, which is their public headers in one include. A consumer of the whole library names only this; the device feature is in it where it was built. |

## Writing a scene

An author builds a fresh `Element` tree every frame and hands it to a
`Scene`, which reconciles it onto what it already holds.

```cpp
#include <sigilworld/scene/Scene.h>

using namespace sigil;
using namespace sigil::world;

motion::Ticker ticker;
Scene scene(ticker);

choreograph::Output<float> spin = 0.0f;   // written by whatever drives it

scene.render(
    Element()
        .key("set")
        .child(Element().key("sun").light(sun({-0.4f, -0.8f, -0.4f})))
        .child(Element().key("eye").along(rail, travelled).camera(lens))
        .child(Element()
                   .key("tube")
                   .mesh(geometry::mesh::pop::sweep(loop, profile))
                   .fill(surface)
                   .rotateY(bind(&spin))
                   .tag("lit"))
        .child(Element()
                   .key("comet")
                   .chain(points)
                   .stamp(bead)
                   .window(head, 0.28f)
                   .tag("glow")));

scene.draw(canvas);   // from the viewpoint the tree declared
```

A tree handed to `render()` is a `Frame` with no passes, which is why the
call above compiles. A frame that has something to say about HOW the
picture is made says it in passes:

```cpp
Frame frame(model);
frame.extent({1280, 720})
     .camera(lens)
     .pass(geometryPass("main").writes("colour"))
     .pass(postPass("bloom")
               .reads("colour")
               .writes("lit")
               .only(sel::tag("glow"))
               .blur(9.0f))
     .pass(postPass("trail")
               .reads("lit")
               .previous("trail")
               .writes("trail")
               .composite(SkBlendMode::kPlus, 0.88f))
     .readback(readback("trail").then(observe));

scene.render(frame);
scene.draw(canvas);   // what the passes wrote
```

### Presets

`kit/` is a handful of trees someone would otherwise write by hand:

```cpp
#include <sigilworld/kit/Kit.h>

world::kit::Set set;
set.rig.extent = 140.0f;   // how far across the subject is
set.table.period = 12.0f;  // seconds for one turn of the camera

scene.render(world::kit::litSet(model, set, seconds));
```

`kit::threePoint(rig)` is three emitters round a subject, stated in the
subject's own extents so one rig serves a thumbnail and a room;
`kit::turntable(table, seconds)` is a camera riding a closed rail and
looking inward, with `kit::rail(table)` the curve itself; `kit::litSet`
is both over a ground plane, with the subject under it.

Two more rails stand beside the turntable's, each a plain `Spline3` a
tree rides with `along()`, scatters a comet on or sweeps a band over.
`kit::wave(w)` is a closed loop that RISES AND FALLS: its stations
alternate between an outer radius standing high and an inner one
standing low, so a tube swept along it or a comet riding it reads as a
curve in space rather than as a ring seen at an angle. `kit::winding(w)`
is a closed loop that WINDS A SHELL: on the ellipsoid its half-extents
name, it climbs and dives `wraps` times a lap while the plane it winds in
turns `turns` times — two counts with no common factor, so no wrap
retraces another and the loop crosses in front of and behind itself. A
sketch that draws one of these names its own radii, heights and shell,
and the preset states nothing but the arrangement.

**Nothing here decides a look.** Each returns an ordinary `Element` whose
every field the caller can read, replace or ignore, and the only
constants in one are the geometry of the arrangement plus a single
neutral grey for a ground plane that was given no surface. A preset is
worth having only for as long as that stays true — which is why a study
that wants lanes on the rig's key light takes the tree the preset
returned and rebuilds it with that child replaced, rather than the preset
growing a hook.

Where a concept exists in two dimensions this spells it the way
SigilCompose spells it — `key`, `child`, `children`, `memo`, `at`,
`scale`, `transformOrigin`, `fill`, `cache`, `bind`, `animate`, and the
`Selector` combinators `|`, `&` and `!`. The new spellings are the ones a
plane does not have: `translateZ`/`rotateX`/`rotateY`/`rotateZ`/`scaleZ`
and `rotate(axis, degrees)`, the geometry slot (`mesh`, `cloud`, `chain`,
`stamp`, `generate`), `window`, `along`, `tag`, `light`, the emitter's
dials `intensity` and `emission`, and `camera`.

A closed solid keeps its reverse-wound triangles hidden by default. A sheet,
screen or other open surface that must remain visible as the viewpoint passes
behind it declares `backface(Backface::Visible)`; the choice reaches both the
CPU rasterizer and the device pipeline.

## Mental model

**A description is a value; a node is what it became.** `Element` is
copy-on-write and comparable. `propsEqual` rules on every one of its
fields, pinned by a field count that fails the build when the struct
changes, and answers false for anything it cannot compare — a field left
out of the comparison does not produce a wrong answer where the mistake
is, it produces a node that never patches on that field again.

**There is no kind field.** The geometry slot's VALUE TYPE is the kind: a
node holding a `Mesh` and a node holding a `Chained` are told apart by
what they hold. So `remountRequired` answers false, always, and a node
that changes from one to the other resolves new resources in place while
its entity and its lanes stand.

**Three lifetimes, and none of them is the others'.** A node — its key,
its lanes, the motions in flight on them and its `entt::entity` — lives as
long as its key is in the tree. A resource — what a geometry slot cooked
to — lives in the content-keyed store, reference-counted, shared by every
node describing the same geometry, and dropped when the last of them lets
go. An extracted frame lives for one draw.

**The store is keyed by the geometry VALUE.** A cheap signature over
counts and kinds — SigilCoreCompute's FNV fold, so a bucket is one number
wherever it is computed — buckets a lookup and `operator==` decides it,
so two different geometries can never be served one artefact however
their bytes happen to fold together. Two nodes describing one chain cook
it once.

**Lanes are addressed by where the motion lives.** One fixed row per lane
per node — the nine placement lanes, the three origin lanes, the axis
turn, the along-distance, the window's head and span, the emitter's
strength and three colour channels, and the environment's seven
(`diffuse`, `specular`, `roughnessBias`, `crossfade`, `exposure`,
`backdrop` and its blur) — so a ramp survives a patch that changed what
the node holds. The four EMITTER rows and the seven ENVIRONMENT rows
stand at their own value's fields rather than at a fixed default: a
light whose strength lane is dropped ramps back to the strength
`light()` declared, which is what makes the lanes dials on a value
rather than a second copy of it, and why `light::Light` itself carries
no animation.

**There are exactly two write paths**: `Scene::render`, and the live
values a description's lanes are bound to. Nothing writes onto a retained
node from outside, and there is no `entt::registry` accessor: EnTT is
internal to `scene/` the way Yoga is internal to SigilCompose.

**Execution never reads the Element tree.** Extract is the one crossing:
it writes an entity's components — its placement, the mesh to draw, the
surface, its key and ancestry, its tags — and the draw and every pass
read those and nothing else. What a pass is handed is a `View`: a span
of `Draw` values, the lights, the viewpoint and the extent.

**A pass is never a scene child.** It is a stage of making the frame, not
a thing standing in the world, so it is declared on the `Frame` and never
under an `Element`.

**Nothing states an order.** A pass declares what it `reads` and what it
`writes`, and the graph derives the sequence, the barriers and the shared
surfaces from those declarations alone.

## Frames, passes and the ordering

A `Frame` is three declared things: the scene, an ordered list of passes,
and the readbacks — plus the two dials that say where the picture lands.
`extent(size)` is what its targets are made at, and a frame declaring
passes needs one; `camera(c)` is the viewpoint for a tree that declares
none of its own; `present(name)` names the resource the finished picture
is in, and an unset one means the last image any pass wrote.

`geometryPass(name)`, `computePass(name)` and `postPass(name)` each open
a pass, and each is a comparable value — a frame prunes on a pass the way
a tree prunes on a node.

| verb | what it declares |
|---|---|
| `reads(names…)` / `writes(names…)` | the resources this pass touches, by name. The first name it writes is what its stage paints into |
| `previous(name)` | that resource AS IT STOOD at the end of the frame before. It orders nothing, which is how a feedback loop is declared without a cycle |
| `only(Selector)` | which bodies the pass addresses — `sel::tag`, `sel::key`, `sel::under`, `sel::material`, composed with `\|`, `&` and `!` |
| `variant(Material)` | …drawn again in that surface |
| `realise(Selection)` | override how the selection reaches the pixels |
| `clear(SkColor4f)` | what a geometry pass clears its target to |
| `chain(geometry::mesh::pop::Chain, geometry::mesh::pop::Runtime)` | the points a compute pass cooks, into the point set it writes |
| `stamp(geometry::mesh::Mesh)` | the body a geometry pass stands at every point of every point set it reads |
| `blur(sigma)` / `levels(gain, lift, tint)` / `composite(mode, opacity)` | what a post pass does to what it reads |
| `body(…)` | THE ESCAPE: a callable handed the extracted `View` and the frame's `Targets`, which runs instead of the stage's own work and keeps its declarations |

**The order comes off the declarations.** Every resource has versions —
one per pass that writes it, in declaration order — and three edges
follow: a read runs after the write it sees, a write runs after the write
before it, and a write runs after every read of the version it replaces.
Among the passes whose dependencies are all met, the one declared first
runs first, so an order is a function of the declarations and never of
the machine — and a pass written down before its producer still runs
after it. A cycle is an error naming the passes on it, and no plan is
produced.

**A target has ONE geometry pass.** A geometry pass clears its target
and then paints, so a second one over a resource that has already been
written does not stand over that picture — it throws it away and keeps
its own bodies. That is refused while the plan is read, naming both
passes and the target, because the result of allowing it is a plausible
picture that says nothing about the one that went missing. Laying one
picture over another is what a post pass is, and a post pass may write
what a geometry pass wrote; a geometry pass carrying a `body` is outside
the rule, since a body runs instead of the stage and clears nothing.

**Two resources whose lives do not overlap share a surface.** A resource
lives from the step that first writes it to the last step that touches
it; the transients are given the lowest-numbered free surface, so a frame
pays for the most resources alive at once rather than for the number of
names. A resource that outlives the frame — read back, read as a
`previous`, or the one the picture is presented from — is never aliased.

**The barriers are a plan, not an API call.** One between each pair of
consecutive touches of a resource where either of them writes, and one
where a surface passes from one resource to the next. The CPU executor
performs the steps in order and needs none of them; the plan is built and
checked all the same, because an ordering that only states its hazards
where a device is present states them where they cannot be tested.

**How a selection is realised is inferred, and can be overridden.**

| the pass declared | what happens | why |
|---|---|---|
| nothing narrowed | `Selection::None` — every body | there is no selection |
| a geometry pass with `only` | `Cull` — only the selected bodies are drawn | a pass that paints bodies can simply paint fewer |
| a post pass with `only` | `Mask` — the picture stands everywhere and the op reaches it through coverage, which the graph makes the last geometry pass before it also write | a post pass has no bodies; it has pixels, and the selection has to arrive as pixels too |
| `variant(surface)` | `Variant` — the selection is drawn again in that surface | it is a re-draw by definition |
| `realise(…)` | exactly that | a pass that knows better says so |

A narrowed post pass with nothing painting bodies ahead of it is an error
naming the pass, because the coverage it needs cannot be taken.

`Scene::plan()` is the whole reading — the steps, the barriers, the
resources and their surfaces — and `Scene::error()` is what stopped it.

## What an executor performs

Two ship: `Runtime::cpu()`, which paints into raster surfaces and needs
no device, and `diligent::runtime(device)`, which rasterises on a GPU.
They are the same seam — a frame names one and every declaration around
it is unchanged.

Beside `execute`, an `Executor` is told when a frame opens and when it
closes. `beginFrame(targets)` is where one holding resources of its own
sizes them and makes what the frame before wrote into what this frame's
`previous()` names — which cannot wait until the frame ends, because
between the last pass and the next frame is exactly when the picture is
presented and a readback is taken. `endFrame(targets)` runs after
everything the frame read back has been taken, and is where such an
executor lets go of what the frame stopped needing. The CPU executor
needs neither.

### The CPU executor

`Runtime::cpu()` is the built-in `Executor`, and it paints into raster
surfaces:

- a **geometry pass** clears its target and paints the bodies its
  realisation leaves it, from the view's camera and under the view's
  lights, plus the stamps of every point set it reads;
- a **compute pass** cooks its chain on the `pop::Runtime` it carries
  into the point set it writes;
- a **post pass** takes its layers — the images it reads, then the
  images it named through `previous()` — softens, grades or lays them
  one over another, and writes the result. Masked, the first layer
  stands everywhere and the op reaches it only through the coverage.

A pass carrying a body runs that body instead, and the declarations
around it are unchanged.

### The device executor

`diligent::runtime(device)` performs the same passes on the device the
`diligent/` feature brought up. What it does with each:

- a **geometry pass** rasterises the bodies its realisation leaves it
  into a device texture, DEPTH-TESTED, from a pipeline built out of each
  material's own `Target::Slang` body. It draws in the back-to-front
  order the extracted view already carries and writes depth for an opaque
  body only, so a blended one is laid over what stands behind it; the
  order and the depth buffer agree wherever the view's centroid sort is
  right, and where it is not the depth buffer is the one telling the
  truth.
- a **compute pass** cooks its chain on the DEVICE when the whole of it
  can be, and on the host when it cannot. A pass carries the host runtime
  until it is given another, so a pass that named one of its own keeps
  it; otherwise `pop::deviceRuntime(device)` takes the cook, and only
  when EVERY operator in the chain has a kernel — a chain that would stop
  partway through is cooked on the host instead, whole, rather than
  declined. Either way the points are uploaded like any other geometry
  when a stamp is stood at them.
- a **post pass** is a shader pass: one triangle covering the target and
  one fragment stage per layer. A texture's origin is its top left and
  clip space counts y upward, so that triangle turns the vertical
  coordinate over — without which a chain of such stages would be right
  only when its length was even. A blur is a separable Gaussian in two
  draws through a working target, a grade is one draw, and a composite
  lays each further layer over the first under a blend state. Masked, the
  picture is copied first and the op reaches it through the coverage.
  A device cannot sample an image it is drawing into, so every stage that
  reads and writes at once takes a working target of its own — including
  a pass that declares it writes what it reads, which on the host is
  answered by taking the layers as snapshots first.

**A cooked artefact is named by a NUMBER, not by its address.** A
renderer holding buffers per geometry keys on `Draw::geometry`, which the
resource store counts up once for the process. An address cannot serve:
an artefact that is dropped frees its memory and the next one cooked can
land on it, and a count per store would hand two scenes' artefacts one
number.

**A STAMPED POINT SET IS AN ARTEFACT LIKE ANY OTHER.** A geometry pass
draws the stamps of every point set it reads, every frame, and forming
one costs the whole cloud times the stamp's vertices — so it is formed
ONCE per distinct (cloud, stamp) and uploaded once, and a set that has
not moved between two frames is neither instanced again nor re-uploaded.
`Targets::stamped()` is where it is formed and held;
`world::stampKey()` is the number the two values fold to, read from
their CONTENT because that is what "the same stamping" means — an
address cannot say it and a shape cannot — and the device tier keys its
upload by that same number. `Targets::stampings()` counts what has
actually been formed, which is what the test asserts does not move
across three frames of a still set. A stamping no pass asked for in a
frame is let go at the end of it.

**The pixels stay on the device.** The frame's resources are device
textures for as long as the runtime lives, and nothing crosses back until
something asks for a resource BY NAME — a declared `readback`, or the
picture being presented. That is what `Targets::source` is: a runtime
that executed elsewhere answers for one name at a time, so a frame that
reads nothing back pays for no crossing at all. With a source installed
`Targets::previous()` answers null and `endFrame()` keeps nothing,
because the executor that owns where the pixels are owns what last frame
means for them — the device executor keeps each resource's previous
texture beside its current one and exchanges the two when the next frame
opens.

**What a material reaches the device as.** A recipe's Slang body is one
function, `float4 surface(float2 uv)`, returning the surface's own colour
with straight alpha. The scaffold around it supplies the vertex stage,
the lighting and the premultiply, and every uniform — the recipe's
parameters and the scaffold's own — is written at the offset the compiler
REPORTED for it, so a body that declares one more parameter moves nothing
a renderer has to be told about. The variant axis is one bit, `kVariantLit`:
without it the lighting, the uniforms it reads and the loop over the
emitters are not in the compiled program. Which build a body is drawn
with is the body's own answer as much as the pass's — a surface that is
its own light takes the unlit one whatever the pass asked for. The mesh vertex layout is not a
variant axis, because there is one — position, normal, uv and tint, with
the lanes a mesh does not carry filled in on upload; nor is the blended
build, which is the blend and depth state a pipeline is created with.

A pipeline is assembled through the engine's own create-info builder,
and its blend and rasterizer states are the engine's named ones — a
premultiplied-alpha blend, an additive one, and blending off for a draw
that replaces what stands; solid fill culling back faces at the
counter-clockwise winding, or culling none. Two things stay written out.
The mapping from an `SkBlendMode` to one of those states is ours because
`SkBlendMode` is Skia's word and no Diligent type names it. And the depth
comparison is LESS-OR-EQUAL where both named depth states compare
strictly, so that a body redrawn over itself does not lose to the depth
it wrote the first time.

A material whose recipe has no Slang body is painted in the colour the
frame extracted — the same reading the CPU tier makes — and the program
cache has already reported the recipe and the target once.

**The map a body is dressed with** is the `material::Texture` in its
surface's base-colour slot, and both tiers sample it. It is read off the
material ONCE, at extract, so an execution reading a body never walks a
material tree; a mesh carries normalised uvs and a texture states its
placement in the image's own pixels, so the placement is carried across
rather than copied — inverted, because a texture's matrix puts the image
INTO the space it is sampled in and a lookup goes the other way, and
taken through the image's size, so a scale and an `at()` mean the same
thing and point the same way on a mesh as they do in a plane. On the
device the map multiplies the SHADED colour rather than the surface
before it, because the host tier's rasteriser can only modulate a texture
against the colour it already shaded, and a map that landed on one side
of the lighting here and the other side there would make the two tiers
different pictures.

The map is read through the sampler its texture asked for — one per
filter, made once with the device and picked per draw. Everything with no
texture to ask, a target a post stage reads among them, takes the linear
one.

**Every OTHER sampled slot the recipe declares is bound too** — normal,
roughness, metallic, occlusion, emissive, opacity — from the material's
own child slots, by the NAME the program declared them under. A slot
whose texture is the neutral dressing a surface is built with is left
UNBOUND, and an unbound slot reads one white texel: the neutral for every
map a scalar multiplies, and the one value a tangent-space normal cannot
mean, since such a normal's x and y are centred on a half and only its z
reaches one. So white IS "no map here", exactly and with no threshold to
pick — which is what lets a body tell a dressed slot from an undressed
one, and what keeps a surface nobody dressed the picture it already was.

**A body states what its surface IS.** The scaffold declares a set of
variables a body writes and it reads: `gSurfaceNormal` in tangent space,
`gSurfaceGloss` as a Blinn exponent, `gSurfaceMetal`, `gSurfaceRoughness`,
the three glass terms `gSurfaceTransmission`, `gSurfaceIor` and
`gSurfaceThickness` with `gSurfaceAbsorption` beside them, and
`gSurfaceReflection` for how an environment reaches the surface. Those
are the surface's standing whether or not a map varies them — a mirror
carrying no maps still has to reflect, and only the surface knows how
rough it is.

**A body may ask to be shaded again, per pixel.** The scaffold shades
per VERTEX, and one thing cannot survive that: a MAP that varies the
surface across a face. A body dressed with one raises
`gSurfacePerPixel`, and the emitter loop runs again where those values
can be seen. A body that raises nothing keeps the terms the vertex stage
interpolated, down to the bit. The tangent frame a normal map is authored against is
read off the screen derivatives of the view position and the uv, because
a mesh carries no tangent lane and every generator would have to fill
one.

## A 2D scene as a texture

A compose scene reaches a 3D surface as a `material::Texture` and by no
other door. There is no panel element, no card and no branch anywhere
here on "is this a scene": what a surface holds in its base-colour slot
is a texture value like any other, and the tiling and the placement reach
it as they reach any other image — with one wrap for both axes, because a
mesh's sampler has one and clamping the axis that was asked to repeat
would drag one edge's pixels across a whole face.

**World does not link SigilCompose and no world header names a compose
type.** The arrow runs the other way: `SigilComposeTexture` keeps a
composer and the surface it paints into, and hands out a texture value.
A host that owns both — a study, an application — makes the scene, hands
it the tree each frame, and puts the value it returns in a material slot.
That is the whole handoff.

**The device reaches those pixels through one narrow value.** A
`material::TextureSource` may answer a `DeviceImage`: the device that
owns the texture, and the texture as the graphics API's own object,
bridged to opaque values. SigilMaterial reads none of it. The device
executor here asks a map where its pixels stand, and when the answer
names the very device this frame is running on it wraps that image and
samples it where it is; when the answer names another device, or none,
it brings the source's `image()` over once and holds it under the id of
the image it came from. So a compose scene painted into a texture on the
shared device is sampled by a 3D pass with no copy in either direction,
and the two libraries still know nothing about each other.

## The host contract

`Scene::Impl` implements SigilCore's `ReconcileHost` operations on
itself. Operation by operation:

| core operation | what this host does |
|---|---|
| `keyOf` | the description's `key` |
| `equal` | `propsEqual` — every field of `ElementNode`, with the geometry slot's variant equality standing in for a kind comparison |
| `reconcilesChildren` | true: children are described, never filled by another path |
| `children` / `descriptionOf` | the description's `children`, and the node handle off each `Element` |
| `memoOf` / `produce` | the description's `Memo`, and the deferred describe run under the environment its author had |
| `create` | a node, an entity with a `Placement`, and the first patch — with the child's ordinal read through the parent's `staggerChildren()` schedule, so the entrance the patch mounts is delayed by where this child sits in the cascade |
| `onPatched` | retargets the lanes (mounting entrances on the first patch, at whatever the enclosing cascade delayed this branch by), marks the geometry slot for resolution when it or its window changed, and stales every bake above |
| `reorder` | stales every bake above when a child mounted, unmounted or moved |
| `remountRequired` | **false, always** — nothing a node retains is welded to what its slots hold |
| `invalidate` | stales every bake above |
| `destroy` | destroys the subtree's entities and releases its resource references |

**Entrances cascade.** `Element::staggerChildren(motion::Spread)` puts a
schedule on a node, and each child that MOUNTS enters at the start time
that schedule gives its ordinal — an even ladder, a fixed total divided
across however many children turn up, an irregular cue table, one of five
orderings, a distribution curve. It is SigilMotion's schedule, the same
body a paragraph's glyphs cascade through, so `From::Center` means one
thing in a set and in a line of type. The delay compounds down the
subtree and only children that actually mount are delayed: appending one
node to a live list enters it at once rather than making it wait out the
whole list.

**Lanes and the values on them are SigilMotion's.** `Lane`,
`retargetSlots`, `mountEntrance`, `isLive` and the comparators that decide
two animatable slots are the same live in `<sigilmotion/values/…>`; this
library names the FAMILY (`LaneFamily::Slot`), the 27 rows, and what each
row's standing value is when a description does not carry the block that
holds it.

**Phases** are declared through `core::Phase` and run by `core::runPhases`:
`describe` → `lanes` → `derive` (converging) → `extract` → `graph` →
`execute`. Describe reconciles the tree the author handed over; lanes
sample every binding once; derive resolves placements top-down and
converges because a node's placement is read by everything under it;
extract is the one crossing into the state a draw reads; graph turns the
frame's declarations into an order and gives its resources surfaces; and
execute performs that order. The last two do nothing for a frame that
declared no passes.

**The caching proof** rides extract. Each node declares what moves —
a bound or ramping placement lane is composite motion, a bound window
lane rebuilds the geometry, a live material rebuilds the surface, and a
generator that cannot say whether it is the same generator is volatility
no value comparison can see — and `core::foldSubtree` answers what the
subtree promises. A `core::Settle` over the node's sixteen placement
floats is what separates "a binding is connected" from "the value is
moving": once a placement resolves identically for three frames the node
stops declaring the motion, and the frame it moves again it re-declares
before anything holding its old reading replays.

That re-declaration is the hold's rescan side, and it runs in the phase
runner's settle hook — between the converging rounds, so derive has
written the new placements and extract has not yet read an artefact. It
visits every node rather than only the ones the proof released, because
a bake here is decided on declarations alone: a node with no lane of its
own declares no placement motion and takes an artefact whether or not
its hold has warmed up, and an ancestor's lane can move it afterwards.

**The bake's one tier is a draw order**: the entities a settled subtree
contributes, recorded once and replayed until something in it moves. The
artefact carries each entity's placement, so it is asked for on
`volatileAbove` rather than `subtreeVolatile` — a node whose own
placement moves cannot replay one any more than its ancestor can — and
one artefact covers a whole settled subtree rather than one per node.

## What the CPU tier can and cannot say

`Scene::draw` runs on `geometry::mesh::render::Runtime`, whose built-in
executor shades on the CPU. That executor's shading is directional and
per vertex, so:

- a `material::Material` is carried and compared in full, and the tier
  reads its `baseColor` field when the recipe declares one, plus the
  `material::Texture` in its base-colour map slot. A recipe's body is a
  program, and the CPU tier has no compiler to run one.
- a STACK of surfaces — `material::over` — reaches this tier as the
  surface at the BOTTOM of it, because the mask that decides where the
  top shows is a program too. Both the colour and the map are read
  there.
- a sun reaches the shading as itself; a point or spot light reaches it
  as the direction from where it stands toward the origin, at the
  strength it has there. The full falloff is `light::attenuation`.
- bodies are sorted back to front by view depth, stably, so two at one
  depth land in tree order.
- a surface that says light does not reach it — `material::kit::unlit`
  — is drawn unshaded, here and on the device alike: what it shows is
  its base colour and the mesh's own tint, with no ambient under it and
  no emitter, specular or rim over it. The answer is read off the
  material once at extract, beside the map, so it is a property of the
  BODY and not of the pass that draws it.
- a texture states how it is read BETWEEN texels and both tiers honour
  it. Nearest keeps a texel's edge hard and takes no mip level with it,
  because blending two levels is the same bleed arriving by the other
  door; linear reads between texels and between levels.
- the lit sum ends at the same TONE CURVE the device's does, at the same
  exposure, and so does the sky this tier paints. The curve is
  transcribed here rather than shared, on the same terms as every other
  shading term — one arithmetic, two spellings, each pinned by its own
  test.
- an ENVIRONMENT MAP reaches this tier in full, and its terms are the
  same arithmetic the device evaluates: the panorama's cosine
  convolution replaces the flat ambient, the split sum adds what the
  surface mirrors off the reflected view vector, a metal takes the light
  out of its diffuse, and a crossfade samples both maps and mixes. What
  differs is the RATE. This tier evaluates them once per vertex and Skia
  interpolates between, so a coarse mesh under a bright sky reads as
  facets where a device reads as a curve, and the two tiers' plates are
  compared within a ceiling that says so. The surface's metallic and
  roughness are read off the material's params by name, one number over
  the whole body: there is no per-pixel half here and no map is sampled
  for either.
- GLASS is where the two tiers part company most. `transmission`, `ior`
  and `thickness` reach the device and not this tier, because a
  refracted ray is a per-pixel question — a per-vertex one would bend
  the sky at four corners and interpolate a colour across the middle,
  which is not a picture of anything. A glass body here is its diffuse
  and its reflection.

That is what a machine with no Vulkan runtime can honestly answer, and it
is what the plate ledger's 3D tier is judged on. It is not a substitute
for a device.

## Studies

A study is one 3D frame, stepped from zero at a fixed 1/60 to its
declared moment and photographed — so a plate is a function of the
declaration alone and never of how fast the machine ran.

A study is a **sketch**, and the harness that runs one belongs to
SigilSketch rather than here: this library draws a frame and says nothing
about how a frame is photographed. `src/sketch/README.md` is the canon
for the registry, the live host and the plates.

```sh
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
    --headless <outdir> --kind set [--sketch <name>] [--gpu]
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook --list --kind set
```

`--sketch` takes a case-insensitive substring, which is the loop for
visual iteration. A study joins the registry by being a file in
`src/sketch/sketches/`.

`--gpu` renders every study through the device runtime instead. A study
that declared no passes is wrapped in one geometry pass clearing to its
background, because an executor is only reached through passes and a
study about the scene must be able to say what it looks like on a device
too. The flag answers with the device or with nothing: on a machine with
no Vulkan runtime it reports that and fails, rather than quietly putting
the CPU's plate under a name that asked for the device's. The device is
brought up by the BINARY and installed once for the process, so no
feature here but `diligent/` links one, and a machine with no GPU still
renders the CPU tier.

Fifteen sketches draw through the Set runtime, and between them they
exercise every feature this library has:

- **`first_light`** — the scene: a tube swept along a closed loop, a
  comet of stamps riding a moving window of that same loop, a plate under
  both, a sun and a lamp, and a camera on a rail of its own.
- **`glow_trail`** — the passes. Its set is drawn once, and what is
  tagged "glow" is then reached three ways, one per realisation: a
  narrowed post pass lifts the beads in place through the coverage the
  geometry pass before it was made to write; a narrowed geometry pass
  draws the same beads alone into a target of their own, which is
  softened and dimmed; and that is laid over its own output from the
  frame before, so the comet drags a tail no single frame contains. Six
  passes, no stated order, and three of its surfaces are taken in turns.
- **`material_lab`** — what a surface is made of, and the difference
  between the tiers. Five curved cards over a floor wearing a texture
  set: one plain, one a STACK of two through a mask, one wearing a normal
  map, one wearing a packed roughness-and-metallic map read at two
  channels, and one that emits in a pattern. Every card is chosen because
  the device SHADES it, so the device plate is what the params and the
  maps say; the CPU plate is five flat colours and the floor's weave,
  because that tier reads a base colour and a base-colour map and nothing
  else. The cards are curved rather than flat, because a Blinn highlight
  on a flat card is one value over the whole face and a card meant to
  show a highlight narrowing has to present a range of normals to the
  key. The turntable is PARKED: a lab is read rather than watched, so the
  live picture and the plate are the same picture. The texture set is
  GENERATED in the study rather than read off
  the disk, through the same `texture::` door a scanned folder arrives
  by: a plate is a function of the declaration, and what a machine
  happens to have under `build/assets` is not.
- **`scene_surfaces`** — a compose scene as an ordinary texture, and
  every sampling dial applied to it. Three flat cards on an arc, one
  curved band under them and one swept ribbon whose card repeats along
  the band's length each wear a compose tree rendered by a composer of
  its own; the screens are unlit, so what they show is what the trees
  painted, and the ribbon is a lit surface, so the same texture is read
  through shading beside them.
- **`reflection_lab`** — what a body sees when it looks past the lights.
  Four spheres in a row under a sky — chrome, a rough metal, a
  dielectric and glass — each legible only because of the environment
  map; the sky is a node whose `rotateY` turns the reflections while the
  lights and bodies stand still, and the row stands under a held
  crossfade of two panoramas.
- **`set_stagger`** — the entrances of a set's children, cascaded, and
  the two selectors that address a subtree afterwards. Two rows differ
  only in their spread's origin, so at one moment they hold different
  shapes of the same cascade; `sel::under` and `sel::material` narrow a
  pass to one of them.
- **`key_light`** — the emitter's dials. One still set under the kit's
  three-point rig, with the key light's strength and colour bound to live
  values: nothing about the description changes from frame to frame, and
  what moves is what the lanes are bound to.
- **`dart_flight`** — `along()`, and nothing else. One winding closed
  loop swept into a rail, a dart flying it at a distance that is a
  function of the scene time, and gates standing on the same loop at
  constant distances and rolled about it — one verb serving a moving
  body and a still one, and composing with the rotation lanes rather
  than replacing them.
- **`scattered_model`** — the import door. A model decoded through the
  mesh codec and fitted to the stage, with a cloud scattered over its
  surface and a flake stamped at every point; the codec's output is the
  same `Mesh` a generated body is, so the tree past the import cannot
  tell which it holds. It reads `res://models/`, which nothing in this
  repository mounts, so what a plate is taken from is the generated
  subject.
- **`deformed_cloud`** — the point operators, in a room. A band across a
  scattered body is selected once and addressed twice: the points inside
  it are pushed out along their own normals, and inverting the same
  region turns everything outside it about the up axis. The chain is a
  value the node carries and the frame's runtime cooks it.
- **`lantern_room`** — the three emitters together. Four unlit lantern
  shells each carrying a coloured point light, a spot opening downward
  onto the cluster between them, and a sun faint enough to be an
  outline. An emitter stands where its node stands and carries no
  geometry, so a lantern here is two siblings sharing a placement.
- **`compute_variant`**, **`import_native`**, **`vagrant_story_target`**
  and **`world_hud`** — the pass verbs that draw nothing by themselves
  (cooking points, re-drawing a selection, asking for a resource back),
  the zero-copy import door, and two studies that hang a compose overlay
  on an unlit quad filling the frustum over a lit set.

Most of them are built out of `kit/`: `kit::threePoint` puts three
emitters round a subject in its own extents, `kit::turntable` rides a
closed rail looking inward, and `kit::litSet` is both over a ground
plane. Every one returns an ordinary `Element`, which is why `key_light`
can take the rig the preset returned and put lanes on its key light
without the preset offering a hook for it — and why `lantern_room`,
which lights its own room, takes the turntable alone and leaves the rest
of the preset behind rather than describing a second room over the top
of its own.

A study returns a `Frame`, and an `Element` is one with no passes, so a
study about the scene says nothing about passes at all. The host writes
the plate's size and its viewpoint into whichever it was handed.

## The plate ledger's 3D tiers

`scripts/plate_ledger.py --tier world` renders every study to its
declared moment on the CPU and hashes the bytes against its own baseline,
`build/plate_baseline_world_<config>.sha256`. It is the same question the
2D tiers ask — did any byte move that I did not mean to move — of a
different registry, and it needs no device:

```sh
python3 scripts/plate_ledger.py --tier world --rebase   # adopt a baseline
python3 scripts/plate_ledger.py --tier world            # sweep and judge
python3 scripts/plate_ledger.py --tier world --stability 2
python3 scripts/plate_ledger.py --tier world-gpu        # the device tier
```

`--tier world-gpu` renders the same studies through the device runtime
and is the ONE TIER NOT JUDGED ON BYTE IDENTITY. It has no baseline: each
plate is compared against the CPU tier's plate of the same study, and the
same sweep renders both. Two rasterisers are not asked to agree bit for
bit — the host paints shaded vertices through a per-triangle sort with
Skia's antialiasing, the device rasterises the same shading through a
depth buffer with none, and a blur is a box approximation on one side and
a Gaussian on the other. What is measured instead, per colour channel in
0..255 over every pixel, is the MEAN absolute difference (which says the
two are the same picture), the 99th PERCENTILE (which says the
disagreement is confined) and the WORST channel — which is an edge, or a
body a centroid sort ranked wrongly on the host and a depth buffer ranked
rightly on the device, and is reported rather than judged. Each study names its own mean and p99
ceilings in the script, set from what the two tiers do rather than from a
wish. With no device the tier reports that and exits green, because a
machine with no Vulkan runtime has nothing to disagree about.

### The mesh painter on the device

`diligent::painterRuntime(device)` is a `geometry::mesh::render::Runtime`
whose executor draws on the device: one pipeline over the mesh's
vertices, the style's three modes as a uniform rather than three
programs, the shading per vertex in view space exactly as the host
executor's is, the primitive lane multiplying the shaded colour, and the
texture read through the sampler its placement, its wrap and its filter
ask for. The pixels are then READ BACK and drawn onto the canvas the
caller passed, premultiplied and under whatever transform that canvas
carries.

**It is a readback, and this page says so rather than implying
otherwise.** A canvas does not name the texture behind it, so there is
nothing to compare against this device to decide that the pixels could be
bound where they stand — the zero-copy path SigilSkia offers needs a
caller holding both the surface and the device, and a `geometry::mesh::render::Executor`
is handed neither.

**Each mesh draw is a device frame of its own**, because the heap a
draw's uniforms are written into is refilled once a frame. The command
context is shared with every other runtime on the device, so a draw taken
from inside a frame's pass body would close that frame early; a canvas
draw stands between frames, which is where this belongs.

**A PANEL draw is the canvas's own.** Both executors concat the same
perspective transform and hand the canvas to the caller, because that
content is Skia's to rasterise and a panel on a GPU-backed canvas is
already on the GPU. The two are therefore the same BYTES for a panel, and
the test says exactly that rather than measuring a distance.

**How far this stands from the host executor is not asked in a test.**
The host sorts triangles back to front and antialiases their edges, this
depth-tests them and does not, so the two draw the same picture and not
the same bytes; what a whole-picture comparison of them is worth depends
on the subject, which is a scene's property rather than this code's. That
judgement belongs to the plate ledger's device tier, which makes it
against a committed baseline. `painter_gpu` and `floating_panels` stand
a mesh on a canvas through `painterRuntime`, and `--gpu` rasterises a
canvas sketch's mesh painter on the device, so the quick tier's plates
of those two are what put the mesh painter under that judgement.

### The swept rings on the device

`pop::sweepDeviceRuntime(device)` is a `pop::SweepRuntime` whose
executor forms a sweep's ring vertices on the device: the rail and the
profile uploaded, one compute dispatch, both output lanes read back in
one crossing. Everything else a sweep is made of stays on the host and is
not a second piece of arithmetic — the quads, the cap fans and the
geometric averaging are integer or a reduction over triangles the
vertices have to exist first for, and a TAPER is an arbitrary host
function evaluated once per ring and carried across as the number that
ring scales by.

**The two tiers are held to BIT IDENTITY**, on the same three pins the
point operators stand on, and for the same reason: the ring vertex is one
piece of Slang compiled twice. SigilGeometry's `mesh/pop/test/DeviceSweepTest.cpp` is the
conformance — every normal rule, on a closed loop and on an open arc,
with a round profile and a flat one, swept both ways and compared bit for
bit.

A device that refuses the kernel forms the rings on the host instead.
That is honest precisely because the two answers are the same bits: where
the vertices were formed is not what they are, which is the one thing a
caller holding a runtime must not have to check for.

**World's own geometry slot has no swept kind**, so nothing in `scene/`
reaches for this: a sweep is formed by whoever describes the geometry,
and a host that holds the device puts the runtime in the `SweepOptions`
it sweeps with. A `Chained` slot is the one that carries a runtime,
and the device executor swaps the host pop runtime into it when the whole
chain has kernels.

### The point operators on the device

`pop::deviceRuntime(device)` is a `pop::Runtime` whose executor cooks a
chain on the device: the chain's generator is run on the HOST and its
lanes uploaded — a generator makes the points rather than mapping over
them, and a seed that differed would make every comparison after it
meaningless — and every operator after it is one compute dispatch over
those lanes, in chain order, with the cooked lanes read back once at the
end. One buffer per lane, every one writable, because a filter that edits
a lane in place is one resource read and written by one dispatch rather
than the same memory claimed two ways. Between dispatches every lane is
transitioned from its state to itself, which is the barrier: nothing else
about the bindings tells the driver that the next operator reads what the
last one wrote.

Thirteen operators have kernels — `Jitter`, `Ramp`, `Vary`, `LookAt`,
`Math`, `Fill`, `Atlas`, `Lookup`, `Select`, `Affine`, `Peak`, `Mix` and
`Normal` —
and the runtime's `supports()` answers from `kernel::has()` rather than
from a list of its own. What it declines it declines by name, the way any
unsupported operator stops a cook: `Relax` reads points it does not own,
`Sort` is a permutation, `Promote` addresses primitives no sink has
formed yet, and `Noise` and `Deform` are defined in terms of a library
sine, which is a different function from the polynomial a portable kernel
would have to use.

**The two tiers are held to BIT IDENTITY, not to a distance.** That is
the one place in this library where two backends are, and it is possible
only because the operators are one piece of arithmetic compiled twice
under a float model pinned at both ends. Three things pin it, and each of
them is load-bearing: the generated C++ is compiled with
`-ffp-contract=off`; the SPIR-V carries one `NoContraction` decoration per
arithmetic result, which the emitter does not put there; and
`MVK_CONFIG_FAST_MATH_ENABLED` is set to 0 before the Vulkan instance
exists, because this driver otherwise takes a square root as an
approximation and a divide as a reciprocal and a multiply. Remove any one
and SigilGeometry's conformance test — every supported chain cooked both
ways and compared bit for bit — fails on the first expression of the
shape `a + b * c`.

The last of the three is DEVICE-WIDE and it is not free: the graphics
pipelines pay it too, and a frame that leans on the post stages is
measurably slower for it (the bench ledger owns the number). It is set
with `setenv(..., overwrite=0)`, so a process that has already put
`MVK_CONFIG_FAST_MATH_ENABLED` in its own environment keeps whatever it
asked for — which is the way to buy the faster device back, at the cost
of a device pop cook that no longer answers what the host answers.

## What the DEVICE tier can and cannot say

The twin of the CPU tier's paragraph above, for the same scene shaded on
a device. The lighting is directional, per vertex, and the model is
ambient plus Lambert scaling the surface colour, plus a Blinn highlight
and a rim term that nothing scales. So:

- a recipe's `Target::Slang` body IS run, which is the whole difference
  from the CPU tier: the parameters, the sampled slots and the arithmetic
  a material describes reach the pixels. A recipe with no Slang body is
  painted in the colour the frame extracted, the same reading the CPU
  tier makes.
- a STACK of surfaces SHADES as a stack. `material::over` composes one
  Slang body out of its three operands' own bodies, so both surfaces are
  evaluated and their colours mixed by the mask, and what each of them
  said per pixel — a normal, a Blinn exponent, a metal weight — is mixed
  by the same coverage, so a top wearing a normal map bumps the surface
  only where the mask lets the top show. A stack whose operands do not
  all have a Slang body is not composed and reaches this tier as the
  surface at the bottom of it, the way the CPU tier reads one.
- **a stack running its own body owns every map in it.** The frame
  extracts the map of the material at the bottom of a stack, because
  that is what a tier with no compiler can answer with; where the
  composed body IS run it samples both operands' maps itself, through
  slots of its own, and the scaffold is handed no map at all — otherwise
  the bottom's would land a second time and over the whole face rather
  than where the mask says.
- the occlusion, emissive and opacity maps reach the pixels through the
  kit's own body: occlusion darkens the albedo at its strength, emission
  is added at its own colour and strength, and `alphaCutoff` turns the
  opacity map into a CUTOUT — below the threshold the surface is absent
  rather than translucent.
- the normal map perturbs the shading, and the shading is evaluated again
  per pixel **where a map varies the surface across a face, or where the
  set carries an ENVIRONMENT MAP**. The first is not a shortcut: a
  surface whose roughness is one number over the whole of it is already
  what a per-vertex shading says it is. The second is not optional: a
  reflection is a function of the view vector, which turns under every
  pixel of a curved body, and a per-vertex one reads as facets. Where the
  shading is evaluated again, roughness sets the Blinn exponent — the
  mirror end of the range a narrow highlight, the rough end a wide one —
  and metallic takes the light out of the diffuse term and puts the
  surface's own colour into the highlight.
- **with an environment map the model has a Fresnel and an environment
  term**, composed from the material kit's shading terms: the flat
  ambient constant is replaced by the panorama's cosine convolution
  sampled by the normal, and the split sum — prefiltered radiance times
  the surface's own reflectance and its Fresnel — is added for what the
  surface mirrors, off the reflected view vector at the level its
  roughness picks. `transmission`, `ior`, `thickness` and the medium's
  absorption reach the shading too: the refracted ray reads the same
  panorama, attenuated by Beer-Lambert over the thickness it crossed,
  and Fresnel decides how much of the light went that way. That is glass
  against the WORLD; what stands behind a body ON SCREEN is a backdrop
  pass and not a shading term, and there is none.
- **the lit sum ends at a TONE CURVE, at the set's exposure.** A
  panorama holds values far above one — that is what makes a sun a sun
  rather than a white disc — and every lit sum carries them through, so
  cutting it off at one would flatten every highlight to the same white
  and lose exactly the range the map is kept in floating point to hold.
  `kit::termsSource`'s `toneMap` is what runs instead, on both tiers and on the
  sky pass alike: the radiance times the environment's `exposure`,
  divided by one plus its own luminance. A surface that is its own light
  and a coverage mask are drawn with the unlit build and are not curved:
  their colour is authored, not integrated.
- **it is still not a path tracer's answer and this page does not call it
  one.** There is no importance sampling, no multiple scattering and no
  shadowing between bodies; the prefilter is nine box-blurred levels
  rather than a GGX convolution, and the split sum is an analytic fit of
  the integral rather than a lookup table. What is implemented is the
  arithmetic above, and a metallic-roughness texture set therefore reads
  as a plausible surface rather than as the one a renderer with those
  three would produce from the same params.
- a foreign texture — one another engine, a decoder or a capture painted
  with the graphics API — reaches a slot through
  `diligent::importNative`, and is bound where it stands. It answers no
  host image at all, so a renderer on another device draws the body
  undressed rather than something it invented.

- **the sky SHOWN behind the set is `Backdrop`**, drawn on both tiers as
  one triangle over the target with each pixel reading the panorama
  along the ray the eye looks through it, at the backdrop's strength and
  blur. Past a `groundRadius` of zero the panorama is projected onto a
  sphere of that radius centred at `projectionCenter`: the pixel reads
  where its ray leaves the sphere, along the direction from the centre
  to that point, so an eye moving through the set sees the horizon shift
  the way it would outdoors. An eye at the centre, or on or outside the
  sphere, reads by direction — the sky at infinity, which is what a
  radius of zero means. The projection reaches the backdrop alone: what a
  surface mirrors stays at infinity.

## What the environment map does not reach

- **Glass refracts the world and not what is behind it.** A refracted
  ray reads the panorama and never the colour target, which is right for
  a body with sky behind it and wrong for one with another body behind
  it. Screen-space refraction wants the colour target as it stood before
  the body was drawn, which is a pass that reads what another pass wrote
  — an order the frame graph can express.

### Where the shaders come from

The shader modules under `diligent/shaders/` are compiled TWICE. `slangc`
compiles each when this library is built — which is what makes a mistake
in one a build failure rather than a first-frame surprise — and the build
also embeds each module's text in this library's archive, because the
source a material's body is appended to cannot be finished until the
material exists, and a shader that had to be found on disk at run time
would be a second way for a build to be incomplete.
`<sigilshaders/WorldDiligent.h>` is how this backend reads its own text
back. At run time the scaffold's text, a recipe's generated
declarations, its body and one fragment entry point are assembled into
one module and compiled through `material::slang::compileModule`, which
is also what reports every uniform's offset. `Programs.h` is where this
backend's own four programs live — the scaffold in its lit and unlit
builds, the sky, the mesh painter and the post stages — each compiled
once for the process; `installSlangCompiler()` registers the one that
appends a recipe's body to the scaffold, because only this backend knows
what that scaffold is.

Neither `Portable` nor `Shading` is this library's module. `Portable` is
SigilMaterial's Slang backend's — the subset one source can be compiled
twice from and still answer once: arithmetic plus the operations IEEE 754
pins exactly, with `sqrt`, `dot`, `length`, `mix`, `smoothstep` and the
trigonometric functions written out, because a library intrinsic is two
different pieces of code on two targets. `Shading` is the material kit's
shading TERMS. Both are loaded into every compiler session by name — out of the
archives that own them, never out of a directory — so the scaffold's
shading and every material body compiled beside it call one definition of
a term rather than a copy apiece; the build-time compile reads the same
files on disk, which is why `slangc` is pointed at both directories. Slang emits no contraction decoration in its SPIR-V, so
a driver is free to fuse a multiply-add inside a module compiled here; a
kernel that needs the unfused answer has to reach the same result without
depending on it.

### The one device

**The device is not made here.** Diligent creates the Vulkan device and
cannot attach to one that already exists, so the single point where a
device is made has to sit at or below every consumer of one — which is
SigilGeometry's `device` feature, and its README is canon for what a
device is, how it is adopted and what the shared queue's lock rules are.
This library takes one and executes a frame's passes on it:

Bringing the device up — `DeviceConfig`, `Device::create` and the
`error` it fills when there is no Vulkan runtime, the Diligent side that
is never null on a created device and the adopted Graphite side that is
null when adoption failed — is SigilGeometry's, and its README's device
section shows it. What this library adds is one line over that device:

```cpp
#include <sigilworld/diligent/Runtime.h>

world::Scene scene(world::diligent::runtime(*device));
```

There is no Metal path here, because Diligent has no Metal backend:
`create` fails on a machine with no Vulkan runtime, and the answer for
such a machine is the CPU executor, not a second GPU path.

## Testing and benchmarks

```sh
ctest --test-dir build -C Release -R world_
```

A case here asserts one thing this library promises through its public
headers and is named that promise as a sentence, so a failure line reads
as the claim that broke. It pins only what editing this library could
falsify — a comparator's field walk, a derived ordering, a closed form,
one description drawn two ways — never an anti-aliased byte, a fitted
tolerance or elapsed time: how close two rasterisers stand is the plate
ledger's to judge and how long anything takes is the bench ledger's. A
claim made N times with one thing varying is one `TEST_P` whose
parameter is that thing, with a name per row. **A binary exists where it
links a strictly smaller set of targets than its neighbours and that
boundary is a promise somebody could read**; two binaries over one
closure are one binary.

**The fixtures every one of these binaries shares live in `test/`**, and
there is one: `test/TestMaterial.h` — the throwaway comparable surface a
test paints with, in a plain build and a Slang-bodied one, the camera
square on to the origin at whatever distance the case wants, and the
half-plate ink count a selection is read by. A test target adds that one
directory and includes the header by name; no library's include path
carries it, so nothing shipped can reach a fixture. A body that needs a
shape takes SigilGeometry's `quad()` rather than building one, and the
one hand-built mesh left is a single TRIANGLE, kept because a stamp
standing at every point of a cloud is counted in triangles.

| binary | what it proves | label |
|---|---|---|
| `world_element_test` | the description and the emitters it carries | — |
| `world_frame_test` | the declarations and the CPU executor | — |
| `world_graph_test` | the ordering derived from the declarations | — |
| `world_scene_test` | the retained side | — |
| `world_kit_test` | the presets | — |
| `world_diligent_test` | the device executor | `gpu` |

`world_element_test` covers the description: copy-on-write, the
structural prune **field by field as one `TEST_P` whose parameter is the
field**, each said two ways, so every row shows a field to tell two
values apart as well as to be in the comparison at all — the geometry
slot's value type standing in for a kind, the lane list including the
emitter rows standing where the emitter stands, the cook, the selectors,
and the emitter values themselves: what each factory fixes, the windowed
falloff reaching exactly zero at the range, and the spot's cone. The
emitters are here rather than in a binary of their own because this
target links the light feature — every target that reaches the emitters
reaches the description too, so there is no boundary for a second binary
to draw.

`world_kit_test` covers the presets: what tree each returns, that the rig
is stated in the subject's own extents and puts every lamp at the subject
when there are none, that a whole turn of the turntable is where it
started and a rail asked for fewer than three stations is still a closed
loop, that the wave alternates between its two radii and two heights
round its centre and the winding stays on its shell while crossing its
own plane twice a wrap and turning the laps it was asked for, and that
the one colour this library states is the ground's.

`world_scene_test` covers the retained side, every case over one fixture
holding a clock and a scene reading it: an emitter dial reaching the
light it scales while the tree stands still, identity across a keyed
reorder, the three lifetimes pulling apart under a geometry-slot change,
the store sharing one cooked artefact, a lane ramping a placement, the
bake taken once and lost to a driven lane below it, a culled pass and a
narrowed post pass each reaching only their selection — read off the
pixels, since which realisation the ordering DERIVED is the graph
binary's claim rather than this one's — and a draw that is a function of
the description alone.

`world_frame_test` covers the declarations and the CPU executor without
anything retained: a pass compares field by field, a mask realisation
writes the coverage and a variant realisation redraws the selection in
its surface, a post pass reads what stands and what stood last frame, a
compute pass cooks, a still point set is stamped once however many frames
draw it, two names on one slot share the surface, and a declared body is
handed the extracted view. It is handed the realisation rather than
deriving one.

`world_graph_test` covers the ordering: the order from the declarations
and its independence from the order they were written in, a cycle named,
`previous()` breaking one, the surfaces counted and shared, the hazards
stated, and **every selection realisation as one `TEST_P` whose parameter
is the declaration** — a narrowed geometry pass culled, a pass that
narrows nothing addressing every body, a narrowed post pass masked, a
narrowed pass carrying a surface redrawn in it, and a pass that says how
it wants to be realised overriding the rule. The coverage a masked pass
reads and the pass ahead of it writes is its own case beside them.

`world_diligent_test` covers the device side. Every case reads this
feature through its public headers alone — the source directory is not on
the binary's include path — so a claim about a compiled program or an
uploaded map is a claim somebody outside can make: how deep a chain a map
of a given size is uploaded with is `mapMipLevels`, asserted as a closed
form with no device in reach. `diligent/test/DeviceSeams.h` holds the two
seam values that stand on a device, the two cameras every case looks
through, the card it photographs, the texture the 2D path paints on the
device, and the worst channel two plates differ by; **the device itself
is SigilGeometryDevice's**, whose `test/support/OnDevice.h` brings up ONE
for the process, because that library is the one point in the tree where
a device can be created at all.

`diligent/test/PainterTest.cpp` is the mesh painter: the runtime as a
value, a surface that is its own light standing brighter than a lit one
on both executors, and a panel that is the same bytes on either.
`diligent/test/SurfaceTest.cpp` is the sampled slots and the import door —
an occlusion map darkening only where it is dark, an emissive map
carrying its own colour, a cutout dropping texels outright, a normal map
tilting the two halves of one flat card apart, a surface dressed with
white in every slot being the same picture as one dressed with nothing, a
texture painted with the graphics API on this device coming in through
`importNative` with no host image at all — so a picture carrying its
colour cannot have come from a copy — standing where a raster one of the
same colour would, and an import of nothing answering no texture rather
than one that lies. `diligent/test/StackTest.cpp` is what `material::over`
composes for this target: that the composed recipe compiles, and that
what it shades where the mask is half is a picture neither operand alone
produces. `diligent/test/RuntimeTest.cpp` covers the frame: a pipeline
off a recipe's Slang body with its parameter at a reflected offset and
the lit build carrying shading the unlit one does not, a cooked chain
that matches the host's cook exactly, a readback that arrives the frame
after, a masked pass that lifts the selection and leaves the ground where
it stood, the mip rule, and a map whose pixels already stand on this
device being bound where they are. **A claim a picture has to make
whichever rasteriser drew it is written once with the TIER as a
parameter** and answered on both: the map a body is dressed with reaching
the pixels, a nearest-filtered map being two colours and one edge, a
linear one being a gradient, a map asked to repeat being as many of
itself as it was asked for, and a surface that is its own light standing
at its base colour while a lit one of the same colour under a sun aimed
away stands darker.

That every recipe this repository ships compiles is not asked here:
`material_slang_test` compiles the kit's own surfaces through the same
backend and `material_gpu_test` draws every recipe the material library
ships on a device, so a sweep here would be a third reading of one fact.

**How far the two tiers stand apart is asked nowhere in these binaries.**
Two rasterisers are not the same bytes, the distance between them is a
different number per subject, and it moves with the scene rather than
with this code — so it is judged over the whole registry against a
committed baseline by `plate_ledger.py --tier world-gpu`, and the only
distance a test here reads is the worst channel, as an INEQUALITY saying
an operation reached the pixels at all. The conformance of the chain cook
and the swept rings is not here either: those executors are
SigilGeometry's, and `geometry_mesh_pop_test` is where every chain and
every sweep the device says it can do is done both ways and compared bit
for bit.

**What `world_diligent_test` checks on a machine with no Vulkan runtime**,
which is why it carries the ctest label `gpu`: the Slang compile of a
recipe's own body, the mip rule, and the host half of every claim written
over the tier parameter. Every other case skips, and a skip is not
coverage: `ctest -L gpu` is the run a device verdict may be read out of,
and a run that excludes the label has asked the device nothing. A device
wants `brew install molten-vk vulkan-loader`. No other binary here skips
or vanishes, and none of them needs a font or a network.

Nothing puts a world source directory on the kit's include path, so the
retained side's own header is unreachable from kit code — which is what
makes "the kit sees public headers only" a property of the build rather
than a convention.

`world_element_bench`, `world_frame_bench`, `world_graph_bench`,
`world_scene_bench`, `world_light_bench`, `world_kit_bench` and
`world_diligent_bench` build through the `benches` target and run through `scripts/bench_ledger.py`;
use a Release build. The device bench measures the four costs a device
has that the host does not: turning the device Diligent made into a
device both APIs draw on, turning a recipe's Slang body into a program, a
steady frame with every pipeline and every mesh already uploaded, and a
point-operator chain cooked on the device — readback included, because a
cook whose answer nobody could read would not be a cook.

Two costs on the way to a first frame are REPORTED THERE AND NOT TIMED,
as Google Benchmark counters, because the ledger judges every timed
number against a band and neither of these is a number this library can
move: the driver's own device creation, which costs more the more devices
a process has already made, and the Slang standard library, which a
process loads once with its first compile. `bringup_ms` is the whole way
in — that device creation and the adoption together — and
`first_compile_ms` is that load plus the compile that provoked it.
