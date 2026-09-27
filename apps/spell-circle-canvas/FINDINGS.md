# Findings

## Group ruby is fixed in the library and `ruby_kenten` still asks for words

`weave::Unit::Selection` numbers one unit per extent a selector
addressed, `TextAnnotations.cpp` associates one reading with that whole
unit, and a base broken across columns partitions the reading across its
fragments instead of repeating it. Two extents that touch are still two
units.

`src/sketch/sketches/ruby_kenten.cpp` asks for a different unit. Its
GROUP specimen and its SPLIT specimen both pass `weave::Unit::Word`, and
word units divide `書物` and `国語辞典` into several units, so those two
panels show one reading repeated over each piece of the compound. The
file's own header comment and its GROUP panel caption name the word unit
with them.

The specimens should ask for the unit that means what they demonstrate.
Moving those two call sites to `weave::Unit::Selection`, with the comment
and the caption, and rebasing the plate — GROUP and SPLIT move, MONO,
JUKUGO and KENTEN must not — closes this entry. The library behaviour is
already asserted by `ComposeAnnotate` and `TextVertical`; what a test
cannot see is which unit a specimen names, so the plate is the check.

## `ksp_mapview` describes its map twice for a light that keeps its source

A bright pass is a colour program: `skia::Effect::brightPass` sets a
colour filter and leaves the image filter null, so a light built over it
carries no program over coordinates and the layer beneath
`Effect().emit(brightPass().then(<blur>))` is kept at the device's own
resolution, which
`SkiaEffect.AnEmittedLightLeavesTheSharpLayerAtDeviceResolution`
asserts. One description of a layer is enough to bloom it.

`src/sketch/sketches/ksp_mapview/ksp_mapview.cpp` builds its bloom out
of a second `mapLayer(ctx)`, filtered and composited back over the first
with `kPlus`, and the comment above it states that the duplicate
describe "is not avoidable" and that only a gathering light would need
one describe. Neither holds of the pass the scene uses: an emitted
bright pass keeps its own source, at one tap and a separable blur.

The scene should describe its map once and emit the bright pass and its
blur from that one layer, with the light's strength folded into the
light and the comment saying what the seam costs. The library half is
asserted already; what a test cannot see is how many times a specimen
describes a layer, so the plate is the check, and it moves where the
composite differs — a plate rebase names the cause.

## A study turns a screen's light off and builds it again outside the recipe

`eva_magi_interior/EvangelionUi.h` sets `uBloom` to zero on the tube
(`evangelion::crtTube`, `eva_magi_interior/Crt.h`)
and adds the tube's light itself — two Gaussians, two weighting matrices
and a table holding the sum at half — because the recipe used to gather
that light per pixel. It does not any more: the light is a slot an
executor fills with the layer blurred once, at a cost that follows its
own radius. The comment above the workaround states a tap count the
shader no longer spends, and because a recipe's slots are read from its
body text rather than from the strength of a uniform, the study still
pays for one blur of the whole layer every frame that nothing reads.

The three MAGI studies should take the recipe's own light, at the radius
and strength that match the look they have now, and drop the hand-built
one. `uBloomRadius` is a Gaussian sigma rather than the reach of a
fixed-tap gather, so the number is a fresh choice. A plate rebase names
the cause.

A test should describe each of the three studies and assert that no
scene under them builds a blur of its own — the only Gaussian beneath a
MAGI screen is the one the `bloom` slot's executor fills — and that the
screen each describes asks for that light, with `uBloom` above zero.
The rebased plates are what hold the radius and strength chosen.

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

## A varying rail's fold is struck at one width and met at another

`offsetJoins` in `src/common/geometry/path/Contour.cpp` reads a width
law once per corner, at the vertex, and strikes from that one number
both the place the two offset edges fold across each other and the
window of samples the corner answers for. The window reaches the fold,
which below a right angle is longer than the offset and grows without
bound as the turn approaches a reversal, so at a sharp corner the
samples at the window's ends stand far enough along the contour for the
law to read a different width there. The rail steps sideways by that
difference where the window ends, and the step closes a small loop
against the cut the join wrote. A four-arm zigzag under a width that
swells over its length shows it below about 30 degrees of interior
angle, on the side of travel the corner turns into, one crossing
enclosing a few pixels of rail at 30 and more of them as the corner
sharpens. A constant law on the same spine has nothing to read twice
and is clean to the sharpest corner a spine can carry without folding
its own arms onto the rail.

The offset edges a varying law cuts are not parallel to the edges they
came from — they slant by the law's own rate along the contour — and
the fold is where those slanted edges meet, not where the edges of one
width would. `Band.ACornerSharperThanARightAngleLeavesNoSpurEitherSide`
already sweeps its zigzag from 150 degrees of interior angle down, with
both rails to 30 and the constant rail on to 10; closing this entry
means carrying the varying rail down to 10 beside it, with every corner
blunter than a right angle and every constant rail unchanged to the
bit.

