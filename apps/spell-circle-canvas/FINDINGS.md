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

## `connect::Along`, `pin::`, `outline::` and `stamp::` are not bound in Python

`src/common/python/compose/Schemes.cpp` builds an `Operator` from every
stock arranging value and from `connect::Between` and `connect::ByLane`,
and `_t.OperatorLike` in `apps/python/sigil/typing/_types.pyi` names the
same set. `connect::Along` and the three adder families landed after the
bindings branched, and the `Anchor` readings the rail bindings carried
(`readAnchor`, `readAnchors`) went with the rail. The Python surface is
meant to be complete over the kit's operators. `stockOperator` should
take each of the four, the readings should return for `Along`'s stops,
`OperatorLike` should name them and `PARITY.md` should move them across;
a test should build each from Python and assert its additions by key.

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

## Python binds no path construction outside `sigil.skia`

`src/common/python/geometry/Shapes.cpp`, `Polylines.cpp` and the path
operation and profile files register `sigil.geometry.shapes`,
`sigil.geometry.path.operations`, `.profile`, `.blend`, `.crossing` and
`sigil.geometry.sections` as empty modules, and `compose.Shape` has no
builder of its own. A curve a compose signature takes — `TextPath.path`,
`Element.shape`, `MotionPath` — can therefore be made from Python only with
`sigil.skia.PathBuilder`, a Skia type the compose vocabulary does not
spell.

The empty modules evidently stand for `geometry::shapes` and
`geometry::path` (the Catmull-Rom polyline back to a path, the shape
generators), which `PARITY.md` lists as unbound. The smallest useful cut is
a cubic or a smooth curve through points returned as the `Shape` compose
already takes.

A test should build a curve through three points from `sigil.geometry`
alone, hand it to `TextPath(path=…)` and assert the run lays out along it.
Wanted by `python_type_atelier` (its curved baseline), `python_kit_specimen`
(the same cubic) and `python_live_signals` (its traces).

## A keyframe's colour terms are values, so a table cannot follow the sheet's palette

`textFx::tween` keyframes write `GlyphModifier::colorMultiplier`,
`colorAdd` and `colorScreen` as `material::Color` values when the table is
built (`sigilcompose/typography/TextFx.h`). A sheet that states its
palette as custom properties cannot hand one to a keyframe, so a table
that colours its glyphs in the sheet's colours states them twice.
`elastic_type`'s blush keyframes multiply each glyph toward the two
series colours the sheet also states as `--x` and `--y`, once as custom
properties for the traces and once as constants for the glyphs; a sheet
that restates `--x` recolours the trace and leaves the letters behind.

`textFx::tint` naming only `.from` already comes to rest at the glyph's
own ink, read when the effect is applied (`GlyphInfo::ink`). A keyframe
evidently means the same reach: a colour term that can name a custom
property (or the ink in force), resolved against the leaf's custom
properties when the effect is applied. How a keyframe spells that — a
`Fill` or `VarRef` in `GlyphModifier`, or a separate colour table beside
the geometric one — is an owner decision.

A test should ink a leaf under a rule setting `--x`, run a one-keyframe
tween whose multiplier names `--x`, and assert a glyph at local 1 draws
the property's colour over its ink; then restate `--x` on a class the
leaf carries and assert local 1 follows it.

## A run can be measured to the pen only from a held typeface, never from the family list or the leaf a sheet sets

`compose::runPens`, `measureRun`, `fitRun`, `atCapHeight` and `metrics`
(`sigilcompose/core/Measure.h`) take a `weave::TextStyle` or a
`weave::Type`, and both carry the face as a held `sk_sp<SkTypeface>`. The
resolution from a family name to that face exists:
`weave::FontContext::familyTypeface(family, style)`
(`sigilweave/fonts/FontContext.h`) answers one family name, and
`Scene2DContext::fonts` is that context. What is missing is narrower:

- the measuring doors take the held face, which is a Skia value a sketch
  may not hold, so the one family door ends in a type the sketch cannot
  pass on;
- no door takes a family LIST (`".SF NS, SF Pro, system-ui"`, as a rule
  declares it) or a `Text` leaf resolved through the cascade the page is
  set by, which is the resolution `intrinsicSize` already runs for a bake;
- a face's variation axes and their ranges can be read only through Skia:
  the compose kernel reads them itself (`FaceChoice.cpp`,
  `getVariationDesignParameters`) and exposes nothing.

So the one door left to a sketch whose type is named on a rule is
`Scene2DContext::measure` over an element, and that answers the leaf's
box: `LayoutText.cpp` rounds a passage's measured size up to whole pixels
(`std::ceil` on `measuredSize`), with no pen positions and no metrics.

The measuring doors evidently mean to answer for the type a page is
actually set in, now that a page names families rather than faces: a
leaf-taking (or family-list-taking) `runPens`, `fitRun`, `atCapHeight` and
`metrics`, plus an axis query, all resolving the face the way the cascade
does.

A test should set a text leaf in `fontFamily("Helvetica Neue")` at 34 px
with 0.6 px tracking under a sheet, and assert that a leaf-taking `runPens`
returns the same pens as `runPens` over a `TextStyle` holding the face
`familyTypeface("Helvetica Neue")` returns (so `.back()` is fractional,
not a whole pixel); that a leaf-taking `fitRun` to a width lands within
0.5 px of it; and that an axis query over a leaf set in the system
interface family reports `GRAD` with its minimum and maximum, and reports
none for a family without one.

