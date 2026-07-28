# SigilWorld

Namespace `sigil::world`, target `SigilWorld`, headers
`include/sigilworld/`. Diegetic surfaces in real 3D, rendered by
[Diligent Engine](https://github.com/DiligentGraphics/DiligentEngine)
(the `diligent-engine` port from the sigil-vcpkg-registry). Where
SigilShape's `Space.h` fakes depth inside one SkCanvas, SigilWorld owns
a GPU device and renders meshes with a depth buffer, MSAA, and a
PBR-lite shading model — panels IN a scene, not sprites OVER one.

## The three bridge contracts

- geometry is `sigil::shape::Mesh` — extrude/revolve/grid/cylinderPanel
  output uploads directly;
- panel content is any `SkImage` — a compose scene, a web view, a
  loader-decoded SVG — uploaded as the surface's baseColor texture
  (`Material::unlit` for self-lit screens);
- the camera is `sigil::shape::space::Camera`, so a Skia-composited
  scene and a World render agree about where things sit.

Headless by design: `World::create()` needs no window, `render()` draws
into an offscreen target (RGBA8 + D32, MSAA resolve), `readback()`
returns the frame as a raster SkImage, `savePng()` encodes it. A
swapchain path can join later without touching the scene API.

## Backend story (macOS)

Vulkan over MoltenVK: `brew install molten-vk vulkan-loader`. The
Metal backend is upstream-commercial and not in the port; the GL
backend is compiled but unwired here (the engine layer keeps D3D/GL
open for the Windows/Linux ports). Two local pieces make the Vulkan
path work without environment surgery:

- **VolkShim.c** — the port's static archives expect the volk loader;
  the shim compiles vendored volk (`thirdparty/volk`, MIT, pinned
  vulkan-sdk-1.4.321.0) with a replaced `volkInitialize` that also
  tries the absolute Homebrew paths (`/opt/homebrew/lib`) and points
  the ICD loader at MoltenVK's manifest when the environment doesn't
  (macOS `dlopen` searches neither `/opt/homebrew/lib` nor
  already-loaded leaf names, so stock volk cannot find a Homebrew
  install). `SIGILWORLD_VULKAN_LIBRARY` overrides the candidate list.
- **Shading**: one HLSL source (compiled to SPIR-V by the glslang
  Diligent ships) with a GGX + hemisphere-ambient PBR-lite pixel
  shader; matrices upload as raw column-major glm::mat4 dumps with
  `mul(M, v)` column-vector math — deliberately dodging HLSL
  `row_major` translation quirks. Diligent's Vulkan backend normalizes
  clip-space y, so the projection carries no flip (the
  `QuadAtPositiveYAppearsInTopHalf` test pins this).

`Material`: baseColor (alpha < 1 routes to the blended, depth-sorted
pass), metallic/roughness, emissive, texture (sRGB view), unlit, and
a uv window (`uvScale`/`uvOffset`, applied at sample time, clamped at
the edge). A mesh's baked `colors` lane rides along too — what pop's
`cookMesh` fades and import's `merged()` bake multiply into the shaded
color on the plain pipeline, and multiply the per-instance tint on the
instanced one (white when absent; both pinned by
`BakedVertexColorsTintBothPipelines`), matching the Skia painter's
`drawMesh` behavior. The window is LIVE like the colors: animate `uvOffset` on
the `MaterialComponent` and content scrolls across the surface with
zero texture uploads — the marquee mechanism, pinned by the
`UvScaleOffsetSelectsTexelLiveAcrossFrames` test. Geometry can move
the same way: `setSurfaceMesh()` replaces a surface's mesh in place
(UpdateBuffer when the vertex/index counts match, recreate
otherwise — vertex buffers are USAGE_DEFAULT for exactly this), the
towed-flag path pinned by `SetSurfaceMeshMovesGeometryInPlace`. And
generation itself can live on the GPU: `addSweep()` is the first
POP-style generator — the loop's control points sit in a device
buffer and a compute pass (parameter-exact `shape::Spline3` closed
Catmull-Rom, gravity rig) rewrites the surface's vertex buffer in
place; `setSweepWindow()` is two floats in a cbuffer and the re-sweep
runs at the next render(). Pinned by
`GpuSweepGeneratesAndSlidesOnTheGpu`; scaling head-room measured at
10x sections for +0.6 ms (the CPU path would have paid ~8 ms).
`addFlock()` is the second: a compute pass scatters `count` instances
along a window of the loop (stable per-point radial offsets, sin-field
drift, a tail-to-head tint ramp) and packs the InstanceAttribs stream
the instanced VS consumes — the points never exist on the CPU, and
`setFlockWindow()` streams the whole flock along the loop. Pinned by
`GpuFlockStreamsAlongTheLoop`; measured at ONE MILLION particles for
~5.8 ms/frame, all of it raster fill — the pack itself is a dispatch.

And the third is the COMBINATOR layer, `World::pop` — the
TouchDesigner POP lesson (GPU-resident attributes, generators vs
filters, nondestructive chains) made native. A `pop::Chain` is a
vector of operator VALUES — `SplineScatter` (generator: P, T, Dir),
then filters `Jitter`, `Noise`, `Ramp`, `Vary`, `LookAt`, `Math`,
each declaring the Lane it touches (P / Dir / Color / Scale / T,
packed as three float4 GPU buffers). `addPoints(stamp, chain,
material)` cooks the chain as sequential compute dispatches with UAV
barriers between and an implicit Copy-POP sink packing the instanced
stream; `setPoints(id, chain)` re-describes — same op kinds and count
is a parameter re-cook, anything structural rebuilds lanes and
bindings. The chain is data: edit a field, re-describe, nothing
mutates upstream. Pinned by `PopChainCooksAndRedescribes` (which
appends a Math mirror op live). v1 simplifications, on the roadmap to
lift: the conventional lanes only (custom named lanes later),
whole-chain re-cook (copy-on-write attribute references later), no
Feedback/Delete/Sort yet. MoltenVK gotcha for future operators:
RWStructuredBuffers sharing one element struct collide in SPIRV-Cross
MSL naming — give each lane a distinct struct type, and write whole
elements, not members.
`Lighting`: one sun + sky/ground hemisphere — the ambient base that
registry lights layer on top of.

## The entity layer

Surfaces ARE entities: World owns an `entt::registry` (the ECS the
repo's scene decoding already trusts) and `addSurface()` ids are entt
entity values. `Components.h` publishes `TransformComponent` and
`MaterialComponent`; a private GPU component carries the device
objects. `World::registry()` opens the door to systems: attach your
own components, iterate views, mutate transforms/material parameters
and the next `render()` draws the result (texture swaps still
re-create the surface — the SRB is baked). The scene layer below and
any gameplay/animation systems above meet in the same registry.

Lights and cameras are entities too:

- **`LightComponent`** — directional or point (color, intensity,
  direction/position, point falloff `range`: intensity fades as
  `(1 - (d/range)^2)^2`, so artist numbers stay in the sun's 1..5
  ballpark). `render()` gathers up to `kLightBudget` (8) light
  entities each frame and evaluates the same GGX lobe per light on top
  of the sun + hemisphere, so scenes without light entities render
  unchanged. `World::addLight()` is the one-line factory; mutating the
  component through the registry is live.
- **`CameraComponent`** — an entity with `active = true` overrides
  `setCamera()` (which stays the fallback); deactivate or destroy it
  to fall back. Keep one active: with several, whichever the registry
  iterates first wins.

## Instancing

`addInstanced(stamp, cloud, material, lanes)` renders one
`shape::Mesh` stamped at every point of a `shape::Cloud` in ONE draw —
the GPU sibling of `points::instance()` for the thousands range where
a merged mesh wastes vertices. `InstanceLanes` mirrors
`points::InstanceOptions` (scale + scaleLane/tintLane/orientLane + up;
same orientation basis, so a cloud renders identically merged or
instanced). The per-point data rides a second vertex stream (a 3x4
transform + tint, `INPUT_ELEMENT_FREQUENCY_PER_INSTANCE`) through a
parallel instanced PSO pair; tints multiply into baseColor in the
shared pixel shader. The flock is one surface: one id, one
`TransformComponent` moving the whole flock, one `MaterialComponent`
whose alpha routes it opaque or blended (the blended pass sorts the
flock as one item — instances are not sorted against each other).
`setInstances(id, cloud, lanes)` refreshes the points (UpdateBuffer
in place when the count is unchanged, recreate otherwise); an empty
cloud is a valid invisible flock awaiting points.

## The easel

`Easel.h` (`sigil::world::easel`, header-only) is the artist layer for
PLACING things — SigilShape's easel covers 2D marks, this one covers
the stage:

```cpp
auto stage = easel::stage(world);
stage.sun({-.4f, -.8f, -.5f}, 2.4f)
     .light({200, 300, 0}, cyan, 3)          // registry point light
     .place(mesh, gold).at({0, 0, 0}).turned(30).key("star")
     .panel(image, 380, 252).at({0, 60, 0}).key("hud")
     .swarm(cloud, quadStamp, glowMat).key("sparks")  // addInstanced
     .commit();                              // reconcile, return Stats
```

`at()/turned()/sized()/key()` style the LAST declared placement, so an
expression stays one sentence. `commit()` reconciles against the
previous commit and returns `scene::Scene::Stats`: placements ride the
Scene reconciler (by-value meshes get content-hash identity, so
re-declaring the same mesh is a keep, not a re-upload), swarms map to
addInstanced/setInstances by key (an unchanged cloud skips the upload
entirely), lights to `LightComponent` entities by declaration order.
The description is consumed per commit — re-declare each frame, and
keep the Stage alive across commits (a fresh Stage forgets what it
placed). `sun()/sky()/look()` apply on commit only when called.

## The scene layer

`Scene.h` applies SigilCompose's core lesson without importing its
kernel: describe the 3D scene as a value tree
(`scene::group/surface/panel` with keys, transforms, children), call
`Scene::render(root)`, and a reconciler diffs against the last render —
transform-only changes are `setTransform`, identical leaves are kept,
only genuinely new/changed surfaces upload. `render()` returns
`Stats{added, removed, moved, kept}` so pruning is observable, the same
visibility compose's ledgers taught. Identity is the key path; meshes
reuse by shared_ptr identity; `panel()` quads are cached per size so
panels are stable by construction.

## Demo and tests

```
./build/bin/Debug/world_demo [outdir] [assetdir]   # 6 PNG camera shots + scroll frames
./build/bin/Debug/world_test                        # skips without a Vulkan runtime
```

The demo builds a cockpit: three emissive UI cards, a curved
`cylinderPanel` ticker, a brushed floor slab, gold extruded star,
chrome superellipsoid, a glass pane — and, when `fetch_assets` has
run, the Ghostscript tiger decoded from SVG through SigilLoader onto a
poster panel (`world_poster.png` frames it). The stream — a spline
tube with camera-facing cards — is declared through the Scene layer,
and the dressing (two colored point lights pooling on the floor by
the props, a 3000-spark instanced swarm riding the arc) through the
easel.

The marquee (`world_marquee.png`, `world_marquee_flight_*.png`): THE
YARN — a ~24k-wu sparse ball winding wrapping the whole scene
(latitude swings seven times while the winding plane precesses twice,
coprime so the wraps spread), carried as a 301-wu-wide band painted
END TO END with one SigilCompose COLUMN in the PERPENDICULAR
orientation: every line of type reads ACROSS the band's width and the
stack advances ALONG the winding (the hanging-scroll orientation
riding the yarn). The column is packed with numbered SECTORS — a
numeral, a narrow-column paragraph (a ten-text pool, cycled), and a
graphics stretch (ruler / waveform / swatch run / dot ellipsis, each
parameterized by sector index so no two render alike) — with only
thin grow gaps between: no empty stretches anywhere on the loop. One
element tree, snapshotted ONCE as a vector SkPicture, SLICED straight
down into 10 tiles of 506x4096 (texture x = u, y = v — no transpose;
drawn mirrored in x so the wall's u-mapping restores unmirrored
glyphs), texel-continuous across seams. The cloth is gravity-rigged
like a real towed banner (parallel-transport frames roll upside-down
somewhere on any ball winding), and each frame every arc re-sweeps
one step forward ON THE GPU — the arcs are `addSweep()` surfaces, so
the march is ten `setSweepWindow()` calls (two floats each) and ten
compute dispatches; no CPU mesh exists for the band at all. A
300,000-particle `addFlock()` comet streams behind the dart on the
same loop (tail-fading tint ramp, drift noise), its instance stream
packed by compute each frame. Timed on Apple-silicon Vulkan-on-Metal,
1440x810 MSAA 4x, full set: ~2.7 ms/frame submitted+flushed — and at
one million comet particles, ~5.8 ms, all raster fill; the CPU never
touches a point. SigilWorld does NOT depend on compose
or weave; the demo composes them, world just samples SkImages. Tests
skip — not fail — when no Vulkan runtime exists, so CI without a GPU
stays green.
