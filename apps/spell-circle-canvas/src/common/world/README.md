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
phases and the caching proof are SigilCore's; the device, its handles and
Graphite are SigilSkia's; animation is SigilMotion's; counters and timers
are SigilMeasure's.

Namespace `sigil::world`, headers under `include/sigilworld/`. Each
feature is its own static archive with its own tests and benchmark, and
links only the features beneath it. This page says which features are
built and which are not, rather than describing a library that is not
here.

## What is here

| directory | target | namespace | holds |
|---|---|---|---|
| `element/` | `SigilWorldElement` | `sigil::world` | `Element` and its verbs, the transform lanes, the geometry slot, tags, `Selector` and the `Generator` seam. No device, no retained state. |
| `frame/` | `SigilWorldFrame` | `sigil::world` | `Frame`, `Pass`, `Readback`, the `Targets` a frame's passes write, the `View` they read, and the `Runtime`/`Executor` seam with its CPU executor. No device, no retained state. |
| `graph/` | `SigilWorldGraph` | `sigil::world::graph` | the `Plan`: the order the passes run in, the surfaces they share, the barriers between them, and how each selection is realised. It reads declarations and draws nothing. |
| `scene/` | `SigilWorldScene` | `sigil::world` | the retained side: the reconcile host, the entity store, the content-keyed resource store, the declared phases, the execution of a frame's passes, and the draw. |
| `light/` | `SigilWorldLight` | `sigil::world::light` | emitters as plain comparable values over glm: a sun, a point light, a spot, their falloffs and the per-frame budget. |
| `testing/` | `SigilWorldTesting` | `sigil::world::testing` | the study harness — a frame stepped to a declared moment on the CPU and photographed — and the `world_studies` binary the plate ledger's 3D tier drives. |
| `diligent/` | `SigilWorldDiligent` | `sigil::world::diligent` | the one GPU device 2D and 3D share. |

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

Where a concept exists in two dimensions this spells it the way
SigilCompose spells it — `key`, `child`, `children`, `memo`, `at`,
`scale`, `transformOrigin`, `fill`, `cache`, `bind`, `animate`, and the
`Selector` combinators `|`, `&` and `!`. The new spellings are the ones a
plane does not have: `translateZ`/`rotateX`/`rotateY`/`rotateZ`/`scaleZ`
and `rotate(axis, degrees)`, the geometry slot (`mesh`, `cloud`, `chain`,
`stamp`, `generate`), `window`, `along`, `tag`, `light` and `camera`.

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
counts and kinds buckets a lookup and `operator==` decides it, so two
different geometries can never be served one artefact however their bytes
happen to fold together. Two nodes describing one chain cook it once.

**Lanes are addressed by where the motion lives.** One fixed row per lane
per node — the nine placement lanes, the three origin lanes, the axis
turn, the along-distance and the window's head and span — so a ramp
survives a patch that changed what the node holds.

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

## What the CPU executor performs

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
  reads its `baseColor` field when the recipe declares one. A recipe's
  body is a program, and the CPU tier has no compiler to run one.
- a sun reaches the shading as itself; a point or spot light reaches it
  as the direction from where it stands toward the origin, at the
  strength it has there. The full falloff is `light::attenuation`.
- bodies are sorted back to front by view depth, stably, so two at one
  depth land in tree order.

That is what a machine with no Vulkan runtime can honestly answer, and it
is what the plate ledger's 3D tier is judged on. It is not a substitute
for a device.

## Studies

A study is one 3D frame, stepped from zero at a fixed 1/60 to its
declared moment and photographed — so a plate is a function of the
declaration alone and never of how fast the machine ran.

```sh
build/bin/Release/world_studies --headless <outdir> [--study <name>]
build/bin/Release/world_studies --headless <outdir> --list-studies
```

`--study` takes a case-insensitive substring, which is the loop for
visual iteration. A study joins the registry by being named in
`testing/studies/Studies.h` and listed in `testing/CMakeLists.txt`.

`first_light` is the first of them: a tube swept along a closed loop, a
comet of stamps riding a moving window of that same loop, a plate under
both, a sun and a lamp, and a camera on a rail of its own.

`glow_trail` is the first about the passes. Its set is drawn once, and
what is tagged "glow" is then reached three ways, one per realisation:
a narrowed post pass lifts the beads in place through the coverage the
geometry pass before it was made to write; a narrowed geometry pass draws
the same beads alone into a target of their own, which is softened and
dimmed; and that is laid over its own output from the frame before, so
the comet drags a tail no single frame contains. Six passes, no stated
order, and three of its surfaces are taken in turns.

A study returns a `Frame`, and an `Element` is one with no passes, so a
study about the scene says nothing about passes at all. The harness
writes the plate's size and its viewpoint into whichever it was handed.

## The plate ledger's 3D tier

`scripts/plate_ledger.py --tier world` renders every study to its
declared moment on the CPU and hashes the bytes against its own baseline,
`build/plate_baseline_world_<config>.sha256`. It is the same question the
2D tiers ask — did any byte move that I did not mean to move — of a
different registry, and it needs no device:

```sh
python3 scripts/plate_ledger.py --tier world --rebase   # adopt a baseline
python3 scripts/plate_ledger.py --tier world            # sweep and judge
python3 scripts/plate_ledger.py --tier world --stability 2
```

## What is coming

These are not built yet. They arrive in this order, each one leaving the
tree buildable:

| directory | target | holds |
|---|---|---|
| `kit/` | `SigilWorldKit` | presets that compose elements: a three-point rig, a turntable, a lit set |

`diligent/` grows the rest of the execution side beside its device:
pipelines from resolved `material::Program`s, the Slang compiler
registration, the GPU `Runtime` value and `importNative`. An umbrella
interface target named `SigilWorld` gathers every feature once there is
more than one worth gathering.

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
in for a kind, the lane list, the cook, and the selectors.
`world_scene_test` covers the retained side: identity across a keyed
reorder, the three lifetimes pulling apart under a geometry-slot change,
the store sharing one cooked artefact, a lane ramping a placement, the
bake taken once and lost to a driven lane below it, and a draw that is a
function of the description alone. `world_light_test` runs anywhere.
`world_diligent_test` needs a Vulkan runtime (`brew install molten-vk
vulkan-loader`) and *skips* rather than fails without one, so a machine
with no GPU stays green.

`world_frame_test` covers the declarations and the CPU executor without
anything retained: a pass compares field by field, each realisation
lands the pixels it promises, a post pass reads what stands and what
stood last frame, a compute pass cooks, two names on one slot share the
surface, and a declared body is handed the extracted view.
`world_graph_test` covers the ordering: the order from the declarations
and its independence from the order they were written in, a cycle named,
`previous()` breaking one, the surfaces counted, the hazards stated, and
each selection realisation ruled on.

`world_element_bench`, `world_frame_bench`, `world_graph_bench`,
`world_scene_bench`, `world_light_bench` and `world_diligent_bench` build
through the `benches` target and run through `scripts/bench_ledger.py`;
use a Release build.
