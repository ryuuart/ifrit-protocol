# SigilWorld

SigilWorld draws 3D scenes on a real GPU without opening a window. You
give it triangle meshes and images; it owns the graphics device, an
offscreen colour-and-depth target with multisampling, and a
physically-based shading pass, and it gives frames back as CPU-side
raster images or PNG files. Scene objects are entities in an entity
registry, so transforms, materials, lights and cameras are ordinary data
you can read, mutate, or drive from your own systems. Geometry
*generation* can run on the GPU too: swept ribbons and a general
point-operator chain execute as compute dispatches whose
results never travel back through the CPU.

Namespace `sigil::world`, target `SigilWorld`, headers under
`include/sigilworld/`. One feature stands apart: `SigilWorldLight`
(`sigil::world::light`, `include/sigilworld/light/`) is emitters as plain
comparable values over glm alone — no device, no registry — so a consumer
that only needs to say where the lights are links it without the
renderer. The backend is
[Diligent Engine](https://github.com/DiligentGraphics/DiligentEngine) on
Vulkan — through MoltenVK on macOS.

## Using it

```cpp
#include <sigilworld/Components.h>
#include <sigilworld/World.h>

#include <sigilgeometry/mesh/Mesh.h>

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
    w->place(geometry::mesh::torus(140, 40), glm::mat4(1.0f), gold);

geometry::mesh::camera::Camera camera;
camera.eye = {0, 220, 620};
camera.target = {0, 0, 0};
w->setCamera(camera);

w->render();
w->savePng("frame.png");
sk_sp<SkImage> frame = w->readback();  // raster RGBA, opaque

// The prop id IS an entity. Move it and render again.
w->registry().get<world::TransformComponent>(world::entity(ring)).model =
    glm::rotate(glm::mat4(1.0f), 0.4f, glm::vec3{0, 1, 0});
w->render();
```

A textured panel is the same call with an image in the material:

```cpp
world::Material screen;
screen.texture = someSkImage;
screen.unlit = true;  // self-lit UI, no shading applied
w->place(geometry::mesh::quad(380, 252), placement, screen);
```

A material can wear others on top of it, each where a mask says — one
value, shaded once:

```cpp
world::Material hull = steel
    .over(rust, world::Mask::fromMap(steel.occlusionMap).invert().fit(0.35f, 0.75f))
    .over(moss, world::Mask::slope({0, 1, 0}, 0.55f, 0.9f));
w->place(mesh, placement, hull);
```

A scanned or authored material is the full texture set, read from the
folder a tool exported by its file names:

```cpp
#include <sigilworld/TextureSet.h>

for (const world::textures::TextureSet& set :
     world::textures::discover("assets/textures/metal_plate")) {
  world::Material plate = world::textures::material(set, decodeImage);
  plate.uvScale = {4, 4};  // the set tiles; lay it down four times
  w->place(geometry::mesh::torus(150, 60), placement, plate);
}
```

## The mental model

**Four things cross the boundary, and nothing else does.** Geometry
comes in as a `geometry::mesh::Mesh`. Panel content comes in as any `SkImage` —
whatever produced it. The camera comes in as a
`geometry::mesh::camera::Camera`, the same value type SigilGeometry's software
renderer uses, so a Skia-composited image and a SigilWorld render agree
about where things sit. Frames leave as raster `SkImage`s. There is no
window, no swapchain, and no scene file format.

**Three nouns for what a prop is made of.** A **texture set** is raw
maps by usage word (`baseColor`, `normal`, `orm`…), from any source. A
**material** is what a prop is made of — the parameters a pixel is shaded
with — and a texture set becomes one through `textures::material()`. A
**mask** is *where*: a scalar over the prop's surface from a constant, an
image channel, the mesh's vertex colour, the slope against an axis or the
height along one, shaped by `fit` and `invert`. One verb joins them:
`Material::over(material, mask, blend)` puts a material on top of another
where the mask says, and the result is a `Material` — so layering never
introduces a type, and every door that takes a material takes a layered
one. Up to `Material::kMaxLayers` layers are evaluated live per pixel:
each slot's maps are sampled to a parameter set, the sets are blended in
order (`Mix` lerps everything; `Add` and `Multiply` do so to base colour
and emission), the blended tangent-space normal is applied once, and one
shading pass follows. A layer is one material, not a tree — a layered
material placed as a layer contributes only its base.

**The vocabulary is small and used at every layer.** A **prop** is a
thing in the world: geometry, a placement and a material, one entity.
**place** is the verb that puts one there, in every layer — `World::place`,
`scene::place`, `stage.place` — and its variants say what the geometry
is: `placeStamps` (one stamp mesh drawn at every point of a cloud),
`placeChain` (a `pop::Chain` cooked on the GPU and stamped), `placeSweep`
(a ribbon swept along a loop by a compute kernel). What a prop is made of
is its **material**; where a mask over it says is a **mask** — the same
word `pop` uses. "Surface" is deliberately not an API word here: in
shading it means the material side, so it is left to prose.

**A prop can wear several materials, by face.** `place(mesh, model,
slots)` takes a `std::vector<Material>`; the mesh's `"Material"` prim
lane (its `.x` per triangle) says which slot each triangle wears — glTF
material indices and Houdini's `shop_materialpath` both arrive on that
lane through import, and `textures::materials(model, decode)` builds
the slot list a `Model::merged()` mesh expects. One entity, one
transform, one draw per slot; slot 0 is `MaterialComponent::material`,
the rest `MaterialComponent::slots`, all as live as each other. A mesh
without the lane, or placed with one material, wears slot 0 throughout.

**A prop id is an entity.** `place()` returns a `uint32_t` that
`world::entity(id)` casts to an `entt::entity`, and `registry()` hands
out the registry itself. Attach your own components to a prop, run
your own views over the same entities, mutate `TransformComponent` and
`MaterialComponent` directly — the next `render()` reads them. Entity 0
is consumed during initialization, so a valid prop id is never 0 and
0 always means failure. Lights and cameras are entities too
(`LightComponent`, `CameraComponent`).

**Four ways to get geometry onto the GPU, increasingly device-resident.**

1. `place()` uploads a CPU-built `geometry::mesh::Mesh`.
   `setMesh()` replaces it in place — matching vertex and index
   counts update the existing buffers, a different shape recreates them.
2. `placeStamps()` uploads one stamp mesh plus a per-instance stream
   built from a `geometry::mesh::Cloud`, drawing every point in one call.
   `setStamps()` refreshes the points.
3. `placeSweep()` builds its geometry with a compute kernel: control
   points live in a device buffer, and the vertex stream is written on
   the GPU. The points never exist on the CPU at all.
4. `placeChain()` cooks a whole `geometry::mesh::pop::Chain` — a generator plus a
   list of filter operators — as one compute dispatch per operator over
   GPU-resident attribute lanes. `placeChainOn()` feeds one cooked chain
   into another without a CPU round trip. `readChain()` copies the
   cooked lanes back as a `geometry::mesh::Cloud` when you want them. A scattered,
   drifting, tinted flock is a chain (`pop::on(loop).spread().noise().fade()`),
   not a door of its own.

**Windows are the animation primitive.** Every GPU generator takes a
`head` and `span` window into a closed loop. Sliding that window is two
floats and a re-dispatch — `setSweepWindow()`, `setChainWindow()` — so a
comet marching along a curve costs two numbers per frame and no geometry
work on the CPU.

**Three stacked authoring layers, and you can mix them.**

- *Imperative*: the `World` methods above. Call, get an id, mutate.
- *Declarative*: `Scene.h`. Describe the scene as a value tree of
  `scene::group()` / `scene::place()` / `scene::panel()` nodes with
  keys, call `Scene::render(root)`, and a reconciler diffs it against
  the previous description. Transform-only changes become
  `setTransform`; identical leaves are kept; only genuinely new or
  changed props upload. `render()` returns
  `Stats{added, removed, moved, kept}`, and `Scene::find(keyPath)`
  resolves a node's key path to its entity.
- *Fluent*: `Easel.h`. `easel::stage(world)` chains one sentence per
  thing on the set — `sun()`, `light()`, `place()`, `panel()`,
  `placeStamps()`, `placeChain()` (a GPU-cooked `pop::Chain`, compared by value
  on the next commit) — with `at()/turned()/sized()/key()` styling the
  last declared item, and `commit()` reconciling the lot through the
  Scene layer.

**Declared motion is an orthogonal door with a device-free half.**
`Animation.h` adds six components — `AnimatedTransform`,
`AnimatedMaterial`, `AnimatedLight`, `AnimatedCamera`,
`AnimatedWindow`, `AnimatedChain` — whose fields are `Animatable<float>`
lanes from SigilMotion. `AnimatedChain` reaches any operator dial of a
point chain by (operator index, field name) — `"amount"`, `"center.x"`,
`"seed"` — through SigilGeometry's `pop::setField`, and re-describes the
prop only when a lane moved. Attach them and the values follow whatever
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
| `sigilworld/World.h` | `World`, `WorldConfig`, `Material`, `Lighting`, `StampLanes`. Device bring-up, every `place*` door, camera and lighting setters, `render`/`readback`/`savePng`. |
| `sigilworld/Components.h` | The registry face: `TransformComponent`, `MaterialComponent` (`material` and further `slots`), `LightComponent`, `CameraComponent`, the `kLightBudget` constant, and `entity(id)`. |
| `sigilworld/Scene.h` | The declarative reconciler: `scene::Node`, `scene::group/place/panel`, `scene::Stack`, `scene::Scene` with `render`, `find` and `clear`. |
| `sigilworld/Animation.h` | Declared motion: the six `Animated*` components, `CameraPath`, `AnimationStats`, `resolveValue`, both `resolveAnimation` overloads, and the SigilMotion value vocabulary re-exported into `sigil::world`. |
| `sigilworld/Easel.h` | Header-only fluent stage: `easel::stage()`, `easel::Stage`. |
| `sigilworld/TextureSet.h` | The tools' texture sets read back into a world `Material`: `material()` from a set, from usage-keyed images, or from an imported `geometry::mesh::codec::decode::Part` (glTF's material, factors and all). The vocabulary it reads — `textures::Role`, `classify()` a file name, `roleForUsage()` a channel word, `discover()` a folder into `TextureSet`s — is SigilMaterial's, spelled here under the same names. |
| `sigilworld/light/Light.h` | Emitters as values: `light::Light`, its `Kind`, the `sun`/`point`/`spot` factories, `attenuation()` and `radiance()`, and the `kBudget` count. Nothing else in this library is needed to use them. |
| `sigilworld/Adapt.h` | Transitional. `surfaceOf(Material)`, `maskOf(Mask)`, `lightOf(LightComponent)` and `sunOf(Lighting)` — this library's own structs handed over as the values that own those subjects, for a consumer that takes those (`SigilUsd`'s writer does). |

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