## A wire an operator attaches cannot be hit, and an addition cannot say whether it may be

`connect::wire` (`src/common/compose/kit/Connect.cpp`) marks every wire
`hitTestable(false)`. A wire is a `pathFigure` whose shape is one OPEN
path, and the hit test asks `SkPath::contains`, which answers for the
region the fill's implicit close encloses — the lens under an arc, the
triangle inside an elbow — so a wire left testable would answer for hits
on empty space beside it. The routed elements this replaced carried a
stroke-expanded hit path ±6 px around the route, and a graph could learn
which edge was under the pointer.

Two things are meant here. A figure whose shape is an open path should
be hit along the path within a stated tolerance, not inside its closure:
`pathFigure` should carry, or the hit test should derive, the
stroke-expanded region the retired route had. And an addition should be
able to state `hitTestable`, `cache` and the rest of a node's identity
verbs, since `Operator` states only `zIndex` and `styleClass` for what it
attaches. A test should attach a `connect::Between` wire over two boxes
and assert `hitTest` answers the wire's key on the route, the box's key
inside the box, and nothing beside the route; and a second test should
attach an element the operator marks untestable and assert the node
under it answers.

## `Scope` copies other than `snapshot()` route attachments to the wrong scope

`Scope::Node` holds a back-pointer to the scope it was read from
(`Scope::Node::m_scope` in `core/Operator.h`), and `Scope::snapshot()`
exists to null it on a copy. The implicitly generated copy constructor
and assignment do not: a `Scope` copied any other way hands out nodes
whose `attach` appends to the source scope's attachments, or dangles once
the source is gone. Nothing in the tree copies a scope that way today
(`DrawWith::add` copies a snapshot), so the defect is latent.

A scope is meant to be read where it is handed over and copied only as a
snapshot. The copy constructor and assignment should be deleted, or made
to unbind the nodes as `snapshot()` does. A test should copy a scope
through the remaining door and assert `attach` on the copy's nodes
attaches nothing to the original.

## The operator-order report names a keyless node as `""`

`Composer::Impl::rebuildKeyIndex` (`core/Reconcile.cpp`) reports an
arranging operator listed after an adding one as
`.operators() on "<key>"`, and a node with no key prints as `""`, which
tells the author nothing about which node to look at. The report is
meant to locate the list. It should name the node by its key when it has
one and otherwise by its place — the path of child indices from the root,
or the nearest keyed ancestor and the index under it. A test should
build a keyless container with the two operators reversed and assert the
report names a place rather than an empty string.

## The header says a later operator reads an addition and the code does not

`core/Operator.h`'s `Scope` comment says an addition is an ordinary
element "a later operator in the list reads", and `Additions.cpp`
collects the scope once before the operator loop with
`if (node.added()) continue;` in `collectScope`, so no operator ever
sees an addition. The README states the rule the code follows — an
addition is read by no operator — and no longer makes the first claim.
The header should say the same. A test should attach an element from
one adding operator and assert a second adding operator in the same
list does not find it by key.

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

The study rewrites of 2026-09-24 (minard_1869, penrose_paving,
chaucer_astrolabe, dunhuang_star_chart, lain_navi, fallout2_charsheet,
sigillum_aemeth, ds2_bench, thunder_fulu, rota_convocationis, ksp_mapview)
state paper grain, stone, wax, brush ribbons and lettering as material
paints and shapes in the tree with no `Cache::Texture` over them, so a
plate whose picture does not move between frames is re-rasterised on
each one. The headless sweep measured minard_1869 at 873 ms of paint per
frame and sigillum_aemeth at 360 ms, against roughly 60 ms for the
sketches they replaced; the 60 FPS gate fails all of them, and a sweep
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

## `text_paints::sparkle` alone ignores the run's extent, so it cannot follow the type on the default ink box

Five of the six text paints read the run's `extent` from
`TextPaintParameters` and work in `(point - origin) / extent`. `Sparkle.sksl`
reads `origin` alone: its cells are a fixed 22 units of the coordinates the
shader is sampled in, and its points a fraction of a cell. Under
`PaintBox::Element`, the default box of an ink, `Composer::Impl::textInkOf`
maps [0,1]² onto the passage's metric band and resolves the material
against a 1 × 1 box, so the whole passage stands inside one cell and at
most one point of light shows, whatever bounds are passed. Under
`PaintBox::Subtree` or `PaintBox::Canvas` the ink resolves in the node's own
pixels and sparkle twinkles as it does as a fill, but then its cells keep
one pixel size whatever the type's size, so a 104 px wordmark and 9 px body
type take the same grain, where the other five scale with the run.

The preset is evidently meant to behave as the other five text paints do
on the box that is stretched over the type: its cell size should be a
fraction of `extent`, so `sparkle(unit, t)` inked on the default box
twinkles across a word, and a larger word takes proportionally larger
cells.

