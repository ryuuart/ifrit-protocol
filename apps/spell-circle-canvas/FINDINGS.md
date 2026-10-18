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

## A WebSocket client door reads Open before its handshake has reached the server

`hub.listen("ws://host:port/path")` on a client door reports
`ReadyState::Open` as soon as the transport hands back an end, while the
handshake runs later on the client's own thread (`connected` in
`src/common/io/transport/WebSocketClient.cpp`), so `send()` on a door
that reads Open returns false until the handshake completes.
`ReadyState`'s own comment says Open means a door stands and messages
arrive through it; the state before a door stands is `Connecting`. A
caller should be able to wait for the client to reach its server without
probing with a real message. A test should connect a client feed to a
listener in the same process and read `Connecting` until the handshake
completes, then `Open`, with the first `send()` after Open returning
true; a client to a port nobody holds should never read `Open` and should
carry the reason in `state().error`. `wardnet_live/sender.py` offers its
first message again for up to five seconds until this is fixed.

## A settled track that moves nothing places its glyphs on whole pixels

A text leaf under a track whose effect displaces nothing and whose
progress is settled, such as `textFx({.effect = textFx::tween({})})` at
progress one, places its glyphs on whole pixels, while the same leaf with
no track keeps fractional glyph origins, so the two pictures differ by
up to half a pixel per glyph. `TextEffect::displaces` says only a run
whose effect displaces and whose progress is live moves to the finer
grid, so a settled track that changes nothing should place its run as
the bare leaf does; otherwise adding or removing a track, as when an
entrance ends, shifts the letters. A test should assert that a leaf
under an identity track at progress one renders byte-identical to the
same leaf without the track; if whole-pixel placement under any track is
the rule, the text effects reference should say so instead.

## A Python curve or Oscillator in a binding never compares equal

In Python, `motion.Binding(envelope=motion.envelope.shaped(oscillator))`
compares unequal to a second binding built the same way over the same
`Oscillator` object, and so does `ease=` given one Python function;
only the native `ease` values compare equal. In C++ an `Oscillator` is
comparable and callable, so it plugs into `ease` or `envelope.shaped`
and keeps the binding's identity, which is why a curve is carried as a
value rather than a closure; the Python binding wraps it as an opaque
callable, so a tree described again re-patches every node bound through
it. A test should assert that two bindings built with
`envelope=envelope.shaped(oscillator)` or `ease=oscillator` over equal
oscillators compare equal and differ when the oscillators differ.

## Python has no per-value "running" answer

`isRunning(held, value)`, the middle of the three stillness words, has
no Python spelling because `HeldMotion` is not bound; a Python sketch
can only ask the whole engine `isRunning()`. A test should assert that a
Python value animated by a transition answers running while its held
motion writes it, and not running once it lands.

## A draw sampling more than sixteen textures is dropped on Metal without a refusal

`skia::samplerCount` and `kSamplerLimit` are enforced only where a
recipe is compiled (`src/common/material/skia/SkiaCompiler.cpp`); a
composed colour stack lowered by `lowerPaint` is not checked, and
neither is a surface-map pass (`SurfaceMaps::mapShader`, which binds the
colour stack and up to nineteen channel inputs). When their textures
together exceed Metal's sixteen sampler slots, Graphite's pipeline fails
to compile, the render pass is dropped and the page's maps come out
(1, 0, 1), with a Skia warning in the log as the only signal. The stated
rule is that a tree over the limit is refused with both counts named
rather than built into a pipeline the driver rejects, and it should
hold for every program built from a material. A test should assert that
a surface whose colour stack and channel maps together sample seventeen
or more textures is refused with both counts named and is never painted
as a dropped pass.

## Two sketches render differently on every device run

Three device-tier sweeps in a row gave three different hashes for
`nostromo_monitor` against its manifest entry, with no change to the
sketch between them, and `borges_library.py` (The Library of Babel)
disagrees with itself across four renders under `--stability 3`; neither
uses a text track. A headless capture at a declared moment should hash
the same every time; the sketch's `Tube.sksl` and `Wall.sksl` programs
are where to look. A test should assert that two device renders of
`nostromo_monitor` at its capture moment are byte-identical, and the
plate run's `--stability` pass should attribute the scene to itself
until they are.

## `Composer.beatsOf` is missing in Python

`src/common/python/compose/Composer.cpp` defines `beatsOf` only when
`registered<compose::Beat>()`, and no binding registers `compose::Beat`,
so the method is never there and `test_compose_elements_composer.py`
skips its `beatsOf` case without saying so. A Python author was meant to
read a track's schedule back as C++ does. A test should assert that
`Composer` has `beatsOf`, and that a text leaf under a track at progress
one reports one `Beat` per unit with `localProgress` one and a non-empty
`rect`.

## The element factory comments call free row and column factories that only Python has

The comments in `src/common/compose/include/sigilcompose/core/Factories.h`
near lines 86, 109 and 128 spell free `row()` and `column()` factories
as the way to build a flex line, but C++ has no such factories; only the
Python package offers them, as sugar over `box().row()` and
`box().column()`. Either the factories should exist in C++, so the
comment is true and the two languages spell the same thing, or the
comments should spell the verbs that exist. A test should assert that
every call a header comment spells compiles, which the README probes do
for READMEs and not for header comments.

## Thumbnail workers open Python kinds on the process device

`onRuntime` and `onPainterRuntime` in the sketch host act only on the
C++ `Scene3DKind` and `Scene2DKind` through a `dynamic_cast`, so a
Python source or loaded kind passes through unchanged and a thumbnail
worker opens a Python scene3d sketch, or any scene2d sketch that bakes,
on the process device from a worker thread. The empty runtime a
thumbnail states is meant to reach every kind. A test should assert
that `thumbnailAvailable` on a Python scene3d entry answers unavailable
when `thumbnailKind` states an empty runtime.

## The compose texture scene is bound from the sketch adapter

`compose.TextureScene` (`render`, `texture`, `revision`, `size`) is
bound in `src/sketch/python/host/WorldScene3D.cpp` and registered into
`_sigil.compose`, because a Python scene3d sketch paints an overlay
into one; its home is the compose bindings under
`src/common/python/compose/`, beside `SurfaceScene`, so that a consumer
without the sketch adapter can make one. Moving it is one binding and
its registration; a test should assert that `sigil.compose.TextureScene`
imports with the compose module alone.

## The protocol clock policy is the one snake_case name left

`clock.set_policy(budget_seconds=)` in `sigil.protocol` is spelled in
snake_case while every other public name in the package is camelCase
matching its C++ twin. It should be `setPolicy(budgetSeconds=)`, and the
package's naming test should cover the protocol module so no snake name
returns.