Wanted by `axis_ripple` (it prints the `wght` overhang and the `GRAD`
drift to the whole pixel, fits its hero to the measure by measuring two
boxes and solving the line between them, and states the grade's range as
its own constants instead of reading the face's) and `eva_magi_defense`,
which seats its numerals with `atCapHeight` over a face-held type and meets
the same wall when it takes a family list.

## A glyph-outline decoration is cut from the glyphs at rest, so a text shadow cannot follow a track's deformation

`decorationOutline(Boundary::Glyphs)` hands a text leaf's decorations the
outline `TextLayout::glyphOutline()` builds from the placement
(`compose/core/paint/PaintContent.cpp`, cached against `measuredRev`). A
`textFx()` track's `GlyphModifier` — its scale, shear, offset and rotation —
is applied at paint time after that outline is taken, so a coverage
shadow, glow or bevel over that outline (`material::Filter::shadow`,
`Filter::bevel` in the effects of the leaf's ink) on a leaf whose letters a
keyframe table squashes and shears stays drawn around the letters where
they stood. `Boundary::Coverage` would follow them, but it rasterises and
traces the leaf's layer whenever it is invalidated, which a track moving
every frame does every frame. A pixel-reading shadow over the whole leaf,
`Element::filter(material::Filter::dropShadow(colour, {.offset}))`, does
follow the drawn letters; what does not is the coverage shadow a text's
own ink carries.

It evidently means a shadow that belongs to what the letters DO: the
glyph outline rebuilt from the deviated glyphs when a leaf carries
displacing tracks (the tracks already know their reach, and a settled
cascade could cache the deviated outline as a static one is).

A test should lay `textFx::tween({.to = GlyphModifier{.scaleX = 2}})` at
progress 1 on a one-glyph leaf with `decorationOutline(Boundary::Glyphs)`
and an outer glow (a zero-offset `Filter::shadow` with a spread), and
assert the glow's painted bounds span the doubled glyph's width rather
than the rest glyph's.

Wanted by `elastic_type`, whose words stand in a pool of shadow attached
under their rest extent because neither route above follows the letters.

The same outline is also painted in ONE colour: a `textFx()` track's
`colorMultiplier`, `colorAdd` and `colorScreen` modulate the glyph passes
and never reach a decoration drawn over `Boundary::Glyphs`
(`compose/core/paint/PaintContent.cpp` takes `glyphOutline()` once per
`measuredRev` and hands the union to the decoration). So a text shadow or
glow in the letters' own colour — CSS's `text-shadow` with no colour, which
is `currentColor` per glyph — cannot follow a wipe: under
`textFx::tint({.from = pale, .to = sung})` it glows the sung colour round letters not yet
sung. The fix that rebuilds the outline from the deviated glyphs should
carry each glyph's modulated colour too, or a glyph-outline decoration
should be able to say "the glyph's own colour".

A test should tint a two-glyph leaf with a track holding glyph 0 at local 1
and glyph 1 at local 0 (`tint({.from = black, .to = white})` on a white leaf), dress it with
`decorationOutline(Boundary::Glyphs)` and a zero-offset shadow in the ink
in force, and assert pixels of the shadow beside glyph 0 are lit and beside
glyph 1 are not.

Add `karaoke_wipe` to that entry's wanted-by list: its glow is a second,
blurred copy of the sung line sung from black, where one glyph-outline
glow on the line would do.

## An echo takes its colour as a value, so it cannot follow the sheet's ink

An echo is `material::Filter::shadow(material::Color, {.offset})` in the
effects of a node's fill or ink (`sigilmaterial/filter/Filter.h`). It is
not a `Fill`, so it cannot be written as the ink in force or as a custom
property, where `compose::Shadow::ink` now can. `nightingale_coxcomb`
still reads its palette's ink out of code for the echo under each display
line, and a theme swapped by a different token sheet would leave the echo
behind.

It is evidently meant to paint like every other decoration: the ink in
force when unstated, a property when named. Either a Compose echo verb
taking a `Fill`, or SigilMaterial's shadow colour made optional and read
as CSS's `currentcolor` where the effect is applied — an owner decision,
since the second reaches into SigilMaterial. A test should set
`ink(var("accent"))` on a root with `var("accent", red)`, give a child
text an echo naming the ink in force, and assert the echo's pixels are
red.

## The Python typing modules have not been regenerated since a custom property could hold a paint

`compose::VarValue` holds a paint and `var(name, Material)` sets one, and
the Python binding takes it, but the committed `sigil` typing modules were
not regenerated after the binding and `typing/refinements/compose.py`
changed, so they may still describe a custom property as a colour or a
length (`varDefaults` in the refinements still does). Run
`apps/python/sigil/typing/generate.py` against the matching extension,
widen `varDefaults` to take a material if the binding does, and commit
what it writes; the typing check should then accept a material beside a
colour and a length.

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

## A glow on changing text is a filter over the leaf's whole box, so it re-runs every frame

A text leaf's glow, halation or blur — an ink material's effects stage
or `Element::filter` — is a layer filter: the leaf's glyphs are drawn into
a layer the size of the leaf and the filter runs over every pixel of it.
When any glyph changes (a `textFx` track fading, tinting or substituting
it) the layer and the filter run again. On a sheet of rain like
`matrix_rain`'s, given a glow ink and run under `--bench` on raster, two
1280x780 leaves under dilate + colour + blur dominate the frame, where
the same leaves without the glow cost next to nothing. A glow that belongs to each glyph, and
follows that glyph's brightness, has no statement that costs per glyph.

The evident intent of "the ink glows" is a glyph-sized cost: a glyph drawn
with its halo, dimmed and tinted by the same `GlyphModifier` as its body.
What is missing is a filtered-glyph sprite: a blur (and a colour) that
lowers to a per-draw mask filter on the glyph pass, which Skia keys into
its strike cache, so each glyph at each size is blurred once and every
later frame blits it under the glyph's own colour and alpha. `textStroke`
is the one per-glyph pass that exists, and it can only say a hard
outline. `matrix_rain` states its heads' bloom as a gradient spot moved
by a bound transform, which is exact only because a head is always the
brightest the rain gets; a tail's glow, which should fade with it, is not
stated at all.

A test should draw one text leaf with a glow ink whose tracks change one
glyph's alpha per frame and assert that the per-frame paint cost does not
scale with the leaf's box (a 64x64 leaf and a 1280x780 leaf holding the
same glyphs cost alike within a factor), and that the halo about a glyph
at alpha 0.2 is 0.2 of the halo at alpha 1.

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


## API request: Python layout strings stop at the clipping declaration

The macrodata-terminal study naturally writes `.overflow("clip")`, which
raises TypeError: `Element::overflow` accepts only `Overflow`. The supported
`.overflow(Overflow.Clip)` works. Exact neighboring declarations
`.alignItems("center")` and `.justifyContent("space_between")` accept strings,
as confirmed by a native host probe and the checked converters in
`src/common/python/compose/Convert.cpp`. The interruption is therefore
specific to inconsistent convenience across these layout options. This is
an authoring request, not a rendering defect; other setters such as
`flexDirection` and `textAlign` also use native enum arguments.

Wanted: a checked Python string convenience for common layout values,
documented alongside the native enum form. `overflow` could accept
`"visible"` and `"clip"` while preserving native enum arguments. A regression
should compare enum and string clipping at the same overflowing content boundary
and assert that an unknown word raises a clear ValueError.


## API request: Python studies cannot declare a study category or display name

The Borges architectural study carries literary-study tags and prose, but
its native catalog row is registered with category `"Python"` and a display
name equal to its file stem. `registration` in
`src/sketch/python/cmake/register_sketches.py` fixes those arguments for
every Python entry. The `@sketch` declaration accepts size, background and
capture time; the sketch context does not expose a title or category setter.
C++ study registration can declare both independently of the lookup key.

Python is an authoring language rather than a visual subject. Wanted: a
literal category and display-name declaration that the existing Python AST
registration pass can read without importing the sketch, alongside its
existing dependency metadata. A study should be able to declare
`CATEGORY = "Study · Literature"` and an authored display name while keeping
`borges_library` as its stable key. Other metadata spellings are possible;
the requirement is equivalent author control across the two languages.

A regression should register an explicit Python study category and display
name, verify both in its catalog row and verify stem-based lookup. An entry
without those declarations should retain documented defaults. The metadata
reader should not execute module code to discover either value.


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


## Bug: Material root anchoring is discarded by Skia lowering

`Material::worldSpace` stores its flag in
`src/common/material/core/Material.cpp`, and its public contract anchors
the material to the root. `src/common/material/skia/Bases.cpp` copies a
composed gradient's PaintPart into the accumulated Paint and returns it
without transferring that flag. `Material::geometryDependent` also omits
the flag.

A native C++ probe uses ordinary Compose fills for both rows. A gradient
Material marked `worldSpace` restarts its red-to-blue ramp in each
translated 310-pixel panel, while a Paint marked `worldSpace` and wrapped
as a Material stays continuous on the 800-pixel root. At root x406 the
Material-level row is RGB(251, 65, 68), and the Paint-level row is
RGB(158, 97, 161). This is distinct from the hosted Pen defect: no Pen
leaf participates in this probe.

The intended root anchor should survive Material lowering and participate
in geometry-dependency classification. A regression should render both
authoring forms at identical root coordinates across translated, scaled
and rotated nodes, assert matching pixels, and assert geometry changes
invalidate the anchored Material's cached result. Local-space materials
should retain their existing behavior.


## Bug: Python clip callback typing disagrees with runtime invocation

`src/common/python/draw/Pen.cpp` invokes the mask callback passed to
`Pen.clip` with no arguments, while
`apps/python/sigil/typing/refinements/pen.py` declares `_t.DrawCallback`.
The generated type defines that as `Callable[[Pen], None]`. The Shahnameh
study's mineral outlines render with zero-argument closures over the
supplied Pen, but the correctly annotated one-argument callback cannot be
invoked by this binding.

The callback protocol and generated declaration should agree. A regression
should type-check the callback form accepted at runtime and render its
normal and inverted clipping, with the outer clip restored. An exception
inside the callback should preserve the same restoration behavior.


## API request: paragraphs cannot state the bidi base of their page

`src/sigilweave/paragraph/ParagraphAnalysis.cpp` calls
`unicode::bidi(m_text)` with automatic LTR fallback. The existing Unicode
primitive accepts `BaseDirection`, but Paragraph, ParagraphBlock,
ParagraphStyle and LayoutOptions do not carry it through.

The Shahnameh folio's Persian text shapes correctly. The limitation arises
when a numeric folio label is followed by English on a Persian page.
Native analysis of `۱۵۲۵ · Folio 22v` gives one level-zero run
automatically, versus three runs with levels 2, 1 and 2 under explicit RTL.
A paragraph partial accepting the existing BaseDirection value would let
authors preserve their page's bidi context without injecting directional
control characters into the content.

A regression should compare Paragraph's run levels with the explicit
Unicode primitive and verify that changing the base invalidates cached
analysis while an unchanged base reuses it.


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


## API request: Python filters cannot declare their sampling reach

The metal-cover relief samples its input on either side of a cut to derive
a surface normal. Native `Filter::of(material, sampleRadius)` lets an
author declare that reach to the Skia runtime filter. The Python binding
in `src/common/python/material/PaintEffect.cpp` exposes only the one-argument
form, and the generated declaration has the same restriction. Python
authors cannot express the sampling contract of this otherwise supported
native material. This is a parity gap, not a demonstrated image defect.

Wanted: the existing optional sampling reach on the Python construction
door. A binding regression should accept a positive reach and match the
native two-argument form on tiled GPU rendering, including a cut crossing
tile and layer boundaries. The one-argument form should retain its existing
behavior.


## API request: Python texture-slot typing excludes the empty layer input

`material.shader` types every `textures` value as a media or Skia Image.
Its runtime converter deliberately accepts None as an empty PixelSource.
That empty slot is how the Voyager and monochord filters declare `content`
for the executor to supply from the painted layer. Both studies render
successfully with `textures={'content': None}`, but that supported call
disagrees with the generated declaration.

The declaration should include the runtime's empty slot form, or expose an
equivalent typed declaration for executor-provided layer inputs. A regression
should type-check that form and render a content-sampling filter through it,
while still rejecting values that cannot be image sources.


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


## API request: Python cannot load a bundled typeface as a resource

The Mother-wall study authors its own machine glyphs and font data. The
native C++ surface can construct a Skia face from data and adapt it into
Weave. The inspected Python binding exposes Typeface.familyName and
family-based weave.typeface lookup, but no file or bytes constructor.
The study must assemble cached vector glyphs and their spacing itself to
keep the authored face, despite Weave already owning those responsibilities.

Wanted: a resource-hub typeface loader shared by both authoring languages,
with the same cache, lifetime and reload semantics as other bundled assets.
A regression should load a bundled face through both hosts, verify matching
family and glyph advances, and replace its resource without leaving the
previous face in a restarted session. This is a binding/resource gap, not
a claim that native font loading is absent.


## API request: Python cannot author the native text entrance

The Mother-wall study's typed inquiry uses retained native vector glyph
entrances. `src/common/python/compose/TextEffects.cpp` creates the textFx
submodule without exposing the native entrance, and the Python parity
chapter identifies Text.textFx and typeOn as missing. Reproducing the
entrance locally duplicates glyph selection, spacing and timing already
owned by native Compose and Weave.

Wanted: Python access to the existing native text entrance with the same
font, layout and scene-clock contract. A regression should render identical
face, text and progress through both authoring languages and compare which
glyphs are revealed, their advances, retained layout and final pixels.
An unchanged progress value should preserve the same retained text work.


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

## Performance gap: held textures still pay for their retained subtree

`BM_Draw_HeldTexture_SubtreeScaling` holds one 256 x 256 texture over
100, 500 and 2000 static children. Description and initial baking happen
outside the timed loop; each measured frame draws the unchanged composer.
All three arms report zero texture bakes and the same texture extent, yet
on a Release raster run the frame time still grows with the child count.

The texture avoids repainting content, but bound calculation still depends
on the size of content it already holds.
`core/cache/TextureBake.cpp` asks for local bounds before reusing an image;
`core/paint/Bounds.cpp` computes bake bounds by visiting descendants. The
composer's layout and volatility walks are gated on these warmed static
frames, and the benchmark reads recursive statistics outside timing. The
scaling establishes a remaining subtree cost; a profile is needed to
attribute the fraction spent in bound calculation.

Wanted: reuse settled subtree bounds and avoid static descendant preparation
when its invalidation inputs have not changed. Preserve changes in child
geometry, effects, world-space paint, inherited state and bound values;
holding a texture must not conceal a live input.

A regression should assert unchanged pixels and zero rebakes across held
frames, then change each relevant input and assert fresh bounds and pixels.
Traversal counters should verify that an unchanged held subtree can skip
its descendants. Keep the scaling benchmark as the cost comparison rather
than a machine-dependent timing assertion in a unit test.

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

## Bug: flow exclusions can read fixed-size glyphs before text layout

`core/layout/FlowAround.cpp::boundaryOutlineOf` reads the text layout's
glyph outline without first preparing a fixed-size target. Yoga can skip
measurement when both dimensions are given, and the two initial revision
sentinels compare equal. The derived exclusion can therefore be the target
rectangle while its later decoration uses the correctly laid-out glyphs.
This remains source-backed and needs a reduced rendering case.

Wanted: derive exclusions from the target's current placed glyphs. A
regression should wrap text around a keyed, fixed-size O or H on its first
frame and compare with an equivalent measured target. It should differ
from a rectangular exclusion and follow a centered target after resizing.

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


## Contract gap: a material stroke keeps its prepared surface in the description

`detail::MaterialStroke` in
`src/common/compose/core/paint/MaterialEffects.h` holds a
`mutable std::shared_ptr<MaterialStrokeCache>`, and its const `paint()`
in `MaterialEffects.cpp` replaces and mutates it. A `Decoration` is a
value an author keeps and hands to several trees, so two composers
painting one description — a `TextureScene` beside the window's
composer — write the same cache, and neither owns its lifetime.

A description is immutable once described; retained paint state belongs
to the instance that paints. The prepared `LitSurface` and its surface map
passes should stand beside the instance's other lit inputs, keyed by the
mark's place among the node's strokes. A test should paint one
description through two composers painting different surface maps and
find each painted as its own map; a second should describe the stroke
once, paint it, and find the description's bytes unchanged.

## Performance gap: a local bake inside a recording is always a raster

`src/common/compose/core/cache/GroupBake.cpp` and `TextureBake.cpp` make
a local bake's surface with `canvas.makeSurface(...)`. While a picture is
being recorded the canvas is the recorder's, which makes no surface, so
both fall back to `SkSurfaces::Raster` on a device destination and the
bake is uploaded each time it is drawn. `Composer::Impl::bakeSurface`
allocates against `paintDestination(canvas)`, which is the device under
the recording.

The local bakes should allocate against the same destination. A test on
a device should record a picture holding a `Cache::Texture` child and
find the child's bake texture-backed.

## Rendering gap: glyph batches draw a span's material unshaded

`GlyphRSXformBatches::addGlyph` in
`src/sigilweave/choreograph/Choreograph.cpp` builds every pass from
`style.foreground` and `PaintLayer::resolvedPaint`, and never consults
`PaintStyle::foregroundMaterial` or `PaintLayer::material`; the batches
have no material resolver. The paragraph draws, `ParagraphLayout::draw`
and `drawBatched`, paint those passes through `paint::setMaterialResolver`
over the glyph ink bounds. So a span whose material paints it at rest
draws in its configured foreground as soon as a text effect moves its
glyphs into the batches.

One material should paint a glyph the same way at rest and in motion. A
test should draw one styled glyph with a `foregroundMaterial` and an
installed resolver twice, through `drawBatched` and through
`GlyphRSXformBatches` at the same pose, and assert that both resolve the
material once per bucket and produce the same pixels. Layer materials
want the same.

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

## World reads no additive reflection

`SurfaceOptions::reflectionWeight` and `surface::Reflection::Additive`
lower into the surface program, whose body writes the weight into
`gSurfaceReflection` (`src/common/material/surface/Lower.cpp`,
`surface/shaders/Surface.slang`). World's scaffold
(`src/common/world/diligent/shaders/Surface.slang`) declares the variable
and reads nothing from it: every surface mirrors the environment through
its own reflectance and Fresnel, so the additive recipe draws exactly as
the split-sum one does.

The additive recipe was evidently meant to add `reflectionWeight` of the
environment with no Fresnel, as its header says. Either the scaffold
honours it — a term `environmentReflection` already exists — or the
choice and the field go. A test should shade one card under one
environment with each recipe and assert the additive one differs by the
weight, or, if the field goes, that no consumer spells it.

## A stack of two surface programs cannot be drawn on the device

A surface program declares ten sampled slots
(`src/common/material/surface/Surface.cpp`) and World's scaffold six of
its own (the base-colour map, two panoramas, two convolutions and the
split sum's table), which is the sixteen a fragment program may declare.
`material::over` of two surface programs asks for twenty-six and is
reported over budget, so a stacked surface is drawn as its bottom.

A stack of two lit surfaces was evidently meant to shade as a stack
(`reference/DEVICE.md`). Wanted: slots an operand never reads left out of
the composed program, or the scalar maps packed into one texture per
surface as a page's are. A test should compose two surface programs with
`over`, compile it for the device and assert it stays within the budget
and draws both operands.

## The environment's prefiltered chain is a box blur, not the lobe it is read by

`EnvironmentMap::chain` (`src/common/material/texture/EnvironmentMap.cpp`)
builds each roughness level by three wrap-aware box blurs whose radius is
a fixed function of the level, in panorama texels — the same width at the
poles as at the horizon — while the device reads it as the GGX lobe of
that roughness and weights it by a split sum integrated from that lobe.
A rough surface's reflection is therefore narrower or wider than the
energy the table assigns it, by an amount that changes with latitude.

Evidently intended: each level the panorama convolved with the GGX lobe of
its roughness, importance-sampled in direction rather than in texels.
A test should bake a panorama with one bright texel, prefilter it, and
assert the lobe's angular width at each level matches the width the
distribution gives that roughness, within a stated bound, at the horizon
and near a pole alike.

## Translucent authored content is mixed with the scene in light

A surface drawn as authored (`material::surface::unlit`) is written into a
frame's half-float target as the light the final stage turns back into its
colour, so where it is opaque it leaves exactly as authored. Where it is
translucent — the antialiased edge of a wire, a half-transparent HUD panel
on a quad at the near plane — it is blended with what is behind it in
light, before the tone curve, where a page composited over a picture is
blended in display values. `vagrant_story_target`'s reach sphere and HUD
plates and `metal_instrument`'s overlay differ from their earlier plates by
up to 32 levels along such edges.

Wanted: a frame overlay — a page laid over the finished picture after the
final stage, in display values — for content that is about the picture
rather than standing in the scene, as the kit's planned frame overlay would
be. A regression should lay a half-transparent grey panel over a bright scene
both ways and assert the overlay matches a Skia composite of the same
panel over the same picture to within a level.

## Bloom blurs at full resolution and stops widening past a radius

`world::bloom` thresholds and blurs at the frame's own size, through the
post stage's separable Gaussian, whose taps stop at sixty-four a side. The
widest spread is therefore a sigma near twenty-one pixels, and a
`Bloom::radius` much past seventy pixels is drawn as that. A large frame
pays for every tap at full resolution.

Wanted: a chain of half-resolution targets, blurred and added back up, so
the radius is a property of the chain rather than of one blur. A regression
should bloom a small emitter at two radii past the current limit and assert
the wider one reaches farther, and a bench should hold the cost at a
1440 × 960 frame.

## Python cannot build a flipbook or a flow map

`material::flipbook` and `material::flowmap` take a `material::Texture`,
which Python does not bind (an image reaches Python materials only through
`image(...)`). `material.channel`, the surface's `blend`, `depthWrite` and
`depthFade`, `world.bloom`, `world.Facing` and `Mesh.facing` and
`media.decode(..., colorSpace=)` are bound.

Wanted: a Python `Texture` with `colorSpace`, `mipmaps`, `sampling` and
tiling, and the two factories over it. A Python test should play a 2 × 2
sheet at two times on a page and read the two cells.

## A World body reads an animated texture at its first frame

`geometry::device::TextureResidency::sample` reads `Texture::frameAt()` with
no time, and `Texture::deviceImage` reads its source at time zero, so a video
or an animated image worn by a World body shows its first frame whatever
the frame's time. A flipbook advances because its clock is a uniform, not a
new image.

Wanted: the residency handed the frame's time and keyed by the image the
source answers then. A device test should wear a two-frame animated image
and read each frame at its own time.

## The USD reader ignores a texture's stated colour space

`usd/read/Material.cpp` fetches each `UsdUVTexture`'s file and drops its
`sourceColorSpace`; the role a map fills decides colour or data afterwards.
A texture authored `raw` in a colour role, or `sRGB` in a data role, is read
by its role. The writer states the colour space it read.

Wanted: the decoded part carrying each texture's stated space, and
`texture::fromUsageMap` honouring it. A regression should round-trip a
material whose base colour is stated `raw` and read it back as data.

## The USD reader drops the optional lobes the writer keeps

`usd/write/Material.cpp` keeps a surface's sheen, brushing, thin film and
specular in the material prim's custom data (`sigil:sheenColor`,
`sigil:anisotropy`, `sigil:iridescenceThickness`, `sigil:specular` and
their siblings), because UsdPreviewSurface has no input for them.
`usd/read/Material.cpp` reads `sigil:transmission` back and none of
these, nor the transmission lobe's `sigil:thickness`,
`sigil:attenuationColor`, `sigil:attenuationDistance` and
`sigil:dispersion`: `geometry::mesh::codec::decode::Part` has no field for
them, so a stage written and read back loses every lobe but the
transmission weight.

Wanted: the decoded part carrying the lobes and the reader filling them,
and the part's lowering into a material stating them. A regression should
write a sheened, brushed surface and a tinted glass and read them back
with the same lobes.

## A decoded part's transmission reaches no material

`geometry::mesh::codec::decode::Part` carries `transmission` and `ior`,
which the glTF reader (`mesh/codec/Gltf.cpp`, from
`KHR_materials_transmission` and `KHR_materials_ior`) and the USD reader
fill, but nothing that lowers a decoded part into a material reads them,
so an imported glass model draws opaque. The glTF reader reads neither
the volume (`KHR_materials_volume`: thickness, attenuation colour and
distance) nor `KHR_materials_dispersion`. Intended: a part states the
whole transmission lobe and its lowering states it on
`SurfaceOptions`. A test should import a glTF glass cube with a volume and
find `material::lobes` of the lowered material carrying transmission at
the file's thickness and attenuation.

## A stack does not mix the optional lobes by its mask

`material::over` mixes each operand's per-pixel surface terms by the
coverage between them (`core/shaders/OverPrelude.slang`: `overClear`,
`overTaken`, `overSay`), and none of the sheen, brushing, film,
specular, transmission or distortion variables are among them. A stack
whose operand transmits is also drawn among the bodies laid over, not
those that see through themselves (`world/diligent/Geometry.cpp`,
`seesThrough` asks `surface::lobes`, which answers nothing for a composed
recipe), so it reads no copy of the scene and refracts the panorama only. A stack whose operand states a lobe
compiles the lobe in, and the variables stand as whichever operand wrote
them last, over the whole face rather than where the mask lets that
operand show.

Wanted: the lobe variables cleared, taken and mixed with the others,
without making every stack carry every lobe — the composed body could
write a lobe's variables only when an operand's body does. A test should
stack a sheened surface over a plain one under a half mask and read the
sheen at half its strength.

## A brushed highlight shows a coarse mesh's facets and a uv pole

The tangent a brushing runs along is read off the screen derivatives of
the position and the uv (`world/diligent/shaders/Surface.slang`,
`tangentAxes`), which are constant across each triangle, so a strongly
brushed highlight on a coarse mesh steps at the triangle edges; on a
generated sphere the uv's poles pinch the brushing into a knot. No mesh
lane carries an authored tangent (`geometry/device/residency/Meshes.cpp`
packs position, normal, uv and colour).

Wanted: an optional tangent lane — a mesh primitive the generators fill
and the vertex layout carries when present — read in preference to the
derivative frame. A device test should brush a sphere of few segments and
assert the highlight varies smoothly across a triangle edge.

## A directional light's cascades are refitted every frame

`fitCascades` and `fitDirectional` in
`src/common/world/diligent/Geometry.cpp` fit each cascade to the box of
what the camera sees of the scene in its slice, snapped to whole texels.
The box changes size as the camera turns, so the texel size changes and a
still shadow shimmers along its edges while the camera moves. The
intended fit is a bounding sphere of each slice, whose size does not
change with the camera's turn, snapped in the light's own space. A test
should turn the camera a few degrees over a still scene and assert that a
shadow edge seen at the same world point moves by no more than a pixel.

## A transmissive body casts no shadow at all

`casts` in `src/common/world/diligent/Geometry.cpp` leaves out every body
that sees through itself, so a glass, a water surface or a tinted slab
lets the whole of a light through, uncoloured. The intended behaviour is
a shadow that keeps what the body transmits: tinted by its base colour
and its attenuation over its thickness, and partial where its
transmission is. It needs a coloured shadow map beside the depth atlas,
or a transmittance read through the body. A test should stand a green
transmissive slab between a casting light and a white plate and assert
that the plate behind it is darker than beside it and greener than red.

## A shadow's softness does not widen with distance from its caster

The filter in `shadowAt` in `src/common/world/diligent/shaders/Surface.slang`
spreads every edge over the same few texels, so a shadow beneath a
resting body is as soft as one thrown far across the ground. Real
penumbrae widen with the distance between caster and receiver. The
intended form searches the map for the blockers around a point and
scales the filter by their distance and the light's size. A test should
compare the edge width, in pixels, of a shadow cast from a card 10 units
above a plate with one cast from 100 units above, and assert the second
is wider.

## Per-body shadow flags do not reach a USD stage

`Writer::mesh` in `src/common/usd/write/Mesh.cpp` writes geometry,
placement and material, and nothing of a body's `castShadow` or
`receiveShadow`; a stage read back casts and receives everywhere. The
lights' own shadow switch and settings are written. The intended mapping
writes a body that opts out as custom data on its prim, or folds it into
each casting light's shadow link. A test should write a mesh with
`castShadow(false)` and assert that the body read back carries the flag.

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

## A sized light's shadow is as hard as a point light's

A rect, sphere or disk light that casts does so from its centre through
the same maps a spot or point light uses (`drawShadows` in
`src/common/world/diligent/Geometry.cpp`); the filter's radius does not
widen with the light's size or with the distance from the caster, so the
penumbra does not grow away from the contact point as it does under a
real softbox. A rect or disk light's map opens fifty degrees about its
normal and nothing further off its face casts. Intended: contact-hardening
shadows whose filter widens with the light's size times the
caster-to-receiver distance over the light-to-caster distance, searched in
the light's map, and a panel's map covering its whole front half. A test
should assert that a sphere's shadow edge under a large rect light is
sharper at the contact point than a body's length away from it.

## A sized light's sheen, brushing and range are simplified

Under a rect, sphere or disk light a sheen is lit as by a point light at
the light's centre carrying the whole light, and a brushing keeps the
isotropic representative point and stretches the lobe there. Neither
follows the light's shape, a rect and a disk have no `range` window, and
a node's scale does not resize a sized light. A test should assert that a
brushed plate under a long strip light aligned with the brushing shows a
longer highlight than one across it, and that a sheen under a wide panel
grazing a cloth plate spreads over the panel's width.

## API gap: a lane a point chain fills does not reach its instances

`pop::Builder::instances(stamp, attributes)` in SigilGeometry carries a
named lane to the instances only when the cooked points hold it as one
scalar per point. The lanes a chain itself creates — `fill`, `cluster`,
`select` — hold four numbers per point, so naming one in `attributes`
carries nothing, and a chain cannot feed a flipbook's `frameAttribute`.
`flipbook_fire` and its Python port build their `Instances` by hand for
that reason.

A lane the chain filled should reach each instance as the scalar the
material asks for, by its first number. A test should assert that
`pop::on(loop).fill("cell", {3, 0, 0, 0}).instances(quad, {"cell"})`
answers 3 for every instance's `"cell"`.

## A scene3d sketch's first frame still builds its pipelines and its fixed programs on the thread that draws

A live scene3d sketch no longer waits for a surface program — a body whose program
is compiling is drawn as a stand-in — but the pipeline built from each
program (`PipelineCache::pipeline` in
`src/common/geometry/device/residency/Pipelines.cpp`) and the backend's
own fixed programs (`scaffold`, `shadowScaffold`, `postPrograms`,
`backdropProgram` in `src/common/world/diligent/Programs.cpp`, compiled on
first use) are still built inside the frame on the render thread. The
driver's pipeline cache is kept between runs, so this costs the first
launch on a machine and the first use of each fixed program in a process.
The intended behaviour is that a pipeline is built on a worker as a
program is, and the fixed programs are compiled when the device comes up,
off the thread that draws. A test should open a scene3d sketch whose pipelines are
not cached and assert its first live frame returns before any pipeline
is built, and that a later frame draws the body whole.

## A canvas on the shared Vulkan device still reads a scene3d sketch's picture back

`Gpu::present` in `src/common/world/diligent/Gpu.cpp` hands a picture
over in place only to a Metal recorder — a window's. A canvas drawn
through the device's own Graphite on Vulkan (`--bench --gpu`, a headless
sweep on the device) stands on the very queue the picture was drawn on
and could wrap the image with no fence at all, but is read back. Only a
live run would take that road: a capture that will be diffed reads back
by design. A test should present a live frame onto a surface of the
device's own Graphite and assert `SceneStats::readbacks` is zero and the
pixels equal the readback's.

