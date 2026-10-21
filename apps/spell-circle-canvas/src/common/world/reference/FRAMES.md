# SigilWorld — the frame, the ordering and the executors

The chapter on how a picture is made: what a frame declares, what one
pass touches, what the ordering reads off those declarations, what a
pass is handed and what it writes into, and the two executors that
perform it. `README.md` beside the library is the front page; the
description a frame is made from is `reference/NODES.md`.

## Frame: one frame, declared

A `Frame` is built fresh every frame and thrown away, the way a node is:
it holds no surfaces, no order and no device. A frame with no passes IS
its scene — reconciled and extracted, its bodies, lights and camera — so
an author reaches for passes only when there is something to say about
how the picture is made.

`Frame::environment` is THE ENVIRONMENT THE SET STANDS IN: a panorama
every lit body samples by its normal for what falls on it from all
around, and by its reflected view vector for what it mirrors. It is the
frame's because it surrounds everything rather than standing anywhere,
and a frame holds one. `EnvironmentOptions` is what of it is believed:
`EnvironmentOptions::intensity` over everything it contributes,
`EnvironmentOptions::diffuse` and `EnvironmentOptions::specular` for its
two sides apart — pushing one and not the other is a look, not a physical
claim — `EnvironmentOptions::roughnessBias` added to every surface's
roughness before it picks a prefiltered level, a second panorama
`EnvironmentOptions::next` crossfaded over the first by
`EnvironmentOptions::crossfade`, and its turn about the up axis,
`EnvironmentOptions::rotation`. What it lights and what it shows are two
questions: `EnvironmentOptions::shown` is how strongly the panorama is
drawn behind the scene — zero draws none of it, so the dial is also the
switch — blurred by `EnvironmentOptions::blur` in the roughness units a
reflection reads, and projected onto a ground sphere of
`EnvironmentOptions::groundRadius` where that is past zero. Every dial is
a `motion::Animatable`, read once a frame into an `Environment`, which is
what an executor reads. `EnvironmentMap` is the
panorama itself: SigilMaterial's type, because SigilGeometry's device
residency uploads it and Geometry stands beneath this library.

`Frame::camera` names the camera the frame looks through by its key; a
tree holding one camera needs no name, and one holding several with none
named is an error.

`readback` names the resource handed back the frame after it was
written.

`Frame::overlay` lays a picture over the finished frame — a heads-up
display, a caption, a page about the picture rather than standing in the
scene. It is drawn over the whole picture when the scene is drawn on a
canvas, after the camera's reading, in the canvas's display values, so a
translucent panel's edge is mixed with what is behind it as a page
composited over a picture is, rather than in light before the reading as
an unlit body in the scene is. Overlays are laid in the order they were
added; the pass that paints one must be on the frame, as for a texture a
body wears.

`Frame::backdrop` is its mirror: a picture laid UNDER the scene's bodies —
a grid, a paper, a painted ground the scene stands in front of. Every
geometry pass that paints the picture lays it over its target after the
clear and any sky the environment shows, before the first body, written as
the light the camera's reading turns back into exactly its colour, as an
unlit surface is; it shows as authored wherever no body covers it, a
translucent body is mixed with it in light, and a post pass reads it as
part of the picture. A target that is a part of a larger picture
(`Frame::viewOffset`) shows its own window of each backdrop. Backdrops are
laid in the order they were added, and the pass that paints one must be on
the frame.

## Pass: one stage of making a frame

A pass declares the resources it reads and writes by name, and the
ordering derives the sequence, the barriers and the transient aliasing
from those declarations alone — a pass never states its own position.
`Pass::previous` names a resource read as it stood at the end of the
frame BEFORE, which is how a feedback effect declares itself without
asking anything to run before it has run.

`geometryPass` rasterises bodies, `computePass` cooks points and writes
no pixels, `postPass` reads targets and writes one, and `texturePass`
paints the textures a body wears with a `TextureProducer`, before every
geometry pass.

`PassBody` is a body carried as a comparable value. A model with `==`
declares its own identity; a model without one — the lambda door —
compares equal to nothing but its own copies.

`Coverage` is ONE COVERAGE a geometry pass paints for a masked pass
behind it: the resource it goes into, and whose coverage it is. Two
masked passes behind the same producer asking for the same selection
share one entry; asking for different selections gets one entry each,
because a coverage answers for exactly one collection.

`PassWork` is WHAT THE ORDERING DECIDED about one pass, in terms an
execution can act on without knowing how the decision was made. It points
at the pass it describes, so it stands only as long as the passes it was
built from do.

## Plan: the ordering, read off the declarations

The frame graph is the order a frame's passes run in, derived from what
they declared they read and write; the surfaces two resources share when
their live ranges do not overlap; the barriers a backend needs between
them; and how each pass's selection is realised.

