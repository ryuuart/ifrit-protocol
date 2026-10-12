# Findings

## Explicit texture bakes change deterministic reference samples

The rotating text bands in `rota_convocationis` and the retained ink in
`thunder_fulu` request local texture bakes. Their content, geometry and
materials stay fixed, but their cached pixels are resampled onto the
capture's grid. `src/common/compose/core/cache/TextureBake.cpp` uses the declared
bake density for these images; disabling automatic promotion does not
disable an author's explicit `Cache::Texture` request.

The reference photographs consequently differ along glyph and brush
edges. The sketch pass's unchanged-plate requirement needs a decision
about whether a reference capture should evaluate the drawing without
explicit bakes or accept their sampling differences. The standing plates
remain unchanged. A test should compare a turning text ring and a seeded
fibre stroke at the same moment and fractional capture density under the
chosen capture policy, with an explicit bound on the permitted difference.

## Two MAGI studies build the tube's light outside its recipe

`eva_magi_interior/EvangelionUi.h` sets `uBloom` and `uBloomRadius` to zero on the tube
(`evangelion::crtTube`, `eva_magi_interior/Crt.h`)
and adds the tube's light itself — two Gaussians, two weighting matrices
and a table holding the sum at half. The zero radius makes the unused
recipe slot an identity; no unused blur remains. The recipe can supply
light through a slot an executor fills with the layer blurred once.

Defense and deliberation use this shared treatment; interior already
uses the recipe's own light. The intended consolidation is for all three
to take the tube light from the recipe. It needs a visual decision:
the shared treatment blurs two lobes after curvature, while the recipe
supplies one Gaussian before curvature. Radius and strength must match
the reference before an approved plate rebase names the cause.

A test should assert that each tube requests its light with `uBloom`
above zero and has no separate tube-light pass after it. The phosphor
treatment before the tube is a separate stage and stays. The approved
plates should hold the chosen radius, strength and order.

## A stack's programs cannot be stood up before the sketch wearing it is read

Grimoire stands its device programs up before the canvas draws: every
stock body is declared before the first Graphite context exists, the
programs a run builds are written down under the platform cache
location, and the next launch replays that set on a worker while the
canvas holds its frames. A headless `--gpu` sweep declares and records
the same way and fills a store that stands empty, so a machine that has
swept arrives at its first interactive open with something to replay.

How much of that first open it pays for is unmeasured, and the case it
is written for is the one nothing has measured. The written set is cut
at `kMostKeysWrittenDown`, and what survives the cut is what the run
recorded first, so a sweep of the whole registry leaves a set chosen by
the order the selection was drawn in rather than by the sketch someone
opens next — while the canvas holds its frames for every key in it.
`sketch_pipeline_cold_store` sweeps one sketch and then another, which
is the shape that cannot reach the ceiling. A test should sweep the
whole selection on the device, then open one sketch over the set it
left, and assert that the open builds fewer programs for a draw than
the same open over an empty store and holds no more frames for the
replay than it saves.

What is left is a store that is cold for the STACK a sketch wears, which
is what an effect pass hits after every shader edit. Nothing can be
replayed then, and what stands up instead is only the effect bodies that
declare no child: a described paint is expanded into every combination
it allows, so offering a child as an image makes a two-child body
hundreds of programs — the device is asked to hold hundreds it may never
draw, the driver stops compiling once its compiled variants no longer
fit, and the warm-up never returns. A composed stack, which is what the
MAGI studies wear and what costs the most, cannot be described ahead
from the catalogue at all: the backend inlines the whole chain into one
program and which chain a sketch wears is not known until the sketch has
been read.

One route reaches it and has not been taken. A sketch declares what it
draws before it draws it, so the host could describe the stacks a
SELECTED sketch wears — from its own description rather than from the
catalogue — and precompile those. It is the same route the browser
flipping between sketches needs, where each new sketch brings a stack
the context has never built. A second open of the SAME sketch inside one
process is not that case and is nothing to remove: the programs are
already standing in the context's own in-memory cache, which is what the
tally counts as found, so it builds nothing whether a recorded set
exists or not.