## The geometry kit's solids are not comparable values a node can keep

`gm::superellipsoid`, `quad` and the other solids in
`src/common/geometry/include/sigilgeometry/kit/Solids.h` return a formed
mesh, so a scene3d sketch that spells one in `describe()` builds the triangles every
frame only for reconcile to find them equal; `world::Generator` keeps a
cook by value but needs a hand-written model. The shape vocabulary in
2D already answers with comparable values. The intended shape is a solid
spelled as a value (its radii, exponent and divisions) that a node's
geometry slot holds and cooks once while it is unchanged. `set_stagger`
forms its meshes once in members until then. A test should describe a
scene3d sketch naming a solid value twice and assert the solid was formed once.

## The lit bodies laid over are most of a full-size scene3d sketch's device time

Timed with `Frame::timeStages`, a full-size window frame of `first_light`,
`shadow_play`, `soft_boxes` and `world_hud` spends almost all of its
device time in `colour.laid` — the clear, the sky and every opaque body
drawn through its surface program — and little in the shadow stage, the
copies or the final stage. `drawBodies` in
`src/common/world/diligent/Geometry.cpp` draws the opaque bodies back to
front, at the frame's full device-pixel size, and every lit pixel runs the
surface program over every light reaching it plus a nine-tap shadow
filter per casting light. What is evidently intended is that the frame's
cost follows what is seen, not the window's pixel count times the
overdraw. Which part dominates — overdraw, the light loop or the shadow
filter — is not yet measured: a test should time `colour.laid` for a set
of overlapping opaque cards and assert it stays near one card's cost
whatever their count, and time one lit card under one casting light and
under none and bound the difference.