Nothing in it draws, and nothing in it names a backend. A `Plan` is a
reading of the declarations, and the same reading whichever executor
performs it — which is what makes an ordering bug a test failure rather
than a picture. A plan points at the passes it was built from, so it
stands only as long as they do; build it again when they change.

The rules, in full:

- a step runs after every step that WRITES what it reads; after every
  step that READS what it writes; and after every earlier declared step
  that writes what it writes;
- `Pass::previous` orders nothing, because it names last frame's copy —
  which is how a feedback loop is declared without a cycle;
- among steps whose dependencies are all met, the one declared first runs
  first, so an order is a function of the declarations and never of the
  machine;
- a cycle is an error naming the passes on it, and no plan is produced.

## View: what one frame extracted

`View` is handed to every pass that runs over a frame. The bodies arrive
sorted back to front by view depth — stably, so two at one depth stand in
tree order — because that is the order a blended body has to be drawn
in over what stands behind it, and sorting once per frame rather than
once per pass is what keeps two passes over one view drawing the same
picture. A geometry pass draws them in three phases over that order —
the bodies laid over normally; then, after the picture so far is copied,
the bodies that see through themselves (transmission, distortion), which
read that copy; then the blended ones — and a frame with nothing that
sees through itself copies nothing. Before any of them, a geometry pass
under a light that casts shadows draws each such light's shadow map, and
`SceneStats::shadowMaps` counts the lights it drew one for — zero in a
frame where no light casts, which draws none.

`View::environment` is THE SET'S ENVIRONMENT MAP, oriented: the panorama
every lit body samples for what reaches it from every direction, with an
orientation carrying a world-space direction into the panorama's own
frame.

`Sampling` is A TEXTURE AS A MESH SAMPLES IT: the image, where it is read
at over the mesh's own uv coordinates, and whether it repeats outside
them. A mesh carries normalised uvs and a `material::Texture` states its
placement in the image's own pixels, so the matrix is carried across
rather than copied: it is inverted — a texture's matrix puts the image
INTO the sampled space, and a lookup goes the other way — and taken
through the image's size, so a placement and a scale mean the same thing
and point the same way on a mesh as they do in a plane.

## Targets: what a pass wrote

`Targets::image` is a resource the frame's passes wrote, as an image,
answered by the executor that holds its pixels. What a resource was at
the end of the frame before is that executor's to keep, and a post pass
reads it through `Pass::previous`.

`Targets::stamped` is a mesh STANDING AT EVERY POINT of a cloud as
instanced geometry, placed by the point operators' own table, formed once
per distinct pair and kept while both stand, and null when there is
nothing to stamp.

A geometry pass draws the stamps of every point set it reads, every
frame. Forming them in the draw would instance a scene that has not moved
again, however still the frame is — so the answer is held here under `stampKey`, which is a
fold over the two VALUES. The fold buckets the lookup and the pair itself
decides it, so two pairs that fold to one number are two stampings under
two numbers and neither is ever served the other's instances. A frame that
asks for one it already has pays the fold and the confirmation and
nothing else, and the device keys its upload by the number this
answers, so a still scene is neither formed twice nor uploaded twice.

What is not asked for in a frame is let go at the end of it. The optional
key receives the number a stamping is held under, for an executor that
keys an upload by the same one, so that the fold is paid once per frame
and not twice.

`Targets::stampings` is how many stampings have been FORMED, over
the store's whole life. A frame drawing a scene that has not moved must not
move that number.

`Targets::ImageSource` is WHERE AN IMAGE COMES FROM: the executor that
performed the frame's passes on a device, installed by it at the start of
every frame it runs. It answers for one name at a time, so only the
resources something actually asks for cost the crossing back. With none
installed, `Targets::image` answers null.

## Runtime: the seam a pass executes through

`Runtime` holds an executor — the one operation a pass is performed by —
as a value, so the frame, the ordering and the retained scene name no
device and link none. The executor is the device's, supplied as a value
by the feature that owns a device, which keeps every dependency pointing
down. An empty runtime is no executor: a frame that declares passes and
carries one runs none of them, and `Scene::error` says so.

What an executor must reproduce: a geometry pass paints the bodies its
realisation leaves it into the first image it writes, from the view's
camera and under the view's lights; a compute pass cooks its chain into
the point set it writes; a post pass applies its operation to the images
it reads and writes the result. A pass carrying a body runs that body
instead, given the view and the targets.

## The GPU executor

The GPU executor performs a frame's passes on a device, and it is a
VALUE.

WHAT RUNS WHERE. A geometry pass rasterises, depth-tested, into a device
texture, from a pipeline built out of the material's own Slang body. A
post pass is a shader pass over the textures it reads. A compute pass
cooks its chain on the point runtime the pass carries, which is the CPU
one until a kernel exists for the device — a pass that named a device
runtime gets it, and one that named none is cooked on the host and
uploaded like any other geometry.