`--frame` is not a way in and is not the missing half: a capture
photographs a scene2d sketch on a raster surface so the picture is reproducible,
and a scene3d sketch is drawn by the device's own renderer, so the lane builds no
Graphite program at all and has nothing to record. Only the `--gpu`
sweep and the window build them.

A test should open a sketch in the window with a cold store and assert
that its stack's programs are standing before its first frame, and that
the set a headless sweep left is the one that open replays —
`sketch_pipeline_cold_store` asserts what the sweep leaves and
`sketch_pipeline_warmup` what a second launch replays, but nothing yet
joins the two lanes end to end.

## Replaying a recorded pipeline key walks a null name

`PrecompileContext::precompile` builds the program a serialised key
names, and the key names the pieces that program is inlined out of by
number. A piece the reading run cannot put a name to is not refused: the
generator reads the name anyway and walks a null string, which is a
segmentation fault inside `ShaderInfo::generateFragmentSkSL`. The pieces
this reaches are the backend's own — Skia's `$1DBlur16` and its
neighbours are made on first use — so a key recorded by a run whose
draws made one is a crash in a run whose draws have not yet, which is
every second launch of a sketch that blurs.

`apps/grimoire/src/PipelineWarm.cpp` works around it by keeping each key's
description beside it and reading the description back before replaying:
a piece with no name reads back as a hole, the description differs, and
the key is dropped. It carries a `workaround:` marker. The fix belongs
upstream — `precompile` should refuse a key it cannot resolve — and a
test should assert that a key naming an unmade piece comes back false
rather than taking the process down.

## A zoom that settles re-rasterises everything at once, and varied glyphs grow with the scale

When the pane's zoom settles, `GrimoireRenderer` redraws the frame at
the new scale, and everything rasterised per device scale is redone in
that frame: texture bakes on their coarse scale ladder, and every glyph
at its new pixel size through the glyph atlas. A fast zoom in and out
crosses several rungs in a second, so each settle costs a spike rather
than a steady frame. `axis_ripple`, whose glyphs are each a varied
typeface instance, slows with the zoom itself: every instance
re-rasterises at the larger size and the atlas, sized by an environment
variable, fills and evicts.

A zoom is evidently meant to cost what the pane costs at any scale, as
the pane-sized render target now makes true for fill, with a settle
paying once and a varied glyph drawn as a path where the size makes an
atlas entry wasteful.

Once measured, `sketch_bench` should carry an arm that frames
`axis_ripple` at zoom 1, 2 and 4 reading back the reshaped-words and
atlas counters, and an arm that alternates two scales across a settle,
so a test can assert that a settle's cost is bounded and that a varied
passage's frame cost does not grow with the zoom.

## A run on a closed baseline cannot be fitted to the baseline's length

`TextPath` places a run along its path at the size the run was set in,
and a run shorter than a closed baseline leaves a gap at the seam while
a longer one is cut off at the end. A ring inscription girds its circle
exactly, so `rota_convocationis` measures each band with
`Scene2DContext::measure` and rescales the font size twice (tracking is
px and does not scale with the type) before it describes the tree:
twelve seals and four bands, each a probe outside the tree.

`TextPath` is evidently meant to be able to say "fill the baseline",
the way a paragraph says justify: a field (a fit mode of none, size or
spacing, with a fill fraction) resolved at layout against the resolved
path's length, so a ring's lettering closes on its seam from the
declaration alone and follows a face or word change without a
re-measure.

A test should assert that a run on `shapes::circle()` with a size fit
at fraction 1 ends within a pixel of where it starts, for two runs of
different lengths in the same box, and that a spacing fit leaves the
font size unchanged.

Also wanted by `cosmati`: the onyx roundel's brass ring is sized by eye to come near its seam, and its gap comment states the constraint.