A test should ink a word with `sparkle(SkRect::MakeWH(1, 1), t)` on
`PaintBox::Element` and assert that light lands on several separate
glyphs, that the same word at twice the size takes cells twice as large,
and that the material as a fill over a 220 × 70 box is unchanged. Wanted
by `text_paints`, whose SPARKLE OVER A BASE cell states its ink over
`PaintBox::Subtree` to sample the field in pixels; `stock_materials` shows
it only as a fill.

## Ten Data sketches still snapshot and print a connection's vitals by hand

`data::Connection::vitals()` answers one comparable
`data::Connection::Vitals`, and `sketch::kit::connectionReadout(connection,
{.door, .rows})` is the one readout over it; `feed_sky` reads both. Ten
registered sketches still declare their own `Reading` (or `Vitals`)
struct of those fields, fill it by hand, test it for a change and set
their own rows in an order of their own: `phone_sky`, `webrtc_sky`,
`osc_desk`, `feed_events`, `grpc_watch`, `midi_pads`, `serial_sensor`,
`artnet_lights` and `channel_bind` over a `data::Connection`, and
`feed_vitals` (`Vitals`, `vitalsOf`) over the `io::Feed` beneath one.

What the sketches should say: a door keeps the `Vitals` it last showed
and describes again when `vitals() != shown`, and its readout is
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

## A mark's insets are read as a positioned child's only in part: `bottom` and `right` size it, a sum or an `lh` reads as zero, and its margin is not read

`Text::textAttach` promises that a mark "is written in exactly the
placement longhand a `positioned()` child takes — px or pct `left`, `top`,
`right`, `bottom`, `width`, `height`, measured inside that rect and free to
sit outside it". The mark's box is resolved in `Composer::Impl`'s rect pass
(`src/common/compose/core/Rects.cpp`, the anchored branch): `left` and
`top` default to 0 when unstated, `right` and `bottom` are read only to
derive a missing width or height, and every inset goes through one
`resolve` that answers px, pt, pct, pw and ph and returns nothing for any
other unit. So a mark with a stated height and `bottom(-27)` stands at the
rect's top, not 27 px below its foot; `top(1_lh)` and
`top(Dimension(1_lh) + Dimension(12))` both stand at the rect's top; and
`top(pct(100) + Dimension(12))` is refused with the flex world's warning,
though a mark's rect is not laid out by Yoga and the sum has a value there.
Nor does that branch read a margin at all: it returns the anchor's corner
plus `left` and `top` and nothing else, so `top(pct(100)).marginTop(12)` —
the other way CSS says "12 px below the foot" — stands at the foot, and a
`marginTop` on a mark is accepted and silently inert.

It evidently means what a positioned child means: a `bottom` (or `right`)
beside a stated extent places the far edge, a `calc()` sum of pct and px
resolves against the rect, a font-relative length resolves against the
text the mark stands on, and a margin offsets the box from where its insets
put it — so a ruler hung "12 px below each letter's foot" is one
`top(pct(100) + Dimension(12))`, or `top(pct(100)).marginTop(12)`.

A test should attach a 1x8 mark to one letter with `top(pct(100) +
Dimension(12))` and assert its box's top is the letter rect's bottom plus
12; attach one with `height(8).bottom(Dimension(-20))` and assert its top
is the rect's bottom plus 12; and attach one with `top(1_lh)` and assert
its top is the rect's top plus the leaf's line height; and attach one with
`top(pct(100)).marginTop(12)` and assert its top is the rect's bottom plus
12.

Wanted by `karaoke_wipe`, which hangs its ruler's ticks and playhead at
the letter's foot with `top(pct(100))` and carries them the rest of the way
down with a constant `translateY`, under a `workaround:` line; and by
`axis_ripple`, whose level bars stand at each letter's foot with
`top(pct(100)).marginTop(kLevelDrop)` and so sit on the foot with no drop,
the `marginTop` inert. Two sketches want the drop, so this is the rect
pass's to fix rather than a kit component's to paper over.

## `textFx::tint` takes its two colours as values, so a wipe cannot follow the sheet's ink

`textFx::tint(from, to)` (`sigilcompose/kit/Kinetic.h`) computes its
per-channel multiplier as `from / to` from two `material::Color` values
when the effect is built. It must be used on a line whose ink IS `to`: the
multiplier only ever takes the drawn colour down toward `from`. A sheet
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

Wanted by `karaoke_wipe`, whose wipe is `textFx::tint(kPale, kSung)` over
a lyric inked `var("sung")`.

`elastic_type` meets the same constraint from `textFx::keys`: its blush
table writes each stop's `GlyphModifier::colorMultiplier` from the two
series colours the sheet also states as `--x` and `--y`, so the palette is
stated twice, once as custom properties for the traces and once as
constants for the glyphs. Whatever fixes `tint` (a `VarRef` end resolved
against the leaf's custom properties at apply time) should reach a
keyframe entry's colour terms too. Add `elastic_type` to that entry's
wanted-by list.

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

## A mark's margin is not read, so it places nothing

`Text::textAttach` says a mark is written as a positioned child is, against
the rect its selector resolved. `Composer::Impl::positionedRect`
(`core/Rects.cpp`) builds a mark's rect from its left, top, right, bottom,
width and height and never reads its margin, so a mark hung at
`top(pct(100))` with `marginTop(10)` stands at the letter's foot, not 10 px
below it. A positioned box's margin offsets it from its insets in CSS; the
entry on a mark's insets (bottom and right size a mark, a sum reads as
zero) is the same pass's other half.

