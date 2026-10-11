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

## A nib brush lays white discs inside a compose pen node

`draw::brush::paint` with a `Tip::Nib` tool (colour cinnabar, opacity 1,
`pressureOpacity` 0) inside `compose::pen(...)` lays each dab as a WHITE
disc at the right size, with only a thin line of the tool's colour down
the middle of the run and the final dab alone in colour; inside
`compose::graphics(...)` the same stroke comes out as that thin line with
the discs missing. On a light ground the white is invisible, which is why
`brush_dynamics` looks right. The nib's dabs go down as one sprite batch
(`drawStamps` → `sigil::skia::draw::drawSpriteAtlas`, the white
`roundTip()` sprite tinted by per-sprite colours under `SkBlendMode::
kSrcOver`); the tint is evidently meant to multiply the white sprite, so
each dab is a disc of the tool's colour. `thunder_fulu` lays its strokes
with `Tip::Fibres` until then, which draws through the pen's own verbs.

A test: one nib stroke of a saturated colour at opacity 1 over a black
raster, through `compose::pen`; every pixel inside the stroke's width is
within a small distance of the tool's colour, and none is lighter than it.

## The rewritten studies paint their static art on every frame

The rewritten studies (minard_1869,
chaucer_astrolabe, dunhuang_star_chart, ksp_mapview)
state paper grain, stone, brush ribbons and lettering as material
paints and shapes in the tree with no `Cache::Texture` over them, so a
plate whose picture does not move between frames is re-rasterised on
each one. Each fails the frame-rate gate under `--bench`, and a sweep
that renders a few hundred frames per sketch for its statistics is
slow for every one of them.

Each study evidently means to paint its static subtree once and replay
it, the way `eva_magi_deliberation` keys its ground as one texture, with
only the motion it is about (a turning rete, a rolling die, strokes
writing in) outside the bake or as a bound transform over it. The fix is
one or two `.cache(Cache::Texture).key(...)` boundaries per study over
the largest static subtree, placed so no blur or blend crosses them; the
pixels must not move. A test cannot see where a specimen puts its cache,
so `--bench` on each entry is the check: at or under the gate where the
original met it.

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

## A keyframe's colour terms are values, so a table cannot follow the sheet's palette

`GlyphModifier`'s three colour terms are `material::Color` values, so a
keyframe table cannot name the ink in force or a custom property of the
sheet, and a palette change leaves the table's colours behind. The
intended shape is a colour term of the kinds a `Fill` has: a value, the
ink in force, or a custom property, resolved against the glyph's own
custom-property access when the effect is applied, with a plain colour
still spelled as today; the same term would serve `TextShadow::color`.
The alternative is a separate `motion::Tween<Fill>` per term beside the
geometric keyframes. A test should drive a table naming the ink in force,
change the sheet's ink, and find the glyphs following it.

## The glyph boundary outline is cut from the glyphs at rest

A shadow or glow that follows a track's deformation in each glyph's own
colour is `textShadow`, which Weave's glyph pass draws per glyph. The
`Boundary::Glyphs` outline itself, which bevels, strokes and ink-effect
shadows cut from, is still taken from the glyphs at rest
(`decorationBase = &inst.glyphOutline` in `core/paint/PaintContent.cpp`),
so a bevel on a track that scales or shears its glyphs sits on the
undeformed letters. When the leaf's tracks displace, the outline should
be built from the deviated glyph matrices, a text-engine operation on
`TextPainterOperations` that the painter calls. A test should bevel a
word under a track that scales one glyph and find the bevel on the
scaled outline.

## An escaped Python connection may keep its door open after the session closes

`src/common/python/data/Connection.cpp`'s `ConnectionHandle` owns its
`data::Connection` outright, the way the feed wrapper in
`src/common/python/io/Resources.cpp` owned its `io::Feed` until that wrapper
was made to hold only a weak reference to the session's lease. A connection
wrapper a study stores in `builtins` therefore outlives the session and, with
it, the feed's door and inlet. The session evidently means to end every door it
opened when it closes, whatever Python still holds. A test should store a
connection wrapper in `builtins`, close the session, and assert the feed's state
is closed and its inlet expired, as `SketchPython.ASessionClosesFeedsDespiteEscapedPythonWrappers`
asserts for a feed.