## A turning sheet wearing a coverage shadow repaints the whole canvas every frame

`chaucer_astrolabe` fails its frame gate: its static parts are promoted
textures, but the rete turns inside a full-canvas sheet that wears a
coverage-derived bevel and drop shadow, so the sheet's own paint is
re-recorded over the whole canvas each frame and a `Cache::Texture` over
the rete's engraving bakes nothing. Compose's retained drawing of a
coverage decoration under a moving child should let the unchanged parts
hold while the mover is composited. A test should bench a full-canvas
sheet with a coverage shadow over one turning child and find its frame
within a small multiple of the child's own cost; the plate moves.

## `text_paints::sparkle`'s cells follow the run's extent, and no test binary can hold that

`Sparkle.sksl`'s cell (`kSparkleSkSL` in `text_paints/TextPaints.h`) is
now `extent.y * 22/70`, so as a fill over a 220 × 70 box it is the 22 px
cell it was, a word twice the size takes cells twice as large, and inked
on the unit box of `PaintBox::Element` several cells stand across a
passage; `text_paints` states its SPARKLE OVER A BASE ink there.

What is not done is the test the fix wants: ink a word with
`sparkle(SkRect::MakeWH(1, 1), t)` on `PaintBox::Element` and assert that
light lands on several separate glyphs, that the same word at twice the
size takes cells twice as large, and that the fill over 220 × 70 is
unchanged. The paint is a look the sketch keeps beside itself, and no
test binary compiles sketch code: `compose_test` including a sketch's
header would import a sketch into a library's tests, and `sketch_test`
covers the host library, not sketches. It needs a decision on where a
sketch's own assertions run (a sketch-side check printed at startup, as
`black_watch` prints its invariants; a test binary over the sketches'
headers; or the text paints lifted into a kit that a library test can
reach).

## Ten Data sketches still snapshot and print a connection's vitals by hand

`data::Connection::state()` answers one comparable
`data::ConnectionState`, and `sketch::kit::connectionReadout(connection,
{.door, .rows})` is the one readout over it; `feed_sky` reads both. Ten
registered sketches still declare their own `Reading` (or `Vitals`)
struct of those fields, fill it by hand, test it for a change and set
their own rows in an order of their own: `phone_sky`, `webrtc_sky`,
`osc_desk`, `feed_events`, `grpc_watch`, `midi_pads`, `serial_sensor`,
`artnet_lights` and `channel_bind` over a `data::Connection`, and
`feed_vitals` (`Vitals`, `vitalsOf`) over the `io::Feed` beneath one.

What the sketches should say: a door keeps the `ConnectionState` it last
showed and describes again when `state() != shown`, and its readout is
`connectionReadout`. `feed_vitals` reads the feed with no connection
over it, so it either opens one or keeps its own rows and says why. Each
plate moves only where the kit's rows differ from the sketch's own (a
row's name, its order, `-` for no sender, the error in the sender's
row), and each commit names those rows.

## Separating a text bevel from changing ink can change its coverage

A stationary bevel on an overlaid text copy can produce rectangular coverage
where the material was intended to follow the glyphs. Keeping the bevel on the
same material as the changing reflection preserves the glyph boundary, but
repeats edge work when that ink changes. A text leaf's declared glyph boundary
should remain the same when the ink and its stationary edge treatment are
composed separately.

A regression should compare constant and changing ink on identical shaped
text, assert that both edge treatments follow the glyph boundary, and assert
reuse of stationary edge coverage across light-only updates. The machined
film-title sketch uses retained vector contours; that avoids this text case
without establishing that the underlying coverage issue is fixed.


## API request: curved inscriptions must repeat the figure's resolved geometry

The cosmic-monochord study draws musical-interval arcs and places shaped
Latin inscriptions along those same curves. `TextPath::path` accepts a
`Shape`, resolved against the text leaf's own width and height. It cannot
name a keyed figure's resolved outline. Consequently the study's arc and
label factories repeat the bounds, start angle and sweep; changing one
without the other silently separates the inscription from its bracket.