**Samplers are three, shared.** Every map is a separate texture; the
sampler it takes — clamp, repeat, or the panorama's wrap-u/clamp-v — is
chosen in the shader by the slot's `tile` flag. The device caps samplers
per stage far below textures, which is why the choice is a flag and not
a sampler per map.

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

**Layer masks read the geometry as the prop shows it.** A `Slope` mask
dots the shaded normal (before the normal map) with its axis, so what a
pixel sees is what the mask sees — a sphere viewed from close has few
pixels whose normal points straight up. `Height` dots the world position.
`VertexColor` reads the mesh's own colour lane raw, before it multiplies
the base colour. `Map` masks sample at the prop's uv through the mask's
own uv window and tile flag.

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

**A panorama replaces the hemisphere, not the sun.** `Lighting::environment`
is an equirectangular image (u = 0.5 faces -Z, v = 0 straight up, turned
about +Y by `environmentRotationDeg`) uploaded as half float with a full
mip chain, so a float-decoded HDR keeps its range. The lit shader reads
diffuse light from a five-tap cosine hemisphere at a blurred level and
specular from the reflection direction at a roughness-chosen level,
weighted by the analytic split-sum environment BRDF; `ambient` scales
the whole term and occlusion darkens it. The sun and the registry lights
add on top exactly as before. The panorama is compared by pointer like a
material image and bound per prop, so a new one rebinds every prop
once; the mip levels are a *box-filtered* chain, not a proper
convolution — good for reflections and the look of a lit set, not a
radiometric irradiance.

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
to front** — alpha below 1 in `baseColor`, an `opacityMap`, or any
`transmission` (`Material::blended()` is the rule). A stamps prop is one
item in that sort — its stamps are not sorted against each other.