## A phosphor bloom on a text leaf cuts away whatever the leaf paints outside its box

`Filter::phosphorBloom` painted in a box is cropped to that box grown by
its reach (`makePhosphorBloom`, `material/skia/EffectBloom.cpp`, handed
`paintFrame->size` in `Effect.cpp`). The box is the node's layout box, but
a leaf's layer holds more than that: its `textAttach` marks, which reserve
nothing and stand above and below the line, and a `textStroke` wider than
the reach. On `karaoke_wipe`'s sung line, `.filter(Filter::phosphorBloom(9))`
on the leaf erases the ruler hung below it and cuts the bouncing ball off
at its middle, where `Filter::blur(9)` on the same leaf keeps both. So the
sketch keeps its second, blurred copy of the line for its glow.

The crop evidently means "no further than this layer's content can
reach": it should be the bounds the leaf actually paints (its ink
overflow, attachments included) grown by the reach, as the layer a plain
blur runs over already is. A test should give a text leaf a `textAttach`
mark standing wholly below its box, filter the leaf with a phosphor bloom,
and assert the mark's pixels are drawn; and assert the bloom's cost still
follows the painted bounds, not the canvas.

With the one-node bloom in place of the copy, `karaoke_wipe` still meets
the frame-rate gate under `--bench`, though it costs more than the copy,
and the sung letters come out paler, the bloom's retained source
whitening the yellow; the owner decides the look once the crop is fixed.

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


## API request: the pen cannot fit a Material to each shape directly

The scan-visor study tries to fill a rounded containment chamber and a
curved specimen with shape-relative gradients. `draw::Pen::fill` and
`stroke` accept `material::Material`, and their `material::Paint`
overloads accept the `SHAPE` fit, but there is no two-argument Material
overload. The natural call
`pen.fill(material::radialGradient(...), draw::SHAPE)` fails to compile.
The source confirms this split in `src/common/draw/include/sigildraw/Pen.h`;
`src/common/draw/Pen.cpp` already lowers the one-argument Material form
through `material::Paint::recipe`.

The intended fit operation is available. The study uses the older Paint
gradient factories to reach it, so this is an authoring-API gap rather
than missing rendering capability. Wanted: `fill(const Material&, Constant)`
and `stroke(const Material&, Constant)`, lowering through the same recipe
bridge and retaining the fit as pen style. The material builder should not
require a second gradient vocabulary merely because it paints a shape.

A regression should compile both forms, draw differently sized closed
Bezier shapes under `SHAPE`, and assert that each receives its own complete
gradient. It should compare the Material form with an explicitly wrapped
Paint recipe, including a shader or layered material, and assert that
push/pop restores the earlier fit and that a one-argument fill returns to
canvas-relative coordinates.

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


## API gap: captured program filters cannot receive later parameter or slot edits

`material::Filter::of` captures a material's uniforms, frame inputs and
children at construction. Moving the original bindings afterward does not
animate the filter; `SkiaEffect.RecipeSnapshotsKeepCapturedLiveValuesApart`
asserts that intentional snapshot contract. Automatic resolution and time
are captured from empty frame data, so a program needing dimensions or a
clock must receive explicit values before conversion.

The converted value also rejects subsequent explicit `Filter::set`,
`Filter::bind` and `Filter::slot` calls, even for valid declarations.
`src/common/material/skia/Effect.cpp` retains the built image filter and
comparison snapshot, but leaves the writable program handle empty.
`EffectDoors.cpp` consequently reports that it has no parameters or slots.
To reproduce, convert a material whose program takes a gain through
`Filter::of` three times: with initial gain .2, with initial gain .2 then
explicitly bound to .8 after conversion, and with initial gain .8, and
paint each over the same tile. The bound tile keeps the .2 pixels instead
of matching the .8 one. This missing editing capability is separate from
intentional capture of the original material's motion.

A filter constructed directly through `material::skia::program` supports
these explicit operations and resolves newly supplied child paints against
the node's current frame. It requires the consumer to compile and retain
the runtime program separately from the material declaration.