This is an ergonomics request, not a claim that curved text is absent.
`Text::textOnPath` already shapes and places a whole run correctly.
`strand::from(key)` in `sigilcompose/core/Stroke.h` already borrows a
resolved outline for a decoration, and `PaintContext::borrowedPath` supplies
that outline after derive. That value is a StrandPath, not the Shape
`TextPath::path` takes; the existing borrow does not connect this text case.

Wanted: a curve source for TextPath that can either hold authored shape
geometry or name a keyed node, with the existing declared-read and cycle
rules. A natural declaration would be
`text("Diatessaron formalis").textOnPath({.path = baseline::from("formal.quart"), .at = 0.5f})`.
A shared helper that emits both arc and text remains possible, but requires
the sketch author to manage their relationship.

A regression should place an arc and its inscription in a transformed
parent, resize and move the keyed arc, and assert that the text follows its
resolved baseline without duplicated coordinates. The run should remain
shaped once when only the borrowed curve's placement changes, and the
borrow should obey the same missing-key and cycle policy as other derived
geometry.


## Authoring difficulty: C++ studies depend on the mutable library header set

While another pass edits public library headers, an existing Grimoire
host refuses to compile a new C++ study. `Host::startCompile` checks every
public framework header against the running image's stamp; the refusal
correctly prevents a new object layout from entering an older host. The
result is that reference drawing and visual refinement stop until a
matching host is rebuilt, even when the study needs only existing APIs.
Python studies can continue through that host's already compiled bindings.

A narrowed registry via `SIGIL_SKETCH_ONLY` already reduces the sketches a
separate build tree must compile. It does not provide a coherent immutable
header set for an already running host. Wanted: an authoring SDK snapshot
that pairs a host with its public headers, compiler/link flags and native
artifacts, so a study can select a consistent runtime while library work
continues elsewhere. This request must preserve the ABI refusal; it is not
a request to disable the guard or adjust file timestamps.

A regression should compile and render an external study against a selected
SDK snapshot while the working checkout changes public headers. The live
compiler should use the selected snapshot's headers and artifacts, reject
an explicitly mismatched set, and leave the last working session intact
when compilation fails. A later snapshot should let the same study be
reviewed against the changed library without changing its lookup key.


## Authoring difficulty: optical bloom has no shared parameter preview

The macrodata-terminal study can express its narrow luminous text with
the existing `BloomOptions` in `sigilmaterial/filter/Filter.h`. Finding that
look requires coordinated edits to extraction threshold and knee, near
radius and strength, broad spread and tail, and core whitening, dilation
and halo deepening. Each control is documented. The existing `crt_bloom`
specimen compares a fixed glow with an additive construction, but does not
expose these extraction, core and two-halo parameters together. The study
needs no replacement bloom implementation.

Wanted: an authoring preview or stock material specimen that exposes the
coupled controls together, with a few neutral starting configurations for
fine luminous type and broad emissive surfaces. It should use the same
filter and color pipeline as the production renderer and keep the controls
composable, rather than make a named film treatment part of the core API.
A verification should apply an identical parameter set in the preview
and a captured native text leaf, assert matching optical output, and make
the near and broad halo roles visible when either strength is zero.


## API request: retained subtree filters cannot sample a previous frame

The Nostromo monitor's native tube filter bends and modulates the current
rendered screen. Stock bloom spreads the current layer spatially; these
filters do not carry a prior-frame input for temporal phosphor decay.
SigilDraw's Graphics buffers already retain pixels and copy them on resize,
so accumulation exists for immediate drawing. Using that route for this
screen would require the author to redraw its native text and diagram into
the manual buffer, or manage capture and feedback outside the subtree.

Wanted: a runtime-owned prior-frame input that a retained Compose subtree
can use with decay measured in scene time. This is an API desire, not a
demonstrated rendering defect or a claim that Graphics cannot accumulate.
The monitor currently uses spatial optics without simulated persistence.