## The scene's smaller levels are made in full for any surface that sees through itself

`drawBodies` in `src/common/world/diligent/Geometry.cpp` copies the lit
colour and generates every level down to one texel
(`Gpu::sceneColorForReading`) in every frame that holds a transmissive or
distorting body, whatever the roughest such body can read. What is
intended is that a frame makes only the levels its seen-through bodies'
roughness reaches. A test should time a frame holding one smooth glass
pane and assert no `colour.mips` stage beyond the levels a roughness of
zero reads, and that a rough pane's picture is unchanged.

## Device tier: four scene2d sketches stand beyond tolerance of their CPU plates

`sigil.py plates --tier device` renders every scene2d sketch on the CPU
and on the device and compares them per colour channel. Four stand OVER
the tolerance (mean above 12, p99 above 128): `andromeda_optical_lab`,
`stargate_slit_scan`, `voyager_instruction_plate` and
`substance_swatches`. Each of them paints through a runtime effect or a
material map, so the drift is in how Graphite and the raster backend
execute the same program, not in the sketch. The tier's exit status is
1 while the rows read OVER, and the scene3d set's byte-neutral verdict
stands beside it.

A scene2d picture should be the same picture on either backend within
the tolerance the tier states, or the sketch should say which backend it
is drawn on. A test should render each of the four on both backends at
the plate moment and find the per-channel mean and p99 within the
tier's tolerance; until then the four rows are the finding, and a run
of the tier is read by its rows and not by its last line.