Wanted: preserve `Filter::of`'s initial capture while retaining its reflected
program, captured inputs and layer-input declarations for explicit edits.
Later `set`, `bind` and `slot` operations should use the existing filter
doors without reconnecting the original material's bindings. Derived layer
inputs must follow an explicitly changed amount, an author-filled child
must win over a derived input, and the sampling radius must remain fixed.

A regression should retain an initial capture after advancing its original
live source, then edit a copied filter and assert fresh pixels, appropriate
volatility and an unchanged original. Cover scalar and block bindings,
static and live child replacements, node-local and root-anchored children,
derived blur inputs, malformed uploads and immutable sampling reach.
Explicit child edits must resolve against the current frame without
changing untouched captured frame uniforms. Check raster and device output,
clipping and restored canvas state; uniform edits must reuse the program.


## Bug: a world-space material restarts inside each hosted Pen leaf

The scan-visor and registered-print studies need one material field to span
separately positioned drawings. A single `Material` wrapping
`Paint.linearGradient((0, 0), (1, 0), stops).worldSpace()` remains continuous
across ordinary Compose fills, but starts the full gradient again inside
each `compose::pen` leaf. Changing a pen leaf's width changes the gradient's
scale even though the root canvas size is unchanged.

Two native Python probes reproduce this in the loaded Grimoire host. On
an 800-pixel-wide canvas, ordinary fills and Pen fills occupy matching
x ranges, with Pen fills on a second row. At x406 the control pixel is
(158, 97, 161) and the 310-pixel-wide Pen leaf is (251, 65, 68). A second
probe with 180- and 420-pixel-wide Pen leaves demonstrates the same loss of
root extent. A C++ probe on the matching host reproduces the same samples
using the exact same wrapped Paint for ordinary fills and Pen fills.

The current source supplies local size and time in
`src/common/compose/draw/Draw.cpp`'s `Held::frameIn`, and
`src/common/draw/Pen.cpp`'s `paintFrame` supplies local resolution, time and
content scale. Neither transfers root resolution or the node-to-root
transform. Ordinary fills transfer both in `frameOf` in
`src/common/compose/core/description/Fills.cpp`. The documented world-space material
contract requires root canvas coordinates and root resolution.

A regression should paint the same root-space linear gradient and a
root-space runtime shader through ordinary fills and translated Pen leaves
of unequal width, then compare interior pixels at matching root positions.
It should resize the root and move the leaves, asserting continuity and
root-sized shader resolution. A translated/scaled parent and pen-local
transforms should preserve the field's root anchoring; ordinary local-space
paints should retain their existing local behavior.


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


## Bug: file-path video export ignores the requested study

A live Python study renders correctly when supplied by path with `--frame`.
The same file with `--video` is accepted by argument parsing and file
validation, but `main.cpp` dispatches `runVideo` before the live-file lane.
`apps/grimoire/src/VideoLane.cpp` selects only the registry index and category;
it never reads `Arguments::sketchFile`. Consequently the file does not
select the output. A native probe using the macrodata terminal, one video
frame and a small output size exits with the registry's set-rendering
requirement, even though that file is an ordinary CPU-renderable Compose
study. With the device enabled, the code would select the registry montage
instead of the supplied file. That broad encode was not launched.

The intended authoring action is to export the current study. Wanted: the
video lane should render and encode a file-selected session, or reject the
unsupported file-plus-video combination before selecting other sketches.
A silently substituted registry selection is not an acceptable fallback.
The limitation affects newly authored sources that are not yet registered
in the installed host as well as file edits newer than its bundled copy.

A regression should supply a temporary study file whose content differs
from every registry entry and assert that only its frames reach the video
encoder. If the combination is unsupported, it should instead assert a
clear early error naming that combination, before device selection or
montage rendering. File-based stills and registry-based montages should
retain their existing selections.


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


## Authoring ergonomics: optional positional captures receive paint context

The optical-record study's ordinary Python capture
`lambda pen, width=64: draw_mark(pen, width)` receives a PaintContext in
`width`, then raises TypeError when drawing divides the value. The callback
dispatcher in `apps/python/sigil/sigil/_callbacks.py` deliberately selects
the largest compatible positional prefix, and
`src/common/python/compose/PaintPrograms.cpp` supplies the context whenever
two arguments fit. A default capture is therefore indistinguishable from
an optional context parameter. This follows the dispatcher contract but
conflicts with a common Python authoring idiom.

