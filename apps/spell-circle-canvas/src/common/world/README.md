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

### The transfer function, made symmetric (2026-07-28)

**Every world plate moved on this date, on purpose.** Panel textures are
uploaded `TEX_FORMAT_RGBA8_UNORM_SRGB`, so the sampler decodes with the
exact piecewise IEC 61966-2-1 curve; the render target is plain
`RGBA8_UNORM`, so the encode is the shader's to spell. It used to spell
`pow(c, 1/2.2)` — in BOTH the unlit and the lit/tonemapped branch — and
that is not the inverse of the piecewise curve. An unlit panel is
supposed to be a pure pass-through path, and it was not passing through:
a compose-authored texel came back out of the readback shifted, +9/255
at byte 8, +6 at bytes 2 and 24, +3 at byte 55, converging inside 1/255
only above ~96.

Both branches now use `LinearToSrgb()`, the piecewise encode
(`c <= 0.0031308 ? c*12.92 : 1.055*pow(c, 1/2.4) - 0.055`), constants
matched EXACTLY to `shape/Blend.cpp`'s `linearToSrgb()`. The cost is
that every rendered plate shifts: all 12 `world_demo` PNGs moved, mean
absolute delta 0.53–3.65/255, max 9/255, darkest content moving most
(`world_close_panel` 86% of bytes, the marquee frames ~19%). The
`shape_demo` panels do not use this path and are byte-identical.

`World.UnlitSrgbTexelSurvivesTheRoundTrip` is the pin: nine grey levels
sampled one texel at a time (`uvScale {0,0}`, so no filtering) must
survive upload → sRGB decode → shader encode → UNORM readback within
1/255. Restore the old `pow(1/2.2)` and it fails on the first four
levels.

### `clearColor` is encoded sRGB, by intent (2026-07-28)

The transfer-function work above raises the obvious next question, and
the answer is a deliberate asymmetry worth writing down: **every colour
in this API is linear EXCEPT `WorldConfig::clearColor`, which is encoded
sRGB.** `Material::baseColor`/`emissive`, `Lighting`'s sun/sky/ground and
the registry `LightComponent` colours all reach the target through the
shader, which shades in linear and applies `LinearToSrgb()` on the way
out. The clear does not pass a shader — `ClearRenderTarget` writes the
value straight into the `RGBA8_UNORM` target — so its components ARE the
bytes the background pixel gets.

The ruling is that this is correct, on three pieces of evidence:

1. **The default renders as what it says.** `{0.028, 0.03, 0.045}` lands
   as byte `(7, 8, 11)`, a near-black navy, and that exact triple is the
   single most common pixel in `world_poster`, `world_stream`,
   `world_cockpit` and `world_low_orbit` — 23–47% of each frame. Read as
   linear, it would encode to `(47, 48, 60)`, a mid slate that would
   wash the backdrop out of every "space" shot. The plates were authored
   and accepted against the dark reading.
2. **It was born an `SkColor4f`** (commit `0fda6c3`, retyped to
   `glm::vec4` only for the glm migration in `f6f0e94`), and this repo's
   `SkColor4f` convention is display-encoded: `shape/Blend.cpp` runs
   `srgbToLinear()` on `SkColor4f` components before going to OKLab.
3. **It is the meaning that matches the job.** Authoring a background is
   choosing the pixel you want to see; a display-space number is exactly
   that. Nothing composites the clear with lighting, so there is no
   linear-math argument pulling the other way.

Pinned by `World.ClearColorIsEncodedSrgbNotLinear`, which clears
`{0.5, 0.25, 0.75}` and requires bytes `(128, 64, 191)` — the existing
`RendersClearColorWhenEmpty` uses 0 and 1, the fixed points of the sRGB
curve, and so pins nothing about the space. Add an encode and the pin
fails on all three channels (it would read `(188, 137, 225)`), which is
the point: doing so would move every world plate a second time. A caller
who wants a background matching a linear `Material` colour encodes it
themselves with `shape::blend`'s `linearToSrgb` curve.

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
appends a Math mirror op live). Since written, custom named lanes and
`Lookup` have landed (see below); still v1: whole-chain re-cook
(copy-on-write attribute references later). Sort, Feedback and the
count-changing ops are RULED ON, not pending — see *Lookup, and the
permutation boundary*.

