# SigilWorld — the device executor

The chapter on the executor a scene is drawn by, which rasterises on a
GPU: what it does with each pass, what a material reaches the device as,
the two seams below this library whose device executors stand beside
their CPU ones, what a shaded scene can and cannot say there, where its
shaders come from, and the one device it all stands on. `README.md`
beside the library is the front page.

## The device executor

**No header of this library names a Diligent type.** The executor holds
plenty of them — targets, pipelines, bindings, the buffers and textures
a residency put on the device — and every one of them is spelled in
`diligent/Gpu.h`, which is this feature's own header and not the
library's. The words for them come from SigilGeometry's device feature,
whose headers likewise sit beside its sources rather than under its
include tree; this target puts that directory on its PRIVATE include
path, which is the one door onto those interfaces, and walks through it
because a frame's passes ARE engine calls. `Import.h` and `Runtime.h`,
the two headers a consumer reads, spell a device, a scene and a runtime
and nothing of the engine.

`diligent::runtime(device)` performs a frame's passes on the device the
`diligent/` feature brought up. What it does with each:

- a **geometry pass** under a light that casts opens with THE SHADOW
  STAGE, before its target is cleared: each casting light's map is drawn
  into one depth atlas, in a tile of its own, from the bodies that cast,
  writing depth and nothing else ([LIGHTING.md](LIGHTING.md) says which
  lights and bodies, and how the maps are fitted and read). A pass under
  no light that casts draws no map and binds no atlas. Then it rasterises
  the bodies its realisation leaves it
  into a device texture, DEPTH-TESTED, from a pipeline built out of each
  material's own `Target::Slang` body, into a half-float target holding
  linear light. It draws the bodies laid over normally first, in the
  back-to-front order the extracted view carries, writing depth for an
  opaque one only; then, in a frame holding a body that SEES THROUGH
  ITSELF — one whose surface transmits or distorts — it copies the lit
  colour, deriving a chain of smaller levels each the average of the one
  above, and the depth, and draws those bodies back to front reading that
  one copy; then the blended ones — added, multiplied, screened, writing
  no depth by their own say, or fading into the scene — back to front
  over them all, each in the blend state its surface names. A body that
  fades reads the depth copied for the bodies that see through
  themselves, or copies it itself before the first of them when no such
  body is in the frame. So the phases are: laid over; copy; seen through;
  blended — and a frame with nothing that sees through itself copies
  nothing and draws laid over, then blended, as it would without the
  middle phase. A transmissive body sees the opaque scene, never another
  transmissive body nor a blended one; a glow standing behind glass is
  not seen through it. The order and the depth buffer agree wherever the
  view's centroid sort is right, and where it is not the depth buffer is
  the one telling the truth.
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
  only when its length was even. A grade is one draw; a composite of two
  layers is one draw reading both, and of more lays each further layer
  over the first under a blend state. A masked grade is one draw too —
  the picture, and its grade laid over it where the coverage stands;
  any other masked operation copies the picture first and reaches it
  through the coverage. A device cannot sample an image it is drawing
  into, so every stage that reads and writes at once takes a working
  target of its own — including a pass that declares it writes what it
  reads.
- **A BLUR AND A BLOOM SPREAD AT A REDUCED LEVEL.** A blur is a separable
  Gaussian. A narrow one is two draws at full size; a wider one is taken
  at the smallest level — the frame's size halved one to four times —
  where its Gaussian is still two of that level's texels wide: the
  picture halved down to it (each halving averages four by four texels
  of the level above), the Gaussian taken there, and the result read
  back at full size through a cubic B-spline in the same draw that lays
  it into the target. The Gaussian taken at the level is narrowed by
  what the halvings and the read back spread on their own, so the three
  together are the width asked for. **THE RULE: a blur's footprint is
  honoured at the level it is taken at,** so its picture may differ from
  the full-size Gaussian by what that level cannot resolve — a few
  levels of eight bits inside the picture, a few more within one level
  texel of its edge, where the level holds its edge over several pixels.
  A bloom takes its threshold while halving once, so a single bright
  texel spreads as it would at full size; spreads the result to a ninth
  of its radius at the level that width allows, spreads that again by a
  third of the radius at a smaller one, and lays the picture and both
  spreads together in one draw at full size. Its spreads carry light and
  no alpha, so they are held in three packed floats where the device
  draws into them (`R11G11B10`), and in the frame's own half floats
  where it does not.