A regression should flash a subtree, remove its current content and assert
that the residual fades with elapsed scene time. Identical fixed-step
captures should reproduce the same history. Resize and session reload
should follow an explicit reset or preservation policy, and a skipped
frame should not silently change the stated decay law.

The Passage optical study adds a related desire: a bounded exposure window
over an arbitrary retained source. Analytic procedural samples and the
existing `slitscan_2001` weighted sprite construction already express a
finite exposure without stored history. Python exposes those sprite
seams, including per-instance additive blending; no new history API is
required for that construction.

The remaining source contract matters when the exposed subject is an
arbitrary retained composition. `SceneSource::frameAt` currently returns
its latest image without evaluating the requested time, and
`TextureScene::render` advances its engine by a nonnegative time delta.
These are current-image semantics, not random-access scene history.
The requested temporal window should distinguish sources that can be
reconstructed at a time from live sources requiring buffered frames. Its
exposure interval, sample weights, history bound and reset behavior should
be explicit. This is a source-backed API request, not a failed runtime
probe or a rendering defect.

Acceptance should compare a reconstructed exposure after direct seeking
with one reached by sequential stepping, preserve integrated energy when
sample count changes, evict frames beyond the stated window and expose a
defined reset/resize policy. Capture cadence should not change the
exposure's duration.


## Bug: sprite nib strokes lose continuous coverage below a pixel

The monochord and miniature studies need continuous fine ink. Drawing
marker splines at widths 0.35, 0.6, 1, 2 and 4 with all scatter, jitter
and noise disabled reproduces it on raster: the thin nib splines become
separated dots, while adjacent Pen curves at the same nominal width remain
continuous. Selecting the existing fibre tip with one bristle gives
continuous antialiased ink.

`src/common/skia/include/sigilskia/draw/Direct.h` sets antialiasing on the
paint for its textured sprite vertices and states that this makes a
subpixel sprite contribute coverage. It then calls SkCanvas::drawVertices.
The installed Skia header explicitly states that drawVertices ignores
paint antialiasing. The nib stamp path reaches this sprite lowering, so
the flag cannot provide its stated coverage behavior. This differs from
the white-disc tint defect in hosted nib strokes.

The sprite lowering should preserve smooth area coverage for narrow
sprites through a supported Skia drawing operation or equivalent coverage
construction. A regression should draw colored thin sprites and dense nib
strokes at several subpixel translations and scales, assert nonzero smooth
coverage along the whole run, and exercise raster and Graphite. It should
also preserve tint and the stated destination blend mode.


## API request: a tonal screen cannot use an arbitrary child material

The radio poster screens a shaded metal horn with positive ink dots and
negative paper holes. The stock pattern halftone exposes a fixed-radius
tile; the field halftone ramp varies radius vertically. Neither accepts an
arbitrary luminance child such as a metal surface with a throat, rolled
rim and reflected light. The generic shader API can express it, but the
author must implement area mapping, the dot-to-hole crossover and sampling
at device scale each time.

The custom poster shader initially used a fine logical-pixel screen whose
cells became unresolved at the fitted window scale. Both raster and
Graphite showed the same moiré islands. Using the existing content-scale
uniform for edge filtering and unresolved mean coverage corrects the
study. This was an authoring error, not a renderer defect.

Wanted: a stock tonal-screen operator over a child material, with pitch,
angle and ink/paper colors, positive dots, negative holes and device-scale
mean coverage. A regression should verify solid ink/paper endpoints,
continuous coverage through the dot-to-hole crossover, and matching mean
coverage above and below the scale where individual cells are resolvable.
The unresolved screen should not form large alternating islands.


## Renderer gap: a material channel with no Slang body is not bound by World

