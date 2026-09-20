# SigilWorld

SigilWorld describes a 3D scene as comparable values, turns those values
into a frame — a scene, an ordered list of passes and the readbacks the
caller asked for — and executes that frame. It owns three things and
nothing else: the 3D scene description, the frame graph that orders
passes from their declared inputs and outputs, and the execution of that
graph. It holds no window, no swapchain, no clock, and no second
copy of anything a library beneath it already defines: meshes, point
operators, splines, cameras — including the clip-space view a device
draws with, which is `camera::Camera::clipProjection()` — the CPU
mesh executor, and the residency that puts a mesh's buffers and a map's
texture on a device are SigilGeometry's;
materials, recipes and programs are SigilMaterial's; the reconciler, its
phases and the caching proof are SigilCore's, as are the erased value a
`Generator` and a `PassBody` take, the field pin every hand-written
comparator here sits under, and the fold a geometry signature accumulates
with; the device and its handles are SigilCoreHardware's, and Graphite is
SigilSkia's; animation is SigilMotion's; counters and timers are
SigilMeasure's.

Two dependencies reach a consumer through the public headers rather than
staying behind them: **Skia**, which is genuine vocabulary here — a frame
hands back an `SkImage`, a pass draws into an `SkSurface`, and a target's
size is an `SkISize` — and **Boost.Container**, which is not: the frame
targets keep six ordered tables of their surfaces, points and stampings
as private members, and a private member in a header is still an include
every consumer pays for.

Namespace `sigil::world`, headers under `include/sigilworld/`. Each
feature is its own static archive with its own tests and benchmark, and
links only the features beneath it; **`SigilWorld`** is the umbrella over
them, so a consumer of the whole library names only that. This page says
which features are built and which are not, rather than describing a
library that is not here.

## What is here

| directory | target | namespace | holds |
|---|---|---|---|
| `element/` | `SigilWorldElement` | `sigil::world` | `Element` and its verbs, the transform lanes, the geometry slot, tags, `Selector` and the `Generator` seam — with `each`, the children a range or a COUNT describes, the spelling a compose tree uses on this side of the seam, so a ring of N posts is one `children` block and not a loop of appends. No device, no retained state. |
| `frame/` | `SigilWorldFrame` | `sigil::world` | `Frame`, `Pass`, `Readback`, the `Targets` a frame's passes write, the `View` they read, and the `Runtime`/`Executor` seam with its CPU executor. No device, no retained state. |
| `graph/` | `SigilWorldGraph` | `sigil::world::graph` | the `Plan`: the order the passes run in, the surfaces they share, the barriers between them, and how each selection is realised. It reads declarations and draws nothing. |
| `scene/` | `SigilWorldScene` | `sigil::world` | the retained side: the reconcile host, the entity store, the content-keyed resource store, the declared phases, the execution of a frame's passes, and the draw. |
| `light/` | `SigilWorldLight` | `sigil::world::light` | emitters as plain comparable values over glm: a sun, a point light, a spot, their falloffs and the per-frame budget. |
| `kit/` | `SigilWorldKit` | `sigil::world::kit` | presets that compose elements: a three-point rig, a turntable, and the lit set both make over a ground plane; and the rails a body rides — the turntable's ring, a loop that rises and falls, a winding round a shell. Nothing here decides a look. |
| `diligent/` | `SigilWorldDiligent` | `sigil::world::diligent` | the programs this backend draws with — the scaffold, the sky and the post stages, compiled through SigilMaterial's Slang backend — the `Runtime` that performs a frame's passes on that device, and `importNative`, the door a foreign texture reaches a material slot by. What stands on the device beneath all of it is SigilGeometry's: `geometry::device::MeshResidency` and `TextureResidency` put a mesh and a map there, `PipelineCache` builds a pipeline out of a compiled program, and this feature asks them. So are the device executors of every seam it is not — the chain cook, the swept rings and the mesh painter each stand beside the CPU executor of their own seam. |
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
        .children({
            Element().key("sun").light(light::sun({-0.4f, -0.8f, -0.4f})),
            Element().key("eye").along(rail, travelled).camera(lens),
            Element()
                .key("tube")
                .mesh(geometry::mesh::pop::sweep(loop, profile))
                .fill(surface)
                .rotateY(bind(&spin))
                .tag("lit"),
            Element()
                .key("comet")
                .chain(points)
                .stamp(bead)
                .window(head, 0.28f)
                .tag("glow"),
        }));

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
               .only(selectors::tag("glow"))
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
SigilCompose spells it — `key`, `children`, `memo`, `at`,
`scale`, `transformOrigin`, `fill`, `cache`, `bind`, `animate`, and the
`Selector` combinators `|`, `&` and `!`. The new spellings are the ones a
plane does not have: `translateZ`/`rotateX`/`rotateY`/`rotateZ`/`scaleZ`
and `rotate(axis, degrees)`, the geometry slot (`mesh`, `cloud`, `chain`,
`stamp`, `generate`), `window`, `along`, `tag`, `light`, the emitter's
dials `intensity` and `emission`, and `camera`.