A keyword-only capture and a one-argument closure both preserve the value
and produce identical native Raster images; swapping either for the
defaulted positional capture in a Python study reproduces the TypeError.

Wanted: an explicit way to request context, or a documented callback
adapter that preserves ordinary captured defaults. The public callback
guidance should show the keyword-only form and explain the positional
injection. A regression should preserve explicit `(pen, context)` calls,
assert the chosen behavior for optional positional captures, and verify
that keyword-only captured values retain their values.


## Authoring ergonomics: CLI captures do not share an exact-time request

The optical-record focus probe asks for a Python file still at 0.25
seconds. Its update log visits 0.25 and then 0.26666666666666666 seconds
before writing the image. `src/sketch/scene2d/Session.cpp` deliberately
paints a still through `frame(..., stillStep())`, and
`src/sketch/live/Host.cpp` advances the requested duration before invoking
that photograph. The extra step is intentional, but the file lane's
reported requested time does not identify the time its pixels observe.

The same probe's live blur map matches a static map at the actual observed
time exactly. Nominal-time comparisons produce false differences. This
is a capture contract issue, not a mapped-blur defect. Asking the file
lane for a still at `--at 0.25` of any clock-driven Python sketch and
logging its updates reproduces the extra step.

The native window screenshot in `apps/grimoire/src/main.cpp` waits for a live
session and warm-up frames, but does not consume the file lane's `--at`
setting. A matching window composition currently needs a clearly
identified fixed-time capture fixture or clock control outside that CLI
option.

The headless protocol already supports a held clock, repeated stills
without advancement and a current-clock result. Those existing semantics
are the model for the authoring request, rather than missing primitives.

Wanted: CLI capture requests that expose a held scene time and report the
timestamp actually drawn, while retaining any redraw needed for capture
density. A regression should put a time readout and
moving edge in one scene, request the same timestamp through file and
window lanes, and verify both reported time and edge position. Repeated
captures should not advance the scene unless advancement was requested.


## API request: the GPU window cannot expose its capture backend to a client

The optical-record sequence draws successfully in the native Graphite
window. `GrimoireRenderer::installCaptureBackend` gives its Host a
Graphite surface, ordered canvas and device readback. The window's
Inspection endpoint, however, mounts only host and registry agents.
Its advertised domains agree with that restriction. Requests for
`clock.setPolicy` and `session.still` both return `notMounted`, even while
host.describe reports the live optical-record canvas.

The protocol already defines clock stepping and session sequence capture,
and the Host already has the device-backed capture seam. An author cannot
combine those existing facilities through the window endpoint. The
file-frame lane deliberately captures scene2d sketches on Raster, including
when `--gpu` enables the device for mesh or scene3d content. A procedural
optical movie therefore cannot request the window's 2D backend through
either public route. Connecting a client to a running window's endpoint
and sending `clock.setPolicy` or `session.still` reproduces the refusals.

Wanted: safe client access to the active window's clock and capture
session, or an explicit file-sequence backend selector using the existing
device capture seam. A regression should export only the selected file,
report the actual backend and scene times, keep Graphite work on its
owning thread, and verify repeated held captures and the requested frame
count. Detaching the client should restore the host's ordinary clock and
leave the live window usable.

## Performance gap: nested arrangements repeat Yoga settlement

`core/layout/Layout.cpp` restores authored child styles and recalculates Yoga
for each arranging depth, then settles the resulting placements. This keeps
modifier inputs independent of previous output, including nested containers,
but a viewport change repeats whole-tree layout during convergence.

`BM_Layout_NestedArrangement_ViewportToggle` exercises an outer grid and
100, 500 or 2000 independently arranged inner rows, each with two children;
its CPU time per changed frame grows with the row count. A CPU
sample of that benchmark shows Yoga recalculation inside the arrangement
and initial-layout phases as substantial work. The sample also includes
benchmark setup and calibration, so its call counts are not per-frame
traversal counters.