WHERE THE PIXELS ARE. The frame's resources live on the device for as
long as the executor does. Nothing crosses back until something asks: the
value installs itself as the image source, so a readback or the presented
picture costs one crossing for that one resource and every other resource
stays where it was written.

A BLUR OR A BLOOM IS TAKEN SMALL. A blur wide enough is spread at a
level of the frame halved one to four times and read back at full size,
and a bloom always spreads at reduced levels; the footprint asked for is
honoured at the level it is taken at, so the picture may differ from a
full-size Gaussian by what the level cannot resolve. `DEVICE.md` states
the levels and the bound.

WHAT EACH STAGE COST THE DEVICE. A frame that states
`Frame::timeStages(true)` writes a timestamp at every stage boundary, and
the scene's `SceneStats::deviceStages` then names each stage with the GPU
time it took — a geometry pass's pages made ready, its shadow maps and
their tint, its bodies laid over, the copy of the scene and its smaller
levels, the copy of the depth, the bodies that see through themselves,
the blended ones, and what it drew after them (`<pass>.pages`,
`<pass>.shadow`, `<pass>.tint`, `<pass>.laid`, `<pass>.colour-copy`,
`<pass>.mips`, `<pass>.depth-copy`, `<pass>.seen`, `<pass>.blended`,
`<pass>.extras`); a post pass under its own name; and the final stage a
picture left the frame through (`present`, `readback`). A device answers
a timestamp only once it has finished the frame that wrote it, so the
stages are those of the newest timed frame that has — a frame or a few
before the one rendered — numbered by `deviceStagesFrame`, and
`deviceTiming` says whether they were measured, are still pending, or
cannot be because the device writes no timestamps. A frame that does not
ask writes none.

Two frames carrying the value made by one call to `diligent::runtime`
compare equal; two separate calls do not, because they hold separate
device state. Every value made from one device shares its queue, and
every submission it makes is taken under the device's queue lock.

`diligent::installSlangCompiler` registers the compiler that turns a
recipe's Slang body into a pipeline's shaders, with the shared program
cache. It is idempotent and `diligent::runtime` calls it; a caller that
wants to resolve a material for that target before it has a device calls
it itself. A recipe with no Slang body is reported once by the cache,
naming the recipe and the target, and the body it would have painted is
drawn in the colour the frame extracted instead.

`diligent::kVariantLit` is one of THE VARIANT BITS this backend
specialises a program on. A recipe compiles once per variant, so a bit
belongs there only when it changes the PROGRAM, and this one does:
without it the lighting, the uniforms it reads and the loop over the
lights are not in the compiled shader at all. `kVariantAreaLights`,
`kVariantCascadedShadows`, `kVariantPointShadows` and
`kVariantTintedShadows` are the others: the renderer sets each from the
lights a draw is handed and what they pass through, and a lit program
built without one has no code for that kind of light.
`kVariantUnshadowed` is their opposite: the renderer sets it where no
casting light's map can reach the body — a frame in which nothing casts,
or a body that receives no shadow — and a lit program built with it holds no
shadow terms at all.

Two things a backend might be expected to specialise on are not bits,
because neither changes a program. The mesh vertex layout is one: every
body reaches a pipeline as position, normal, uv and tint, with the lanes
a mesh does not carry filled in on upload, so there is one layout and
nothing to tell apart. The blended build is the other: it is the blend
and depth state a pipeline is created with, which the pipeline cache keys
on beside the program rather than compiling a second one.

## Importing a foreign texture

`diligent::importNative` is THE DOOR A FOREIGN TEXTURE COMES IN BY.
Something else on this machine — a video decoder, another engine, a
capture — holds a texture as the graphics API's own object. The device's
handle table gives it a handle over that object, and the value handed
back answers a device image naming that device, so a renderer standing on
it binds those pixels where they stand. Nothing is copied in either
direction.

IT HAS NO HOST IMAGE. The image side is null, which is not an omission:
it is what says the pixels were never read back, so a picture carrying
their colour cannot have come from a copy. A renderer holding another
device — or none — finds the device image names a device that is not its
own, asks for the host image, gets nothing, and draws the body undressed
rather than something it made up.

Taking ownership hands the texture to the device, which releases it when
the last copy of the value handed back goes; without it the caller keeps
the texture alive and the device only forgets its handle. Either way the
handle is let go when the value is, so a texture imported for one frame
does not accumulate a handle per frame.

The answer is an empty texture — with no source at all — when the device
has no adopted GPU device, or when the import was refused because the
texture is missing or belongs to another API.

## See also

`reference/NODES.md` for the description a frame is made from.