A test should attach a 1x8 mark to one letter with `top(pct(100))` and
`marginTop(10)`, and assert its rect's top is the letter rect's bottom plus
10; and with `left(0)` and `marginLeft(4)`, that its left is the letter's
left plus 4.

Wanted by `axis_ripple`, whose meter stands off the letters by a constant
drop, and `karaoke_wipe`, whose ruler does the same; both carry the drop
as a translate today.

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

## Two skew angles are one shear pair, where CSS's `skewX(a) skewY(a)` composes two shears

A glyph's `skewXDeg` and `skewYDeg` (`TextFxPainting.cpp`, the per-glyph
matrix route) and a node's `skewX` and `skewY` both build ONE shear matrix
`[1 tan a; tan b 1]`, by design, so a glyph naming both leans without
scaling. CSS's transform list `skewX(a) skewY(a)` is the product of two
shears, `[1 + tan a·tan b, tan a; tan b, 1]`, which also widens the element
along x by the product of the tangents. Animate.css's `jello` is written
that way, so its extreme pose (a = b = −12.5°) is about five per cent wider
in a browser than the word `elastic_type` draws from the same table.

It evidently intends to read as CSS reads wherever a CSS name is borrowed:
either the node's skew lanes follow CSS's order (x shear then y shear), or a
transform list is expressible (an ordered `transform(...)` value on the node
and in a `GlyphModifier`) so a table transcribed from CSS lands the browser's
matrix. A test should set `skewX(-12.5)` and `skewY(-12.5)` on a 100 px box
and assert its painted bounds match the CSS matrix — width
`100·(1 + tan²12.5°) + 100·tan 12.5°` — or, if the pair stays the lane's
meaning, that a CSS-ordered list door produces that width. Wanted by
`elastic_type` (the jello row, per letter and on the whole word).

## `textFx::scramble` takes its charset as UTF-32 while every text door takes UTF-8

`compose::textFx::scramble(std::u32string charset, int steps)`
(`typography/TextFx.h`) is the one text-facing value in Compose that is
spelled in UTF-32: `text()`, `Text::span`, `document::*` and
`sketch::kit::Document::phrase` all take `compose::Utf8`. A charset that is
words — read from a sketch's `data/` file, or shared with the text it
churns — reaches the effect only through a conversion at the call site
(`weave::unicode::toUtf16`, then `weave::unicode::decodeAt` in a loop).
`matrix_rain` reads its two advance classes (half-width katakana, digits)
from `data/rain.json`, deals its field from them as UTF-8, and converts the
same strings to UTF-32 under a `workaround:` line only to hand them to
`scramble`.

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

## Two sketches still build the English hyphenator SigilWeave now holds

`weave::kit::englishHyphenator()` answers one held `PatternHyphenator`
over the English table. Two registered sketches outside the link that
grew it still build their own:

- `src/sketch/sketches/paragraph_sheet.cpp` (around line 87) and
  `src/sketch/sketches/manuscript/manuscript.cpp` (around line 90) each
  build a `PatternHyphenator` over `englishHyphenationPatterns()`, so each
  passage holds a table of its own where one expression, `hyphens({.patterns
  = weave::kit::englishHyphenator()})`, shares the kit's.

What the sketches should say: both take the held table. Taking it where
the hand-built one held the same patterns should leave `paragraph_sheet`
and `manuscript` byte-identical, and that is the check.

## A glyph-outline decoration is cut from the glyphs at rest, so a text shadow cannot follow a track's deformation