World's device binder writes a child material in a surface slot into the
body that reads it, so a recipe with a Slang body plays as a channel and
one without is reported once and reads as empty. `field.noise`,
`field.ripple`, `field.halftoneRamp`, `pattern.scanlines` and
`pattern.stipple`, and the OCIO `color.lut3d` and `color.response1d`
recipes, carry SkSL bodies only. Each wants a Slang body. A test should
put each on a World card's base colour or normal and find it varying
across the card as the page draws it.

## Authoring gap: material strokes separate cap and join control from surface relief

`Element::stroke(Material, StrokeOptions)` states width and placement, but
does not expose cap or join choices. `PathFormat` carries those geometry
choices over a Fill, whose lowering preserves the colour stack rather than
the full surface response. The `metal_linework` study therefore asks Skia
for closed stroke coverage, retains it as a Compose shape and attaches
outline relief to that shape. The geometry is correct, but a simple plated
line requires several authoring steps.

Wanted: a stroke over a full Material with the geometry library's cap and
join values, and optional contour relief derived from that same coverage.
A regression should compare its silhouette with Skia's stroke construction
for round, miter and bevel joins and butt, round and square caps. Cover zero
and fractional widths, miter limits, open and closed contours, clipped ends,
signed relief and material effects.

## Authoring gap: a persistent Compose drawing hides its sampled output

`draw::Graphics::image()` already supplies an ordinary texture through
`material::Texture(buffer.image())` with the media Skia adapter included.
The author can refresh that snapshot in a shader slot and re-describe its
consumer after painting. No additional Graphics texture wrapper is needed.
The persistent buffer inside `compose::graphics` is private to its node,
however, so that node's accumulated marks cannot also be sampled directly
by a sibling material. An author must own a separate Graphics buffer or
render an independent tree through `TextureScene`.

Wanted: share a persistent drawing's output through the existing pixel-source
seam where authors need both the retained node and a sampled input. Keep
brush tools, material slot declarations and retained layout in their owning
libraries, and state whether publication is a snapshot or live. A regression
should paint successive strokes, preserve their history, update a shader
consumer once per publication and reuse it between edits. Cover clear,
resize, density mapping, source lifetime and compatible device sampling.

## Rendering gap: painted brush height differs between raster and Graphite

The deterministic bristle height input in `reflection_lobe` is identical
across its native-density GPU captures, including promotion changes. The
matching raster capture differs in its height-preview interior at 4,739
pixels, with a maximum channel difference of 42. The backend difference
cannot be attributed solely to text or edge antialiasing; the brush input
differs before it drives the normal map.

Seeded fibres painted into a fresh `draw::Graphics` on raster and on the
device reproduce the input difference — the fixture of
`GraphicsHeightGpu.EachBackendRepeatsItsHeightKeepsAnUploadAndDerivesUnitNormals`
in `src/common/draw/test/GraphicsHeightGpuTest.cpp`, which asserts what
holds on both and leaves the difference between them unasserted — and
replaying one
fixed set of recorded paths into Graphics on raster, Metal and Vulkan
reproduces it on each. Graphics stores the heights as N32 even when its host destination
is floating. Each backend repeats its seeded result exactly, and uploading
the unchanged raster height to either GPU preserves every pixel. The native
heights themselves differ from raster: Metal changes 39,505 of 131,072 pixels
with a maximum of 85 N32 codes; Vulkan changes 36,593 pixels with a maximum
of 67 codes. Normal generation from one shared raster height remains a
separate control; the large native-height normal difference is already
present in its input.

Replaying identical positive-width, round-cap paths without live random
generation, into N32 and F16 targets, separates single-path coverage from
eight overlapping draws. Same-origin F16 controls confirm different
overlap behavior: an opaque bent path keeps its single-draw coverage on
Metal, while raster and Vulkan accumulate pixel coverage. Metal retains
Skia's default internal multisampling; Vulkan explicitly selects a single
internal sample and raster coverage atlases. Converting strokes to filled
outlines does not establish a common contract: very thin filled outlines can
lose substantial coverage even on raster. Neither a global antialiasing
change nor normal-map correction is justified by these results.

