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

## A surface is lit in light and presented as though it never was

No body on either surface path applies a transfer function in either
direction. `src/common/material/kit/shaders/Unlit.sksl` and
`Surface.sksl` multiply `baseColor` by the sample of `baseColorMap` and
return the product; the raster surfaces they are drawn onto carry no
colour space, so Skia transforms neither the texel on the way in nor the
product on the way out. `src/common/world/diligent/shaders/Surface.slang`
multiplies by the same map, compresses the sum through
`material::toneMap` on luminance, and writes to a UNORM target; no
texture and no target in `src/common/world` or `src/common/geometry/device`
is created in an sRGB format. Every number on the path is therefore the
number an image stores and a display shows.

Two things in the tree say the path was meant to carry light. `toneMap`
states that what it takes is a radiance read at the set's exposure and
compressed onto what a display can hold, and a radiance is linear;
`SurfaceParameters::gold()` and `chrome()` carry measured reflectances,
and gold's `(1.0, 0.766, 0.336)` is its linear value — the same colour
encoded is near `(1.0, 0.894, 0.625)`. So a lit surface integrates a
measured quantity against map samples that are display numbers and hands
the result to a target that takes it for a display number, and the metals
the kit ships read duller than the metals they were measured from.

The bodies should linearise the samples they multiply and encode what
they return, and the tone map should then stand between two quantities
of light. A test should assert that a mid-grey texel over a white base
colour comes back the mid-grey it went in as; that doubling a light moves
a mid-grey by a stop rather than by doubling its code; and that gold's
measured reflectance under a white environment presents at the code that
colour presents at. Every plate carrying a lit or unlit surface moves,
and a plate rebase names the cause.

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
photographs a canvas on a raster surface so the picture is reproducible,
and a set is drawn by the device's own renderer, so the lane builds no
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
`SketchContext::measure` and rescales the font size twice (tracking is
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
`SketchContext::fonts` is that context. What is missing is narrower:

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
`SketchContext::measure` over an element, and that answers the leaf's
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

## Catalog plate extents differ beyond the permitted material and label changes

The Release CPU sweep renders the following 84 sketches at different
extents from the standing machine-local plates. A change to grain,
glyph-edge effects or a readout label does not explain these page-size
changes. For example, `cjk_rules` and `shape_tour` have no source edits in
the sketch pass, yet both now capture taller pages. Some retained plates
therefore also disagree with presentation choices already in the source.
The larger canvases have not been adopted as API-migration baselines.

The intended constraint is to preserve each sketch's picture unless its
particular change is approved. A test should capture the declared frame
with promotion off and assert the intended viewport extent and the
positions and sizes of its content; a caption or material change should
not silently resize the rest of a page. The owner should resolve the
source-versus-baseline intent before these plates are rebased.

The two pictures for each row remain under
`build/plates_Release/baseline/plate_<name>.png` and
`build/plates_Release/cpu/plate_<name>.png`. The existing
`python3 scripts/sigil.py plates compare build/plates_Release/baseline build/plates_Release/cpu`
command reports the size disagreements.

| Sketch | Standing extent | Rendered extent |
|---|---:|---:|
| `blend_options` | 2400x2660 | 2400x2780 |
| `blur_falloff` | 2160x860 | 2200x1920 |
| `border_weave` | 2200x848 | 2200x1680 |
| `bound_lane` | 2400x1687 | 2400x1931 |
| `cascade` | 2336x1624 | 2200x2300 |
| `channel_bind` | 2360x1136 | 2360x1480 |
| `chevreul_circle` | 2400x1600 | 2400x3000 |
| `cjk_rules` | 2200x724 | 2200x1620 |
| `codec_roundtrip` | 2240x1400 | 2240x1820 |
| `contour_poses` | 2200x1544 | 2200x1880 |
| `corner_notched` | 2200x1240 | 2200x1640 |
| `coverage_boundary` | 2200x848 | 2200x1880 |
| `crossing_rule` | 2240x1472 | 2200x1900 |
| `crt_bloom` | 2000x1000 | 2000x1480 |
| `curve_shelf` | 2200x1196 | 2200x2300 |
| `data_sources` | 2200x840 | 2200x1280 |
| `decay_step` | 2200x800 | 2200x1540 |
| `ember_decode` | 2000x860 | 2000x1000 |
| `encode_write` | 2200x800 | 2200x1400 |
| `env_faces` | 2200x1308 | 2200x2440 |
| `env_lanes` | 2200x848 | 2200x1880 |
| `eva_magi_interior` | 2400x1753 | 2400x1344 |
| `exact_tangent` | 2200x800 | 2200x1700 |
| `exr_channels` | 2160x792 | 2200x1640 |
| `fallout2_charsheet` | 2560x2176 | 2560x2320 |
| `feed_vitals` | 2400x787 | 2360x1440 |
| `field_shelf` | 2200x1272 | 2200x2320 |
| `formation_bands` | 2200x1496 | 2200x1880 |
| `frame_grid` | 2200x1480 | 2200x1880 |
| `frame_inputs` | 2200x1292 | 2200x1620 |
| `fx_scatter_mix` | 2200x800 | 2200x1400 |
| `genesis_fire` | 2400x1543 | 2400x1600 |
| `geo_groups` | 2400x880 | 2400x1300 |
| `gif_frames` | 2240x1120 | 2240x1520 |
| `grid_layouts` | 2200x1244 | 2200x1520 |
| `half_float` | 2200x800 | 2200x1640 |
| `hitman_verlet` | 2400x1415 | 2400x1433 |
| `hub_reload` | 2200x800 | 2200x1400 |
| `keeps_and_frames` | 2200x1208 | 2200x2360 |
| `kinetic_card` | 2360x1240 | 2360x1860 |
| `lane_retarget` | 2200x800 | 2200x1380 |
| `live_settling` | 2200x800 | 2200x1180 |
| `material_atlas` | 2200x1320 | 2200x1760 |
| `material_slots` | 2120x1380 | 2200x1860 |
| `matte_luma` | 2360x1240 | 2200x2360 |
| `mesh_generators` | 2400x857 | 2400x1182 |
| `net_policy` | 2200x800 | 2200x1300 |
| `night network` | 1800x1280 | 2400x1259 |
| `nine slice` | 1800x1280 | 2200x1660 |
| `noise_shelf` | 2200x800 | 2200x1760 |
| `ocio_view` | 2200x1280 | 2200x1880 |
| `over_under` | 2200x1244 | 2200x2360 |
| `paint_shelf` | 2200x1292 | 2200x2440 |
| `painter_gpu` | 2360x1400 | 2360x1680 |
| `paragraph_sheet` | 2400x1987 | 2120x2560 |
| `path_booleans` | 2400x1393 | 2400x1896 |
| `pattern_sequence` | 2200x1272 | 2400x1620 |
| `pixfont_dotsprite` | 2200x848 | 2200x1680 |
| `place_repeat_tiles` | 2200x940 | 2200x1780 |
| `pop_billboards` | 2200x1496 | 2200x1800 |
| `pop_deform` | 2400x1664 | 2400x1722 |
| `pop_math` | 2200x1344 | 2400x1840 |
| `pop_order` | 1520x1000 | 2200x1380 |
| `pop_prims` | 2400x1393 | 2400x1509 |
| `pop_stamps` | 2400x1393 | 2400x1741 |
| `psx_doom_fire` | 2720x1520 | 2720x1536 |
| `rich_slot_reserve` | 2200x848 | 2200x1600 |
| `routers_straight` | 2200x800 | 2200x1800 |
| `routes_probe` | 2200x940 | 2200x1520 |
| `sdf_star` | 2200x1280 | 2200x2000 |
| `shape_tour` | 2240x1680 | 2240x1960 |
| `shapeworks_lab` | 2400x1462 | 2400x1856 |
| `slang_portable` | 2200x1748 | 2200x2720 |
| `stock_materials` | 2300x1800 | 2300x2000 |
| `substance_swatches` | 1856x1324 | 1880x640 |
| `svg_silhouette` | 2200x1368 | 2200x1720 |
| `threaded_story` | 2360x1400 | 2360x1520 |
| `ticker_lanes` | 2200x800 | 2200x1440 |
| `tile map` | 1888x662 | 2240x1420 |
| `usd_roundtrip` | 2400x1040 | 2400x1480 |
| `volatility_cost` | 2400x1781 | 2400x2470 |
| `warichu_placeholder` | 2200x848 | 2200x1650 |
| `web_script` | 2400x758 | 2400x1164 |
| `yarn_marquee` | 2400x1320 | 2400x1640 |

## Catalog plates of the same extent still have unattributed pixel changes

These 77 sketches render successfully at their standing extent but do
not match their baseline pixels, and their whole difference has not been
attributed to a permitted material, timing or label change. Their
baselines remain unchanged:

`annotated_margin`, `artnet_lights`, `astral_tome`, `bg3_dice_roll`, `black_watch`,
`bousen`, `bristle_bloom`, `brush_custom`, `brush_engine_atlas`, `brush_live_tutorial`,
`bullets_dropcap`, `card_flip`, `cde_motif`, `chaucer_astrolabe`, `chrome_type`,
`data_scales`, `ds2_bench`, `dunhuang_star_chart`, `eva_magi_defense`,
`eva_magi_deliberation`, `feed_events`, `feed_sky`, `floating_panels`, `flourish`,
`grpc_watch`, `guest_body`, `guest_picture`, `hello`, `hit_slots`, `horizontal_flow`,
`import_native`, `ksp_mapview`, `lain_navi`, `loot grid`, `material_lab`,
`mawarikomi`, `mesh_normal_bridge`, `midi_pads`, `minard_1869`,
`observable_circle_packing`, `observable_l_system`, `observable_l_system_tree`,
`observable_reynolds_steering`, `optical_kerning`, `osc_desk`,
`passive tree`, `penrose_paving`, `phone_sky`,
`python_live_signals`, `python_type_atelier`, `reflection_lab`, `rota_convocationis`,
`ruby_kenten`, `schema_scene`, `serial_sensor`, `set_stagger`, `sigillum_aemeth`,
`slitscan_2001`, `spacejam_1996`, `spacing_passes`, `sticker_collection`,
`stroke_atlas`, `surface_components`, `thaumonomicon`, `thunder_fulu`,
`twoadvanced_equipment`, `twoadvanced_v3`, `twoadvanced_v4`, `vagrant_story_target`,
`video_compose`, `video_compositing`, `web_panel`, `webrtc_sky`, `winamp_base`,
`xcom_battlescape`, `zellige`.

The discrepancies include presentation changes already present before
the API translation: `artnet_lights` wraps its output in a titled page
instead of the baseline's full-canvas composition, `zellige` already
declares a different subtitle and heading layout, and `slitscan_2001`
has a different subtitle.
`surface_components` changes type sizes and panel spacing;
`stroke_atlas` changes much more than the permitted crosshatch edge.
A small hash difference alone is also insufficient to establish intent:
`bristle_bloom`, `brush_live_tutorial`, `horizontal_flow` and
`spacejam_1996` remain unadopted rather than accepting an arbitrary
pixel tolerance.

All eleven replay sketches need particular care: `artnet_lights`,
`channel_bind`, `feed_events`, `feed_vitals`, `feed_sky`, `grpc_watch`,
`midi_pads`, `osc_desk`, `phone_sky`, `serial_sensor` and `webrtc_sky`.
Their permitted readout changes do not authorize the changed page
presentation. A test should replay the same recorded input to the same
capture time and assert the output picture's geometry, colours and
values, with only the specified readout rows allowed to differ.

The intended check is byte identity at the declared capture frame once
the owner resolves each unexplained difference. Both pictures remain in
the baseline and CPU plate directories, so an authorized change can be
reviewed and adopted individually; none of these hashes was rebased.

## Thirty renderable sketches have no standing machine-local plate

These sketches render successfully but have no entry in
`build/plate_baseline_Release.sha256` and no baseline picture to judge:

`alpha_ground`, `attribute_ring`, `connect_by_lane`, `document_styles`,
`draw_with_scope`, `ink_units`, `paint_boxes`, `pins_and_hulls`, `python_alpha_ground`,
`python_botanical_study`, `python_compose_stamps`, `python_dashboard`,
`python_data_garden`, `python_document`, `python_hello`, `python_hello_compose`,
`python_kit_specimen`, `python_liquid_glass`, `python_liquid_layers`,
`python_memo_station`, `python_mesh_observatory`, `python_motion_signals`,
`python_observable_flowfield`, `python_observable_l_system`,
`python_observable_reaction_diffusion`, `python_observable_reynolds`, `python_orbits`,
`python_world_study`, `spell_circle`, `text_wrap`.

The plate check is intended to detect changes to a known picture; a
successful first render cannot prove that. Each needs an owner-reviewed
baseline before a later sweep can assert byte identity. The CPU plates
remain available for that review and were not silently adopted by this
migration.

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
before writing the image. `src/sketch/canvas/Session.cpp` deliberately
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
file-frame lane deliberately captures 2D canvases on Raster, including
when `--gpu` enables the device for mesh or set content. A procedural
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

## Renderer gap: 2D surface transmission channels do not implement transport

`SurfaceOptions` carries transmission, refractive index, thickness and
absorption, but the 2D lowering in `src/common/material/skia/Lit.cpp` and
`skia/shaders/LitSurface.sksl` does not apply those channels. Painting
pairs of swatches through the planar executor that differ in one surface
channel each shows it: changing transmission, index, thickness or
absorption leaves the pair identical, while clearcoat, roughness,
metallic, occlusion, emission and normal conventions change their
swatches. Nothing tells the author the request was dropped.

`Filter::glass` supplies bounded planar backdrop refraction through a
separate destination-filter operation. Surface coating and reflection remain
material fills. That operation does not implement the surface value's volume
transmission, absorption length or dispersion. World's device surface
lowering (`surface::lower`, `src/common/material/surface/Lower.cpp`)
passes transmission, index, thickness and absorption on to the device
program, but does not implement clearcoat; it reports a nonzero clearcoat
request once.

The planar executor lights a page; volume transport is not its job and
belongs to SigilWorld's surfaces. Wanted on the planar side: only a
once-only report that transmission, index, thickness and absorption are
not drawn on the page, in the shape of the report World's lowering makes
for clearcoat, rather than an unchanged swatch presented as a successful
response. Wanted on the World side: its device lowering keeps reporting
the clearcoat it does not implement, once, until it implements it. A
regression should paint a surface with each unsupported channel through
each executor, assert the report is made once and only for the channels
that executor does not draw, and assert supported channels make none.

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

## API gap: canvas sequence sampling advances during photography

The native protocol sequence steps by one over the requested rate between
stills. A canvas still also draws an extra frame and advances by one
sixtieth of a second, while a set still presents its held frame. The same
25-frame request at rate 24 therefore advances the stone and optical clocks
by 1.4167 seconds, but the metal set clock by one second. Requesting that
sequence through the protocol from a canvas study and a set study, then
seeking each directly to the sequence's final reported time, reproduces
it: the direct seeks match the actual final sequence time exactly, not the
requested one.

This is documented still behavior, but it makes the requested sequence rate
insufficient to state the scene's sampling interval across runtime kinds.
Wanted: photograph each requested scene time while retaining redraw at the
capture density; count redraws separately from clock advancement. A
regression should sample a clock-driven color or position at the same
requested times in a canvas and a set, assert those times and frame count,
and compare fresh direct seeks at several densities. Paused stills should
remain held and repeatable.

## Rendering gap: CPU body ordering hides a foreground instrument meter

The metal study's display is a quad in front of its faceplate, filled with
an unlit surface whose base-color slot is the live Compose meter texture.
The CPU facing, hero and exploded plates hide that meter; native GPU plates
show its luminous bars and text at the same moments. Base-color textures
and unlit shading are supported by the CPU tier, so their documented
shading limits do not explain the missing foreground content. World's
`scene/Execute.cpp` orders whole bodies by their transformed origins;
triangle sorting within one body cannot repair overlaps between bodies.
This is an ordering candidate, not a reduced proof of the cause. Reproduce with
`metal_instrument` at zero, 2.4 and nine seconds, comparing World's CPU
tier with its native GPU device at each time.

Wanted: preserve the foreground textured display's visibility against the
enclosure and its thin cover. Reduce the set to a large opaque faceplate,
a smaller unlit textured quad placed slightly forward, and an optional
transparent cover. A regression should retain the display pixels under
front, pitched and yawed cameras, and distinguish depth ordering from
texture readiness before changing the executor.

## Authoring ergonomics: outline relief is a decoration rather than a fill or ink

Compose's material fills and inks shade their existing coverage. The shared
`relief` brush can derive a normal field from the actual shape or placed
glyphs, but it is attached as a background or foreground decoration. Text
must select the glyph boundary and make its ordinary ink transparent. The
stone, ceramic and foil studies repeat those three operations. A surfaced
material supplied directly to `fill` or `ink` does not acquire contour relief.

Wanted: one outline-aware paint operation over the existing fill and ink
seams, preserving the material's layers, effects and normal detail. A
possible spelling is `ink(relief(material, options))`; this is an API request,
not an available overload. The contour-aware paint must remain in the
inherited ink lane until the receiving leaf has its outline and final
paint-box mapping. Its colour mapping and logical-pixel contour normals
have different coordinate requirements.

A helper that writes transparent ink, selects glyph boundaries and adds a
foreground would change inherited child ink, redirect unrelated decorations
and retain relief after a later ink assignment. Its foreground also uses
resting glyph outlines rather than the ordinary text-effects draw poses.

Regressions should compare shape fills and shaped text, including counters,
signed depth, shared brushes, density and live channels, without drawing
ordinary ink a second time. Cover inherited parent ink, explicit and cleared
child ink, repeated assignment, current-ink marks, rich spans, independent
decorations, paint-box mapping and animated or path-placed glyphs.

## Renderer gap: composed material channels are not bound by World

`surface::blendNormals` produces a sampled Material with two children, and
the surface lowering accepts it as a normal channel. World's device binder
retrieves channel slots through `surface::map`, which returns direct Texture
leaves. It does not evaluate a non-image material channel, so the composed
normal field is absent. Compose's Skia executor evaluates the composition.

Wanted: either evaluate general material channel programs in the device
executor or reject them with an explicit capability diagnostic. A regression
should compare two direct normal textures with their composed normal field
under the same lighting, preserving UV placement, normal convention and
live inputs. Until then, a World study must supply a direct texture slot.

This is the door a lit Compose interface in space goes through: the marks
a composition bakes into a height texture reach a World surface as its
normal channel through `surface::normalFromHeight`, which is such a
program. With it unbound, a Compose texture can dress a World surface's
base colour and nothing else, and the relief the planar executor shows
flat on the page is absent on the same surface turned in a set.

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
signed relief, material effects and live inherited lighting.

## Authoring gap: a coating cannot state its own finish in Compose

`SurfaceOptions` exposes a clearcoat weight. The Skia surface shader uses
one fixed coat roughness and the substrate's normal for the coating. The
`layered_material_type` study can make porcelain, applied metal and its
coating visible together, but cannot independently specify a smooth glaze
above a rough impressed substrate. Material colour layers share one
surface response; they are not independently shaded films.

Wanted: an explicit coating finish with independent roughness and normal
where the executor supports it. A regression should keep the substrate
unchanged while only the coating highlight width or normal changes, verify
zero coating against the ordinary surface, and preserve live map inputs.

## Rendering gap: Graphite promotion changes fine ribbon coverage

The fixed comparison coupons in `pigment_brushes` use full-material ribbons
under static lighting. At two seconds and native density, Graphite captures
with automatic promotion disabled and eager promotion enabled agree on the
main material shading but differ at narrow ribbon boundaries and isolated
coverage seams in the coupons. The metal and typography studies have only
small channel-rounding differences under the same comparison. These captures
use the same Graphite executor for the live paint and the device bake.

Wanted: device promotion preserves the live ribbon's coverage, including
overlapping sections of a swept band. A reduced regression should compare a
wide curved band with a thin band under the same static material and light,
with promotion off and eager. Cover clipped coupons, negative origins,
partly transparent layers, joins and a picture parent. Compare edge coverage
separately from fully covered material shading; treating a large sparse edge
difference as a small whole-frame average conceals the defect.

## Performance gap: moving-light material studies are expensive on raster

A picture records drawing commands, not their shaded pixels. A moving
light rightly prevents holding the final shaded surface, but the
stationary colour, height and normal channels beneath it re-run their
material programs on every paint, so a lit material study under a moving
light pays for all of its channel programs each frame.

`--bench` on `pigment_brushes`, `metal_linework`, `layered_material_type`,
`reflection_lobe` and the `painted_fields` steel workbench with a front
point light shows it on raster: the frame time is spent in paint while
description, reconciliation and layout stay small, and each misses the
frame budget that `--window-bench` meets on the native window. The two
lanes do not measure the same work: the window's figures are CPU activity
and frame cadence, not GPU execution time. In `painted_fields` and
`reflection_lobe` the outer custom paint callback includes a nested
Compose description, so paint time does not isolate one shader; and the
directional stone baseline and point-lit steel version of `painted_fields`
change both material and lighting, so their difference does not isolate
positioned lighting. In `reflection_lobe`, which retains its painted input
and moves its mounted point lights, a plain metal crop below the letters
and above the bristles changes as the sources move, independently of
their visible markers.

The `LitRaster` benchmark compares flat paint, positioned sources,
procedural normals, procedural and prepared-image environments,
painted-height normals, composed normals and coating at equal pixel
extents; its mapped-normal arms use the procedural environment and must
be compared with that arm. Point lights already skip the axis and cone
work only a spot light needs, and a prepared environment panorama
replaces the procedural one; neither moves the dominant raster cost, and
the clearcoat arm is the most expensive.

Wanted: retain stationary channel work as textures at the paint density,
without freezing the light, UV placement or live inputs. Establish the
dominant programs with a CPU sample before changing the executor, and
compare retained channel textures with the procedural inputs at equal
density. A regression should prove unchanged pixels under moving light
and fresh channels after a source change; timing belongs in the
benchmark.

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
across all thirty native-density GPU captures, including source, light,
coating and promotion changes. The matching raster capture differs in its
height-preview interior at 4,739 pixels, with a maximum channel difference
of 42. The environment-only scalar coupon interiors agree within one
channel level, while a bristle highlight elsewhere differs by 224 levels.
The full-frame backend difference cannot be attributed solely to text or
edge antialiasing; the brush input differs before it drives the normal map.

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

## Performance gap: identical environments prepare separately for each receiver

Every independently constructed LitSurface owns a lowered environment Paint
and an atlas preparation cache. Identical environments can therefore repeat
lowering and preparation across receivers with equivalent resolved inputs.
This duplication follows from the ownership in the code; native per-frame
duplication and its cost have not been quantified.

Wanted: the nearest scene or lighting context can retain both the lowered
panorama and its preparation. Sharing only the atlas cache is insufficient
because separate lowerings produce separate shader identities. Reuse must
account for the resolved source, extent and recorder, preserving time,
bindings, receiver geometry and root placement. An authored-material-only
global cache cannot establish those equivalences.

A regression should prove one preparation across equivalent receivers,
retention through rotation and intensity changes, and refresh or separation
for different source frames, extents, backends, receiver contexts, nested
scenes and lighting overrides. Measure actual preparation counts and cost
before selecting a public host seam.

## Rendering gap: finite environment samples alias narrow source support

The spherical environment kernel uses a fixed deterministic sample sequence:
64 samples for sharp lobes and 512 for broad or cosine-weighted lobes.
Constant skies are preserved, but narrow off-axis support can fall between
samples. More samples do not monotonically reduce that error.

An independent spherical integral of a radiance-six north cap with an
11.25-degree radius gives 0.161475896 for the cosine response at a normal
45 degrees from north. The selected 512-sample cosine sequence gives
0.140625, about 12.9 percent below that value. A sharp lobe at roughness
0.125 and a normal 22.5 degrees from north has a reference response
0.016016172 but receives zero from the selected sequence. These are
numerical kernel witnesses, not an accuracy bound for native filtered images.

Wanted: controlled sampling error for small emitting support without
per-material gain or a panorama-space blur. Compare representative caps,
strips and off-axis tails against independent spherical quadrature, retaining
constant-sky identity, longitude continuity, HDR and deterministic results.
Then measure source refresh and native normal-ramp continuity; a larger
fixed loop alone does not establish either accuracy or usable refresh cost.

A bounded 225-case CPU study compared 1,800 estimates using a float panorama
and material/image importance sampling. A 512-wide panorama with 64 samples
from each proposal improves the two cap witnesses to 0.16043424 and
0.01537085, but loses a half-degree strip entirely. A 1024-wide panorama
oversizes that strip's response by 52.7 percent. Supersampling source cells
helps some cases while retaining substantial tiny-feature errors. Constants,
black and the tested longitude seams are preserved. The estimates are
numerical research, without half-precision, device, atlas-interpolation or
moving-source validation. Source-bake convergence and fallback are unresolved;
additional samples or resolution do not improve monotonically. Keep the
production kernel until that contract and native refresh cost are established.

## Rendering gap: HDR highlight coverage differs between raster and Graphite

The native `metal_linework` blue state uses one frontal source at intensity
0.35 with ambient zero and no environment. Raster and Graphite captures
at exactly 26.6666666667 seconds use the same host, description and density.
Most channel differences are small, but 258 pixels differ by more than
64 channel levels. At glyph pixel (1133, 56), Graphite is RGBA
(15, 9, 255, 255) and raster is (15, 9, 90, 255); the maximum difference
across the plate is 240. Identical red and green backdrop channels at this
pixel make changed silhouette coverage an insufficient explanation.
Capturing `metal_linework` at that time on raster and on Graphite and
comparing the two reproduces it; with zero direct RGB the maximum backend
difference is eight instead.

The lit shader returns unclamped radiance with premultiplied alpha. A sharp
highlight can exceed one even when the source RGB is one. In the 351 pixels
with channel differences above 32, GPU blue is always brighter and the black
control differs by at most one. Plain material ink contains outliers, so
outline relief is not required. Subtracting the black control from selected
glyph edges gives a GPU/raster blue ratio near 4.8, consistent with the
roughness-0.25 surface's finite highlight peak rather than half overflow.

Skia's raster-pipeline blitter (`SkRasterPipelineBlitter`) clamps shader
values for a normalized target before it applies path or glyph coverage,
so raster clips an HDR highlight before coverage where Graphite need not;
that different clipping order is the likely cause, not yet confirmed
against the installed Skia. The float16 light-color regression retains HDR range and
agrees between executors, so clamping the common lighting shader would
discard intended output rather than resolve the coverage contract.

Wanted: a declared display-range and HDR compositing boundary. A regression
should draw the same partially covered glyph, curved stroke and filled edge
with bounded and HDR shader values into N32 and float16 targets, over an
opaque colored ground and transparency. Compare coverage separately from
radiance, preserve full-coverage HDR values, and establish where display
clipping occurs on both executors. Keep comparing the two executors at a
matching clock; do not rebase away the sparse highlight differences as ordinary antialiasing.

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


## Contract gap: a lamp on the page changes which normal the environment reads

Under `material::LightingFrame::Surface`, a directional source reads the
node's own normal. `src/common/material/skia/Lit.cpp` selects
`LitPositioned.sksl` as soon as any source is a point or a spot, and
`LitSurface.sksl` then replaces the shaded normal with the page normal
for everything after it: the ambient tint, the environment reflection,
the view cosine and the coat. So a turned or scaled node keeps its
reflections where they were until a lamp is added anywhere in the rig,
and then they turn. `Lighting.h`, `Lit.h` and the Material README state
this as the rule.

Which normal a surface's environment reads should follow from the frame
it states, never from which kinds of source happen to be present. In the
Surface frame the ambient, environment and coat terms would read the
node's own normal, and only a positioned source's direct term the page
normal. A test should hold a rotated lit node's environment reflection
unchanged when a point light of zero intensity is added, and unchanged
again when it is removed. Every plate with a positioned light over a
turned node moves.

## Renderer gap: the page and the set shade direct light differently

`src/common/material/skia/shaders/LitSurface.sksl` and
`src/common/world/diligent/shaders/Surface.slang` execute one surface
value. The page's direct term is the shared one in
`src/common/material/core/shaders/Shading.slang`: a highlight whose
exponent comes from the roughness, normalised, multiplied by the facing
cosine and coloured by the surface's reflectance. The set's is written in
its own shader: an exponent taken from the scene or a per-pixel gloss,
no normalisation, gated by the facing cosine but not multiplied by it,
tinted white for a dielectric, with no ambient share on a metal and a rim
term the page has none of. A material moved from a page to a set
therefore changes its highlight width, strength and colour.

The set should call the shared direct term, reading roughness where it
reads gloss. A test should shade one surface under one directional light
through both executors, viewer straight on, and compare the centre pixel
within a tolerance; a second should vary roughness and see the highlight
widen in both. Every lit World plate moves, and the rebase names this
cause.

## Contract gap: a material stroke keeps its prepared surface in the description

`detail::MaterialStroke` in
`src/common/compose/core/paint/MaterialEffects.h` holds a
`mutable std::shared_ptr<MaterialStrokeCache>`, and its const `paint()`
in `MaterialEffects.cpp` replaces and mutates it. A `Decoration` is a
value an author keeps and hands to several trees, so two composers
painting one description — a `TextureScene` beside the window's
composer — write the same cache, and neither owns its lifetime.

A description is immutable once described; retained paint state belongs
to the instance that paints. The prepared `LitSurface` and its lighting
pass should stand beside the instance's other lit inputs, keyed by the
mark's place among the node's strokes. A test should paint one
description through two composers under different lighting and find each
shaded under its own; a second should describe the stroke once, paint it,
and find the description's bytes unchanged.

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

## Study queue: the material studies are written around the libraries

The sixteen `Study · Materials` sketches under `apps/grimoire/sketches/`
were written while the lighting vocabulary was growing and say with raw
mechanism what the libraries now say directly. What a sketch pass should
change, by kind:

- **Raw Skia.** `ceramic_glaze`, `metal_instrument` and `struck_metal`
  build their maps in `SkBitmap` pixel loops; `luminous_layers` makes and
  reads back its own surfaces; `light_table` tracks the backend's
  recorder; `struck_metal` builds outlines with `SkPathBuilder` and takes
  glyph outlines from `SkFont`. A map is a material painted into
  `ctx.textureScene()`; an outline is `geometry::path::Outline`.
- **Hand-written shading inputs.** Central-difference normals in
  `ceramic_glaze`, `embossed_foil`, `pigment_brushes` and `stone_relief`
  are `material::surface::normalFromHeight`; rounded shoulders in
  `carved_marks`, `ceramic_glaze`, `stone_relief`, `optical_liquid` and
  `wet_glass_console` are `compose::relief`, and the hand-written ones
  ignore corner radii and circles; sine-hash noise in six studies is
  `material::noise` or `material::grain`; `optical_liquid`'s refraction
  is `material::Filter::glass`; `metal_linework`'s stroke region is
  `geometry::path::operations::offset`.
- **Trees rebuilt every frame.** `carved_marks`, `painted_fields`,
  `reflection_lobe`, `luminous_layers` and `light_table` describe their
  whole page inside a pen callback under `Cache::None`, and
  `metal_instrument` rebuilds its materials in `describe`. The tree is
  the composer's, described when state changes. `painted_fields` does
  not reach its capture moment on the CPU plate tier inside the sweep's
  per-scene ceiling.
- **Hand-rolled controls.** `carved_marks`, `light_table`,
  `luminous_layers` and `reflection_lobe` hit-test hard-coded rectangles;
  `sketch::kit::Controls` and `Meter` answer the pointer.
- **Copies.** `label` stands in fifteen studies, `rule` in fourteen, a
  clamp with a fallback in seven, a number formatter in nine
  (`compose::kit::formatted`), a hex colour in three
  (`material::hexColor`), a button in five, a studio environment bake in
  nine, a texture stretched over an extent in five. `carved_marks` and
  `layered_material_type` share most of their page.
- **Outside the page's lighting.** `painted_fields` and `reflection_lobe`
  stand softboxes, strips and rings in as environment panoramas and
  brushed anisotropy as groove normals; `metal_instrument` lights a
  Compose plane turned under `perspective`. Emitters with an extent,
  anisotropy and a turned plane are a set's.
- **Checks that throw while painting.** `carved_marks`,
  `layered_material_type` and `luminous_layers` verify revisions each
  frame and throw from `update` or `paint`; `light_table` and
  `luminous_layers` carry read-back functions nothing calls. Those are
  tests.
- **Wrong pictures.** `struck_metal`'s key light travels upward, so the
  piece is lit from below. `stone_relief`'s dial ticks and
  `wet_glass_console`'s turned film are lit in their own frame, so the
  light turns with them. `carved_marks` differentiates a unit-square
  height at a step wider than its ribs for glyph and word units.
  `embossed_foil` and `reflection_lobe` do not close their loops.
  `painted_fields` and `carved_marks` stretch a field across boxes of
  another aspect. `reflection_lobe`'s vertical cylinder is concave.

What the libraries lack and the studies spell by hand, each a growth of
the library named:

- `material::Light::position` is a plain vector, so a moving lamp
  re-describes the page (SigilMaterial).
- `draw::Graphics::image()` and `extent()`, `brush::Ribbon::band` and
  `compose::TextureScene::make` and `size()` take and return Skia types
  (SigilDraw, SigilCompose).
- A contour bevel as a normal map exists only behind
  `sigilmaterial/skia/Bevel.h` (SigilMaterial).
- `material::EnvironmentMap::baked` says named bakes live in a kit; no
  kit has one (SigilMaterial).
- A texture has no placement that stretches it over an extent
  (SigilMaterial).
- Shaped text has no door to `geometry::path::Outline` (SigilWeave).
- A page has no display transform; `luminous_layers` writes its own tone
  curve (SigilCompose).

A pass is done when no study includes a Skia header, each is shorter, and
the copies above are one kit piece or a library call.

## Rendering gap: glyph batches draw a span's material unshaded

`GlyphRSXformBatches::addGlyph` in
`src/sigilweave/choreograph/Choreograph.cpp` builds every pass from
`style.foreground` and `PaintLayer::resolvedPaint`, and never consults
`PaintStyle::foregroundMaterial` or `PaintLayer::material`; the batches
have no material resolver. The paragraph draws, `ParagraphLayout::draw`
and `drawBatched`, shade those passes through `paint::setMaterialResolver`
over the glyph ink bounds. So a span whose material shades it at rest
draws in its configured foreground as soon as a text effect moves its
glyphs into the batches.

One material should shade a glyph the same way at rest and in motion. A
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