The executor boundary, and how it declines (2026-07-28): this executor
cooks the **point class** — named lanes in a device arena, packed into
the instanced stream. SigilShape's **primitive class**
(`Mesh::prims`, see its README) has no counterpart here, because the
GPU sink is instanced stamps rather than a `Mesh` value. So
`addPoints`, `addPointsOn` and `setPoints` **DECLINE** (return 0 / no
change) any chain carrying `pop::Promote`, exactly as they decline
`pop::MeshScatter` — never drop the op and cook something subtly
wrong. Pinned by `PrimitiveClassChainsAreDeclinedNotDropped`. Note for
whoever lifts this: the op variant's ORDER is ABI — `popPsoIndex()`
maps variant index to compute PSO, `Promote` (variant 11) has no PSO
of its own, and new ops must be APPENDED to the variant, never
inserted.

MoltenVK gotcha for future operators:
RWStructuredBuffers sharing one element struct collide in SPIRV-Cross
MSL naming — give each lane a distinct struct type, and write whole
elements, not members.

### The variant→PSO map is a table now (2026-07-29)

`popPsoIndex()` used to be arithmetic — `variantIndex <= 7 ?
variantIndex : variantIndex - 1` — encoding the single hole
`MeshScatter` left. It was already documented as WRONG for any variant
past index 10 (it would land on the copy-back entry and cook the wrong
kernel), and unreachable only because validation happened to decline
those. It is now `kPopOpPso[]`, **one row per `pop::Op` alternative,
index-aligned**, with `kPopNoKernel` for the CPU-only kinds — and a
`static_assert` against `std::variant_size_v<pop::Op>`, so appending an
op without ruling on its row is a BUILD failure instead of a runtime
mystery. The same table drives validation (`popChainRunsOnGpu`), so the
mapping and the decline list cannot drift apart, and `bindPopSrbs`
refuses `kPopNoKernel` outright rather than binding a neighbour's PSO.
Pinned by `EveryGpuOpMapsToItsOwnKernelAndAgreesWithTheCpu` — a chain
carrying all eleven GPU ops, compared to the CPU cook lane by lane.
(Control: restoring the old arithmetic fails it, while the two
pre-existing parity tests still pass — the hazard really was unpinned.)

### Lookup, and the permutation boundary (2026-07-29)