Wanted: choose whether a material height field must retain one coverage
image across executors or follow the host's ordinary brush rasterization.
For the former, the available narrow path is one retained raster input at
the authored density, uploaded unchanged; Graphics currently forms through
its host canvas and does not promise backend-identical coverage. A regression
should preserve fine bristles and overlap, prove the shared input and native
upload controls, and retain finite unit-facing normals, seeded repeatability,
constant height and zero depth. No tolerance for independently rasterized
height fields has been adopted.

## Rendering gap: HDR coverage differs between raster and Graphite

A paint whose shader returns values above one with premultiplied alpha —
an HDR colour stack, a bright runtime shader — composites differently on
the two executors where its coverage is partial. Skia's raster-pipeline
blitter (`SkRasterPipelineBlitter`) clamps shader values for a normalized
target before it applies path or glyph coverage, so raster clips an HDR
value before coverage where Graphite need not; on glyph edges Graphite
comes out several times brighter than raster while a black control agrees.
That clipping order is the likely cause, not yet confirmed against the
installed Skia.

Wanted: a declared display-range and HDR compositing boundary. A regression
should draw the same partially covered glyph, curved stroke and filled edge
with bounded and HDR shader values into N32 and float16 targets, over an
opaque colored ground and transparency. Compare coverage separately from
radiance, preserve full-coverage HDR values, and establish where display
clipping occurs on both executors. Keep comparing the two executors at a
matching clock; do not rebase away sparse edge differences as ordinary
antialiasing.

## `texture::fromUsageMap` ignores a texture's stated colour space

The decoded part carries each texture's colour space, read from a USD
`sourceColorSpace` and from the glTF file, but the texture-set door in
`material/texture` still decides colour or data by the role a map fills.
A texture authored `raw` in a colour role should lower as data. A test
should lower a part whose base colour is stated `raw` to a `Texture`
whose `colorSpace()` is `Data`.

## A sized light's highlight is a representative point's, not the lobe's integral

`src/common/world/diligent/shaders/Surface.slang` lights the specular side
of a rect, sphere or disk light from the point of the light nearest the
reflected ray, with the lobe widened by the solid angle the light fills
(`areaNormalization` in `src/common/material/core/shaders/Shading.slang`).
The outline a mirror shows is exact; the brightness on a rough surface is
an estimate, the highlight of a light partly below the horizon is lit as
if all of it were above, and the rectangle's highlight on a rough surface
is rounder than the lobe integrated over the rectangle would be. The
intended treatment for a rect is linearly transformed cosines: two tables,
fitted on the host by roughness and viewing angle the way the split-sum
table is integrated, mapping the GGX lobe onto a clamped cosine so the
rectangle clipped to the horizon is integrated against it exactly. A test
should compare the rect's specular response on a plate at roughness 0.3,
0.6 and 0.9 against a direct numerical integral of the lobe over the
rectangle, within a stated bound, and the fitted tables against direct
integration at their texel centres.

## A sized light's sheen and brushing are simplified

Under a rect, sphere or disk light a sheen is lit as by a point light at
the light's centre carrying the whole light, and a brushing keeps the
isotropic representative point and stretches the lobe there; neither
follows the light's shape. A brushed strip aligned with the brushing
already shows a longer highlight than one across it, and a cloth plate
under a narrow and a wide grazing panel show nearly the same profile, so
no image test tells the shape-following treatment from today's; a
discriminating test needs a numerical integral of the lobe over the
light, as the representative-point entry states.

## Performance: a Compose page's path strokes are rasterised on the CPU every frame

