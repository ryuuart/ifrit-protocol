# Findings

## Explicit texture bakes are resampled onto a capture's grid within no stated bound

The rotating text bands in `rota_convocationis` and the retained ink in
`thunder_fulu` request local texture bakes. Their content, geometry and
materials stay fixed, but their cached pixels are resampled onto the
capture's grid, since `src/common/compose/core/cache/TextureBake.cpp`
bakes at the declared density and a turning node can take no
device-aligned bake; disabling automatic promotion does not disable an
author's explicit `Cache::Texture`. A deterministic capture keeps the
explicit bakes, so a plate is what the window shows, and the plate tier
states a tolerance for a drawing with explicit bakes. A test should
compare a turning text ring and a seeded fibre stroke at the same moment
and fractional capture density against the same drawing without its
bakes, and find the difference within that bound.

## A stack's programs cannot be stood up before the sketch wearing it is read

Grimoire's warm-up (`apps/grimoire/src/PipelineWarm.cpp`) describes a
selected sketch's stacks to Graphite's precompile only once the sketch is
read, and cuts the keys it writes down at a fixed count, so a stack's
programs are built on first draw rather than ahead of it. The host's
sketch description should be enough to stand a stack's programs up before
the sketch opens. A test should warm a sketch wearing a stack and find
its first frame building no program.

## A run of driven variation instances costs its atlas entries at every device size

A run whose letters are each a driven variation instance costs an atlas
entry per instance at every device size inside the atlas range: on
Graphite a varied page costs several times at zoom two what it does at
zoom one while the same run undriven is flat, and a settle across sizes
rasterises every instance again. Above the atlas range the glyphs draw
as paths and the cost vanishes. SigilWeave's glyph batches should draw a
driven face as a path above a device size where an atlas entry per
instance is wasteful. `BM_Draw_ZoomedPage_Steady_Graphite` varied and
plain at zoom two should then agree within a stated ratio.

## A dressed sheet over a turning child costs many times the child on the raster

A sheet's coverage decorations over a turning child are drawn from held
masks, each part's coverage and blurred coverage kept in its own frame
and sampled where the part stands, so nothing is blurred anew. On the CPU
raster the dressed sheet still costs many times the turning child alone:
what remains is the per-pixel sampling of three turned masks over their
footprints. A test should hold `BM_Draw_CoverageSheet_TurningChild`'s
dressed arm under a stated ratio of the child's arm on the Graphite arm,
where sampling a turned mask is cheap.

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

## A casting light's shadow filter and tint cost a lit pixel in proportion to its program's size

The lens, the layered refraction and the environment cost a transmitting
pixel little, and dispersion's three readings are the cost of the
picture; the disproportion is the casting light. The shadow filter and
the tint read cost a transmitting or every-lobe surface about three times
what they cost an opaque one, following the program's size rather than
any transmission work, so a pane under a casting light pays more for its
shadow than for its glass. A test should time the seen phase of a
full-frame pane under a casting light against the same pane under a light
that casts nothing, and find the shadow's share within a stated bound of
the opaque card's.

## Design question: the window's endpoint has no client clock or session access

`Grimoire <file> --frame … --gpu` photographs a scene2d sketch on
Graphite and reports its backend and each frame's time, which is the
capture route a client has. The window's own endpoint mounts neither the
clock nor the session: the window's host lives on the Qt render thread
under `GrimoireView::hostMutex`, while a `SessionAgent` opens and owns a
host of its own. Mounting them needs either an agent that adopts an
external host and hops its calls to the render thread, or a different
split; the choice is the owner's.

## A raster image drawn below its smallest mipmap level is point-sampled from the base

Skia's raster mipmap accessor falls back to the full-size level with no
mipmapping when the requested level lies past the image's smallest, so a
raster image drawn below one over two to the levels of its size is
point-sampled from the base and lays its whole area at one placement and
nothing at another. The smallest level should be used. Narrow sprites
are spread to a pixel before they reach this, so sprite runs no longer
show it; other raster draws still do. A test should draw a 32 pixel disc
at 0.35 pixels with linear mipmap sampling and find about three quarters
of its area laid as alpha at every placement.

## Python lacks a flat back

`material::SurfaceOptions::flatBack` and `surface::SurfaceParameters::flatBack`
exist, but `python/material/Core.cpp` and `python/material/Environment.cpp`
bind neither. Every surface option should reach Python. A test should
build a surface in Python with `flatBack=True` and read it back from its
parameters.

## Luminance grain has no Slang body

`field.grain` (`material/field/shaders/Grain.sksl`, whose octave count is
written into the body) has no Slang body, so on a World body it reads as
empty and is reported once. Grain is a channel a body can wear, like the
other fields. A test should put `field::grain` on a World card's base
colour and find it varying across the card.

## Python lacks a text path's fit