`decorationOutline(Boundary::Glyphs)` hands a text leaf's decorations the
outline `TextLayout::glyphOutline()` builds from the placement
(`compose/core/PaintContent.cpp`, cached against `measuredRev`). A
`textFx()` track's `GlyphModifier` — its scale, shear, offset and rotation —
is applied at paint time after that outline is taken, so a
`styles::dropShadow`, an `OuterGlow` or a bevel on a leaf whose letters a
keyframe table squashes and shears stays drawn around the letters where
they stood. `Boundary::Coverage` would follow them, but it rasterises and
traces the leaf's layer whenever it is invalidated, which a track moving
every frame does every frame. And `material::skia::Effect` has no offset
drop shadow (CSS's `filter: drop-shadow(x y blur colour)`): `glow` is the
zero-offset form, so a shadow cast AWAY from a deformed run is a second
copy of the run, translated and blurred with `Effect::blur` — on the bench
that copy is the most expensive node on the plate, at several ms per
display-size word at 2x.

It evidently means a shadow that belongs to what the letters DO: either
the glyph outline is rebuilt from the deviated glyphs when a leaf carries
displacing tracks (the tracks already know their reach, and a settled
cascade could cache the deviated outline as a static one is), or the
effect family carries `dropShadow(offset, sigma, colour)` so one filter on
the leaf casts the drawn letters, deformed, without a copy.

A test should lay `textFx::keys({{0, {}}, {1, {.scaleX = 2}}})` at
progress 1 on a one-glyph leaf with `decorationOutline(Boundary::Glyphs)`
and an `OuterGlow`, and assert the glow's painted bounds span the doubled
glyph's width rather than the rest glyph's; and, for the effect, that a
leaf filtered with a drop shadow of offset (0, 12) paints shadow pixels
12 px below the deformed glyph's lowest ink and none beside its rest
position.

Wanted by `elastic_type`, whose words stand in a pool of shadow attached
under their rest extent because neither route above follows the letters.

The same outline is also painted in ONE colour: a `textFx()` track's
`colorMultiplier`, `colorAdd` and `colorScreen` modulate the glyph passes
and never reach a decoration drawn over `Boundary::Glyphs`
(`compose/core/PaintContent.cpp` takes `glyphOutline()` once per
`measuredRev` and hands the union to the decoration). So a text shadow or
glow in the letters' own colour — CSS's `text-shadow` with no colour, which
is `currentColor` per glyph — cannot follow a wipe: under
`textFx::tint(pale, sung)` it glows the sung colour round letters not yet
sung. The fix that rebuilds the outline from the deviated glyphs should
carry each glyph's modulated colour too, or a glyph-outline decoration
should be able to say "the glyph's own colour".

A test should tint a two-glyph leaf with a track holding glyph 0 at local 1
and glyph 1 at local 0 (`tint(black, white)` on a white leaf), dress it with
`decorationOutline(Boundary::Glyphs)` and a zero-offset shadow in the ink
in force, and assert pixels of the shadow beside glyph 0 are lit and beside
glyph 1 are not.

Add `karaoke_wipe` to that entry's wanted-by list: its glow is a second,
blurred copy of the sung line sung from black, where one glyph-outline
glow on the line would do.

## `Effect::phosphorBloom` over a live layer costs the same whatever the layer's size, and far more than a blur

`Effect::phosphorBloom` (`sigilmaterial/skia/Effect.h`, built in
`skia/EffectBloom.cpp` by `makePhosphorBloom`) says its halo is gathered
over a REDUCED layer and resampled up, which reads as a bloom meant for
live content. Over a live node it costs the whole frame: `karaoke_wipe`
put it on the caption box of its screen (928x318) and on the sung text
leaf alone (810x63), and `--bench` reported the node at about 43 ms per
frame in both cases, p99 45.6 ms, where `Effect::blur(6)` on the same text
leaf costs 3.3 ms and `skia::bloom` (two separable Gaussians) over the
caption box 24 ms.

THE PROBABLE CAUSE, from the source. `makePhosphorBloom` builds two
`SkImageFilters::RuntimeShader` nodes — the gather, given `reach` as its
sample radius, and the composite over it, given none — and wraps neither
in `SkImageFilters::Crop`. In Skia (`SkRuntimeImageFilter.cpp`) a
runtime-shader filter's `onGetOutputLayerBounds` returns
`LayerSpace<SkIRect>::Unbounded()` and its `computeFastBounds` returns
`SkRectPriv::MakeLargeS32()`, whatever its inputs: a shader may paint
where its input is transparent, so Skia assumes it covers everything. The
sample radius only grows the INPUT a node asks for from the output it is
asked to make (`onGetInputLayerBounds` → `applyMaxSampleRadius`); it
never bounds the output. So the outermost node, the composite, declares an
unbounded output, the output asked of it is the whole clip, and the
composite, the enlarge, the gather and the reduce all run over the clip
(the gather over the clip grown by the reach). That is why the cost does
not follow the node. The comment above `makePhosphorBloom` says the
opposite — that the declared sampling radius makes Skia bound the node to
the reduced source grown by the reach instead of giving a runtime shader
the whole clip — and is wrong on this point.

It evidently means a phosphor bloom a live caption can wear: the halo's
cost following the layer it filters, of the order of the blur it replaces
for a caption-sized layer, which a crop of the filter graph to the
content's bounds grown by the effect's reach would give.

A test should build `Effect::phosphorBloom(radius, …)` and ask the filter
it produces for its forward bounds over a node rect
(`SkImageFilter::filterBounds(nodeRect, identity, kForward_MapDirection)`,
and `computeFastBounds(nodeRect)`), asserting both equal the node's rect
grown by the effect's reach on every side, never the canvas or an
unbounded rect. A bench arm in the material library should run
`phosphorBloom` over an 800x60 and a 900x300 live layer and record both,
and the pair should scale with area.

Wanted by `karaoke_wipe`, whose sung line glows through a blurred copy of
itself added under it (a second text leaf on the same tracks) because the
one-node bloom does not fit a frame.

## `grained` puts no grain on a near-black ground

The `grained(over, amount, frequency)` the ground studies carry
(`axis_ripple`, `chladni_tab1`, `elastic_type`, `nightingale_coxcomb`)
layers `material::noise(frequency, {.grain = true})` over `over` with
`BlendMode::SoftLight` at `amount` opacity.
Soft light has two halves, and both scale with the ground's value `d`.
Where the noise `s` is below mid grey it darkens by `d·(1−d)·(1−2s)`;
where it is above, it lightens by `(2s−1)·(D(d)−d)`, and for `d ≤ 0.25`
`D(d)−d = 16d³ − 12d² + 3d`, which tends to `3d` as `d` falls and is
about `2.3·d` at `d = 0.07`. So on a ground as dark as a night sky or
a night sea, the noise moves the ground by under one 8-bit level either
way. `shipping_forecast` measured it on its sea (`0x0B111A`): at
`amount = 0.5` the ground's standard deviation over a 100 px patch at
2x was 0.36 levels against 0.13 with no grain. Laying a mid grey
grained at full strength over the ground as a separate translucent
layer does not help either: the noise the kit keeps has so little
range that at 14% opacity it lifted the ground's mean by ten levels and
its deviation only to 0.7.

The function is meant to dress ANY ground in grain as light, and says
so in its comment ("dressed in light rather than speckled in hue").
Dark grounds are where film grain and phosphor grain are most wanted.

A test should render `grained(c, 0.1f)` over a 256×256 box for a
near-black `c` (`0x0B111A`) and a mid-tone `c`, and assert that the
luminance standard deviation of the dark one reaches at least a stated
fraction of the mid-tone one's (a grain whose strength does not
collapse with the ground's value — for instance soft light replaced by
an additive light term scaled by `amount`, or the noise's range
normalised before the blend), while `amount = 0` still returns `c`
exactly.