A scene3d page draws on the Vulkan Graphite context made with internal
multisampling off (the `workaround:` in
`src/common/skia/graphite/GraphiteContextVulkan.cpp`), so Skia takes its
raster-atlas path strategy and every non-simple path is filled on the
CPU; the scrolling traces of `wet_glass_console` are new geometry each
frame, so the atlas never hits, and the CPU fill is a third of the host
frame. The workaround's reason holds: with multisampling on, a pass that
resumes after a fence loses what the pass before it drew
(`AdoptedGraphite.AFencedPassKeepsWhatThePassBeforeItDrew`). The
multisampled attachment can come back once a resumed pass keeps its
samples, which is a resolve-and-reload of the multisampled target around
the fence. The ribbon band itself, built by `geometry::path::sweptRegion`
as thousands of overlapping quads, is the other half, which the stroked
outline form addresses. The bench arm `BM_Draw_Ribbons_Scrolling` holds
the number either must move.

## Performance: a transmissive pixel costs several times an opaque lit one

In `wet_glass_console` the full-screen cover glass, the beads, and the
thin rivulets, heads and wipe ridges each cost a comparable share of the
device frame, the thin bodies out of proportion to the pixels they cover.
Seen-through shading should cost a small multiple of an opaque lit pixel.
A test should time the seen phase of a full-frame pane against the laid
phase of an opaque card on the device and find the ratio within a stated
bound.

## Contract limit: a page carries one volume

A page states a range or learns one, but carries the first transmitting
material's index, attenuation and dispersion for the whole page and
reports a second once. A test should paint two transmitting regions with
different indices and find each region's volume in its own maps.

## Limit: a lens's back mirrors its front

The refraction lens assumes a body's back mirrors the front's curvature,
so a drop resting flat on glass, or any plano-convex body, cannot be
stated. A body should state a flat back: `SurfaceOptions::flatBack` in
material core, meaning the back is the plane at the body's own zero
depth; `Lower.cpp` copies it into the surface parameters as a uniform,
`Surface.slang` and its over prelude carry it, World's vertex stage
passes the body's back plane in view space, and the passage intersects
the bent ray with that plane and refracts out through its normal. A test
should put a thick slab with a flat far face over a grid and measure the
grid's displacement across the slab.

## Design question: the window's endpoint has no client clock or session access

`Grimoire <file> --frame … --gpu` photographs a scene2d sketch on
Graphite and reports its backend and each frame's time, which is the
capture route a client has. The window's own endpoint mounts neither the
clock nor the session: the window's host lives on the Qt render thread
under `GrimoireView::hostMutex`, while a `SessionAgent` opens and owns a
host of its own. Mounting them needs either an agent that adopts an
external host and hops its calls to the render thread, or a different
split; the choice is the owner's.

## A world-space material in a pen leaf inside a held recording keeps its old anchoring when an ancestor moves

A pen leaf resolves world-space materials against the node-to-root
matrix it is painted under, but a pen program's materials are invisible
to `nodeUsesWorldSpace`, so the leaf never sets `hasWorldSpaceMaterial`.
When an ancestor moves and the parent's recording is reused at the new
place, the program does not run again and its field stays anchored where
it was recorded. A pen leaf painting a world-space material should be
restaled like any world-space node. A test should paint a root-space
gradient in a pen leaf under a recorded parent, move the grandparent, and
find the leaf's pixels matching an ordinary world-space fill at the same
root positions.

## A grid's pole normals lean toward one column

`grid()` gives every degenerate pole vertex the nearest valid normal in
vertex order, so every vertex of a pole row takes one neighbour's normal
and leans toward that column; at a superellipsoid's or a revolved dome's
apex the normal stands well off the axis. A pole's normal should be the
sheet's limit there, which for a revolved profile is the axis. A test
should find every pole vertex's normal of a revolved dome and a
superellipsoid within a small angle of the axis; every superellipsoid
plate moves.

## Python lacks a rect's and a disk's range and a mesh's tangents

`rectLight` and `diskLight` in `python/world/World.cpp` take no `range`,
and `Mesh.computeTangents` is not bound. Bind both and regenerate the
typing; a Python rect light with a short range should leave a card past
it dark, and a Python mesh with uvs should gain a tangent lane.