Wanted: reduce repeated settlement when a surrounding arrangement has
already established an inner container's extent. Preserve authored flex
inputs, percentage resolution, text reflow, hidden and added children, and
center-pin placement. Reuse temporary child-record storage where useful,
without adding another retained geometry model.

Regressions should preserve nested bounds and pixels on initial layout, an
unchanged frame, a viewport change and a changed outer track. Deterministic
layout-pass counters should establish fewer redundant passes; retain the
benchmark for timing rather than using a machine-specific unit threshold.

## Renderer limitation: Skia's CPU image-filter layer is eight-bit, so raster filters quantize coverage, clip HDR and step contour normals

The pinned Skia CPU bitmap device selects N32 for a layer whose paint has
an image filter, whatever the format of the source, the save layer or the
target. Two paths in this repository show it.

A runtime filter. A pass-through runtime filter that reads an immutable
RGBA F16 source into an RGBA F16 raster target with an F16 save layer
reads requested coverage .75 back as .749023 and an HDR red sample 4.0625
back as 1. The shared glass kernel over procedural children, which takes
no filter layer, preserves both values. These readbacks do not establish
the GPU format-pipeline behaviour.

A contour normal field. `src/common/material/skia/Bevel.cpp` derives
optical normals by drawing path coverage into an N32 raster surface
through a blur image filter, then stores the normals as RGBA8. Copying
that derivation and changing its coverage destination to N32, F16 or F32
leaves the same 256 blurred levels, because the blur's layer is N32;
`glass_atelier` shows the result as wavy calibration lines near the panes'
smooth curved shoulders, on both raster and Graphite. Separately, the RGBA8
normal store cannot encode .5 exactly, so its flat normal decodes to small
nonzero x/y components and displaces even the nominally flat interior;
storing normals as F16 makes flat normals exact at twice the normal bitmap
memory, but the shoulder steps remain, since they come from the coverage.

Wanted: preserve the declared floating-point working format through the
runtime-filter executor and the bevel's coverage, or expose the precision
restriction to consumers; and smooth contour-derived optical normals with
an explicit precision and memory contract. One regression should preserve
an identity filter's coverage and HDR colour with floating-point source,
layer and target, then verify a displaced sample against the same format
contract. Another should preserve an exactly flat bevel interior, compare
a circular shoulder with its radial reference, and separate normal
encoding from coverage and filter precision. Increasing output density or
blurring the final plate does not establish a smoother source field.

## API gap: scene2d sequence sampling advances during photography

The native protocol sequence steps by one over the requested rate between
stills. A scene2d still also draws an extra frame and advances by one
sixtieth of a second, while a scene3d still presents its held frame. The same
25-frame request at rate 24 therefore advances the stone and optical clocks
by 1.4167 seconds, but the metal scene3d clock by one second. Requesting that
sequence through the protocol from a scene2d study and a scene3d study, then
seeking each directly to the sequence's final reported time, reproduces
it: the direct seeks match the actual final sequence time exactly, not the
requested one.

This is documented still behavior, but it makes the requested sequence rate
insufficient to state the scene's sampling interval across runtime kinds.
Wanted: photograph each requested scene time while retaining redraw at the
capture density; count redraws separately from clock advancement. A
regression should sample a clock-driven color or position at the same
requested times in a scene2d sketch and a scene3d sketch, assert those times and frame count,
and compare fresh direct seeks at several densities. Paused stills should
remain held and repeatable.

## Renderer gap: a material channel with no Slang body is not bound by World

World's device binder writes a child material in a surface slot into the
body that reads it (`material::inlined`), so `surface::blendNormals`, a
flipbook or a flow map plays as a channel. `surface::normalFromHeight` has
an SkSL body only, so the device cannot compile it: the slot is reported
once and reads as empty. Compose's Skia executor evaluates it.

Wanted: a Slang body for `normalFromHeight` (a central difference over the
height slot at a texel step the recipe states), or a bake of its output into
a texture before it reaches a body. A regression should compare a height
texture's derived normal on a World card with the same normal supplied as a
texture, under one raking light. Until then a page reaches a scene3d sketch through
`compose::SurfaceScene`, whose normal map is an image the binder reads.

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

