# Findings

## Explicit texture bakes change deterministic reference samples

The rotating text bands in `rota_convocationis` and the retained ink in
`thunder_fulu` request local texture bakes. Their content, geometry and
materials stay fixed, but their cached pixels are resampled onto the
capture's grid. `src/common/compose/core/TextureBake.cpp` uses the declared
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

Sketchbook stands its device programs up before the canvas draws: every
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

`src/sketch/book/PipelineWarm.cpp` works around it by keeping each key's
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

When the pane's zoom settles, `SketchbookRenderer` redraws the frame at
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

The study rewrites of 2026-09-24 (minard_1869,
chaucer_astrolabe, dunhuang_star_chart, lain_navi, ksp_mapview)
state paper grain, stone, brush ribbons and lettering as material
paints and shapes in the tree with no `Cache::Texture` over them, so a
plate whose picture does not move between frames is re-rasterised on
each one. The headless sweep measured minard_1869 at 873 ms of paint per
frame, against roughly 60 ms for the sketches they replaced; the 60 FPS gate fails all of them, and a sweep
that renders a few hundred frames per sketch for its statistics takes
minutes per study.

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

## A mark cannot state a percentage-plus-pixel inset

`Text::textAttach` reads the selected unit's rect as the containing box,
but `Dimension` refuses a sum of percentages and other lengths before
that rect is available. Consequently `top(pct(100) + Dimension(12))`
becomes auto even though the mark's rect gives the percentage a definite
basis. Pure font-relative sums, far-edge positioning and margins resolve;
`top(pct(100)).marginTop(12)` expresses the intended drop.

A percentage sum should remain a length until its consumer knows the
containing extent. A test should attach a 1x8 mark to one letter with
`top(pct(100) + Dimension(12))` and assert that its top is the selected
rect's bottom plus 12, including after the type is resized. Flex layout
must either resolve the same sum against a definite containing block or
report its own unsupported case without erasing the authored expression.

## `textFx::tint` takes its two colours as values, so a wipe cannot follow the sheet's ink

`textFx::tint({.from, .to})` (`sigilcompose/typography/TextFx.h`) computes
its per-channel multiplier as each stop over the rest colour, from
`material::Color` values in its `motion::Tween` when the effect is built. It
must be used on a line whose ink IS the rest (`.to`): the multiplier only
ever takes the drawn colour down toward `.from`. A sheet
that states its palette as custom properties inks the line with
`ink(var("sung"))`, but the effect cannot read that property, so the sketch
states the sung colour twice — once on `:root` for the ink, once as the
constant handed to the tint — and a class or a later sheet that restates
`--sung` recolours the letters while the tint still divides by the old
value, which lands the resting colour somewhere neither end names, with no
diagnostic.

It evidently means one statement of the colour: either `tint` takes a
`VarRef` (or `Fill`) for its ends and resolves it against the leaf's
custom properties when the effect is applied, or it takes only the `from`
end and reads the leaf's resolved ink as `to`, which is what it already
requires the ink to be.