## Authoring gap: a body cannot wear one region of a page

`sketch::Page::surface()` is the whole page: a body shows all of it, so a
scene whose bodies each show a different part of one laid-out page paints
one page per body instead. `carved_marks` carries four pages for four
bodies that could share one.

A page's surface should take a region — a rectangle in page units — that
the body's texture coordinates map onto, so one page lays its parts out
and each body wears its own. A test should paint one page with two
coloured halves, dress two cards with the two halves, and find each card
the colour of its half.

## Authoring gap: an extruded solid's walls cannot wear a page

`geometry::mesh::extrude` writes the walls' texture coordinates along the
outline's length, so a page worn by the solid stretches round its walls
and the top face is the only face that reads as the page. `carved_marks`
builds every solid twice, a top that wears the page and walls that wear a
plain surface.

The extrusion should let the caller say what the walls wear: their own
coordinates (the outline's length) or the top's projected down, and a
solid should be able to carry two surfaces, one per face set, as a
`Mesh` with named parts does. A test should extrude a square with a page
projected onto it and find the wall pixels the colour of the page's edge.

## Authoring gap: an extrusion has no rounded or bevelled edge

`geometry::mesh::ExtrudeOptions` states caps and walls and nothing about
the edge between them, so a cut pane with a half-round rim is lofted by
hand from outlines inset along the round, which holds only for a convex
outline without holes. `glass_forms` builds its panes that way.

The extrusion should take an edge: a radius or a bevel width and its
segments, applied round every outline including holes. A test should
extrude a square with a round of radius r and find the rim's normals
turning smoothly from the cap to the wall across r.

## Renderer gap: a height-derived normal cannot dress a device body

`material::surface::normalFromHeight` is a Skia program with no Slang
body, so a body drawn on the device cannot wear a normal derived from a
height material; only an image map reaches it. `glass_forms` still fills
its haze map pixel by pixel from a bitmap for that reason, and any reeded
or hammered glass needs an image.

The normal-from-height recipe should carry a Slang body, as the other
shared recipes do, so a device body takes it like a page does. A test
should dress a device card with a normal from a noise height and find the
shading vary across it.

## Renderer limitation: refraction sees one layer and bends at one surface

A transmitting body reads the opaque scene behind it and bends the eye's
ray at its front surface, carried the stated thickness; it does not see
another transmitting body behind it. So a glass front cannot stand before
a liquid, a rod broken at a liquid's surface needs an optical thickness
far beyond the real depth, a magnifier's power is its thickness and too
much inverts the picture, and an enlarged scene is the rendered pixels
upscaled. `glass_forms` states its thicknesses as optical depths chosen
for the bend and leaves the cell's front as a reflection-only surface.

Transmission should see through transmitting bodies behind it, in order
of depth, for at least one more layer, and a body's exit surface should
take part in the bend. A test should stand a tinted slab behind a clear
one and find the tint through both.

## Authoring gap: no cellular noise

`material::noise` is turbulence and grain; it has no cellular or Voronoi
option, so a crazing or a cell network is built from `path::voronoi` cells
drawn as shapes. `ceramic_glaze` does so.

The noise should offer a cellular mode — distance to the nearest seed and
to the second, with a jitter — as a material, so a crackle is a fill. A
test should sample the cellular noise at a seed and find zero there and a
rise away from it.

## Performance gap: a many-stop gradient mask on a headless page paints per pixel

In a headless plate sweep a page paints on the CPU, where a ramp of two
hundred and fifty-six stops used as a mask is evaluated per pixel in every
map pass; `ceramic_glaze`'s first plate timed out at the sweep's limit
until the depth was painted once into a strip and repeated. A live window
paints the same page on Graphite.

A ramp used as a mask should be sampled from a lookup once per paint,
not evaluated per pixel per pass, and a headless page that is static
should paint once and be kept. A test should time a page masked by a
many-stop ramp on the CPU against the same page masked by a strip and
find them within a small factor.

## Renderer limitation: a distortion map offsets in view space and only where the surface is translucent

A surface's distortion map moves what is seen behind it by an offset in
view space, not along the surface's own directions, so a meniscus map on
a revolved bead cannot push radially; and the offset is laid over the
picture by the surface's alpha, so on an opaque transmitting body it does
nothing. `wet_glass_console` bends its beads by transmission and curvature
alone.

A distortion should be stated in the surface's tangent frame and apply
to a transmitting body's refracted read. A test should dress a sphere
with a radial distortion and find the page behind it displaced outward
on every side.

## Rendering gap: a curved transmitting body shows the page behind it darker and softer than a flat one

Beads and swept trails in `wet_glass_console` show the page roughly a
fifth to two fifths darker and blurred compared with the flat glass sheet
over the same page, at an index of one, zero thickness and a roughness
near zero, with or without the sheet. On a dark page the beads read as
dark lumps with bright rims. The probe stills stand in the session's
scratchpad; the cause is not found.

A transmitting body at an index of one and zero thickness should show
the page behind it unchanged whatever its curvature. A test should stand
a sphere and a flat card, both at that setting, over a bright page and
compare the mean of the pixels through each.

## Performance gap: a page repainted every frame is painted on the CPU in a capture

On the still and plate path a page paints on the CPU raster, so a
page-wide runtime-effect material costs seconds per frame and a capture
stepped to its moment runs for minutes; `wet_glass_console`'s first plate
timed out and its page dropped its halftone and noise layers to render.
The window bench of the same sketch failed its gate on the describe
phase. A live window paints the page on Graphite.

A capture should paint pages on the device it has, as the window does,
and a static layer of a changing page should be kept rather than
repainted. A test should capture a page carrying a runtime effect and
find it drawn through the device recorder.

## Authoring gap: an instance cannot state its own transmission thickness

An instanced stamp carries a colour and named scalars to the surface,
but transmission thickness reads the surface's value only, so
`wet_glass_console` stands its beads in three size classes with three
thicknesses instead of one body whose thickness follows its size.

Thickness, like the flipbook frame, should be readable from a named
instance attribute. A test should instance one bead at two sizes with a
thickness attribute and find the page bent more through the larger.

## Authoring gap: the combined Python carved-and-plated study is not written

The owner wants a Python 3D scene that combines carved marks and metal
linework with Compose driving the materials and no geometry doing it. A
Python sketch now dresses a baked body in Compose through
`ctx.page(size)`, `page.render(tree)` and `page.surface()`, and
`python_plaque` wears one page with raised type; the combined study itself
is not written. It should stand beside `python_plaque` as a Python bake
whose bodies wear pages only, and its device plate is its test.

## Performance: wet_glass_console is very demanding

The owner reports `wet_glass_console` as very demanding in the window;
its window bench fails its gate on the describe phase. Whether that is
the six hundred instanced transmitting beads, the page repainted every
frame, or the three swept rivulets rebuilt per frame is not measured.
Profile it with the per-stage timers and `--bench`, name the cost, and
fix it at the library if the cost is the library's.

## Authoring gap: a span ink and a ribbon answer no unit source, and unit normals stay in the node's frame

A span ink with no unit and a material ribbon answer no unit source, so a
material's relief lies flat under them and its brushing runs along the
surface. A glyph or word ink uses its box's long axis as its direction,
since the layout carries no stem direction; every normal but the
brushing direction is written in the node's frame rather than turned
into the page.

Give the span ink and the ribbon their unit — the ribbon's tangent is its
direction — and turn every normal into the page as the brushing is. A
test should state a relief on a span ink and find the page's height cut;
a ribbon brushed along itself should stretch its highlight along a
curve.

## Rendering probe: a near-mirror surface shows a grid of small soft boxes as one even field

Under no lights and a near-black environment holding a grid of tiny soft
boxes, a chrome swatch at roughness 0.05 shows one even white field rather
than separate reflections, on a page and on a bare quad alike. Whether
the environment's prefiltered chain blurs its first level, or the kit's
soft-box size means something other than its width, is not known. A test
should render a mirror sphere under two small soft boxes and find two
separate highlights.

## Contract limit: a page's range only rises, carries one volume, and clips a live emission past its brightest stated value

The emissive, thickness and distortion maps' white stands for the
brightest value the page's content states, learned from stated values;
so a page that once stated a bright emission keeps the coarser precision,
a live binding that animates an emission past the brightest stated value
clips, and a page carries one volume (index, attenuation, dispersion),
the first transmitting material's, reporting a second once. A page
should take a stated range and a per-region volume. A test should bind
an emission above the page's brightest and find it not clipped.