`TextPath::fit` (none, size or spacing) and `TextPath::fill` resolve a
run against a closed baseline's length, but `python/compose/Elements.cpp`
binds the path's fields one by one with no `Fit` enum. A Python
`TextPath(fit=..., fill=...)` should round-trip, and a ring run with a
size fit should close on its seam.

## Two sketches still hand-fit ring text

`rota_convocationis` measures and rescales its twelve seals and four
bands with `Scene2DContext::measure`, and `cosmati`'s onyx roundel ring
is sized by eye. Both can declare `.fit = TextPath::Fit::Size` and drop
the measurement; their plates move by the fit's residual.

## A window bench of a named sketch prints no row

`Grimoire --window-bench 6 --sketch wet_glass_console` (with or without
`--kind scene3d` and `--window-scale 2`) opens the window, stands its
pipelines up and exits after the stretch with no `WINDOW` row, neither a
reading nor a `SKIPPED` or `FAILED` line, so the presented rate the row
carries cannot be read for one sketch. The cadence evidently never
reaches its read step: the entry on screen is matched by the path the
registry names, and a selection by name may not be what the host opened.
A bench of one named sketch should print exactly one row. A test should
run the window bench for one registry entry and find a `WINDOW` row
naming it.

## An asynchronous pipeline build can abort inside the Metal compiler

With pipelines built on a worker, one run aborted on a dispatch queue in
Metal's `newLibraryWithSource` under MoltenVK's shader library compiler,
through an assertion the crash report does not quote
(`Grimoire-2026-10-11-192549`). The Metal compiler is being driven from
a worker thread while the window's own Metal work proceeds; whether the
assertion is a compile error the synchronous path would have reported
as a failed pipeline, or a threading rule of the compiler, is not known.
A failed asynchronous build must become a pipeline the frame stands in
for, never an abort. A test should hand the pipeline cache a program
that cannot compile and find the body stood in and the error reported.

## The Python kit has no connection readout

`sigil::sketch::kit::connectionReadout` shows a door's state over
`data::Connection::state()` in C++, and `sigil.sketch.kit` binds no
equivalent, so a Python sketch over a connection draws its own state
rows, as `cue_deck` does. A Python sketch should show a door's state
through the same kit readout. A test should call the Python readout over
a connection after an inlet delivery and find rows for readiness,
revision, undecodable, dropped and the error.

## An OSC bundle's elements reach no address handler

`data::Connection` reads an OSC bundle as one message with no name
(`nameOf` in `src/common/data/connection/Connection.cpp`), so only
`on("*")` and `otherwise()` see it and `latest(address)` latches none of
its elements, while a single message is dispatched to every handler whose
pattern matches. OSC dispatch delivers each message inside a bundle to
the methods matching it. A test should deliver a bundle holding `/a/x 1`
and `/a/y 2` to an `osc://` connection, find an `on("/a/*")` handler run
twice, and find both `latest("/a/x")` and `latest("/a/y")` non-empty. An
OSC Out CHOP set to send bundles draws nothing until then.

## SigilIO's frames delivery and capture have no test of their own

`io_test` covers only the refusals of `publish` and `subscribe`; the
cases that proved delivery and capture lived beside the removed wire
tester, which carried a static frame to clients that subscribed after
drawing stopped and checked a captured frame's rows and channels.
`io::frames` should keep its own guarantees under test: discovery through
`sigil::io::frames::publications()`, late subscription to a static
publication, and row and channel order. A `gpu`-labelled `io_test` case
should publish a two-by-two quadrant texture over a `syphon://` name,
find it listed, and have two subscribers that join after drawing stops
each receive the same four quadrants in order.

## The sweep's flush inserts its own recordings without the recorder's waits

`flushHook` in `src/sketch/plate/Sweep.cpp` inserts the recordings of a
timed frame itself, so the GPU waits SigilSkia registered on that
recorder, such as the presented World picture's fence, are not attached
to those inserts; with 2D on Metal a discarded frame can sample a
presented picture before the device finishes it. Plates are unaffected,
since the read-back carries the waits. Every insert the tree makes should
carry the recorder's waits, which wants a door that submits a recorder
with its waits (`PendingWaits` is private to `Waits.*`). A test should
time a scene3d sweep frame and find the picture's presented-fence wait
attached.

## A retain-count assertion in the hardware event test is order-sensitive under a parallel run

`HardwareVulkanOverMetal.AMetalEventHoldsThisQueueUntilMetalRaisesIt`
(`src/common/core/hardware/test/VulkanOverMetalTest.mm`) asserts that
the shared event's retain count returns to what it was before the fence
was imported; under a six-wide ctest run on one GPU the count reads one
higher, while the case passes alone and repeated. The claim is that the
device keeps no reference to a maker's event once the fence is
destroyed, which a retain count read while other processes drive the
same Metal device does not isolate. The test should prove the release
another way, such as a weak reference that nils after the destroy and an
autorelease drain, and the first-frame timing case in `StandIn` should
likewise be read serially or with a bound that a loaded GPU meets.