Wanted by `shipping_forecast` (its night sea carries no grain, and a
comment beside the ground says why). `elastic_type` and `axis_ripple`
also call `grained` and would move with it.

Not a new finding: append `axis_ripple` to the wanted-by list of the
entry of that name (filed from `shipping_forecast`). Its ground asks
for `grained(0x0C0C0E, 0.07f, 0.85f)`, and a 100 x 80 px patch of
that ground at 2x measures a luminance standard deviation of 0.58
8-bit levels with the vignette's slope included, so no grain reads.
The sketch keeps the call, with a comment beside the ground stating
the constraint, so the grain appears when the grain holds its
strength on a dark ground.

## `sigillum_aemeth` still marks a workaround for an echo that now follows the path

`src/sketch/sketches/sigillum_aemeth/sigillum_aemeth.cpp` (around line
130) carries a `workaround:` line above `onSide`, saying a layer
style's echo is stamped as a straight run at the box's origin rather
than along the text's path, so its heptagon runs set with `textOnPath`
go without the incised echo `incised` gives every straight run. A
layer style's echo now re-stamps a path-set run along its path, glyph
for glyph as the real pass places it
(`ComposeTextPathEcho` asserts it), so `grep -r workaround:` lists a
compensation for a defect that no longer exists.

What the sketch evidently intends is every lettered run incised alike:
`onSide` should set its run through `incised` (or state the same
`LayerStyle::echo`) and the `workaround:` line should go. The plate
moves where the seven side runs gain their lit lip below and to the
right of each letter, and nowhere else; that move, explained in the
commit, is the check.

## A colour is read from its CSS text only by the pen library, so a sketch whose palette is in a words file borrows SigilDraw or parses hex by hand

`sigil::draw::parseColor(std::string_view)` (`sigildraw/Color.h`) reads
`#rgb`, `#rgba`, `#rrggbb`, `#rrggbbaa` and the named colours, and it is
the only door in the tree that turns a colour's text into a
`material::Color`. SigilMaterial, which owns what a colour means, offers
`material::rgb(uint32_t)` and SigilCompose `hexColor(uint32_t)`, both
from an integer. So a compose sketch that keeps its palette in `data/`,
as a sketch keeps its words, either links the p5 pen library for one
function (`chladni_tab1` does, in `Figures.h`) or writes
`std::stoul(hex.substr(1), nullptr, 16)` itself: `black_watch`'s
`Tartan.h` `colourOf`, `chevreul_circle` (its colour column) and
`thaumonomicon` each do, and none of the three reads an alpha or a
three-digit form.

It evidently means one reading of a colour's text in the library that
owns colour, with `parseColor` moving to SigilMaterial (and SigilDraw
calling it there), so a palette written as `"#3a3125b8"` in a words file
means the same colour in every sketch and every runtime, and a JSON
value a sheet's custom property is set from needs no local helper.