A closed solid keeps its reverse-wound triangles hidden by default. A sheet,
screen or other open surface that must remain visible as the viewpoint passes
behind it declares `backface(material::Backface::Visible)`; the choice reaches both the
CPU rasterizer and the device pipeline.

## Mental model

**A description is a value; a node is what it became.** `Element` is
copy-on-write and comparable. `propertiesEqual` rules on every one of its
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
| `only(Selector)` | which bodies the pass addresses — `selectors::tag`, `selectors::key`, `selectors::under`, `selectors::material`, composed with `\|`, `&` and `!` |
| `variant(Material)` | …drawn again in that surface |
| `realise(Selection)` | override how the selection reaches the pixels |
| `clear(SkColor4f)` | what a geometry pass clears its target to |
| `chain(geometry::mesh::pop::Chain, geometry::mesh::pop::Runtime)` | the points a compute pass cooks, into the point set it writes |
| `stamp(geometry::mesh::Mesh)` | the body a geometry pass stands at every point of every point set it reads |
| `blur(sigma)` / `levels(gain, lift, tint)` / `composite(mode, opacity)` | what a post pass does to what it reads |
| `body(…)` | THE ESCAPE: a callable offered the extracted `View` and the frame's `Targets` — naming the ones it reads — which runs instead of the stage's own work and keeps its declarations. The comparable `PassBody` beside it is the form a frame prunes on; a callable compares equal to nothing but its own copies |

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
| a post pass with `only` | `Mask` — the picture stands everywhere and the operation reaches it through coverage, which the graph makes the last geometry pass before it also write | a post pass has no bodies; it has pixels, and the selection has to arrive as pixels too |

| `variant(surface)` | `Variant` — the selection is drawn again in that surface | it is a re-draw by definition |
| `realise(…)` | exactly that | a pass that knows better says so |

A coverage answers for ONE selector, so a producer paints one per
selection asked of it: two masked passes behind one geometry pass each
read their own, and two asking the same question share the one already
there. `PassWork::coverageOut` is therefore a list of `Coverage`, each a
resource name and the selector it holds, and `PassWork::coverageIn` is
the one resource a masked pass reads.

A narrowed post pass with nothing painting bodies ahead of it is an error
naming the pass, because the coverage it needs cannot be taken.

`Scene::plan()` is the whole reading — the steps, the barriers, the
resources and their surfaces — and `Scene::error()` is what stopped it.

### For an executor author

An executor sees a frame through `View` and `Targets` and nothing else,
and these are the names it reads them with:

- `subjectOf(draw)` is a body as a `Selector` asks about it, which is
  how a pass realises a `Cull` or paints a coverage;
- `samplingOf(texture)` answers a `Sampling` — the image, where it is
  read at over the mesh's own uvs, whether it repeats and how it is
  filtered — and `surfaceTermsOf(material)` answers a `SurfaceTerms`,
  the metallic, roughness and glass terms behind a body's colour;
- `paintedEnvironment(environment, orientation)` is the set's panorama
  as a mesh painter takes it, and `painterLight(light)` one emitter the
  same way; `dress(style, body)` puts a body's map and its lighting on
  a style a whole list is drawn with;
- `Scene::stats()` answers a `SceneStats`: what the last frame
  described, cooked, drew and let go;
- `lanesOf(node, out)` reads a node's fixed lanes in `Slot` order and
  `standingValue(slot)` is what a lane holds when nothing animates it;
  `localMatrix(values)` is a placement's own matrix;
- `GeneratorOperations` is the seam a geometry that cooks itself implements,
  and `PassBodyOperations` the seam a pass that does its own work implements —
  both carried as comparable values, so a frame holding one prunes on
  it like any other field.

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
  stands everywhere and the operation reaches it only through the coverage.

