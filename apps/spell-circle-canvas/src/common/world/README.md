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
with; the device, its handles and Graphite are SigilSkia's; animation is
SigilMotion's; counters and timers are SigilMeasure's.

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
| `kit/` | `SigilWorldKit` | `sigil::world::kit` | presets that compose elements: a three-point rig, a turntable, and the lit set both make over a ground plane. Nothing here decides a look. |
| `testing/` | `SigilWorldTesting` | `sigil::world::testing` | the study harness — a frame stepped to a declared moment on the CPU and photographed — and the `world_studies` binary the plate ledger's 3D tier drives. |
| `diligent/` | `SigilWorldDiligent` | `sigil::world::diligent` | the one GPU device 2D and 3D share, the Slang compiler the program cache runs, and the `Runtime` that performs a frame's passes on that device. |
| — | `SigilWorld` | — | the umbrella: an interface target over every feature above but `testing/`, and `<sigilworld/World.h>`, which is their public headers in one include. A consumer of the whole library names only this; the device feature is in it where it was built. |

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
                   .mesh(geometry::mesh::curve::sweep(loop, profile))
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
turn, the along-distance, the window's head and span, and the emitter's
strength and three colour channels — so a ramp survives a patch that
changed what the node holds. The four EMITTER rows stand at the
emitter's own fields rather than at a fixed default: a light whose
strength lane is dropped ramps back to the strength `light()` declared,
which is what makes the lanes dials on a value rather than a second copy
of it, and why `light::Light` itself carries no animation.

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
| `chain(Chain, PopRuntime)` | the points a compute pass cooks, into the point set it writes |
| `stamp(Mesh)` | the body a geometry pass stands at every point of every point set it reads |
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
  it; otherwise `diligent::popRuntime(device)` takes the cook, and only
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
number. A frame that cooks a mesh of its OWN — the stamps of a point set
— has no artefact to name and takes a number this frame alone uses.

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

**Not on the device yet**: every OTHER sampled slot. They read one white
texel, so a body multiplied by a map it was not given is the body.

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
| `children` / `descOf` | the description's `children`, and the node handle off each `Element` |
| `memoOf` / `produce` | the description's `Memo`, and the deferred describe run under the environment its author had |
| `create` | a node, an entity with a `Placement`, and the first patch |
| `onPatched` | retargets the lanes (mounting entrances on the first patch), marks the geometry slot for resolution when it or its window changed, and stales every bake above |
| `reorder` | stales every bake above when a child mounted, unmounted or moved |
| `remountRequired` | **false, always** — nothing a node retains is welded to what its slots hold |
| `invalidate` | stales every bake above |
| `destroy` | destroys the subtree's entities and releases its resource references |

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

That is what a machine with no Vulkan runtime can honestly answer, and it
is what the plate ledger's 3D tier is judged on. It is not a substitute
for a device.

## Studies

A study is one 3D frame, stepped from zero at a fixed 1/60 to its
declared moment and photographed — so a plate is a function of the
declaration alone and never of how fast the machine ran.

```sh
build/bin/Release/world_studies --headless <outdir> [--study <name>] [--gpu]
build/bin/Release/world_studies --headless <outdir> --list-studies
```

`--study` takes a case-insensitive substring, which is the loop for
visual iteration. A study joins the registry by being named in
`testing/studies/Studies.h` and listed in `testing/CMakeLists.txt`.

`--gpu` renders every study through the device runtime instead. A study
that declared no passes is wrapped in one geometry pass clearing to its
background, because an executor is only reached through passes and a
study about the scene must be able to say what it looks like on a device
too. The flag answers with the device or with nothing: on a machine with
no Vulkan runtime it reports that and fails, rather than quietly putting
the CPU's plate under a name that asked for the device's. The device is
brought up by the BINARY, so the harness library links none and a machine
with no GPU still renders the CPU tier.

Five of them, and between them they exercise every feature this library
has:

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
- **`material_lab`** — what a surface is made of. Five cards over a
  floor wearing a texture set: one plain, one a STACK of two through a
  mask, one glass, one emissive, and one wearing a map of its own. What
  reaches the pixels is each surface's base colour and its base-colour
  map, so the stack reads as the surface at the bottom of it — the
  honest picture of what this library shades today, and the plate moves
  the day that changes. The texture set is GENERATED in the study rather
  than read off the disk, through the same `textures::` door a scanned
  folder arrives by: a plate is a function of the declaration, and what a
  machine happens to have under `build/assets` is not.