**A cooked artefact is named by a NUMBER, not by its address.** A
renderer holding buffers per geometry keys on `Draw::geometry`, which the
resource store counts up once for the process. An address cannot serve:
an artefact that is dropped frees its memory and the next one cooked can
land on it, and a count per store would hand two scenes' artefacts one
number.

**EVERY BODY IS ONE INSTANCED DRAW.** The stamp's mesh is uploaded once
under its artefact's number, its instances — position, orientation,
scale, colour, texture window and four named scalars each — into a second
buffer that steps once per instance under the same number, and the body
is one indexed draw of as many instances as it has. A body that is not
instanced geometry is drawn the same way with one neutral instance, so
there is one vertex stage, one layout and one pipeline per material and
blend; a neutral instance changes no vertex by so much as a rounding. A
body facing the camera with instanced geometry turns each instance where
it stands in that vertex stage. The copies of one body are drawn in the
order its instances stand in and are not sorted among themselves, which
an additive surface does not mind and a translucent one laid over may.

**A STAMPED POINT SET IS AN ARTEFACT LIKE ANY OTHER.** A geometry pass
draws the stamps of every point set it reads, every frame, as instanced
geometry — the stamp once and an instance per point — formed ONCE per
distinct (cloud, stamp) and uploaded once, so a scene that has not moved
between two frames is neither instanced again nor re-uploaded.
`Targets::stamped()` is where it is formed and held;
`world::stampKey()` is the number the two values fold to, read from
their CONTENT because that is what "the same stamping" means — an
address cannot say it and a shape cannot — and the device keys its
upload by that same number. The fold BUCKETS the lookup and the pair
itself decides it: an entry holds the cloud and the stamp it was formed
from, so two pairs that happen to fold together are two stampings under
two numbers and neither is ever served the other's instances. `Targets::stampings()` counts what has
actually been formed, which is what the test asserts does not move
across three frames of a still scene. A stamping no pass asked for in a
frame is let go at the end of it.

**The pixels stay on the device.** The frame's resources are device
textures for as long as the runtime lives, and nothing crosses back until
something asks for a resource BY NAME — a declared `readback`, whose
`then` offers the result to a callback that names it or not, or the
picture being presented. That is what `Targets::source` is: the
executor answers for one name at a time, so a frame that reads nothing
back pays for no crossing at all. What last frame means is the
executor's too: it keeps each resource's previous texture beside its
current one and exchanges the two when the next frame opens, which is
what a pass's `previous()` reads.

**What a material reaches the device as.** A recipe's Slang body is one
function, `float4 surface(float2 uv)`, returning the surface's own colour
with straight alpha. The scaffold around it supplies the vertex stage,
the lighting and the premultiply, and every uniform — the recipe's
parameters and the scaffold's own — is written at the offset the compiler
REPORTED for it, so a body that declares one more parameter moves nothing
a renderer has to be told about. The first variant axis is `kVariantLit`:
without it the lighting, the uniforms it reads and the loop over the
lights are not in the compiled program. Beside it, the lit build is
specialised on the kinds of light that reach the body —
`kVariantAreaLights` when one of them has a size,
`kVariantCascadedShadows` when one is read through a directional light's
cascades, `kVariantPointShadows` when one is read through a point light's
six faces — so a program holds the code of the lights it is drawn under
and no other. Which build a body is drawn
with is the body's own answer as much as the pass's — a surface that is
its own light takes the unlit one whatever the pass asked for. The mesh vertex layout is not a
variant axis, because there is one — position, normal, uv and tint, with
the lanes a mesh does not carry filled in on upload; nor is the blended
build, which is the blend and depth state a pipeline is created with.

A pipeline is assembled through the engine's own create-info builder,
and its blend and rasterizer states are the engine's named ones — a
premultiplied-alpha blend, an additive one, and blending off for a draw
that replaces what stands — beside three written out for a body's
multiply, screen and modulate, exact over premultiplied light wherever
what stands behind is opaque; a mode no blend state can express would
need to read its own target and is laid over normally, said once; solid fill culling back faces at the
counter-clockwise winding, or culling none. Two things stay written out.
The mapping from an `SkBlendMode` to one of those states is ours because
`SkBlendMode` is Skia's word and no Diligent type names it. And the depth
comparison is LESS-OR-EQUAL where both named depth states compare
strictly, so that a body redrawn over itself does not lose to the depth
it wrote the first time.

A material whose recipe has no Slang body is painted in the colour the
frame extracted, and the program cache has already reported the recipe
and the target once.