A test should ink a leaf with `ink(var("sung"))` under a rule that sets
`--sung` to one colour, apply a tint whose destination is that property
(or the leaf's ink), and assert a glyph at local 0 draws `from` and one at
local 1 draws the property's colour; then restate `--sung` on a class the
leaf carries and assert local 0 still draws `from`.

Wanted by `karaoke_wipe`, whose wipe is `textFx::tint({.from = kPale, .to = kSung})` over
a lyric inked `var("sung")`.

`elastic_type` meets the same constraint from `textFx::tween`: its blush
keyframes write each stop's `GlyphModifier::colorMultiplier` from the two
series colours the sheet also states as `--x` and `--y`, so the palette is
stated twice, once as custom properties for the traces and once as
constants for the glyphs. Whatever fixes `tint` (a `VarRef` end resolved
against the leaf's custom properties at apply time) should reach a
keyframe entry's colour terms too. Add `elastic_type` to that entry's
wanted-by list.

Progress: a tint naming no rest divides by the glyph's ink read at apply time (`GlyphInfo::ink`), test in ComposeTestTextMarks.cpp. Left: run it; karaoke_wipe; a `textFx::tween` keyframe's colour terms still cannot name a property (elastic_type's blush) — needs a design decision.

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

## A rule cannot state a stroke, so a class cannot carry its element's keyline

`compose::Rule` takes `BoxVerbs`, `PaintVerbs`, `ShapeVerbs` and the rest of
the style families, but not the decoration verbs
(`sigilcompose/core/verbs/Decoration.h`), so `rule(".frame").stroke(...)`
does not compile: `stroke()` exists only on an element. A frame, a keyline,
a ring or a card's border is therefore restated on every node that wears it,
even where the class already states that node's size, radius and fill —
`elastic_type` gives the plot frame and the playhead ring their strokes
inline beside a class that says everything else about them.

It evidently means CSS's `border`/`outline`: part of the class's whole look,
cascading like a fill, with the colour free to be a custom property or the
ink in force. A test should state
`rule(".frame").stroke(stroke(1, Fill::var("line")))` in a sheet, apply it
over a box carrying the class, and assert that the box paints the same
stroke as `box().stroke(stroke(1, Fill::var("line")))`; and that a node's
own `stroke()` appends to, or overrides, the rule's by one stated order.
Wanted by `elastic_type`; every sketch that frames cells or cards with a
class (`black_watch` states its keylines inline on each node).

Also wanted by `nightingale_coxcomb`: the key stone's outline round every wedge, the twelve hairline radials, each month's rim flash and the index needle state their stroke widths inline beside the `.key`, `.spoke`, `.flash` and `.needle` classes that already carry their ink.

Also wanted by `cosmati`: every field and every roundel restates its marble fillet inline where one `.fillet` class would carry it.

Progress: `Rule` states marks and the matched rules' marks stand under the node's own (core/RuleMarks.cpp), with tests in brush/test/ComposeTestRuleMarks.cpp; cosmati, black_watch and fallout2 moved onto classes. Left: run compose_test, check those plates with --frame, then elastic_type and nightingale_coxcomb.

## `textFx::scramble` takes its charset as UTF-32 while every text door takes UTF-8

`compose::textFx::scramble(std::u32string charset, int steps)`
(`typography/TextFx.h`) is the one text-facing value in Compose that is
spelled in UTF-32: `text()`, `Text::span`, `document::*` and
`sketch::kit::Document::phrase` all take `compose::Utf8`. A charset that is
words — read from a sketch's `data/` file, or shared with the text it
churns — reaches the effect only through a conversion at the call site
(`weave::unicode::toUtf16`, then `weave::unicode::decodeAt` in a loop).
`matrix_rain` reads the charset its title resolves through from
`data/rain.json` and converts it to UTF-32 under a `workaround:` line only
to hand it to `scramble`.

The effect evidently means to take the characters a text is written in,
in the spelling every other text verb takes. `scramble` should accept a
`compose::Utf8` charset (decoding once, where it already builds its
per-codepoint table); the UTF-32 overload may stay beside it.

A test should build `scramble(u8"ｱｲｳ")` and `scramble(U"ｱｲｳ")` and assert
the two effects substitute identically on one seeded glyph run, and that a
charset holding a four-byte character (outside the BMP) decodes to one
substitution candidate, not four.

Wanted by: `matrix_rain` (removes its conversion). `shipping_forecast`
(its barometer charset) and `daemon_console` spell their charsets as `U""`
literals today and would spell them like the text they churn, or read them
from their words files, once the door is UTF-8.

## A glyph-outline decoration is cut from the glyphs at rest, so a text shadow cannot follow a track's deformation

`decorationOutline(Boundary::Glyphs)` hands a text leaf's decorations the
outline `TextLayout::glyphOutline()` builds from the placement
(`compose/core/PaintContent.cpp`, cached against `measuredRev`). A
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
(`compose/core/PaintContent.cpp` takes `glyphOutline()` once per
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

## An echo and a shadow take their colour as a value, so neither can follow the sheet's ink

An echo is `material::Filter::shadow(material::Color, {.offset})` in the
effects of a node's fill or ink (`sigilmaterial/filter/Filter.h`), and
`compose::shadow(material::Material ink, glm::vec2 offset, float blur)`
(`sigilcompose/brush/Decorations.h`, the `Shadow` value) holds a material.
Neither is a `Fill`, so neither can be written as `Fill::currentInk()` or
`Fill::var(name)`. A stroke's `PathFormat` takes a `Fill` whose default is
the ink in force, and `textStroke` resolves a `Fill::var` against the
tree; an echo under a title and a glow under a needle are the same kind of
mark and cannot.

So a sketch whose colours live in a sheet's custom properties still reads
them out of its own palette to hand to these two: `nightingale_coxcomb`
passes its palette's ink to the echo under each display line and its
palette's brass to the needle's glow, and a theme swapped by a different
token sheet would leave both behind.

They are evidently meant to paint like every other decoration: a `Fill`
defaulting to the ink in force, resolved at paint against the node's
cascade. A test should set `ink(var("accent"))` on a root with
`var("accent", red)`, give a child text an echo whose colour is
`Fill::currentInk()`, and assert the echo's pixels are red; and the same
for a `shadow(Fill::var("accent"), …)` under a box. This is the same
defect as `textFx::tint` taking its colours as values, in two more places.
Wanted by `nightingale_coxcomb`.

Progress: `Shadow::ink` is a `Fill` defaulting to the ink in force. Left: the echo — `material::Filter::shadow` is SigilMaterial's and cannot name the ink in force or a property; needs an owner decision (a compose echo verb taking a `Fill`, or SigilMaterial's shadow colour optional as CSS's currentcolor); then nightingale_coxcomb.

## A custom property holds a colour or a length, never a paint, so a palette of materials cannot be tokens

`compose::VarValue` is `std::variant<material::Color, Dimension>`
(`core/Cascade.h`), so `var(name, …)` on a node or a rule takes a colour or a
length and nothing else. A study whose palette is MATERIALS — `cosmati`'s nine
quarried stones, each a `cosmati::stone` recipe, and the brass its
letters are set in — cannot state them once as custom properties at the root
and read them with `fill(Fill::var("porphyry"))`; the stones are built in code
by a helper and handed to every piece's `fill()` as values, and a class
cannot name "the porphyry" without restating the recipe.

It evidently means CSS's custom properties, which hold any value a property
takes — a gradient or an image as readily as a colour: a `var` holding a
`material::Material` (a colour, a gradient, a shader or a layered look), resolved by `fill`,
`ink` and a stroke's paint exactly as a colour var is, so a quarry is a token
and a class is its whole look.

A test should state `rule(":root").var("stone", someMaterial)`,
fill a box with `Fill::var("stone")` under it, and assert the box paints what
`fill(someMaterial)` paints; and that reading the same property
as a length leaves the target standing and says so once, as a colour var read
as a length does today. Wanted by `cosmati`; `black_watch` holds its board and
yarn paints as members for the same reason.

Progress: `VarValue` holds a paint and `var(name, Material)` sets one (tests in brush/test/ComposeTestCascadePaints.cpp); black_watch's board and yarn and cosmati's fillet are tokens. Left: run the tests and plates; regenerate apps/python/sigil typing; cosmati's pieces keep per-piece cuts on purpose.

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

## A layered brush's passes take a colour, where every other mark takes a material

`StrokeLayer::color` (`sigilcompose/brush/Layered.h`, the pass a
`LayeredBrush` stacks) is a `material::Color`. Every other mark in the
brush tier takes its ink as a material — `Shadow`, `styles::BevelPair`,
`styles::Brackets` and `styles::TickRail` a `material::Material`, the
strokes, borders, lines and hatches a `Fill` — so a layered neon or a
cased road cannot be inked with a gradient or a pattern, and cannot
follow the ink in force, while a single stroke of the same road can.

It is evidently meant to take a material like its neighbours: a
`material::Material` ink (a colour converts) or a `Fill`, lowered where
the pass is painted. A test should give one layer a left-to-right
`material::linearGradient` from red to blue and assert the stroke is red
at its left end and blue at its right, and give another `Fill::color(c)`
and assert it paints exactly what the colour layer paints today.

Progress: `StrokeLayer::ink` is a `Fill` (tests in ComposeTestCascadePaints.cpp). Left: run compose_test.

## A selector rule accepts material effects but loses them on the matched element

`FontVerbs<Rule>::ink(Material)` calls `applyEffects` on the rule's
declaration, storing coverage decorations and the pixel filter there.
`Cascade.cpp` copies the rule's type and base ink into the matched node's
computed style, but those declarations are not the node's own
`backgrounds`, `foregrounds` or `fxData`. A halation material placed on a
`.near` rule therefore colours the letters but drops their glow; placing
that same ink on the text leaf draws it.

A material's effects evidently belong to its ink wherever the ink is
stated. A test should apply one shadow-and-blur material directly to a
text leaf and through a matching rule, then assert equal painted pixels
and bounds. It should also replace the rule and assert that the matched
node removes or updates the effect rather than retaining an old one.

Progress: a rule's ink and fill effects are kept on the rule and dress the matched element where that lane stands (tests in ComposeTestRuleMarks.cpp). Left: run compose_test.

## A glow on changing text is a filter over the leaf's whole box, so it re-runs every frame

A text leaf's glow, halation or blur — an ink material's effects stage
or `Element::filter` — is a layer filter: the leaf's glyphs are drawn into
a layer the size of the leaf and the filter runs over every pixel of it.
When any glyph changes (a `textFx` track fading, tinting or substituting
it) the layer and the filter run again. On a sheet of rain, two
1280x780 leaves under dilate + colour + blur cost about 55 ms each on the
raster gate per frame, where the same leaves without the glow cost about
1 ms. A glow that belongs to each glyph, and follows that glyph's
brightness, has no statement that costs per glyph.

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


## API request: converting a bound material to a layer filter snapshots its motion

The Nostromo monitor declares a tube program whose seconds uniform is
bound to the scene clock, then applies the program to a rendered subtree.
`material::Filter::of` reads the material's current bindings once while
constructing the filter. The binding does not carry into the resulting
filter. Calling `Filter::bind` on that converted value produces a warning
and ignores the binding: it holds an already built image filter and recipe
snapshot rather than the runtime program that `bind` needs. This behavior
is explicit in `sigilmaterial/filter/Filter.h` and implemented in
`src/common/material/skia/Effect.cpp`.

This is documented behavior, not a reproduced violation of the current
contract. It is nevertheless an authoring trap: the same material animates
as a fill, but its ordinary conversion to a layer filter freezes that
animation. The supported workaround is a filter constructed directly from
`material::skia::program`, with static uniforms and its own live binding;
the study's native captures verify that route. It requires the author to
compile and hold the runtime program separately, and shader source changes
require session reload. Wanted: either binding-preserving conversion or an
explicit snapshot spelling beside a live conversion, so the value's
animation does not disappear at an otherwise composable boundary.

A regression should bind a uniform before conversion, advance the same
clock through two known values and pin the chosen conversion's behavior.
For a binding-preserving form it should assert changing output and retained
static content beneath the filter. For an explicit snapshot form it should
assert that the snapshot stays fixed and that a subsequent binding is
either supported or rejected with a clear snapshot-specific diagnostic.
The direct runtime-program filter should remain bindable. The program
should not need recompilation when only the uniform changes.


## Bug: a world-space material restarts inside each hosted Pen leaf

The scan-visor and registered-print studies need one material field to span
separately positioned drawings. A single `Material` wrapping
`Paint.linearGradient((0, 0), (1, 0), stops).worldSpace()` remains continuous
across ordinary Compose fills, but starts the full gradient again inside
each `compose::pen` leaf. Changing a pen leaf's width changes the gradient's
scale even though the root canvas size is unchanged.

Two native Python probes reproduce this in the loaded Sketchbook host. On
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
`src/common/compose/core/Fills.cpp`. The documented world-space material
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

While another pass edits public library headers, an existing Sketchbook
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
`src/sketch/book/VideoLane.cpp` selects only the registry index and category;
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


## Bug: Python shader constructor advertises live uniforms it cannot accept

The generated `material.shader` declaration accepts `_t.UniformValue` for
each parameter. That alias includes live scalar values and describes them
as inputs re-read every frame. `src/common/python/material/Shader.cpp`
instead handles Color, bool, int and float, then iterates every other value.
Passing an Animatable as a shader parameter raises
`TypeError: '_sigil.motion.Animatable' object is not iterable` before the
study can render. The Borges lighting study must construct a static scalar
and call `Material.bind` afterward.

The native reproducer is
`build/media-study-redo/findings/shader_live_uniform_probe.py`; its separate
log preserves the failure. The declared constructor and runtime should
accept the same uniform inputs, or the constructor's type should explicitly
exclude live values. If accepted, a live initializer should establish the
same binding as `shader(source, {'seconds': 0.0}).bind('seconds', value)`.
A regression should initialize a shader through both forms, advance the
scalar, and assert matching pixels before and after the change without a
TypeError. Color strings and other advertised UniformValue alternatives
should be checked through the same conversion seam.


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

The monochord and miniature studies need continuous fine ink. A native
probe draws marker splines at widths 0.35, 0.6, 1, 2 and 4 with all scatter,
jitter and noise disabled. The thin nib splines become separated dots,
while adjacent Pen curves at the same nominal width remain continuous. Selecting the
existing fibre tip with one bristle gives continuous antialiased ink.
The probe and both raster controls are preserved under
`build/media-study-redo/shahnameh_folio/`.

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
study. This was an authoring error, not a renderer defect; the controls
and minimal native sampling probe are preserved under
`build/media-study-redo/findings/` and the radio evidence directory.

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
and produce identical native Raster images. The failing source, exception
and successful controls are preserved in
`build/media-study-redo/andromeda_optical_lab/probes/`.

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
is a capture contract issue, not a mapped-blur defect. Endpoint controls,
observed clocks and pixel comparisons are preserved in
`build/media-study-redo/andromeda_optical_lab/probes/`.

The native window screenshot in `src/sketch/book/main.cpp` waits for a live
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
window. `SketchbookRenderer::installCaptureBackend` gives its Host a
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
either public route. The observed refusals and advertised capabilities
are preserved in
`build/media-study-redo/andromeda_optical_lab/probes/window_protocol_result.json`.

Wanted: safe client access to the active window's clock and capture
session, or an explicit file-sequence backend selector using the existing
device capture seam. A regression should export only the selected file,
report the actual backend and scene times, keep Graphite work on its
owning thread, and verify repeated held captures and the requested frame
count. Detaching the client should restore the host's ordinary clock and
leave the live window usable.
