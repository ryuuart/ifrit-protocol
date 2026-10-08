# SigilWorld

SigilWorld describes a 3D scene as comparable values — a tree of nodes
built fresh every frame — turns it into a frame of passes, and draws that
frame on a device. Its words are the ones 3D graphics already uses: USD,
Blender, three.js and glTF are the references for every name here.

```cpp
#include <sigilworld/World.h>

namespace world = sigil::world;

world::Frame describe(sigil::motion::Duration now) {
  return world::Frame(
             world::group({
                 world::directionalLight({-0.45f, -0.8f, -0.4f},
                                         {.intensity = 2.4f}),
                 world::pointLight({.intensity = world::intensityAt(1.8f, 320)})
                     .at({180, 150, 220}),
                 world::ambientLight({.intensity = 0.07f}),
                 world::kit::turntable({.radius = 470, .height = 190}, now),
                 world::mesh(geometry::mesh::quad(900, 900))
                     .at({0, -150, 0})
                     .rotateX(-90)
                     .fill(material::Color{0.10f, 0.11f, 0.14f, 1})
                     .collection("ground"),
                 world::mesh(tube).key("tube").fill(steel),
             }))
      .environment(studio, {.intensity = 1.4f, .diffuse = 0.5f});
}
```

## The node families

Every node is one of three families, named the way 3D graphics names
them:

- **structure** — `group`, which places the children it holds, and
  `null`, a placement with a key and nothing else, there to be referred
  to: a pivot, or what a light or a camera looks at;
- **geometry** — `mesh`, the one node kind that draws: triangles, or one
  stamp repeated as instanced geometry, which SigilGeometry's point
  operators make (`pop::on(…).instances(stamp)`);
- **influences** — the lights, which act on bodies wherever those stand
  in the tree and are narrowed by a link, never by nesting.

A camera is the viewpoint a frame looks through.

## Three rules

1. **What flows down the tree is placement and visibility, and nothing
   else.** A child stands where its parent's placement puts it, and
   nothing under a hidden node is in the scene. A surface belongs to the
   body that wears it; a group says nothing about what its children are
   made of.
2. **A light lights every body in the scene, wherever it stands in the
   tree.** Its node places and aims it; it never narrows what it reaches.
   A light that must leave bodies out says so with its light link — a
   `Collection` of the bodies it reaches — and never by nesting.
3. **A frame looks through one camera.** With one camera in the tree it
   is that one; with several, the frame names which by key with
   `Frame::camera`, and several with none named is an error that names
   them all.

## The front page

Everything a scene says, from `<sigilworld/World.h>`:

```cpp
// the nodes — every kind places, keys and joins collections
Group  group({child, child, …});              // the one kind with children
Null   null();                                // a placement to refer to
Mesh   mesh(geometry::mesh::Mesh triangles);
Mesh   mesh(geometry::mesh::Instances instances);  // one stamp, repeated
Light  directionalLight(glm::vec3 direction, DirectionalLightOptions = {});
Light  pointLight(PointLightOptions = {});
Light  spotLight(glm::vec3 direction, SpotLightOptions = {});
Light  rectLight(glm::vec3 direction, RectLightOptions = {});  // a panel with a size
Light  sphereLight(SphereLightOptions = {});                   // a point light with a radius
Light  diskLight(glm::vec3 direction, DiskLightOptions = {});  // a round panel
Light  ambientLight(AmbientLightOptions = {});
Light  hemisphereLight(HemisphereLightOptions = {});
float  intensityAt(float strength, float distance);  // inverse square, stated
Camera camera(geometry::mesh::camera::Camera lens, CameraOptions = {});
std::vector<Node> each(range or count, make);  // children a range describes

// every kind
.key(name)  .collection(name)  .at(position)  .rotateX/Y/Z(degrees)
.scale(factor)  .along(rail, distance)  .lookAt(key)  .visible(shown)
// a mesh
.fill(material::Material surface)   // a material::Color is lit at the stock response
.backface(material::Backface)
.facing(Facing)                     // Facing::Camera, Facing::CameraUpright
.castShadow(casts)  .receiveShadow(receives)   // both on unless stated
// a light
.lightLink(Collection)  .shadowLink(Collection)
// a directional, point or spot light's options: .castShadow = true,
// .shadow = {bias, normalBias, radius, mapSize, distance, cascades}

// the frame
Frame(Node scene);
Frame& environment(EnvironmentMap map, EnvironmentOptions = {});
Frame& camera(std::string key);
Frame& pass(Pass);
Pass   bloom(Bloom = {});            // threshold, intensity, radius

// the kit
kit::threePoint(kit::Rig = {});
kit::turntable(kit::Turntable, motion::Duration now);
kit::environmentMap(kit::EnvironmentMapOptions);
kit::visibleLight(Light);           // a sized light beside an unlit mesh of its shape, aimed as one
```