A pass carrying a body runs that body instead, and the declarations
around it are unchanged.

**[reference/DEVICE.md](reference/DEVICE.md)** is the chapter on the
other one: what it does with each pass, what a material reaches the
device as, the seams below this library whose device executors stand
beside their CPU ones, where its shaders come from and the one device
it all stands on.

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
| `equal` | `propertiesEqual` — every field of `ElementNode`, with the geometry slot's variant equality standing in for a kind comparison |
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
  roughness are read off the material's parameters by name, one number over
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

The studies and the ledger over them are their own chapter:
**[reference/STUDIES.md](reference/STUDIES.md)** — what each of the
fifteen draws, the presets they are built out of, and what the
byte-identity, device and promotion tiers each ask of a plate.

## Testing and benchmarks

[docs/overview/testing.md](../../../docs/overview/testing.md) is the
contract every library here is built, tested and measured under: one
`world_test` over every feature's `test/` and one `world_bench`, ctest
one entry per CASE, what a case may pin, and what a label promises. What
is only true of SigilWorld:

**A binary exists where it links a strictly smaller set of targets than
its neighbours and that boundary is a promise somebody could read**; two
binaries over one closure are one binary. **The fixtures every one of
these binaries shares live in `test/`**, and there is one:
`test/TestMaterial.h` — the throwaway comparable surface a test paints
with, in a plain build and a Slang-bodied one, the camera square on to
the origin at whatever distance the case wants, and the half-plate ink
count a selection is read by. A test target adds that one directory and
includes the header by name; no library's include path carries it, so
nothing shipped can reach a fixture. A body that needs a shape takes
SigilGeometry's `quad()` rather than building one, and the one
hand-built mesh left is a single TRIANGLE, kept because a stamp standing
at every point of a cloud is counted in triangles.

`element/test/` covers the description: copy-on-write, the structural
prune **field by field as one `TEST_P` whose parameter is the field**,
each said two ways, so every row shows a field to tell two values apart
as well as to be in the comparison at all — the geometry slot's value
type standing in for a kind, the lane list including the emitter rows
standing where the emitter stands, the cook, the selectors, and the
emitter values themselves: what each factory fixes, the windowed falloff
reaching exactly zero at the range, and the spot's cone. The emitters
are here rather than in a binary of their own because this target links
the light feature — every target that reaches the emitters reaches the
description too, so there is no boundary for a second binary to draw.

`kit/test/` covers the presets: what tree each returns, that the rig is
stated in the subject's own extents and puts every lamp at the subject
when there are none, that a whole turn of the turntable is where it
started and a rail asked for fewer than three stations is still a closed
loop, that the wave alternates between its two radii and two heights
round its centre and the winding stays on its shell while crossing its
own plane twice a wrap and turning the laps it was asked for, and that
the one colour this library states is the ground's. Nothing puts a world
source directory on the kit's include path, so the retained side's own
header is unreachable from kit code — which is what makes "the kit sees
public headers only" a property of the build rather than a convention.

`scene/test/` covers the retained side, every case over one fixture
holding a clock and a scene reading it: an emitter dial reaching the
light it scales while the tree stands still, identity across a keyed
reorder, the three lifetimes pulling apart under a geometry-slot change,
the store sharing one cooked artefact, a lane ramping a placement, the
bake taken once and lost to a driven lane below it, a culled pass and a
narrowed post pass each reaching only their selection — read off the
pixels, since which realisation the ordering DERIVED is the graph
binary's claim rather than this one's — and a draw that is a function of
the description alone.

`frame/test/` covers the declarations and the CPU executor without
anything retained: a pass compares field by field, a mask realisation
writes the coverage and a variant realisation redraws the selection in
its surface, a post pass reads what stands and what stood last frame, a
compute pass cooks, a still point set is stamped once however many
frames draw it, two names on one slot share the surface, and a declared
body is handed the extracted view. It is handed the realisation rather
than deriving one.

`graph/test/` covers the ordering: the order from the declarations and
its independence from the order they were written in, a cycle named,
`previous()` breaking one, the surfaces counted and shared, the hazards
stated, and **every selection realisation as one `TEST_P` whose
parameter is the declaration** — a narrowed geometry pass culled, a pass
that narrows nothing addressing every body, a narrowed post pass masked,
a narrowed pass carrying a surface redrawn in it, and a pass that says
how it wants to be realised overriding the rule. The coverage a masked
pass reads and the pass ahead of it writes is its own case beside them,
with two more for two masked passes behind one producer: different
selections are two coverages and the same selection is one.