- **`woven_card`** — a live 2D scene riding a 3D ribbon. A compose tree
  is rendered into a texture by a composer of its own, and a band swept
  over a two-point profile is made of it; the card repeats along the
  band's length, which is the ordinary uv placement every texture has.
- **`key_light`** — the emitter's dials. One still set under the kit's
  three-point rig, with the key light's strength and colour bound to live
  values: nothing about the description changes from frame to frame, and
  what moves is what the lanes are bound to.

The last three are built out of `kit/`: `kit::threePoint` puts three
emitters round a subject in its own extents, `kit::turntable` rides a
closed rail looking inward, and `kit::litSet` is both over a ground
plane. Every one returns an ordinary `Element`, which is why `key_light`
can take the rig the preset returned and put lanes on its key light
without the preset offering a hook for it.

A study returns a `Frame`, and an `Element` is one with no passes, so a
study about the scene says nothing about passes at all. The harness
writes the plate's size and its viewpoint into whichever it was handed.

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

### The point operators on the device

`diligent::popRuntime(device)` is a `pop::Runtime` whose executor cooks a
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

Twelve operators have kernels — `Jitter`, `Ramp`, `Vary`, `LookAt`,
`Math`, `Fill`, `Atlas`, `Lookup`, `Select`, `Affine`, `Peak` and `Mix` —
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
and the conformance test in `diligent/test/PopTest.cpp` — every supported
chain cooked both ways and compared bit for bit — fails on the first
expression of the shape `a + b * c`.

The last of the three is DEVICE-WIDE and it is not free: the graphics
pipelines pay it too, and a frame that leans on the post stages is
measurably slower for it (the bench ledger owns the number). It is set
with `setenv(..., overwrite=0)`, so a process that has already put
`MVK_CONFIG_FAST_MATH_ENABLED` in its own environment keeps whatever it
asked for — which is the way to buy the faster device back, at the cost
of a device pop cook that no longer answers what the host answers.

## What is coming

Every feature the layout declares is built. `diligent/` still owes
`importNative` and the sampled slots past the base-colour map.

### Where the shaders come from

The shader modules under `diligent/shaders/` are compiled TWICE. `slangc`
compiles each when this library is built — which is what makes a mistake
in one a build failure rather than a first-frame surprise — and the build
also generates a header carrying each module's text, because the source a
material's body is appended to cannot be finished until the material
exists. At run time the scaffold's text, a recipe's generated
declarations, its body and one fragment entry point are assembled into
one module and compiled through the Slang library, which is also what
reports every uniform's offset.

`Portable.slang` is the subset one source can be compiled twice from and
still answer once: arithmetic plus the operations IEEE 754 pins exactly,
with `sqrt`, `dot`, `length`, `mix`, `smoothstep` and the trigonometric
functions written out, because a library intrinsic is two different
pieces of code on two targets. Slang emits no contraction decoration in
its SPIR-V, so a driver is free to fuse a multiply-add inside a module
compiled here; a kernel that needs the unfused answer has to reach the
same result without depending on it.

### The one device

Diligent creates the Vulkan device and SigilSkia adopts it. That
direction is forced: this build of Diligent cannot attach to a device
that already exists and has no Metal backend, so standing a second device
up beside it would mean two queues, two handle tables and a CPU round
trip between 2D and 3D.

```cpp
#include <sigilworld/diligent/Device.h>

using namespace sigil;

world::diligent::DeviceConfig config;
std::string error;
std::unique_ptr<world::diligent::Device> device =
    world::diligent::Device::create(config, &error);
if (!device) return;  // no Vulkan runtime, for instance; `error` says why

skia::GpuDevice& gpu = *device->gpu();
skia::TextureDesc desc;
desc.width = desc.height = 512;
desc.format = skia::TextureFormat::RGBA8Unorm;
const skia::TextureHandle texture = gpu.createTexture(desc);
const skia::FenceHandle fence = gpu.createFence();

// Paint 2D into a texture a 3D pass will sample. Everything that submits
// on the shared queue happens under the lock.
world::diligent::Device::QueueLock lock(*device);
skia::OffscreenSurface surface(*device->graphite(), gpu, texture);
surface.canvas()->clear(SK_ColorBLUE);
surface.submit(gpu, fence);
```

