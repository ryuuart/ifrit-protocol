# Findings

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
of its area laid as alpha at every placement. The port patch
`018-raster-mipmap-smallest-level.patch` is prepared in the registry as
skia 151#6 and the test `DirectDraws.AnImageDrawnBelowItsSmallestMipmapLevelLaysItsArea`
fails until that Skia is built.

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

## Thumbnail workers open Python kinds on the process device

`onRuntime` and `onPainterRuntime` in the sketch host act only on the
C++ `Scene3DKind` and `Scene2DKind` through a `dynamic_cast`, so a
Python source or loaded kind passes through unchanged and a thumbnail
worker opens a Python scene3d sketch, or any scene2d sketch that bakes,
on the process device from a worker thread. The empty runtime a
thumbnail states is meant to reach every kind. A test should assert
that `thumbnailAvailable` on a Python scene3d entry answers unavailable
when `thumbnailKind` states an empty runtime.

## The protocol endpoint test resends its first message until one goes out

`dial()` in `src/common/protocol/endpoint/test/EndpointTest.cpp` offers
its first message again until a send returns true, compensating for a
client door that read Open before its handshake had reached the server.
The door now reads Connecting until the handshake is through, so the
helper should wait for `client.state().isOpen()` and send once, and a
test should find that first send returning true.

## A leaf set in a whole text style ignores its own colour ink

A leaf built with a whole `TextStyle`, `text(utf8, style)`, draws in the
style's colour when given `.ink(Color)` or a colour material, while a
non-colour material ink applies; one rule should hold for both kinds of
ink, so that a clear copy of a leaf can be written the same way whatever
its ink is. A test should give such a leaf a clear colour ink and find
it drawing nothing.

## A bevel or inner shadow on a glyph or shape boundary blurs its band on every replay

Only `Boundary::Coverage` holds its band masks (`HeldCoverageMasks`); a
bevel or inner shadow on a glyph or shape boundary blurs its band again
at every replay, so a stationary edge treatment beside a changing ink is
cut again on each light-only update. Holding the masks for every outline
reuses the edge coverage across ink changes; every bevel plate moves
when it lands. A test should replay a bevelled leaf under a changing
shader ink and find the band blurred once.

## Texture sets are unbound in Python

`src/common/python/material/TextureSets.cpp` is an empty stub, so
`material::TextureMaps`, `Volume` and `fromFiles` have no Python
spelling, while every other material door is bound. A Python author
should load a texture set from files and wear it as C++ does. A test
should build a texture set from the demo assets in Python and read its
maps back.

## A few kit parts and the typeface door still answer the old types

`Ruler.tickLine` (a Python callable as the part), `kit::Channel`,
`specimenTheme`, and the compose kit's `table` and `bars` are unbound,
and `weave.typeface(family, weight, italic)` answers a `skia.Typeface`
where the rest of the package now answers a `weave.Face`. Each should
reach Python in the same shape as its C++ twin, with a test per name.