A test should assert that `#e3d7b6`, `#e3d7b6ff`, `#ed7` (as `#eedd77`)
and `#3a3125b8` read to the colours and alphas their digits say, that a
named colour reads as SigilDraw reads it today, that text it cannot read
answers opaque black as today, and that SigilDraw's `parseColor` answers
the same colour for each.

Wanted by `chladni_tab1`, `black_watch`, `chevreul_circle` and
`thaumonomicon`.
Also wanted by `cosmati` (its nine quarries and four inks live in `data/pavement.json` and it links the pen library for one function), `chevreul_circle` and `thaumonomicon` (each parses hex by hand over `std::stoul`, without alpha), beside `black_watch` and `nightingale_coxcomb`.

## A gradient or grained fill compares by its shader's address, so the same fill described again is a new paint and its bake is taken again

`Fill::operator==` (`sigilcompose/core/Paint.h`) compares `shaderValue`,
an `sk_sp<SkShader>`, by pointer. `linearGradient`, `radialGradient`
(`core/Paint.h`) each mint a new shader on every call. So a node filled
with one of them compares unequal to itself on the next describe, even
with every colour, stop and point the same: it is re-patched, and a
`Cache::Texture` over it is baked again. `chladni_tab1`'s leaf — a
1560 x 2020 grained, vignetted, gutter-shaded paper under a
`Cache::Texture` — was baked again on every describe while the sketch
described its sand each frame, and its capture took minutes instead of
seconds until the leaf was built once in `setup()` and held; its fan
gradients are held for the same reason.