**The map a body is dressed with** is the `material::Texture` in its
surface's base-colour slot. It is read off the
material ONCE, at extract, so an execution reading a body never walks a
material tree; a mesh carries normalised uvs and a texture states its
placement in the image's own pixels, so the placement is carried across
rather than copied — inverted, because a texture's matrix puts the image
INTO the space it is sampled in and a lookup goes the other way, and
taken through the image's size, so a scale and an `at()` mean the same
thing and point the same way on a mesh as they do in a plane. The map
multiplies the SHADED colour rather than the surface before it.

The map is read through the sampler its texture asked for — one per
filter, made once with the device and picked per draw. Everything with no
texture to ask, a target a post stage reads among them, takes the linear
one.

**Every OTHER sampled slot the recipe declares is bound too** — normal,
roughness, metallic, occlusion, emissive, opacity — from the material's
own slots, by the NAME the program declared them under. A slot
whose texture is the neutral dressing a surface is built with is left
UNBOUND, and an unbound slot reads one white texel: the neutral for every
map a scalar multiplies, and the one value a tangent-space normal cannot
mean, since such a normal's x and y are centred on a half and only its z
reaches one. So white IS "no map here", exactly and with no threshold to
pick — which is what lets a body tell a dressed slot from an undressed
one, and what keeps a surface nobody dressed the picture it already was.
A slot the device binds holds an image texture, put on the device as
colour or as data by what the texture states or the slot is for, or a
material whose Slang body `material::inlined` writes into the body that
reads it. A slot a child material brought is read with its own texture's
sampler; the surface program's own slots share the base-colour map's. A
material with no Slang body in a slot has nothing the device can compile:
it is reported once, naming the slot, and reads as empty.

**A body states what its surface IS.** The scaffold declares a set of
variables a body writes and it reads: `gSurfaceNormal` in tangent space,
`gSurfaceMetal`, `gSurfaceRoughness`, `gSurfaceOcclusion`, the clear
coat's `gSurfaceClearcoat`, `gSurfaceClearcoatRoughness` and
`gSurfaceClearcoatNormal`, the transmission terms `gSurfaceTransmission`,
`gSurfaceIor`, `gSurfaceThickness`, `gSurfaceAttenuationColor`,
`gSurfaceAttenuationDistance` and `gSurfaceDispersion`, the displacement
`gSurfaceDistortion`, and `gSurfaceEmission`, the light the surface gives
off of its own, in linear radiance. A body that writes a lobe's variable
states the lobe, and only then is the lobe's term compiled into its
program — `SIGIL_TRANSMISSION` and `SIGIL_DISTORTION` beside the sheen's,
the brushing's, the film's, the specular's and the clear coat's — so a
surface program states the coat only when its weight or its map does, and
a body that writes none
of them compiles to the scaffold every other body does. The colour a body returns is ENCODED, as an author
types it; the scaffold decodes it.

**Every lit pixel is shaded on its own.** The tangent frame a normal map
is authored against is read off the screen derivatives of the view
position and the uv, because a mesh carries no tangent lane and every
generator would have to fill one. What the shading is — the lights'
falloff, the microfacet terms, the clear coat, light from everywhere,
colour and the tone curve — is **[LIGHTING.md](LIGHTING.md)**.

## The swept rings on the device

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
it sweeps with. Instanced geometry is likewise cooked by whoever ends a
chain in it, on the runtime that caller names; a compute pass is the one
place the frame cooks a chain itself.

## The point operators on the device

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
unsupported operator stops a cook: `Smooth`, `Relax`, `Cluster` and
`Transfer` each read points they do not own — the two beside a point in
the chain, the points near it in space, the whole set at once, or another
cloud's points entirely — `Sort` is a permutation, `Delete` changes the
count a per-point map cannot change, `Promote` addresses primitives no
sink has formed yet, and `Noise` and `Deform` are defined in terms of a
library sine, which is a different function from the polynomial a
portable kernel would have to use.

**The two tiers are held to BIT IDENTITY, not to a distance.** That is
the one place in this library where two backends are, and it is possible
only because the operators are one piece of arithmetic compiled twice
under a float model pinned at both ends. Three things pin it, and each of
them is load-bearing: the generated C++ is compiled with
`-ffp-contract=off`; the SPIR-V is compiled under `-fp-mode precise`, so
it carries one `NoContraction` decoration per arithmetic result; and
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

## What a lit scene can and cannot say

The lighting model is **[LIGHTING.md](LIGHTING.md)**. What the device
adds to it:

- a recipe's `Target::Slang` body IS run: the parameters, the sampled
  slots and the arithmetic a material describes reach the pixels. A
  recipe with no Slang body is painted in the colour the frame
  extracted.
- **a light reaches only the bodies its light link names.** Each body is
  drawn under the lights whose `Light::lightLink` contains it, asked of
  the same member a pass's collection reads; an unlinked light reaches
  every body. A light's `Light::shadowLink` narrows, the same way, which of
  the bodies that cast are drawn into its shadow map. A draw carries at
  most sixteen lights.
- a surface that says light does not reach it — `material::surface::unlit`
  — is drawn with the unlit build: what it shows is its base colour and
  the mesh's own tint, as authored. The answer is read off the material
  once at extract, beside the map, so it is a property of the BODY and
  not of the pass that draws it.
- a texture states how it is read BETWEEN texels and the device honours
  it. Nearest keeps a texel's edge hard and takes no mip level with it,
  because blending two levels is the same bleed arriving by the other
  door; linear reads between texels and between levels.
- a STACK of surfaces SHADES as a stack. `material::over` composes one
  Slang body out of its three operands' own bodies, so both surfaces are
  evaluated and their colours mixed by the mask, and what each of them
  said per pixel — a normal, a roughness, a metal weight, an occlusion,
  a coat — is mixed by the same coverage, so a top wearing a normal map
  bumps the surface only where the mask lets the top show. A stack whose
  operands do not all have a Slang body is not composed and is drawn as
  the surface at the bottom of it.
- **a stack running its own body owns every map in it.** The frame
  extracts the map of the material at the bottom of a stack, because
  that is what can be read off a stack without running it; where the
  composed body IS run it samples both operands' maps itself, through
  slots of its own, and the scaffold is handed no map at all — otherwise
  the bottom's would land a second time and over the whole face rather
  than where the mask says.
- `alphaCutoff` turns the opacity map into a CUTOUT — below the
  threshold the surface is absent rather than translucent.
- a surface program declares ten sampled slots and the scaffold six of
  its own — the base-colour map, the two panoramas and their two
  convolutions, and the split sum's table — which is the sixteen a
  fragment program may declare. A stack of two surface programs is over
  that budget and is reported as such.
- a foreign texture — one another engine, a decoder or a capture painted
  with the graphics API — reaches a slot through
  `diligent::importNative`, and is bound where it stands. It answers no
  host image at all, so a renderer on another device draws the body
  undressed rather than something it invented.

- **the panorama SHOWN behind the scene is `EnvironmentOptions::shown`**,
  drawn as one triangle over the target with each pixel reading the
  panorama along the ray the eye looks through it, at that strength and
  `EnvironmentOptions::blur`. Past a `groundRadius` of zero the panorama
  is projected onto a sphere of that radius centred at the world origin: the pixel reads
  where its ray leaves the sphere, along the direction from the centre
  to that point, so an eye moving through the scene sees the horizon shift
  the way it would outdoors. An eye at the centre, or on or outside the
  sphere, reads by direction — the sky at infinity, which is what a
  radius of zero means. The projection reaches what is shown alone: what a
  surface mirrors stays at infinity.

## What the environment map does not reach

- **What a bent ray leaves the picture to see.** Glass reads the copy of
  the scene behind it, and where its bent ray lands off the picture or on
  something nearer than itself it reads the panorama along that ray
  instead — the one place a transmissive body still sees the world rather
  than what is behind it on screen.

## Where the shaders come from

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
backend's own programs live — the scaffold in its lit and unlit
builds, the sky and the post stages — each compiled
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

## The one device

**The device is not made here.** Diligent creates the Vulkan device and
cannot attach to one that already exists, so the single point where a
device is made has to sit at or below every consumer of one — which is
SigilGeometry's `device` feature, and its `reference/DEVICE.md` is canon
for what a device is, how it is adopted and what the shared queue's lock
rules are.
This library takes one and executes a frame's passes on it:

Bringing the device up — `DeviceConfig`, `Device::create` and the
`error` it fills when there is no Vulkan runtime, the Diligent side that
is never null on a created device and the adopted Graphite side that is
null when adoption failed — is SigilGeometry's, and its
`reference/DEVICE.md` shows it. What this library adds is one line over
that device:

```cpp
#include <sigilworld/diligent/Runtime.h>

frame.runtime(world::diligent::runtime(*device));
```

There is no Metal path here, because Diligent has no Metal backend:
`create` fails on a machine with no Vulkan runtime, and such a machine
draws no scene — a frame whose passes carry no runtime runs none of them
and `Scene::error()` says so.