`diligent/test/` covers the device side and carries the `gpu` label.
Every case reads this feature through its public headers alone — the
source directory is not on the binary's include path — so a claim about
a compiled program or a sampled map is a claim somebody outside can
make. What it takes to put a mesh or a map on the device at all is
judged where that code lives, in SigilGeometry's `Device` suite.
`DeviceSeams.h` holds the two seam values that stand on a device, the
two cameras every case looks through, the card it photographs, the
texture the 2D path paints on the device, and the worst channel two
plates differ by; **the device itself is SigilGeometryDevice's**, whose
`test/support/OnDevice.h` brings up ONE for the process, because that
library is the one point in the tree where a device can be created at
all.

`SurfaceTest.cpp` is the sampled slots and the import door — an
occlusion map darkening only where it is dark, an emissive map carrying
its own colour, a cutout dropping texels outright, a normal map tilting
the two halves of one flat card apart, a surface dressed with white in
every slot being the same picture as one dressed with nothing, a texture
painted with the graphics API on this device coming in through
`importNative` with no host image at all — so a picture carrying its
colour cannot have come from a copy — standing where a raster one of the
same colour would, and an import of nothing answering no texture rather
than one that lies. `StackTest.cpp` is what `material::over` composes
for this target: that the composed recipe compiles, and that what it
shades where the mask is half is a picture neither operand alone
produces. `RuntimeTest.cpp` covers the frame: a pipeline off a recipe's
Slang body with its parameter at a reflected offset and the lit build
carrying shading the unlit one does not, a cooked chain that matches the
host's cook exactly, a readback that arrives the frame after, a masked
pass that lifts the selection and leaves the ground where it stood, the
mip rule, and a map whose pixels already stand on this device being
bound where they are. **A claim a picture has to make whichever
rasteriser drew it is written once with the TIER as a parameter** and
answered on both: the map a body is dressed with reaching the pixels, a
nearest-filtered map being two colours and one edge, a linear one being
a gradient, a map asked to repeat being as many of itself as it was
asked for, and a surface that is its own light standing at its base
colour while a lit one of the same colour under a sun aimed away stands
darker.

**What the device suites check on a machine with no Vulkan runtime**,
which is why they carry `gpu`: the Slang compile of a recipe's own body,
the mip rule, and the host half of every claim written over the tier
parameter. Everything else skips, and a skip is not coverage —
`ctest -L gpu` is the run a device verdict may be read out of, and a run
that excludes the label has asked the device nothing. A device wants
`brew install molten-vk vulkan-loader`. No other binary here skips or
vanishes, and none of them needs a font or a network.

Three things are deliberately asked elsewhere. **That every recipe this
repository ships compiles** is SigilMaterial's: its Slang suite compiles
the kit's own surfaces through the same backend and its `MaterialGpu`
suite draws every recipe on a device, so a sweep here would be a third
reading of one fact. **How far the two tiers stand apart** is judged
over the whole registry, each device plate against the CPU plate of the
same run, by `sigil.py plates --tier device`: two rasterisers are not
the same bytes, the distance between them is a different number per
subject, and it moves with the scene rather than with this code — so the
only distance a test here reads is the worst channel, as an INEQUALITY
saying an operation reached the pixels at all. **The conformance of the
chain cook and the swept rings** is SigilGeometry's, whose point-operator
suites do every chain and every sweep both ways and compare bit for bit.

`world_bench`'s device arm measures the four costs a device has that the
host does not: turning the device Diligent made into a device both APIs
draw on, turning a recipe's Slang body into a program, a steady frame
with every pipeline and every mesh already uploaded, and a
point-operator chain cooked on the device — readback included, because a
cook whose answer nobody could read would not be a cook.

Two costs on the way to a first frame are REPORTED THERE AND NOT TIMED,
as Google Benchmark counters, because the ledger judges every timed
number against a band and neither of these is a number this library can
move: the driver's own device creation, which costs more the more
devices a process has already made, and the Slang standard library,
which a process loads once with its first compile. `bringup_ms` is the
whole way in — that device creation and the adoption together — and
`first_compile_ms` is that load plus the compile that provoked it.