`renderDevice()` and `context()` are the Diligent side, and are never
null on a device that was created. `gpu()` and `graphite()` are the
adopted side and are null together when the adoption failed — a driver
without timeline semaphores, for instance, since that is what a SigilSkia
fence is. A failed adoption costs the shared 2D path and nothing else.

The Vulkan loader is opened once, by the volk shim vendored under
`diligent/thirdparty/volk`, and the `vkGetInstanceProcAddr` it resolves
is handed to SigilSkia, so both APIs dispatch through the same entry
points. `SIGILWORLD_VULKAN_LIBRARY` names a Vulkan library to open ahead
of the built-in candidates.

There is no Metal path here, because Diligent has no Metal backend:
`create` fails on a machine with no Vulkan runtime, and the answer for
such a machine is the CPU executor, not a second GPU path.

## Testing and benchmarks

```sh
ctest --test-dir build -C Debug -R world_
```

`world_element_test` covers the description: copy-on-write, the
structural prune field by field, the geometry slot's value type standing
in for a kind, the lane list — including the emitter rows standing where
the emitter stands — the cook, and the selectors. `world_kit_test`
covers the presets: what tree each returns, that the rig is stated in the
subject's own extents, that a whole turn of the turntable is where it
started, and that the one colour this library states is the ground's.
`world_scene_test` covers the retained side: an emitter dial reaching the
light it scales while the tree stands still, identity across a keyed
reorder, the three lifetimes pulling apart under a geometry-slot change,
the store sharing one cooked artefact, a lane ramping a placement, the
bake taken once and lost to a driven lane below it, and a draw that is a
function of the description alone. `world_light_test` runs anywhere.
`world_diligent_test` covers the device side: a pipeline off a recipe's
Slang body with its parameter at a reflected offset and the lit build
carrying shading the unlit one does not, one scene rendered on both tiers
and measured apart, a cooked chain that matches the host's cook exactly,
a readback that arrives the frame after, a masked pass that lifts the
selection and leaves the ground where it stood, the map a body is dressed
with reaching both tiers to the same picture, a map whose pixels already
stand on this device being bound where they are — proven by a source that
answers no host image at all, so a picture carrying its colour cannot
have come from a copy — a texture's filter honoured on both tiers, where
nearest shows two colours and one edge and linear shows the gradient
between them, and a surface that is its own light standing at its base
colour on both tiers while a lit one of the same colour, under a sun
aimed away, stands darker. `diligent/test/PopTest.cpp` is the point
operators' CONFORMANCE: every chain the device runtime says it can cook,
cooked both ways and compared BIT FOR BIT — not a distance and not a
tolerance, because the operators are one piece of arithmetic compiled
twice; plus each declined operator refused by name while the host still
answers it. The program tests run anywhere; the ones that need a Vulkan
runtime (`brew install molten-vk vulkan-loader`) *skip* rather than fail
without one, so a machine with no GPU stays green.

`world_frame_test` covers the declarations and the CPU executor without
anything retained: a pass compares field by field, each realisation
lands the pixels it promises, a post pass reads what stands and what
stood last frame, a compute pass cooks, two names on one slot share the
surface, and a declared body is handed the extracted view.
`world_graph_test` covers the ordering: the order from the declarations
and its independence from the order they were written in, a cycle named,
`previous()` breaking one, the surfaces counted, the hazards stated, and
each selection realisation ruled on.

`world_kit_boundary_probe` is the kit boundary's NEGATIVE CONTROL: a
target that must fail to build, run as a test that builds it and requires
`'SceneImpl.h' file not found` in the output. Nothing puts a world source
directory on the kit's include path, so the retained side's own header is
unreachable from kit code — which is what makes "the kit sees public
headers only" a property of the build rather than a convention. Demanding
that one message, and not merely a non-zero exit, is what keeps an
unrelated breakage from reading as the boundary holding.

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