**Opacity is a channel; a cutoff makes it a cutout.** `opacityMap` reads
one channel (`opacityChannel`) into alpha alongside `baseColor.a` and the
base texture's own alpha; `alphaCutoff` above 0 discards fragments below
it instead of blending them (glTF's MASK), so leaves and grilles keep a
hard edge and write depth.

**Glass is screen-space refraction of the opaque pass.** A surface with
`transmission` above 0 reads the frame's opaque pass — resolved into a
mipped scene-colour texture between the two passes, only in frames that
have such a surface — where the view ray, bent by `ior` and pushed
`thickness` world units into the surface, exits; blurred by roughness,
tinted by `baseColor`, mixed in encoded space, its specular AND its
emission kept on top (edge-lit and neon glass glow at any transmission),
and written OPAQUE (it has composed its own background). The refracted
look goes through the shaded normal, so a normal map on glass shapes the
refraction — a map of vertical half-cylinders is fluted glass, no
geometry needed. Consequences:
glass sees only opaque surfaces (glass behind glass shows the opaque
scene, not the nearer pane); what lies outside the frame clamps to the
edge; and a very rough transmissive surface is a frosted blur of the
mip chain, not a scattering model.

**Swapping an image on a `MaterialComponent` is live, and costs an
upload.** `render()` compares the image pointers and the tile flag a
prop's binding was built from against the live component and rebuilds
the binding when any moved. Colours, scalars and the uv window are read
every draw and cost nothing; an image swap re-uploads every map on that
prop, so it is a change-of-scene operation, not a per-frame one.

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

**An `AnimatedChain` re-describes; an `AnimatedWindow` slides.** Both
are change-detected, but a moved chain lane goes through `setChain`
(a parameter re-cook — cheap, but a re-dispatch of the whole chain, and
a full rebuild when the lane it edits is a lookup table or a loop),
while head and span through `AnimatedWindow` are two floats. Drive the
window with the window component and everything else with the chain.

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
**declined**: `placeChain`, `placeChainOn` and `setChain` return 0 or do
nothing rather than silently cooking a chain with an operator missing.
Everything else runs here: the selector (`Select`), the deformers
(`Affine`, `Peak`, `Deform`), `Mix`, and every filter's `mask` —
a mask lane is just one more slot in the lane arena, and the masked
blend is the same expression the CPU cook uses. A chain led by a
`PointSet` runs too: its cloud is uploaded as the arena's initial
contents (laid out by `pop::seedAttrs`, custom lanes in the chain's
custom slots), its kernel is empty, `setChainWindow` has nothing to
slide, and every `setChain` on such a chain takes the structural path
because the cloud *is* the data.

**A `Deform`'s frame is computed once, on the CPU.** `pop::deformFrame`
normalizes the axis and orthogonalizes the bend direction for both
executors, and the kernel receives that frame rather than recomputing it,
so a degenerate axis or direction falls back the same way on both sides.

**A lookup-table edit is structural.** `pop::Lookup` stops ride an
immutable device buffer, so `setChain()` with a changed table takes the
full rebuild path instead of the cheap parameter re-cook. Same for a
generator's loop points.

**Compute buffers must not share an element struct.** On MoltenVK, two
`RWStructuredBuffer`s declared over the same struct type collide in the
shader translator's naming. Give each lane a distinct struct type, and
write whole elements rather than individual members.

**`readChain()` is synchronous** — a copy and a wait. It is a query
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

Public dependencies: `SigilGeometry` (the mesh, cloud, chain, spline and
camera types), Skia (`SkImage` in and out), `EnTT` (the registry is
public API), `SigilMotion` — public because `Animatable` appears in the
component surface, and safe to expose because SigilMotion links a
timeline library and nothing else — `SigilMaterialKit`, whose texture-set
vocabulary `TextureSet.h` spells and whose surface recipe `Adapt.h` hands
values over as, and `SigilWorldLight`. Private: Diligent Engine's core
and the Vulkan headers.

**The shading model is not defined here.** The metallic-roughness
surface, its masks and the stacking combinator belong to SigilMaterial:
`material::kit::surface` / `unlit` carry the params and the map slots,
`material::kit::maskConstant` and its siblings say where, and
`material::over` stacks them. What this library holds is a struct of the
same fields that the device pipeline reads directly, and `Adapt.h`
converts one to the other. The two follow the same rules — the packed
occlusion-roughness-metallic channels, the scalars a present map starts
at one, the normal convention — and the kit's copy is the canon; this
one goes away with the struct it serves.

The Vulkan loader is vendored (`thirdparty/volk`) and compiled through
`VolkShim.c`, which replaces the stock initializer with one that also
probes the Homebrew library path and points the ICD loader at MoltenVK's
manifest. This exists because macOS `dlopen` searches neither
`/opt/homebrew/lib` nor already-loaded leaf names, so an unmodified
loader cannot find a Homebrew MoltenVK install without environment
surgery. `SIGILWORLD_VULKAN_LIBRARY` overrides the candidate list.

The graphics vertex and pixel shaders are one HLSL source compiled at
runtime by the compiler Diligent ships. The compute kernels are separate:
they are authored in Slang under `shaders/` (`pop.slang` and `sweep.slang`, over the shared `sigilspline.slang` module), compiled to
SPIR-V at build time by `slangc`, and embedded. `slangc` is optional —
the build configures, compiles and links without it, and the whole
rasterization path works normally. What is unavailable in that build is
the compute generators: `placeSweep`, `placeChain` and
`placeChainOn` return 0.

Deliberately absent: any EXPORT of materials — no bake of a layered
material to a texture set, no writing of maps or scene files; this
library imports and renders, and the shader is the material language's
one executor (unlike `pop`, whose CPU reference the Skia painter needs);
a clock or timeline of its own; any dependency on
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

Targets are `SigilWorld`, `SigilWorldLight`, `world_test` and
`world_light_test` (both registered with ctest), `world_demo`,
`world_bench` (Google Benchmark: GPU cooks per frame by count and
operator mix, `readChain`, and a point-set re-upload; skips without a
Vulkan runtime — run it from a Release build) and `world_light_bench`
(the falloff per light per shaded point, by kind; no device needed).
From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug -R world_ --output-on-failure
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

It writes a set of camera shots as PNGs (and, when SigilUsd is built,
the lit material lab as `world_materials.usdc` with its images beside it
— every prop, its slots and materials, the sun, the camera) — a material
lab, twice
(`world_materials.png` under the studio panorama and
`world_materials_dark.png` with no panorama, no sun and a faint ambient,
where the emissive props are the light: the fetched Poly Haven texture set on a floor,
a layered sphere (steel, rust in its grooves by its own occlusion map,
moss on its upward faces by slope), a torus wearing two material slots
by face, a dark sphere lit only
by its emissive map (a
drawn circuit, tinted by the emissive colour), a clear glass sphere, a
frosted pane and a fluted edge-lit pane, the fetched Avocado
wearing the material its glTF carries, plus — when the Substance SDK is installed — the SDK's sample
archive rendered live at two parameter settings), a pop lab
(`world_pops.png`: the fetched Avocado's skin scattered into a cloud
that seeds a chain — a selected band twisted, the rest peaked, colour by
height — cooked on the GPU as one stamps prop), a cockpit
view, a low orbit, a close panel, a poster panel, the stream, and the
marquee — plus a short
marquee flight sequence, a declared-camera flight frame
(`world_camera_flight.png`), and `comet_points.ply` exported from the
GPU-cooked point lanes. A frame count as the third argument dumps a
continuous `world_anim_%04d.png` sequence instead. The poster shot
carries an SVG decoded through SigilLoader when `assetdir` has been
populated by the optional `fetch_assets` target, and renders as a blank
panel otherwise.
