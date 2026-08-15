# SigilWorld

SigilWorld draws 3D scenes on a real GPU without opening a window. You
give it triangle meshes and images; it owns the graphics device, an
offscreen colour-and-depth target with multisampling, and a
physically-based shading pass, and it gives frames back as CPU-side
raster images or PNG files. Scene objects are entities in an entity
registry, so transforms, materials, lights and cameras are ordinary data
you can read, mutate, or drive from your own systems. Geometry
*generation* can run on the GPU too: swept ribbons, particle flocks and
a general point-operator chain execute as compute dispatches whose
results never travel back through the CPU.

Namespace `sigil::world`, target `SigilWorld`, headers under
`include/sigilworld/`. The backend is
[Diligent Engine](https://github.com/DiligentGraphics/DiligentEngine) on
Vulkan — through MoltenVK on macOS.

## Using it

```cpp
#include <sigilworld/Components.h>
#include <sigilworld/World.h>

#include <sigilshape/Mesh.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace sigil;

world::WorldConfig config;
config.width = 1280;
config.height = 720;

std::string error;
std::unique_ptr<world::World> w = world::World::create(config, &error);
if (!w)
  return;  // no Vulkan runtime, for instance; `error` says why

world::Material gold;
gold.baseColor = {0.90f, 0.72f, 0.25f, 1};
gold.metallic = 1;
gold.roughness = 0.25f;

const uint32_t ring =
    w->addSurface(shape::mesh::torus(140, 40), glm::mat4(1.0f), gold);

shape::space::Camera camera;
camera.eye = {0, 220, 620};
camera.target = {0, 0, 0};
w->setCamera(camera);

w->render();
w->savePng("frame.png");
sk_sp<SkImage> frame = w->readback();  // raster RGBA, opaque

// The surface id IS an entity. Move it and render again.
w->registry().get<world::TransformComponent>(world::entity(ring)).model =
    glm::rotate(glm::mat4(1.0f), 0.4f, glm::vec3{0, 1, 0});
w->render();
```

A textured panel is the same call with an image in the material:

```cpp
world::Material screen;
screen.texture = someSkImage;
screen.unlit = true;  // self-lit UI, no shading applied
w->addSurface(shape::mesh::quad(380, 252), placement, screen);
```

A scanned or authored material is the full texture set, read from the
folder a tool exported by its file names:

```cpp
#include <sigilworld/TextureSet.h>

for (const world::textures::TextureSet& set :
     world::textures::discover("assets/textures/metal_plate")) {
  world::Material plate = world::textures::material(set, decodeImage);
  plate.uvScale = {4, 4};  // the set tiles; lay it down four times
  w->addSurface(shape::mesh::torus(150, 60), placement, plate);
}
```

## The mental model

**Four things cross the boundary, and nothing else does.** Geometry
comes in as a `shape::Mesh`. Panel content comes in as any `SkImage` —
whatever produced it. The camera comes in as a
`shape::space::Camera`, the same value type SigilShape's software
renderer uses, so a Skia-composited image and a SigilWorld render agree
about where things sit. Frames leave as raster `SkImage`s. There is no
window, no swapchain, and no scene file format.

**A surface id is an entity.** `addSurface()` returns a `uint32_t` that
`world::entity(id)` casts to an `entt::entity`, and `registry()` hands
out the registry itself. Attach your own components to a surface, run
your own views over the same entities, mutate `TransformComponent` and
`MaterialComponent` directly — the next `render()` reads them. Entity 0
is consumed during initialization, so a valid surface id is never 0 and
0 always means failure. Lights and cameras are entities too
(`LightComponent`, `CameraComponent`).

**Four ways to get geometry onto the GPU, increasingly device-resident.**

1. `addSurface()` uploads a CPU-built `shape::Mesh`.
   `setSurfaceMesh()` replaces it in place — matching vertex and index
   counts update the existing buffers, a different shape recreates them.
2. `addInstanced()` uploads one stamp mesh plus a per-instance stream
   built from a `shape::Cloud`, drawing every point in one call.
   `setInstances()` refreshes the points.
3. `addSweep()` and `addFlock()` build their geometry with compute
   kernels: control points live in a device buffer, and the vertex or
   instance stream is written on the GPU. The points never exist on the
   CPU at all.
4. `addPoints()` cooks a whole `shape::pop::Chain` — a generator plus a
   list of filter operators — as one compute dispatch per operator over
   GPU-resident attribute lanes. `addPointsOn()` feeds one cooked chain
   into another without a CPU round trip. `readPoints()` copies the
   cooked lanes back as a `shape::Cloud` when you want them.

**Windows are the animation primitive.** Every GPU generator takes a
`head` and `span` window into a closed loop. Sliding that window is two
floats and a re-dispatch — `setSweepWindow()`, `setFlockWindow()`,
`setPointsWindow()` — so a comet marching along a curve costs two
numbers per frame and no geometry work on the CPU.

**Three stacked authoring layers, and you can mix them.**

- *Imperative*: the `World` methods above. Call, get an id, mutate.
- *Declarative*: `Scene.h`. Describe the scene as a value tree of
  `scene::group()` / `scene::surface()` / `scene::panel()` nodes with
  keys, call `Scene::render(root)`, and a reconciler diffs it against
  the previous description. Transform-only changes become
  `setTransform`; identical leaves are kept; only genuinely new or
  changed surfaces upload. `render()` returns
  `Stats{added, removed, moved, kept}`, and `Scene::find(keyPath)`
  resolves a node's key path to its entity.
- *Fluent*: `Easel.h`. `easel::stage(world)` chains one sentence per
  thing on the set — `sun()`, `light()`, `place()`, `panel()`,
  `swarm()` — with `at()/turned()/sized()/key()` styling the last
  declared item, and `commit()` reconciling the lot through the Scene
  layer.

**Declared motion is an orthogonal door with a device-free half.**
`Animation.h` adds five components — `AnimatedTransform`,
`AnimatedMaterial`, `AnimatedLight`, `AnimatedCamera`,
`AnimatedWindow` — whose fields are `Animatable<float>` lanes from
SigilMotion. Attach them and the values follow whatever
`choreograph::Output` they are bound to. `resolveAnimation()` has two
overloads: one taking a bare `entt::registry`, which touches no GPU
state at all and is therefore usable and testable with no device
present, and one taking `World&`, which adds the generator-window lanes.
`World::render()` calls the second before it draws anything.

**The library owns no clock.** There is no `tick()` and no `render(dt)`.
Rendering is a pure function of what the bound outputs currently hold.
The caller steps a `motion::Ticker` with the delta it chooses, which is
what makes a headless frame sequence reproducible.

## The headers

| Header | What it is for |
| --- | --- |
| `sigilworld/World.h` | `World`, `WorldConfig`, `Material`, `Lighting`, `InstanceLanes`. Device bring-up, every surface-creating call, camera and lighting setters, `render`/`readback`/`savePng`. |
| `sigilworld/Components.h` | The registry face: `TransformComponent`, `MaterialComponent`, `LightComponent`, `CameraComponent`, the `kLightBudget` constant, and `entity(id)`. |
| `sigilworld/Scene.h` | The declarative reconciler: `scene::Node`, `scene::group/surface/panel`, `scene::Stack`, `scene::Scene` with `render`, `find` and `clear`. |
| `sigilworld/Animation.h` | Declared motion: the five `Animated*` components, `CameraPath`, `AnimationStats`, `resolveValue`, both `resolveAnimation` overloads, and the SigilMotion value vocabulary re-exported into `sigil::world`. |
| `sigilworld/Easel.h` | Header-only fluent stage: `easel::stage()`, `easel::Stage`. |
| `sigilworld/TextureSet.h` | The tools' texture sets read back: `textures::Role`, `classify()` a file name, `roleForUsage()` a channel word, `discover()` a folder into `TextureSet`s, and `material()` from a set or from usage-keyed images. |

## Conventions that will bite you

**Colour spaces are asymmetric on purpose.** `Material::baseColor` and
`emissive`, the `Lighting` sun/sky/ground colours, and `LightComponent`
colours are all **linear** — the shader shades in linear and encodes on
the way out. `WorldConfig::clearColor` is **encoded sRGB**, because the
clear does not pass through a shader: it is written straight into the
target, so its components *are* the bytes the background pixel gets. The
default `{0.028, 0.03, 0.045}` reads back as `(7, 8, 11)`, not the
`(47, 48, 60)` an encode would produce. To match a background against a
linear `Material` colour, encode it yourself.

**The sRGB encode is the piecewise standard curve, not a `1/2.2`
power**, and it is applied in *both* the lit and the unlit branch. That
is what makes an unlit panel a true pass-through: a texel uploaded,
linearized by the sampler, re-encoded by the shader and read back lands
on its own byte. Substituting a gamma power breaks that round trip
through the dark and mid range.

**Colour maps upload as an sRGB format, data maps as plain unorm.** The
base colour and emissive maps are linearized by the sampler on read;
normal, roughness, metallic and occlusion maps are numbers and are read
as stored. The render target is plain unorm and depth is 32-bit float.

**Any image is flattened to 8-bit unpremultiplied RGBA on upload.**
Float and HDR sources lose their range there. No mipmaps are generated.

**The sampler clamps unless the material tiles.** `Material::uvScale`
and `uvOffset` apply at sample time; with `tile` false a window that
runs off the texture smears its edge texels, with `tile` true every map
on the material repeats. Two samplers exist on the device and each
uploaded texture's view carries one of them.

**The texture set is a metallic-roughness set, one channel per scalar
map.** `roughnessMap`, `metallicMap` and `occlusionMap` each read ONE
channel, chosen by their `*Channel` index, so a packed
occlusion-roughness-metallic image serves all three slots with channels
0, 1, 2 — which is what `textures::material()` wires when a set carries
an `_arm` / `_orm` / `OcclusionRoughnessMetallic` file and no separate
maps. A missing map reads as 1, so the scalar next to it (`roughness`,
`metallic`) is the whole value; a present map is *multiplied* by that
scalar, which is why the loader sets the scalar to 1 when the set
carries the map.

**Normal maps need no vertex tangents.** The tangent frame is solved per
pixel from the position and uv screen-space derivatives — the direct 2x2
solve, not the cross-product shortcut, so the frame's signs do not depend
on which way the backend's `ddy` points. `normalMapDirectX` flips the
green axis for maps authored with green pointing down the image; the
default reads OpenGL maps. `normalScale` 0 is a flat map. Where a
surface has no uv gradient (a degenerate uv layout) the geometric normal
is kept.

**Occlusion darkens the ambient term only**, by `occlusionStrength`;
direct light ignores it, as it should.

**Matrices upload as raw column-major memory** and the shader does
column-vector math (`mul(M, v)`). Nothing is transposed anywhere, in
either direction — that symmetry is deliberate and it is what keeps the
glm side and the shader side agreeing.

**Projection is right-handed, y-up, depth 0..1, with no Vulkan clip-y
flip.** Diligent's Vulkan backend normalizes clip-space y internally, so
+y up in clip space is already correct. Adding a flip inverts the image.

**Culling is off; every surface is two-sided.** The pixel shader flips
the normal when it faces away from the eye. Winding order does not
determine visibility, so a mesh with inconsistent winding still renders.

**A translucent material routes into a separate blended pass sorted back
to front** (alpha below 1 in `baseColor`). An instanced surface is one
item in that sort — its instances are not sorted against each other.

**Swapping an image on a `MaterialComponent` is live, and costs an
upload.** `render()` compares the image pointers and the tile flag a
surface's binding was built from against the live component and rebuilds
the binding when any moved. Colours, scalars and the uv window are read
every draw and cost nothing; an image swap re-uploads every map on that
surface, so it is a change-of-scene operation, not a per-frame one.

**An `Animated*` component owns its entity's corresponding component.**
Do not also drive that component by hand: the next resolve overwrites
your write. `AnimatedTransform` owns the whole `TransformComponent`;
`AnimatedMaterial`, `AnimatedLight` and `AnimatedCamera` own only the
lanes they engage, which is why those lanes are `std::optional` —
engaging one lane must not slam the others to defaults.

**An active `CameraComponent` outranks `setCamera()`,** including a
`setCamera()` called afterwards. Deactivate (`active = false`) or
destroy the entity to hand control back. With several active cameras,
whichever the registry iterates first wins — keep one active.

**Every animation lane is a float.** A position is three lanes, not an
`Animatable<glm::vec3>`. The value of a lane is the shaping chain
`bind()` provides (normalise, window, curve, target, quantize, clamp,
wiggle), and that chain is float-only. Some things deliberately get no
lanes: a camera's `up` and a directional light's `direction`, because
three free floats cannot promise a unit vector (use `AnimatedCamera`'s
`rollDeg` for a tilt); `zNear`/`zFar`, which are scene-scale constants;
and colour, which wants a colour type rather than three linear-RGB
floats.

**A camera path drives the eye outright.** While
`AnimatedCamera::path` holds a curve with control points, `eyeX/Y/Z` are
ignored — not blended, not treated as offsets. It drives the target if
and only if `CameraPath::lookAhead` is non-zero; at zero, `targetX/Y/Z`
and any authored target stand. A closed spline wraps (`t` past 1 comes
round, negative `t` runs backwards); an open one clamps at its ends.
Arc-length reparameterization is on by default, so `t` is a constant-speed
fraction rather than a raw curve parameter; the table backing it is
rebuilt whenever the spline it was built from changes, so editing
`path.points` in place cannot leave a stale curve behind.

**The light budget is 8 per frame** (`kLightBudget`). `render()` gathers
that many `LightComponent` entities in registry iteration order and
silently ignores the rest. Point falloff is an inverse-square falloff
squared — `(1 - (d/range)^2)^2` — which keeps useful intensities in a
small artist range rather than physical thousands.

**The `pop::Op` variant order is ABI.** A table in `World.cpp` maps each
variant index to its compute pipeline, one row per alternative, guarded
by a `static_assert` against `std::variant_size_v`. Append new operators
to the variant; never insert. Operators with no GPU counterpart —
`MeshScatter`, `Promote`, `Sort` — cause the whole chain to be
**declined**: `addPoints`, `addPointsOn` and `setPoints` return 0 or do
nothing rather than silently cooking a chain with an operator missing.
Everything else runs here: the selectors (`Group`), the deformers
(`Transform`, `Peak`, `Deform`), `Mix`, and every filter's `mask` —
a mask lane is just one more slot in the lane arena, and the masked
blend is the same expression the CPU cook uses.

**A `Deform`'s frame is computed once, on the CPU.** `popops::deformFrame`
normalizes the axis and orthogonalizes the bend direction for both
executors, and the kernel receives that frame rather than recomputing it,
so a degenerate axis or direction falls back the same way on both sides.

**A lookup-table edit is structural.** `pop::Lookup` stops ride an
immutable device buffer, so `setPoints()` with a changed table takes the
full rebuild path instead of the cheap parameter re-cook. Same for a
generator's loop points.

**Compute buffers must not share an element struct.** On MoltenVK, two
`RWStructuredBuffer`s declared over the same struct type collide in the
shader translator's naming. Give each lane a distinct struct type, and
write whole elements rather than individual members.

**`readPoints()` is synchronous** — a copy and a wait. It is a query
door, not a per-frame path, and it is only valid after a `render()` has
cooked the chain.

**Reconciler entity lifetime.** An entity returned by `Scene::find()` is
valid until the next `render()` that recreates or removes that node. A
leaf whose mesh pointer or material changed is a remove-and-add: the old
entity is destroyed along with every component on it, including your
animation lanes, and `find()` then returns the *new* entity. Re-attach
after a recreate. A kept or transform-only-moved leaf keeps its entity —
and when a kept leaf's declared placement or material is outranked by a
live `Animated*` component, `render()` warns once per node.

## Boundary

Public dependencies: `SigilShape` (the mesh, cloud, chain, spline and
camera types), Skia (`SkImage` in and out), `EnTT` (the registry is
public API), and `SigilMotion` — public because `Animatable` appears in
the component surface, and safe to expose because SigilMotion links a
timeline library and nothing else. Private: Diligent Engine's core and
the Vulkan headers.

The Vulkan loader is vendored (`thirdparty/volk`) and compiled through
`VolkShim.c`, which replaces the stock initializer with one that also
probes the Homebrew library path and points the ICD loader at MoltenVK's
manifest. This exists because macOS `dlopen` searches neither
`/opt/homebrew/lib` nor already-loaded leaf names, so an unmodified
loader cannot find a Homebrew MoltenVK install without environment
surgery. `SIGILWORLD_VULKAN_LIBRARY` overrides the candidate list.

The graphics vertex and pixel shaders are one HLSL source compiled at
runtime by the compiler Diligent ships. The compute kernels are separate:
they are authored in Slang under `shaders/` (`pop.slang`, `sweep.slang`,
`flock.slang`, over the shared `sigilspline.slang` module), compiled to
SPIR-V at build time by `slangc`, and embedded. `slangc` is optional —
the build configures, compiles and links without it, and the whole
rasterization path works normally. What is unavailable in that build is
the compute generators: `addSweep`, `addFlock`, `addPoints` and
`addPointsOn` return 0.

Deliberately absent: a clock or timeline of its own; any dependency on
SigilCompose or SigilWeave (both are test and demo links only); text
layout; windowing and swapchains; an image decoder (`textures::material`
takes a decode callback, `sigilworld/TextureSet.h` classifies names and
wires slots but never opens a file's pixels); the Substance SDK
(SigilSubstance renders `.sbsar` archives and hands over usage-keyed
images; this library takes them through the same by-usage door as any
other pipeline); the primitive attribute class on the GPU (the GPU
executor cooks the point class only); permutation operators;
count-changing operators.

## Build and test

Targets are `SigilWorld`, `world_test` (registered with ctest), and
`world_demo`. From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug -R world_test --output-on-failure
```

Running anything device-backed needs a Vulkan runtime. On macOS:

```sh
brew install molten-vk vulkan-loader
```

Without one, `World::create()` returns null and fills its `error`
string, and the device-backed tests **skip rather than fail**, so a
machine with no GPU stays green. The device-free half of the animation
tests still runs there — everything resolved by
`resolveAnimation(entt::registry&)`, plus the camera, path and layer
geometry pins — which is why the animation semantics stay pinned without
a GPU.

The demo renders a diegetic-panel scene headlessly:

```sh
./build/bin/Debug/world_demo [outdir] [assetdir] [frameCount]
```

It writes a set of camera shots as PNGs — a material lab
(`world_materials.png`: the fetched Poly Haven texture set on a floor,
a sphere and a torus, plus — when the Substance SDK is installed — the
SDK's sample archive rendered live at two parameter settings), a cockpit
view, a low orbit, a close panel, a poster panel, the stream, and the
marquee — plus a short
marquee flight sequence, a declared-camera flight frame
(`world_camera_flight.png`), and `comet_points.ply` exported from the
GPU-cooked point lanes. A frame count as the third argument dumps a
continuous `world_anim_%04d.png` sequence instead. The poster shot
carries an SVG decoded through SigilLoader when `assetdir` has been
populated by the optional `fetch_assets` target, and renders as a blank
panel otherwise.