## Rendering gap: Graphite promotion changes fine ribbon coverage

The fixed comparison coupons in `pigment_brushes` use full-material ribbons.
At two seconds and native density, Graphite captures with automatic
promotion disabled and eager promotion enabled agree on the main material
colour but differ at narrow ribbon boundaries and isolated coverage seams in
the coupons. The metal and typography studies have only
small channel-rounding differences under the same comparison. These captures
use the same Graphite executor for the live paint and the device bake.

Wanted: device promotion preserves the live ribbon's coverage, including
overlapping sections of a swept band. A reduced regression should compare a
wide curved band with a thin band under the same static material, with
promotion off and eager. Cover clipped coupons, negative origins,
partly transparent layers, joins and a picture parent. Compare edge coverage
separately from fully covered material colour; treating a large sparse edge
difference as a small whole-frame average conceals the defect.

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

## Performance gap: a persistent drawing buffer keeps its first backend

`Graphics::form` reuses its surface whenever the rounded pixel extent is
unchanged, without comparing the host backend. A first `begin` through the
headless sweep's nondrawing stepping canvas forms a raster surface. Later
drawing on a Graphite canvas at the same extent keeps that surface, so the
material samples uploaded raster snapshots. A buffer first opened through
the native window can instead form its surface on the device.

Wanted: an explicit formation contract for persistent sampled buffers when
the host changes backend. Device painting should not depend on whether a
discarded frame happened to open the buffer first. Preserve accumulated
marks when moving a buffer; device-to-raster migration needs access to the
device's readback context. A regression should form and paint on raster,
then begin on Graphite at the same extent, verifying preserved pixels and
compatible sampling. Cover the reverse direction, density changes and a
host that cannot form a device surface; fallback must not allocate again
on every unchanged frame. This concerns buffer placement, not whether the
final material executes on Graphite.

## Contract gap: a live uniform block can be replaced without publication

`UniformBlock` has implicit copy and move operations although bindings track
its identity and revision. Assigning another block with the same revision
can change its committed array without advancing that revision. Assigning a
different size also invalidates held spans and the binding's size check;
moving from a bound block leaves the original identity with emptied storage.
The existing consumers hold blocks by shared pointer and do not use these
operations.

Wanted: a fixed-storage identity object whose array changes only through its
draft and `commit()`. Delete copy and move construction and assignment rather
than introduce a second mutation path. Compile-time assertions should prove
all four operations unavailable. The existing held-span and publication
regressions should continue to pass with initialized arrays, unchanged
revision semantics and bindings that retain their block identity.

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

## Rendering gap: instance tints lose HDR range before compositing

`instancing::Pool` stores `material::Color` tints. Its stamp executor converts
each tint to packed `SkColor` before passing the batch to SigilSkia's sprite
executor, whose vertex colors are also packed. A tint component above one
therefore loses its range even when the atlas and destination use float
pixels. Preserving an HDR cell with a neutral tint does not establish HDR
tint support.

Wanted: a clear tint range contract and an executor path that preserves
float modulation when needed. A regression should compare identical float
cells drawn directly and instanced with neutral, bounded and HDR tints,
including a separate alpha lane. Assert stored premultiplied RGB and alpha
independently; changing destination precision must not change authored tint
semantics silently. Keep the bounded batch path when it meets that contract.


## Bug: a workspace's own `assets/` is not what `res://` mounts

`python3 scripts/sigil.py workspace new <dir>` writes `<dir>/<dir>.cpp`
beside `<dir>/assets/`, and the README it writes says that folder mounts
at `res://`; `scripts/README.md` says the same. The host reads an entry
whose directory shares its stem as a directory sketch and mounts `res://`
from the `assets/` folder one level above that directory, so a file put
in the workspace's own `assets/` is not found.

A workspace should mount the `assets/` folder it was written with. A test
should make a workspace, put a file in its `assets/`, open the entry and
read the file through `res://`.

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

## A sized light's sheen and brushing are simplified, and a rect or disk light has no range