**The node kinds.** A `Group` is the one kind that holds children, and it
takes them as its argument: a braced list mixing every kind, and the
lists `each()` makes. A `Null` holds nothing and draws nothing; it stands
where it is placed so that a key can name the spot. A `Mesh` is one body
wearing a surface: a `geometry::mesh::Mesh` drawn once, or
`geometry::mesh::Instances` — one stamp and, per instance, a position,
an orientation, a scale, a colour and named scalars — drawn as the stamp
repeated, each instance carrying its colour and scalars to the surface
(`material::FlipbookOptions::frameAttribute` plays each copy's own frame).
A `Light` is one node type made by five factories — a
directional light, the same everywhere; a point light, shining every way
from where its node stands and falling off with the square of the
distance; a spot light, within a cone; an ambient light, the same from
every direction; a hemisphere light, a sky from above and a ground from
below — each aimed by the direction it travels, which its node's
rotation turns. Nothing lights a scene that the scene did not state. A
`Camera` is Geometry's lens standing where its node does, with the
exposure and the tone curve the picture is read at. Every kind places
itself with the same verbs, answers to a key and joins collections; each
adds only the verbs that mean something for it, so `fill` on a light
does not compile.

**An aim is a placement.** `lookAt(key)` aims any node at a keyed one —
a null, usually — resolved every frame against where that node stands: a
light shines toward it, a camera looks at it, and any other node is
turned so its forward axis points there, in place of its rotation lanes
and a rail's turn, carrying everything under it round. The forward axis
is +z, the way a quad faces, unless whatever built the node says
otherwise: `kit::visibleLight`'s group faces the way its light shines,
so a panel riding a rail and aimed at its subject shines at it and faces
it at every station. A key no node answers to stops the frame, naming
both.

**A collection** is how a set names a set of nodes. A node joins named
collections with `collection("props")`; a `Collection` value includes and
excludes terms, a plain name being the members of that collection and a
term beginning with `/` being the node with that key and everything under
it, the way a USD collection includes a prim's path. A pass narrows to
one with `Pass::only`, a light reaches only one with `Light::lightLink`,
and a light that casts shadows has them cast only by one with
`Light::shadowLink`.

**The frame** holds what is not a node: the environment the scene stands
in, the camera it is seen through when the tree holds several, and the
passes that draw it. A frame with no passes is its scene — reconciled and
extracted, and drawn by nothing.

## Tier two: one options struct per verb

Each factory and frame verb above takes one options struct as its last
argument, written with designated initialisers; every field is defaulted
and a field no consumer sets does not exist.

- `DirectionalLightOptions` — `color` and `intensity`, both
  `motion::Animatable`, so a light bound to a live value changes the
  shading frame by frame without the tree being described again.
- `PointLightOptions` — the same, the intensity being what it delivers
  at one unit, and the `range` a smooth window brings it to nothing at;
  zero reaches everywhere.
- `SpotLightOptions` — the same and the cone: `innerAngle`, within which
  it is full, and `outerAngle`, beyond which it is dark, in degrees.
- `AmbientLightOptions` — `color` and `intensity`.
- `HemisphereLightOptions` — the sky's `color`, the `groundColor` and
  the `intensity`.
- `CameraOptions` — the `exposure` every radiance is multiplied by before
  the tone curve, animatable, and the `toneCurve`: `ToneCurve::Reinhard`,
  `ToneCurve::Neutral` or `ToneCurve::Linear`.
- `EnvironmentOptions` — what of the panorama reaches a surface
  (`intensity`, `diffuse`, `specular`, `roughnessBias`), a second
  panorama crossfaded over the first (`next`, `crossfade`), its turn about
  the up axis (`rotation`), how much of it is shown behind the scene and how
  blurred (`shown`, `blur`), and a ground the shown panorama is projected
  onto (`groundRadius`). Every dial is animatable and read once a frame.
- `kit::Rig`, `kit::Turntable`, `kit::Set` and `kit::EnvironmentMapOptions`
  — the presets', in `reference/PRESETS.md`.

The passes are the frame's second tier: `geometryPass`, `computePass`,
`postPass` and the verbs that declare what each reads, writes, narrows
to and does, in **[reference/FRAMES.md](reference/FRAMES.md)**. The
placement verbs a scene reaches for less often — `translateX/Y/Z`,
`scaleX/Y/Z`, `rotate(axis, degrees)`, `transformOrigin`, the matrix
escape `transform`, and `cache` and `transition` — are on every kind
beside the ones above, in **[reference/NODES.md](reference/NODES.md)**.

## Tier three: `advanced/`

What a host, an executor or a test reaches for, and a scene never does —
each header under `sigilworld/advanced/`:

- `advanced/Node.h` — `Node`, the type-erased handle every
  kind converts to; `NodeDescription`, every field of one node;
  `propertiesEqual`, the structural prune over it; `memo`; and
  `Along`, `Facing`, `CameraDescription` and `ToneCurve`.
- `advanced/Light.h` — `LightKind`, `LightDescription`,
  `PlacedLight`, `held`, `attenuation`, `radiance`, `linearColor` and
  `encodedColor`: a light as values.
- `advanced/Lanes.h` — `LaneFamily`, `Slot`, `lanesOf` and
  `standingValue`: a node's animation lanes.
- `advanced/Geometry.h` — `Generator`, `Geometry`, `Cooked`, `cook`,
  `stampKey` and `signature`: the geometry slot and its cook.
- `advanced/View.h` — `Draw`, `Sampling`, `samplingOf`,
  `ViewOffset`, `View`, `memberOf`, `clipProjection`: what a pass sees.
- `advanced/Runtime.h` — `Executor` and `Runtime`, the seam a
  pass is performed through.
- `advanced/TextureProducer.h` — the seam a texture pass
  paints through.
- `advanced/Plan.h`, `sigilworld/advanced/Stats.h` — the
  ordering a frame derived, and what the frame did, counted.
- `advanced/Diligent.h` — the device executor's compiler and
  program variants.
- `advanced/Skia.h` — the one door to Skia, below.

`Scene`, in `sigilworld/scene/Scene.h`, is the retained side a host
renders frames into; `diligent::runtime(device)`, in
`sigilworld/diligent/Runtime.h`, the executor that draws them.

## What it owns

It owns three things and nothing else: the 3D scene description, the
frame graph that orders passes from their declared inputs and outputs,
and the execution of that graph. A scene is drawn on a device and nowhere
else: there is no CPU executor, and a frame whose passes carry no device
runtime runs none of them and says so. It holds no window, no swapchain,
no clock of its own — its motion runs on the `motion::Engine` its caller
hands the `Scene` — and no second copy of anything a library beneath it
already defines: meshes, point operators, splines, cameras — including
the clip-space view a device draws with, which is
`camera::Camera::clipProjection()` — and the residency that puts a mesh's
buffers and a map's texture on a device are SigilGeometry's; materials,
recipes, programs and the panorama type itself are SigilMaterial's; the
reconciler, its phases and the caching proof are SigilCore's, as are the
erased value a `Generator`, a `PassBody` and a `TextureProducer` take, the
field pin every hand-written comparator here sits under, and the fold a
geometry signature accumulates with; the device and its handles are
SigilCoreHardware's, and Graphite is SigilSkia's; animation is
SigilMotion's; counters and timers are SigilMeasure's.

A frame's colours and blends are Material's (`material::Color`,
`material::BlendMode`), a picture a readback hands back is Media's
`media::Picture`, and a size, a matrix and a placement are glm. No
default include of this library reaches Skia; see *Where Skia still
shows* below. **Boost.Container** reaches a consumer through
`advanced/Skia.h`: the frame targets keep two ordered tables of their
points and stampings as private members, and a private member in a
header is still an include every consumer pays for.

Namespace `sigil::world`, headers under `include/sigilworld/`. Each
feature is its own static archive with its own tests and benchmark, and
links only the features beneath it; **`SigilWorld`** is the umbrella over
them, so a consumer of the whole library names only that.

| directory | target | namespace | holds |
|---|---|---|---|
| `node/` | `SigilWorldNode` | `sigil::world` | the five node kinds over one description, the verb families they share, the transform lanes, the geometry slot, collections, the light values and the `Generator` seam. No device, no retained state. |
| `frame/` | `SigilWorldFrame` | `sigil::world` | `Frame`, `Environment`, `Pass`, `Readback`, the `Targets` a frame's passes write, the `View` they read, the `Runtime`/`Executor` seam the device's executor is handed through, and the `TextureProducer` seam a texture pass paints through. No device, no retained state. |
| `graph/` | `SigilWorldGraph` | `sigil::world::graph` | the `Plan`: the order the passes run in, the surfaces they share, the barriers between them, and how each selection is realised. It reads declarations and draws nothing. |
| `scene/` | `SigilWorldScene` | `sigil::world` | the retained side: the reconcile host, the entity store, the content-keyed resource store, the declared phases, the execution of a frame's passes, and the draw. |
| `kit/` | `SigilWorldKit` | `sigil::world::kit` | presets that compose nodes — a three-point rig, a turntable, the lit set both make over a ground plane, and an environment map baked from a gradient and soft boxes — the rails a body rides, and `World.h`, the front door. Nothing here decides a look. |
| `diligent/` | `SigilWorldDiligent` | `sigil::world::diligent` | the programs this backend draws with — the scaffold, the sky and the post stages, compiled through SigilMaterial's Slang backend — the `Runtime` that performs a frame's passes on that device, and `importNative`, the door a foreign texture reaches a material slot by. What stands on the device beneath all of it is SigilGeometry's: `geometry::device::MeshResidency` and `TextureResidency` put a mesh and a map there, `PipelineCache` builds a pipeline out of a compiled program, and this feature asks them. So are the device executors of every seam it is not — the chain cook and the swept rings each stand beside the CPU executor of their own seam. |
| — | `SigilWorld` | — | the umbrella target: an interface over every feature above, so a consumer of the whole library names one link and includes the headers it spells. The device feature is in it where it was built. |

## Rendering a scene

An author builds a fresh tree every frame and hands it to a `Scene`,
which reconciles it onto what it already holds:

```cpp
#include <sigilworld/World.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilworld/scene/Scene.h>

motion::Engine engine;
world::Scene scene(engine);

world::Frame frame(model);
frame.environment(studio, {.shown = 0.6f})
     .extent({1280, 720})
     .runtime(world::diligent::runtime(device))
     .pass(world::geometryPass("main").writes("colour"))
     .pass(world::postPass("bloom")
               .reads("colour")
               .writes("lit")
               .only("glow")
               .blur(9.0f))
     .pass(world::postPass("trail")
               .reads("lit")
               .previous("trail")
               .writes("trail")
               .composite({.mode = material::BlendMode::PlusLighter,
                           .opacity = 0.88f}))
     .readback(world::readback("trail").then(observe));

scene.render(frame);
world::draw(scene, canvas);  // what the passes wrote — advanced/Skia.h
```

A host states what the scene does not: the extent the targets are made
at, `Frame::defaultCamera` for a tree holding no camera, and
`Frame::cameraOverride` — a lens that wins over the chosen camera's while
that camera still says how the picture is read — for a host that has
taken hold of the viewpoint.

### Presets

`kit/` is a handful of trees someone would otherwise write by hand:

```cpp
world::kit::Set set;
set.rig.extent = 140.0f;                     // how far across the subject is
set.table.period = std::chrono::seconds(12);  // one turn of the camera

scene.render(world::kit::litSet(model, set, now));
```

`kit::threePoint(rig)` is three point lights round a subject, stated in
the subject's own extents and by what each delivers at the subject so one
rig serves a thumbnail and a room, with the key's strength and colour
animatable and an ambient light when the rig states one; `kit::turntable(table, now)` is
a camera riding a closed rail and looking inward, with `kit::rail(table)`
the curve itself; `kit::litSet` is both over a ground plane, with the
subject under it; `kit::environmentMap(options)` bakes a panorama from a
vertical gradient and soft boxes — a studio, a window-lit room, a sky —
as data rather than as a function a scene writes by hand; and
`kit::visibleLight(light)` groups a rect, sphere or disk light with an
unlit mesh of its shape and size in its colour, so the light is seen
where it stands, and the group faces the way the light shines, so
placing, riding and aiming the group moves the two as one.

Two more rails stand beside the turntable's, each a plain `Spline3` a
node rides with `along()`, scatters a comet on or sweeps a band over.
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

**Nothing here decides a look.** Each returns an ordinary value whose
every part the caller can read, replace or ignore, and the only constants
in one are the geometry of the arrangement plus a single neutral grey for
a ground plane that was given no surface. A preset is worth having only
for as long as that stays true.

A closed solid keeps its reverse-wound triangles hidden by default. A
sheet, screen or other open surface that must remain visible as the
viewpoint passes behind it declares
`backface(material::Backface::Visible)`; the choice reaches the device
pipeline's cull state.

## Where Skia still shows

`advanced/Skia.h` is the one door, included by name by an executor, a
pass body or a host, and nothing else here spells Skia:

- **The frame's resources.** `Targets` answers a resource the frame's
  passes wrote as an image, read from the executor that holds its pixels,
  and holds the point sets a compute pass cooks; a pass body and an
  `Executor` are handed one.
- **The host's entrance.** `draw(scene, canvas)` presents the picture the
  last `render()`'s passes wrote on the canvas a host owns.
- **The crossings.** `toSk` turns a `material::Color`, a
  `material::BlendMode`, a `material::Sampling`, a `glm::ivec2` size and a
  `glm::mat3` uv matrix into Skia's own, and `fromSk` brings a matrix
  back.

That list is the boundary, not a queue: every other entrance — a pass's
clear, levels and composite, a frame's extent, a readback's picture, a
sampling's image, uv and filter — speaks Material, Media and glm.

## Mental model

**A description is a value; a retained node is what it became.** Every
kind is a copy-on-write handle on one `NodeDescription`, and comparable.
`propertiesEqual` rules on every one of its fields, pinned by a field count that fails the build when the struct
changes, and answers false for anything it cannot compare — a field left
out of the comparison does not produce a wrong answer where the mistake
is, it produces a node that never patches on that field again.

**The kinds are one model underneath.** A `Group`, a `Null`, a `Mesh`,
a `Light` and a `Camera` are typed handles over one description, so the
reconciler, the prune and the keys see one node whatever made it, and a
group's children are held as `Node`, the type-erased handle every kind
converts to. Which factory made a node decides which fields it ever
writes, and there is no kind field: triangles and instanced geometry are
told apart by the geometry slot's VALUE TYPE — a `geometry::mesh::Mesh`
against a `geometry::mesh::Instances` — so `remountRequired` answers
false, always, and a key that
describes one and then the other resolves new resources in place while
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
their bytes happen to fold together. Two nodes describing one geometry
share one artefact, and the device uploads it once.

**Lanes are addressed by where the motion lives.** One fixed row per lane
per node — the nine placement lanes, the three origin lanes, the axis
turn and the along-distance — so a ramp
survives a patch that changed what the node holds.

**A light's dials and the environment's are values, not lanes.** A
light's colour and strength, a camera's exposure and every environment
dial are `motion::Animatable` fields read once a frame, so one bound to a
live value changes the shading frame by frame while the tree stands
still. What a frame keeps of a light is held — each animatable read and
kept as a constant — so rendering never writes into the live values a
description follows, and a light that changes stales no bake: lights are
gathered on the walk that visits every node every frame, and settled
geometry and material resources stay held.

**There are exactly two write paths**: `Scene::render`, and the live
values a description's lanes are bound to. Nothing writes onto a retained
node from outside, and there is no `entt::registry` accessor: EnTT is
internal to `scene/` the way Yoga is internal to SigilCompose.

**Execution never reads the node tree.** Extract is the one crossing:
it writes an entity's components — its placement, the mesh to draw, the
surface, its key and ancestry, its collections — and the draw and every
pass read those and nothing else. What a pass is handed is a `View`: a
span of `Draw` values, the lights, the environment, the camera and how
its picture is read, and the extent.

**A pass is never a scene child.** It is a stage of making the frame, not
a thing standing in the world, so it is declared on the `Frame` and never
under a node. Nor is the environment, which surrounds everything rather
than standing anywhere.

**Nothing states an order.** A pass declares what it `reads` and what it
`writes`, and the graph derives the sequence, the barriers and the shared
surfaces from those declarations alone.

## Frames, passes and the ordering

A `Frame` declares the scene, its environment, the camera it names, a
list of passes and the readbacks — plus the dials that say where the
picture lands.
`extent(size)` is what its targets are made at, and a frame declaring
passes needs one; `viewOffset(ViewOffset)` makes those targets a part of
a larger picture — `whole` pixels across, the part's corner at `origin` —
formed through the whole picture's projection carried off-centre, so a
host showing a magnified piece of a scene forms only the piece and draws it
where it stands; `defaultCamera(lens)` is the lens for a tree that holds
no camera, and `cameraOverride(lens)` the one a host that has taken hold
of the viewpoint states; `present(name)` names the resource the finished picture
is in, and an unset one means the last image any pass wrote.

`geometryPass(name)`, `computePass(name)`, `postPass(name)` and
`texturePass(name, producer)` each open a pass, and each is a comparable value — a frame prunes on a pass the way
a tree prunes on a node.

| verb | what it declares |
|---|---|
| `reads(names…)` / `writes(names…)` | the resources this pass touches, by name. The first name it writes is what its stage paints into |
| `previous(name)` | that resource AS IT STOOD at the end of the frame before. It orders nothing, which is how a feedback loop is declared without a cycle |
| `only(Collection)` / `only(name)` | which bodies the pass addresses — a `Collection`, or the members of one named collection |
| `variant(Material)` | …drawn again in that surface |
| `realise(Selection)` | override how the selection reaches the pixels |
| `clear(material::Color)` | what a geometry pass clears its target to |
| `chain(geometry::mesh::pop::Chain, ChainOptions)` | the points a compute pass cooks, into the point set it writes, and the runtime they are cooked on |
| `stamp(geometry::mesh::Mesh)` | the body a geometry pass stands at every point of every point set it reads |
| `blur(sigma)` / `levels(Levels)` / `composite(Composite)` | what a post pass does to what it reads |
| `body(…)` | THE ESCAPE: a callable offered the extracted `View` and the frame's `Targets` — naming the ones it reads — which runs instead of the stage's own work and keeps its declarations. The comparable `PassBody` beside it is the form a frame prunes on; a callable compares equal to nothing but its own copies |

**A texture pass paints what bodies wear.** `texturePass(name, producer)`
runs its `TextureProducer` at the frame's time, before every geometry
pass, since wearing a texture names no resource a geometry pass could
read. What it writes holds its own pixels, so it takes no surface and is
never the picture presented. Nothing paints unless its pass is on the
frame: a body wearing a texture whose pass is missing — a texture marked
with `produced(texture, pass)` — is an error naming the body and the
pass, and a texture pass that found nothing changed is counted in
`SceneStats::texturesSkipped` beside `SceneStats::texturesPainted`. A
producer whose painting decides what a body wearing its textures needs —
a page that learned which maps its content states — answers
`TextureProducerOperations::restated`, asked once it has painted, and the
body is drawn wearing that material this frame.

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
where a surface passes from one resource to the next. The plan is built
and checked whether or not a device runs it, because an ordering that
only states its hazards where a device is present states them where they
cannot be tested.

**How a selection is realised is inferred, and can be overridden.**

| the pass declared | what happens | why |
|---|---|---|
| nothing narrowed | `Selection::None` — every body | there is no selection |
| a geometry pass with `only` | `Cull` — only the selected bodies are drawn | a pass that paints bodies can simply paint fewer |
| a post pass with `only` | `Mask` — the picture stands everywhere and the operation reaches it through coverage, which the graph makes the last geometry pass before it also write | a post pass has no bodies; it has pixels, and the selection has to arrive as pixels too |

| `variant(surface)` | `Variant` — the selection is drawn again in that surface | it is a re-draw by definition |
| `realise(…)` | exactly that | a pass that knows better says so |

A coverage answers for ONE collection, so a producer paints one per
selection asked of it: two masked passes behind one geometry pass each
read their own, and two asking the same question share the one already
there. `PassWork::coverageOut` is therefore a list of `Coverage`, each a
resource name and the collection it holds, and `PassWork::coverageIn` is
the one resource a masked pass reads.

A narrowed post pass with nothing painting bodies ahead of it is an error
naming the pass, because the coverage it needs cannot be taken.

`Scene::plan()` is the whole reading — the steps, the barriers, the
resources and their surfaces — and `Scene::error()` is what stopped it.

### For an executor author

An executor sees a frame through `View` and `Targets` and nothing else,
and these are the names it reads them with:

- `memberOf(draw)` is a body as a `Collection` asks about it, which is
  how a pass realises a `Cull`, paints a coverage, and leaves a body out
  of what a linked light reaches;
- `samplingOf(texture)` answers a `Sampling` — the image, where it is
  read at over the mesh's own uvs, whether it repeats and how it is
  filtered;
- `Scene::stats()` answers a `SceneStats`: what the last frame
  described, cooked, drew and let go;
- `lanesOf(node, out)` reads a node's fixed lanes in `Slot` order and
  `standingValue(slot)` is what a lane holds when nothing animates it;
  what the lanes resolve to is Geometry's `geometry::mesh::Transform`,
  whose `matrix()` is a placement's own matrix;
- `GeneratorOperations` is the seam a geometry that cooks itself implements,
  and `PassBodyOperations` the seam a pass that does its own work implements —
  both carried as comparable values, so a frame holding one prunes on
  it like any other field.

## What an executor performs

`diligent::runtime(device)` is the executor a frame names, and it
rasterises on a GPU. The seam it is handed through keeps the frame, the
ordering and the retained scene free of any device: a frame names a
runtime as a value, and every declaration around it is unchanged. A frame
that declares passes and names no runtime runs none of them, and
`Scene::error()` says so.

Beside `execute`, an `Executor` is told when a frame opens and when it
closes. `beginFrame(targets)` is where one holding resources of its own
sizes them, installs itself as where the targets' images come from, and
makes what the frame before wrote into what this frame's `previous()`
names — which cannot wait until the frame ends, because between the last
pass and the next frame is exactly when the picture is presented and a
readback is taken. `endFrame(targets)` runs after everything the frame
read back has been taken, and is where such an executor lets go of what
the frame stopped needing.

A pass carrying a body runs that body instead of the stage's work, and
the declarations around it are unchanged.

**A picture is presented in place or read back.** `world::draw(scene,
canvas, Presentation::OnDevice)` asks the executor for an image the
canvas samples where the device drew it: the final stage is drawn into
a texture handed to the canvas's queue, a fence is signalled behind it
on the device's queue, and the canvas's next submission waits on the
GPU for that signal — the same order a compose texture sampled by a 3D
pass keeps by riding one queue, made explicit because a window draws on
a queue of its own. A texture is drawn into again only once the
window's submission that sampled it has finished. Where the canvas
cannot reach the device — another API, another device, a raster canvas
— or every presented texture is still out, the picture is read back, as
`Presentation::Readback`, the default, always reads it: the same bytes
onto every canvas, which is what a capture that will be diffed draws.
`SceneStats::readbacks` and `SceneStats::presentedOnDevice` count which
way each picture went.

**A live frame does not wait for a program.** With
`Frame::standIns(true)`, a body whose surface program has not compiled
yet is drawn in its base colour, unlit, while
`material::ProgramCache::prepare` compiles it on a worker, and whole on
the first frame after it arrives; `SceneStats::programsPending` counts
the bodies drawn so. A frame left waiting compiles every program before
it draws, so its first frame holds the real surfaces. The pipelines
built from programs go through the driver's cache on the device they
stand on, which a host that names a store keeps between runs.

**A shadow map nothing moved in is kept.** The atlas stands from frame
to frame, and a light's map is drawn again only when what its depth is
a function of changed: its projection, which follows the light and, for
a directional light's cascades, the camera; or a body its link keeps
that casts — where it stands, which cooked triangles it is, its
instances, and, where its surface cuts it out, the moment that surface
is read at. `SceneStats::shadowMapsDrawn` counts the maps a frame drew.

### Drawing a frame: order, light and the final stage

**The order is laid over first, then blended.** Bodies arrive sorted back
to front by where each one's origin stands from the camera. Every body
whose surface composites normally is drawn first, in that order — an
opaque one testing and writing depth, a translucent one testing it only —
then every blended one: a surface that adds, multiplies or screens
(`SurfaceOptions::blend`), one that says it writes no depth
(`SurfaceOptions::depthWrite`), one that fades into the scene
(`SurfaceOptions::depthFade`), back to front over all of them, testing
depth and writing it only where its surface says. The sort is per body,
not per triangle: two blended bodies passing through each other are drawn
in whatever order their origins give, which an additive surface does not
mind. A card whose texture is transparent in places writes depth wherever
it covers; one standing in front of blended bodies states
`depthWrite = false`.

**A body writing depth is opaque.** A body composited normally that
writes depth — stated, or by default because its colour is opaque — is
drawn with blending off: its pixels replace what stands behind them,
alpha and all, and the device skips shading what a nearer opaque body
hides. Translucency is a body that writes no depth
(`SurfaceOptions::depthWrite = false`) or states a blend of its own
(`SurfaceOptions::blend`); either is drawn over what stands behind by its
alpha.

**A blended body fading into the scene** reads the depth of what is opaque
behind it: once the opaque bodies are drawn the depth buffer is copied, and
the copy is read through a view of its own depth format, which Metal
(through MoltenVK) and Vulkan both sample.

**Every target holds light.** A pass writes linear radiance, premultiplied,
in half floats, so a value above white — an emissive surface, a flame, a
sun in the panorama — survives to whatever reads it next. The camera's
exposure and tone curve are applied ONCE, by the final stage, when a
resource leaves the frame — presented, or read back — and encoded for a
display. A surface drawn as authored (`material::surface::unlit`, a page
overlay, a pass's clear colour) is written as the light the final stage
turns back into exactly its colour — the curve run backward — so where it
is opaque it leaves as it was authored, to the byte for a colour eight bits
hold. Where it is translucent it is mixed with what is behind it in light,
before the reading, as anything translucent in a lit scene is.

**Post passes run on light.** A blur, a composite and a bloom spread and
add the light as it stands; a grade (`Levels`) states its gain, lift and
tint as a display reads a colour, so it reads the picture through the
camera's curve, grades it there and writes the result back through the
curve run backward.

**Bloom** is an ordinary post pass: `Frame(scene).pass(world::bloom({...}))`
reads `colour` and writes it back with what passes `Bloom::threshold`, by
luminance, blurred out to about `Bloom::radius` pixels and added at
`Bloom::intensity`. `Pass::bloom` is the same operation on a pass naming
its own resources. Where nothing passes the threshold it adds nothing.

**The clock.** Every body's material is resolved at the frame's time —
`View::time`, the scene time the tree was described at — so a recipe that
reads time (a flipbook, a flow map) plays on a 3D body, is counted as
moving content, and a still taken at a stated time is the same picture
every time.

**Facing.** `Mesh::facing(Facing::Camera)` turns a body square on to the
frame's camera, keeping where it stands and its scale;
`Facing::CameraUpright` turns it about the world's up axis only. The turn
is made where the draws are collected, against the camera the frame looks
through, and replaces the node's own rotation.

**[reference/DEVICE.md](reference/DEVICE.md)** is the chapter on that
executor: what it does with each pass, what a material reaches the
device as, what a lit scene can and cannot say there, the seams below this
library whose device executors stand beside their CPU ones, where its
shaders come from and the one device it all stands on.
**[reference/LIGHTING.md](reference/LIGHTING.md)** is the lighting model
on its own page: what each light delivers and how it falls off — the
inverse square, a range's smooth window, a spot's cone, stated through
`world::intensityAt` — the surface's microfacet terms, the clear coat,
the sheen, the brushing, the thin film and a dielectric's specular, light
from everywhere, how colour is decoded and encoded, and the
camera's tone curves.

## A 2D scene as a texture

A compose scene reaches a 3D surface as a `material::Texture` and by no
other door. There is no panel element, no card and no branch anywhere
here on "is this a scene": what a surface holds in its base-colour slot
is a texture value like any other, and the tiling and the placement reach
it as they reach any other image — with one wrap for both axes, because a
mesh's sampler has one and clamping the axis that was asked to repeat
would drag one edge's pixels across a whole face.

**A channel the device samples is an image texture or a material with a
Slang body.** A material in a slot is written into the body it fills
(`material::inlined`), so a flipbook, a flow map or one channel of a
packed texture plays on a body as it does on a page. A channel filled with
a material that has no Slang body — a normal derived from a height, a
gradient — has nothing the device can compile: it reports the slot once
and the slot reads as empty.

**World does not link SigilCompose and no world header names a compose
type.** The arrow runs the other way: `SigilComposeTexture` keeps a
composer and the surface it paints into, and hands out a texture value.
A host that owns both — a study, an application — makes the scene, hands
it the tree each frame, and puts the value it returns in a material slot.
That is the whole handoff.

**A page the scene lights is a texture pass.** Compose can paint one tree
as the maps of a surface — base colour, normal, roughness, metallic,
occlusion and emissive, the clear coat, and the maps of whichever of the
sheen, the brushing, the thin film and the specular its materials state —
and a host that links both libraries puts that
paint on the frame as a texture pass and the surface it makes in a
body's material, its textures marked as that pass's. The scene's lights,
environment and camera then shade the page; its unlit content arrives in
the emissive map over a black base and shows as it was painted. The
sketch framework's page is that host's door.

**The device reaches those pixels through one narrow value.** A
texture's `media::PixelSource` may answer a frame standing on a device,
which the texture reads as a `DeviceImage`: the device that owns the
texture, and the texture as the graphics API's own object, bridged to
opaque values. SigilMaterial reads none of it. The device
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
| `equal` | `propertiesEqual` — every field of `NodeDescription`, with the geometry slot's variant equality standing in for a kind comparison |
| `reconcilesChildren` | true: children are described, never filled by another path |
| `children` / `descriptionOf` | the description's `children`, and the description off each `Node` |
| `memoOf` / `produce` | the description's `Memo`, and the deferred describe run under the environment its author had |
| `create` | a node, an entity with a `Placement`, and the first patch — with the child's place among the siblings mounting with it, which every staggered tween on it resolves against |
| `onPatched` | retargets the lanes (mounting entrances on the first patch, each resolved at the child's place), marks the geometry slot for resolution when it changed, and stales every bake above |
| `reorder` | stales every bake above when a child mounted, unmounted or moved |
| `remountRequired` | **false, always** — nothing a node retains is welded to what its slots hold |
| `invalidate` | stales every bake above |
| `destroy` | destroys the subtree's entities and releases its resource references |

**Entrances are staggered by value.** A child's own entrance carries
`.delay = motion::stagger(40ms)` — or any other `motion::stagger`, a
fixed total spread across however many children turn up, a
`motion::cues` table, one of five origins, an easing curve — and each
child that MOUNTS resolves it at its place among the siblings mounting
with it. It is SigilMotion's stagger, the same value a paragraph's
glyphs are scheduled with, so `motion::StaggerFrom::Center` means one
thing in a scene and in a line of type. Only children that actually mount
are delayed, and they are placed among the children mounting with them:
appending one node to a live list enters it at once rather than making
it wait out the whole list.

**Lanes and the values on them are SigilMotion's.** `Lane`,
`retargetFixed`, `enter`, `isRunning` and the comparators that decide
two animatable slots are the same live in `<sigilmotion/advanced/Held.h>`
and `<sigilmotion/values/…>`; this
library names the FAMILY (`LaneFamily::Slot`), the 16 rows, and what each
row's standing value is when a description does not carry the block that
holds it.

**Phases** are declared through `core::Phase` and run by `core::runPhases`:
`describe` → `lanes` → `derive` (converging) → `extract` → `graph` →
`execute`. Describe reconciles the tree the author handed over; lanes
sample every binding once; derive resolves placements top-down and
converges because a node's placement is read by everything under it;
extract is the one crossing into the state a draw reads; graph turns the
frame's declarations into an order and gives its resources surfaces; and
execute performs that order on the frame's runtime. The last two do
nothing for a frame that declared no passes.

**The caching proof** rides extract. Each node declares what moves —
a bound or ramping placement lane is composite motion, a live material
rebuilds the surface, and a
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

## Studies

The studies and the ledger over them are their own chapter:
**[reference/STUDIES.md](reference/STUDIES.md)** — what each of the
fifteen draws, the presets they are built out of, and how the plate
ledger's device tier holds each one to a picture of its own.

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
hand-built mesh left is a single TRIANGLE, kept because instanced
geometry's stamp is counted in triangles.

`node/test/` covers the description: copy-on-write, a group taking every
kind in one braced list, the structural prune **field by field as one
`TEST_P` whose parameter is the field**, each said two ways on the kind
that carries it, so every row shows a field to tell two values apart as
well as to be in the comparison at all — the geometry slot's value type
standing in for a kind, the lane list, the cook, collections, and the
light values: what each factory fixes, the windowed falloff reaching
exactly zero at the range, the spot's cone, and a light held still.

`kit/test/` covers the presets: what tree each returns, that the rig is
stated in the subject's own extents and puts every lamp at the subject
when there are none, that its key's strength is bindable, that an
environment map is made from a gradient and soft boxes, that a whole turn of the turntable is where it
started and a rail asked for fewer than three stations is still a closed
loop, that the wave alternates between its two radii and two heights
round its centre and the winding stays on its shell while crossing its
own plane twice a wrap and turning the laps it was asked for, and that
the one colour this library states is the ground's. Nothing puts a world
source directory on the kit's include path, so the retained side's own
header is unreachable from kit code — which is what makes "the kit sees
public headers only" a property of the build rather than a convention.

`scene/test/` covers the retained side, every case over one fixture
holding an engine and a scene running on it: a light bound to a live
value reaching retained bodies while the tree stands still, lights and
cameras riding their nodes' placement, a group and a visible light on a
rail turned onto what they look at at every station, the camera rule — one camera, a
named one, several and none named an error, an override keeping the
camera's exposure — a hidden node taking its subtree out, a body wearing
a texture whose pass is missing being a named error and a texture pass
that painted nothing counted as skipped, the environment read once a
frame, identity across a keyed
reorder, the three lifetimes pulling apart under a geometry-slot change,
the store sharing one cooked artefact, a lane ramping a placement, the
bake taken once and lost to a driven lane below it, a readback handed
over the frame after, a frame with passes and no runtime saying so, and
what a pass is handed being a function of the description alone. NOTHING
IN IT DRAWS: what a pass is handed is read back as values through an
executor that keeps it and paints nothing, so the binary needs no device
and what a device does with it is asked beside the device's executor.

`frame/test/` covers the declarations and the resources without anything
retained: a pass compares field by field, a body compares only with its
own copies and is offered what it names, a still point set is stamped
once however many frames ask for it and let go by a frame that does not,
and an image is whatever the executor holding it answers.

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
than one that lies. `EffectsTest.cpp` is what an authored effect needs: a float, a sixteen-bit and an
eight-bit data texel reaching a shader as stored and a colour texel still
premultiplied, a normal map's alpha saying nothing about its normal, an
additive card adding over a grey plate in either order, a card that writes
no depth hiding nothing drawn after it, a flipbook showing the cell the
clock stands at, blending halfway, looping or holding, and giving way to a
bound frame, light twice white surviving to the final stage and an unlit
colour leaving it to the byte under every curve, a bloom spreading only
what passes its threshold, a card fading where it meets a wall, and a
card facing the camera the same shape from two places.
`StackTest.cpp` is what `material::over` composes
for this target: that the composed recipe compiles, and that what it
shades where the mask is half is a picture neither operand alone
produces. `RuntimeTest.cpp` covers the frame: a pipeline off a recipe's
Slang body with its parameter at a reflected offset and the lit build
carrying shading the unlit one does not, a cooked chain that matches the
host's cook exactly, a readback that arrives the frame after, a masked
pass that lifts the selection and leaves the ground where it stood, the
mip rule, a map whose pixels already stand on this device being bound
where they are, a culled pass drawing only its selection, a visible
backface keeping a plane seen from behind, a light link lighting only
the bodies it names, a `previous()` read carrying
the frame before, a declared body running in place of its stage, the map
a body is dressed with reaching the pixels, a nearest-filtered map being
two colours and one edge, a linear one being a gradient, a map asked to
repeat being as many of itself as it was asked for, and a surface that is
its own light standing at its base colour while a lit one of the same
colour under a sun aimed away stands darker.

**What the device suites check on a machine with no Vulkan runtime**,
which is why they carry `gpu`: the Slang compile of a recipe's own body
and the mip rule. Everything else skips, and a skip is not coverage —
`ctest -L gpu` is the run a device verdict may be read out of, and a run
that excludes the label has asked the device nothing. A device wants
`brew install molten-vk vulkan-loader`. No other binary here skips or
vanishes, and none of them needs a font or a network.

Three things are deliberately asked elsewhere. **That every recipe this
repository ships compiles** is SigilMaterial's: its Slang suite compiles
the surface program through the same backend and its `MaterialGpu`
suite draws every recipe on a device, so a sweep here would be a third
reading of one fact. **Whether a whole scene's picture held still** is
judged over the whole registry by `sigil.py plates --tier device`,
against the device plates that machine keeps — so the only distance a
test here reads is the worst channel, as an INEQUALITY saying an
operation reached the pixels at all. **The conformance of the
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
