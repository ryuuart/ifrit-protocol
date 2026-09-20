# SigilWorld — the device executor

The chapter on the tier that rasterises on a GPU: what the executor
does with each pass, what a material reaches the device as, the three
seams below this library whose device executors stand beside their CPU
ones, what this tier can and cannot say about a shaded scene, where its
shaders come from, and the one device it all stands on. `README.md`
beside the library is the front page, and its CPU-tier paragraph is the
twin of the one here.

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
  picture is copied first and the operation reaches it through the coverage.
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
upload by that same number. The fold BUCKETS the lookup and the pair
itself decides it: an entry holds the cloud and the stamp it was formed
from, so two pairs that happen to fold together are two stampings under
two numbers and neither is ever served the other's mesh. `Targets::stampings()` counts what has
actually been formed, which is what the test asserts does not move
across three frames of a still set. A stamping no pass asked for in a
frame is let go at the end of it.

**The pixels stay on the device.** The frame's resources are device
textures for as long as the runtime lives, and nothing crosses back until
something asks for a resource BY NAME — a declared `readback`, whose
`then` offers the result to a callback that names it or not, or the
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
own slots, by the NAME the program declared them under. A slot
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

## The mesh painter on the device

It is not here. `geometry::mesh::render::deviceRuntime(device)` draws a
mesh onto a canvas on the device, and it stands beside the CPU executor
of that seam, in SigilGeometry — a mesh draw has no pass, no named
resources and no material, so nothing about it is a frame. SigilGeometry's
README is canon for what it does and how far it stands from its host
twin. A host that brought a device up installs it beside the frame
runtime below, from the same device.

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
it sweeps with. A `Chained` slot is the one that carries a runtime,
and the device executor swaps the host pop runtime into it when the whole
chain has kernels.

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

## What the DEVICE tier can and cannot say

The twin of `README.md`'s CPU-tier paragraph, for the same scene shaded on
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
  `material::termsSource`'s `toneMap` is what runs instead, on both tiers and on the
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
  three would produce from the same parameters.
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

world::Scene scene(world::diligent::runtime(*device));
```

There is no Metal path here, because Diligent has no Metal backend:
`create` fails on a machine with no Vulkan runtime, and the answer for
such a machine is the CPU executor, not a second GPU path.