`pop::Lookup` runs on **both** executors: a per-point remap of one
attribute from another through a table of stops
(`key = dot(from, weights)`, remapped from `[low, high]` onto the
table's span, linearly sampled). Count-invariant and per-point, so it
is one more dispatch — `CSLookup`, formula-matched to the C++ twin. The
stops ride an IMMUTABLE `g_Table` structured buffer holding every
Lookup op's table concatenated in chain order; each dispatch carries
its own `(offset, count)` in `PopParams`. Consequence worth knowing: a
**table edit is structural** — `setPoints` compares the concatenated
table and takes the rebuild path, because the buffer it uploaded is
immutable. Same reasoning as the generator's loop points.

`pop::Sort` is **declined**, and that is a ruling, not a gap. A
permutation is not a per-point map: it does not fit one-kernel-per-op
over a fixed arena, it would want a sorting NETWORK (log²(n) dispatches
plus a ping-pong), and its motivating consumer is the *Skia* point sink
— which draws in chain order and has no depth buffer, so painter order
is authored, not rasterised. The CPU executor does it; this one refuses
the chain the way it refuses `MeshScatter` and `Promote`. Pinned by
`PermutationClassChainsAreDeclinedNotDropped`.

Filed, not built: **Copy / Merge / Delete** change the COUNT and so are
not chain ops at all — see shape's README for the ruling. **Particle /
Feedback** need cook N to read cook N−1, i.e. state and a clock; world
owns no clock by design (see *Declared motion*), and a Chain whose cook
is not a pure function of its own values stops being a description.
**Field** wants a field-source currency (SDF/volume/texture) shape does
not have yet; `Noise` is the procedural field we do have. **Line** is a
generator, and the better shape for it is an `open` flag on
`SplineScatter` rather than a fourteenth alternative.
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
any gameplay/animation systems above meet in the same registry — and
since 2026-07-29 one of those systems ships here: see **Declared
motion** below for the `Animated*` components and `resolveAnimation()`.

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

## Declared motion (2026-07-29)

`Animation.h` adds the SECOND animation door, next to — never instead
of — the imperative setters. Until this date world's only way to move
anything was two floats per frame: compute a value, call a setter. That
still works, is still pinned, and is still the right tool for a one-off
poke. What it could not do is DECLARE. `Animatable<float>` is that
declaration, and it became reachable here when `Transition`,
`Transitioned<T>`, `ease::`, `bind()` and `Animatable<T>` moved out of
SigilCompose into `<sigilmotion/Animation.h>`: SigilMotion links
choreograph and nothing else, so **SigilWorld now depends on SigilMotion
(PUBLIC)** and remains compose-free.

```cpp
choreograph::Output<float> phase{0};
ticker.timeline().apply(&phase).then<ch::RampTo>(1.0f, 8.0f);

auto &reg = world.registry();
reg.emplace<AnimatedMaterial>(entity(bandId)).uvOffsetX =
    bind(&phase).target(0.0f, 1.0f);
reg.emplace<AnimatedTransform>(entity(dartId), base).yawDeg =
    bind(&phase).map(ease::inOutBack()).target(0.0f, 360.0f);

for (int frame = 0; frame < 600; ++frame) {
  ticker.tick(1.0 / 60.0);   // the CALLER owns the clock
  world.render();            // resolves, then draws
}
```

Four components, all opt-in, all additive: `AnimatedTransform`
(base matrix + x/y/z + yaw/pitch/roll + scaleX/Y/Z, composed as
`base * T * R * S` — the same order as `scene::Node::localMatrix()`),
`AnimatedMaterial` (opacity → `baseColor.w`, emissiveStrength,
uvOffsetX/Y, uvScaleX/Y), `AnimatedLight` (intensity, position x/y/z)
and `AnimatedWindow` (a generator's `head`/`span`).
`resolveAnimation()` is the system, in two halves: a free function over
a bare `entt::registry` for the component lanes, and a `World&` overload
that adds the window lanes and is what `render()` calls first thing.
Both return `AnimationStats{transforms, materials, lights, windows}` —
the same "pruning is observable" contract `scene::Scene::Stats` sets.

Five rulings, because each of them is a thing we chose NOT to build:

1. **Every lane is a float.** Not `Animatable<glm::vec3>`; a position is
   three lanes. `bind()`'s normalise → curve → affine chain
   (`source`/`window`/`map`/`target`/`quantize`/`clamp`) is FLOAT-ONLY,
   and that chain is most of the value of the door — a vec3 slot could
   hold only a plain constant or a raw binding, i.e. a weaker lane
   wearing a fancier type. `Animatable<float>` converts implicitly from
   float, so `at(0, 60, 0)` still reads like a position.
2. **World owns no clock.** No `world.tick()`, no `render(dt)`, no
   FrameClock in here. `FrameClock::tick()` reads `steady_clock`; a
   world that ticked one inside `render()` would make every headless
   plate a function of wall time, and the 13 `world_demo` artifacts must
   be byte-reproducible. The caller steps a `motion::Ticker` with the
   delta it chooses and `render()` stays a pure function of whatever the
   Outputs hold. Pinned twice — `WorldAnimation.SameTimeYieldsTheSameNumber`
   (frame *k* resolves bit-identically across runs, and a different dt
   sequence lands elsewhere, so the claim is not about a constant) and
   `WorldAnimation.AnimatedFrameRendersIdenticallyAcrossRuns` (frame 30
   of two fresh Worlds is byte-equal, frame 31 is not). Inject a
   wall-clock term into `resolveValue` and both fail.
3. **`animate(...)` lands on its SETTLED value.** The lanes accept
   `animate(to(v))` / `animate(from(a).to(b))` because they are the same
   slot type compose uses — but ramp-on-change needs a CHANGE event, and
   world has no describe/diff over components (they are mutated in
   place). So a transitioned value resolves to its target with no ramp,
   the way compose's `snapshot()` bakes one. To actually ramp, put the
   ramp on the timeline and bind it.
   (`AnimateFormLandsOnItsSettledValue`.)
4. **The GPU-window lane is included, but only because it is
   change-detected.** `AnimatedWindow` is the one lane in front of a
   compute RE-COOK rather than a live shader parameter, and it is
   exactly where an `Animatable` would have been a trap: an
   unconditional write marks the surface dirty every frame and
   re-dispatches forever, even while the bound Output sits still — a
   300k-point flock re-scattering for nothing. So resolve writes ONLY
   what moved, and for one rule everywhere, every other lane is
   change-detected too (compared against the destination, so the first
   resolve of an already-correct value is a no-op as well). A constant
   lane costs exactly one re-cook, ever. The window routes through the
   three public setters, each a documented no-op on a surface of the
   wrong kind, so one component covers sweeps, flocks and pop chains
   without world publishing which kind an entity is. Pinned by
   `WindowLaneReachesTheGpuAndRecooksOnlyWhenItMoves` (which also
   requires the animated cook to match the imperative
   `setPointsWindow` cook point for point) and
   `ResolveIsIdempotentAndReportsOnlyWhatMoved`.
5. **Material/light lanes are `optional`, transform lanes are not.** A
   transform component describes the WHOLE placement, so an unmentioned
   lane genuinely means "no translation" / "unit scale". Material and
   light lanes are PARTIAL overrides of a component the caller also
   authors, so plain defaults would slam a pane's authored alpha 0.4 to
   1 the moment you engaged `uvOffsetX`
   (`MaterialLanesOverrideOnlyWhatTheyDeclare`).

Declined on purpose, and why: **colour** (three independent linear-RGB
float lanes are the wrong default for a colour ramp — this repo runs
OKLab in `shape::blend` for precisely that reason, so a colour lane
wants a colour type, not four floats in a trench coat); **the camera**
(the "two floats per frame" pain scales with the number of surfaces, and
there is exactly one camera, already moved by one `setCamera()` call —
7 lanes for eye/target/fov buy nothing today, and it is the obvious next
lane if a declared dolly is ever wanted); **quaternion rotation** (slerp
needs an `Output<glm::quat>` and gets no `bind()` chain, so euler degrees
— what `scene::Node` and the easel already speak — is the honest v1);
**`pop::Chain` operator parameters** (a jitter amplitude or a ramp
endpoint is just as animatable as a window, but the chain is a SigilShape
value and putting `Animatable` inside it is that library's design, not
this one's); and **`Material::texture`** (the SRB is baked, so it is not
a live lane at all). `world_demo` was deliberately left on the imperative
door: all 13 artifacts hash the same before and after this change.

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
./build/bin/Debug/world_test                        # 43 tests; device-backed ones skip without a Vulkan runtime
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
packed by compute each frame.

The slice goes through compose's door now (2026-07-28). It used to be
three canvas calls spelled out in `world_demo.cpp` — `translate(w, 0)`,
`scale(-1, 1)`, `translate(0, -k*h)` — and that expression was derived
by hand and gotten wrong twice, because the mirror is not the picture's
business: the sweep wall samples its own u backwards, so the tile has
to be baked reversed to read forwards on the band. It is now one call,
`sigil::compose::tiles::window({506, 4096}, k, Flow::Down,
Facing::Mirrored)`, with the orientation trap documented on compose's
side and the marquee's own tile geometry pinned in `world_test`
(`WorldMarqueeSlice`) so a change over there cannot silently mirror or
step the band. The migration is byte-identical: all 13 demo artifacts,
PNGs and `comet_points.ply`, hash the same before and after. The strip
also draws through `tiles::sliceable()`, the same picture re-recorded
behind a bounding-box hierarchy so each tile replays only the ops that
meet it — on this strip (10 tiles of 506x4096 over a 40960 px column,
5202 ops) 6.81 ms of raw replay becomes 0.39 ms of build plus ~4.4 ms
of replay, ~30% off the marquee's one-time bake, pixel-identical.
Timed on Apple-silicon Vulkan-on-Metal,
1440x810 MSAA 4x, full set: ~2.7 ms/frame submitted+flushed — and at
one million comet particles, ~5.8 ms, all raster fill; the CPU never
touches a point. SigilWorld does NOT depend on compose
or weave; the demo composes them, world just samples SkImages. Tests
skip — not fail — when no Vulkan runtime exists, so CI without a GPU
stays green.