The fills are documented as ordinary values ("Both are ordinary
`Fill`s"), so they evidently mean to compare as the values they were
made from: a gradient by its points, colours, stops and tiling, a
grain by its colour, amount and frequency, the way a
`geometry::shapes::` generator compares by its fields.

A test should describe a box filled with `radialGradient({50, 50}, 40,
{a, b}, {0.2f, 1})` under `Cache::Texture`, draw, describe the same box
with a second identical call, draw again, and assert the second draw
takes no bake (the stats' cache writes are zero); and the same for
`linearGradient`; and that changing
one stop does take a bake.

Wanted by `chladni_tab1` (holds its leaf element, its twelve fan fills
and the two gradients its wavefronts are inked with as members for this
reason alone); any sketch that describes again from `update()` over a
gradient or grained ground meets it.
Also wanted by `kumiko_asanoha`, which holds its washi material as a member so the fill keeps one identity across describes, and `black_watch`, which holds its board and yarn paints for the same reason.

## A node filled with a material recipe takes its bake again on every describe, even when the paint is held

`chladni_tab1` fills its stars with `ink`, a `material::skia::Paint`
held as a member and built once in `setup()` as
`Paint::blend({{Paint::solid(ink), kSrc}, {Paint::recipe(field::grain(0.09f, 3, 4.0f, 0.35f)), kSoftLight}})`,
each star under `Cache::Texture`. Describing the unchanged tree again —
the same members, the same held paint — makes the frame of each describe
several times the steady frame, and that frame's bakes are the stars':
with the stars filled by `Fill::var("ink")`, or by a `Paint::blend` of
two solids, the same describes leave the frame flat, and with
`Paint::recipe(field::grain(...))` alone as the fill they spike as the
blend does. So the recipe-backed layer, not the blend, is what keeps the
node from pruning or keeps its texture from surviving the prune. Where
it goes wrong was not traced: `.fill(Paint)` stores a geometry-dependent
paint in the live material slot, and `materialEqual` answers false for
any slot whose paint reports `isAnimated()`; `field::grain`'s shader
reads neither time nor content scale, so either the recipe reports a
frame input it does not read, or the live slot's texture is dropped on
re-patch whatever the compare answers.

It evidently means what `Paint::operator==` documents — "re-running the
same describe code yields EQUAL paints" — and what `materialEqual`
states for geometry-dependent static materials: they "compare by recipe,
so identical re-describes prune like any other static material". A
grain whose recipe, bytes and bindings are the same is the same paint,
and a texture over it stands until something it depends on changes.

A test should fill a 200 x 200 box with
`Paint::recipe(field::grain(0.09f, 3, 4.0f, 0.35f))` under
`Cache::Texture`, draw, describe the identical tree, draw again and
assert the second draw takes no bake; the same with the recipe as the
soft-light layer of a `Paint::blend` over a solid; and that changing the
grain's frequency does take a bake.

Wanted by `chladni_tab1`, which for this reason shows the bowed figure's
live sand by a stepped value over two stampings of every figure's pool,
a baked one and a live one, instead of describing the tree again when
the bow moves to the next figure; any sketch that describes again over a
grained ink or ground meets it.

## An echo and a shadow take their colour as a value, so neither can follow the sheet's ink

`LayerStyle::echo(SkVector offset, material::Color color)`
(`sigilcompose/core/Shape.h`) and `shadow(material::Color color, SkVector
offset, float blur)` (`sigilcompose/brush/Decorations.h`, the `Shadow`
value) hold a colour, not a `SurfacePaint`, so neither can be written as
`Fill::currentInk()` or `Fill::var(name)`. A stroke's `PathFormat` already
takes a `SurfacePaint` whose default is the ink in force, and
`textStroke` resolves a `Fill::var` against the tree; an echo under a
title and a glow under a needle are the same kind of mark and cannot.

So a sketch whose colours live in a sheet's custom properties still reads
them out of its own palette to hand to these two: `nightingale_coxcomb`
passes its palette's ink to the echo under each display line and its
palette's brass to the needle's glow, and a theme swapped by a different
token sheet would leave both behind.

They are evidently meant to paint like every other decoration: a
`SurfacePaint` defaulting to the ink in force, resolved at paint against
the node's cascade. A test should set `ink(var("accent"))` on a root with
`var("accent", red)`, give a child text `layerStyle(LayerStyle::echo({1,
1}, Fill::currentInk()))`, and assert the echo's pixels are red; and the
same for a `shadow(Fill::var("accent"), …)` under a box. This is the same
defect as `textFx::tint` taking its colours as values, in two more places.
Wanted by `nightingale_coxcomb`.

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
`SurfacePaint` (colour, gradient, shader or material), resolved by `fill`,
`ink` and a stroke's paint exactly as a colour var is, so a quarry is a token
and a class is its whole look.

A test should state `rule(":root").var("stone", someMaterial)`,
fill a box with `Fill::var("stone")` under it, and assert the box paints what
`fill(someMaterial)` paints; and that reading the same property
as a length leaves the target standing and says so once, as a colour var read
as a length does today. Wanted by `cosmati`; `black_watch` holds its board and
yarn paints as members for the same reason.

## Two Compose warning cases pass alone and fail when the whole binary runs

`ComposeMaterial.AFillRefusesATextUnitOnASurfaceWithNoPaintToPlace`
(`src/common/compose/core/test/ComposeTestMaterial.cpp:935`) and
`ComposeInkUnits.ARulesUnitLandingOnABoxDrawsAsTheVerbsDoes`
(`src/common/compose/typography/test/ComposeTestInkUnits.cpp:525`) assert
that a warning reaches the captured log, but the warnings they wait for are
issued once per process. Run as ctest runs them, one case per process, both
pass; run as `compose_test` with no filter, an earlier case has already
spent the warning and both fail with an empty find. Intended: each case
states its own warning whatever ran before it. A test should assert the
warning lands when the case runs after another case that triggers the same
warning in the same process — which means the case resets the once-only
state it reads, or the capture the case reads is the one that issues it.

## `Filter::of(material, colourType)` compiles and reads the colour type as a sample radius

`material::Filter::of(const Material&, float sampleRadius)`
(`sigilmaterial/filter/Filter.h`) takes any argument that converts to a
float, and Skia's `SkColorType` is an unscoped enum, so
`Filter::of(program, kRGBA_8888_SkColorType)` compiles and builds the
program with a sample radius of 4 instead of lowering it for an
eight-bit surface. Two Material cases and the OCIO bench were written
that way and silently tested the program path; they now call
`skia::lowered(program, surface)`, which is the entrance that lowers.
Evidently the radius was meant to be a length and nothing else. A test
should assert that `Filter::of` does not accept a colour type — a
deleted overload taking an integral or enumeration argument, or a radius
type of its own — so the mistake fails to compile; the sketches still to
be swept are where it would otherwise recur.

## Two Compose log cases pass alone and fail when the whole binary runs in one process

`ComposeMaterial.AFillRefusesATextUnitOnASurfaceWithNoPaintToPlace`
(`compose/core/test/ComposeTestMaterial.cpp:940`) and
`ComposeInkUnits.ARulesUnitLandingOnABoxDrawsAsTheVerbsDoes`
(`compose/typography/test/ComposeTestInkUnits.cpp:525`) each expect a
warning in the log their case captures. Under ctest, one process per
case, both pass; running `compose_test` whole, both fail, because the
warnings they read are warned once per process and an earlier case in
the same process already spent them.

The cases are evidently meant to assert the refusal whatever ran before
them. A test should reset the warn-once state it reads (or read a
per-composer report), so that `compose_test` run whole and each case run
alone give the same answer.

## A feed a Python study lets escape outlives its session

`SketchPython.ASessionClosesFeedsDespiteEscapedPythonWrappers`
(`sketch/python/test/PythonTest.cpp:109`) fails alone and in the whole
run: after `session.reset()`, both feeds report closed, but the inlet
behind `fixture://input` — the feed the study stored in `builtins` —
has not expired. So something still holds that feed's door after the
session closed it, most likely the escaped Python wrapper keeping a
strong handle.

The intent is that closing a session releases every feed it opened,
whatever Python kept a reference to. The case already asserts the right
thing (`feeds["fixture://input"].expired()`); it should pass once the
escaped wrapper holds nothing that keeps the door alive after close.