Under a rect, sphere or disk light a sheen is lit as by a point light at
the light's centre carrying the whole light, and a brushing keeps the
isotropic representative point and stretches the lobe there. Neither
follows the light's shape, and `RectLightOptions` and `DiskLightOptions`
carry no `range` window. A test should assert that a brushed plate under
a long strip light aligned with the brushing shows a longer highlight than
one across it, that a sheen under a wide panel grazing a cloth plate
spreads over the panel's width, and that a rect light's range darkens
what stands past it.

## Performance: a Compose page's path strokes are rasterised on the CPU every frame

`wet_glass_console`'s page is repainted each frame, and Graphite draws
its ribbon paths through its raster path atlas, Skia's CPU anti-aliased
fill, which is most of the main thread's time in the window. Path
coverage should come from Graphite's tessellating or compute path
renderer, or ribbons should keep their coverage while unchanged. A test
should be a bench arm painting six long ribbons on a large page each frame
whose host time stays within a stated bound.

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
stated. A body should be able to state a flat back: a field on
`SurfaceOptions` in material core, carried to the surface program as a
plane at the body's own zero depth. A test should bend a plano-convex
body like one; a dome's edge shift against the mirrored back is too close
to tell them apart, so the test wants a thick slab whose far face is flat
and a grid behind it, measuring the grid's displacement across the slab.

## A device binding may answer nothing to a recorder that cannot sample its surface

The media `DeviceBinding` contract (`include/sigilmedia/advanced/Skia.h`)
says `image(recorder)` wraps the surface for the recorder or reads back
when the recorder is null, and says nothing of a recorder that cannot
sample the surface, such as a Vulkan Graphite handed a Metal texture. The
Substance binding now answers its host copy there. The video decoder's
`FrameBinding` (`media/video/decode/Raster.cpp`) and the received frames
binding under `io/frames` may still answer null to such a recorder, so an
image leaf drawing them in a headless device sweep draws nothing. A
binding should answer the host copy wherever it cannot wrap, and the
contract should say so. A test should bind each kind of device frame for a
Vulkan Graphite recorder and find a drawable image. The copy that host
answer costs could go once the shared device imports a Metal texture into
Vulkan, as it already exports its own objects the other way.

## Contract: a live frame waits for a newly arrived program's pipeline

When a body's program arrives, the next live frame builds its pipeline on
a worker but waits for it, so hosts and tests that wait only on
`ProgramCache::compiling()`, such as `shadedFrame` in
`src/sketch/scene3d/test/Scene3DSessionTest.cpp`, see a whole body on the
following frame. The body should stand in until the pipeline is built, as
the first frame's bodies do, once those hosts and tests loop until
`SceneStats::programsPending` is zero; then `drawBody` can take the
cache's non-waiting answer for every live body. A test should run a live
scene whose program arrives mid-run and find no frame longer than a small
multiple of the steady frame.

## Present on device covers only the device Graphite's main recorder

`Gpu::present` takes the in-place path only when the canvas's recorder is
the device Graphite's own recorder; a recorder made from the same context
with `makeRecorder()` is read back. Any recorder of that context should be
served in place, which needs a SigilSkia query naming a Vulkan recorder's
device the way `drawsOnDeviceOf` does for Metal. A test should present onto
a surface of a second recorder of the same context and find readbacks zero.

## The tangent lane is filled only by the grid generator

`Mesh::tangents` is filled by `grid()`, so brushing follows the surface on
a sphere, a torus and a superellipsoid, while `revolve`, `extrude`, `box`
and the sweeps carry no tangents and an operation that rebuilds vertices
drops them, since a mis-sized lane reads as none. Every generator should
fill the lane and every rebuilding operation carry it. A test should brush
a revolved cap of a coarse profile and find no facet steps.

## Python lacks the text shadow, the settled text style, a face's axes and a paragraph's base direction

`FontVerbs::textShadow` and `TextShadow`, `compose::textStyleOf`,
`weave::Face::variationAxes` and `VariationAxis`,
`Paragraph::setBaseDirection`, `ParagraphBlock::baseDirection` and
`ParagraphField::BaseDirection` are not bound. Bind them and regenerate
the typing; a Python test should state a glow on a text leaf and find it
following a glyph's fade, and state a page's base direction and find its
levels resolved against it.
