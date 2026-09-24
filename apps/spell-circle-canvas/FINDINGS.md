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

## `Element::fill` refuses the material its own surface value accepts

`compose::SurfacePaint` has an implicit constructor from
`material::Material` (`core/SurfacePaint.h`), `Element::textFill` and
`Element::textStroke` take a `SurfacePaint` by value and so accept a
material, and Python's `Element.fill` accepts one. `Element::fill`'s
surface overload is a template constrained
`std::same_as<std::remove_cvref_t<P>, SurfacePaint>`, which exists to
keep ordinary fills and paints on their own overloads, and that
exact-type constraint also blocks the conversion: `element.fill(recipe)`
does not compile, while `element.fill(SurfacePaint{recipe})` and
`element.textFill(recipe)` both do. The verb's own list of what may be
passed does not mention a material, and the two values the verb
deliberately refuses — a `Pattern` and a `pattern::Tile` — say so with
deleted overloads and a reason.

A material is a surface, so the verb that takes a surface should take
one. A test should fill an element with a recipe material and assert the
pixels match the same material passed as a `SurfacePaint`, and that the
deleted `Pattern` and `Tile` overloads still refuse.

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

`eva_magi_interior/EvangelionUi.h` sets `uBloom` to zero on `kit::crt`
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

## Three reference examples no longer compile, and nothing builds them

`src/common/compose/reference/examples/custom_element.cpp`,
`image_element.cpp` and `text_element.cpp` hand the `material::Color`
that `hexColor()` returns to `SkPaint::setColor4f` and call
`.toSkColor()` on it, which no longer exists on that type. The examples
are not in the compile database, so no target compiles them and the
doc probes, which check names rather than expressions, cannot see it.

Each example evidently intends to cross to Skia where it sets a paint,
which is `material::skia::toSkColor(colour)`.

Once restored, a test should compile every source under a library's
`reference/examples/` the way a sketch is built, so an example that
stops compiling fails the build rather than the reader.

## A paragraph partial's last-line fields are lost under a whole justification

`overlay(ParagraphStyle, ParagraphBlock)` copies the partial's
`justification` whole and ignores its `lastLineAlignment` and
`justifyLastLine`, so a block that states both a justification and a
last-line setting apart has the last-line setting overridden by the
whole it copied, while `justificationMethod` stated apart is folded in.

The stated-apart fields are evidently meant to land in the style's
justification the way the method does.

A test should assert that
`paragraph({.alignment = kJustify, .justification = {}, .justifyLastLine = true})`
on a text leaf sets its last line to the measure.

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

## A fact of a library type stated by a live-compiled sketch is not read back

`Attributes::get<T>` reads a fact with `std::any_cast`, which compares
the type identity the writer's image recorded against the reader's.
When Sketchbook compiles a sketch live and loads it beside the host, a
fact the sketch states in one of the libraries' own types —
`pin::Request` — is written with the sketch image's identity and read by
`pin::ByLane::add` in the host with the host's, and the cast answers
nothing: `pin::ByLane` finds the node through `Scope::having` and then
hangs nothing. `pins_and_hulls` shows it: rendered live with `--frame`,
none of its callouts appear. A fact of a standard type
(`std::vector<std::string>`, `int`) crosses fine, which is why
`connect::ByLane` and `stamp::ByLane` work in the same sketch.

A fact is evidently meant to read back in the type it was written in
wherever the reader was compiled. A test should load a sketch built as
its own image, state a `pin::Request` on a node under `pin::ByLane`,
and assert the pinned element is attached; `thaumonomicon` places its
tooltip by hand until then.

## A layer style's echo ignores the text's path

`Element::layerStyle(LayerStyle::echo(offset, colour))` on a text leaf
that also carries `textOnPath(...)` stamps the echo as a STRAIGHT run at
the node box's origin, while the real pass is laid along the path: on a
ring or a polygon's sides the echo lands as a pile of unrelated
horizontal lettering in the box's top-left corner. The echo is evidently
meant to re-stamp the same placed glyphs at `offset` beneath the real
pass, as it does for a leaf set on a straight line, so an incised or
misprinted run on a curve reads the same as one set flat.
`src/sketch/sketches/sigillum_aemeth/sigillum_aemeth.cpp` works around it
(`workaround:` in `onSide`) by leaving path-set runs without the echo.

A test: a text leaf on `shapes::circle()` with an echo of a known colour
and offset, rendered into a raster; every echo-coloured pixel lies within
`|offset|` plus a glyph's extent of the real pass's ink, and none in the
box's corner away from the circle.

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

## The kit's theme scope declares an exit that may swallow exceptions

The generated declaration of `Provide.__exit__` in the Python kit
bindings returns `bool`, which tells a type checker the context manager
may suppress an exception, so any function that returns from inside
`with kit.provide(look):` is read as possibly returning `None` and a
study written that way fails the typing check.

The scope evidently never suppresses anything, so its exit should be
declared to return `None` (or `Literal[False]`), and a study may then
return from inside the scope as `python_live_signals` did before it was
rewritten around the check.

A typing case should assert that a function returning a page from inside
`with kit.provide(...)` type-checks as returning `Element`.

## A written still is taken one step before the sweep's plate of the same moment

`Sketchbook <entry> --frame out.png --at 1 --scale 2` and the sweep's
plate at `--at 1` are the same size and bake on the same grid, because
the written-still path pins the bake density before its first step. For
a scene that moves they are still different pictures. The written still
is `Host::prepareCapture` stepping to the moment and then `Host::still`,
which calls `Session::repaint` and draws the state the last step left.
The sweep takes its plate through `Session::still`, as a protocol
session's moving clock does, and for a canvas session that draws one
more frame of `stillStep()`, so the plate shows the moment one frame
later. `SketchWrittenStill` holds the two equal only for a scene that
draws once and keeps its canvas, where the extra step cannot show. A
protocol session has the density gap as well unless a client pins
`session.pinDensity` before the open: `takeStill` sets the bake density
at the still, after the frames before it baked at the session's own.

Which moment `--frame --at t` means is a decision: the sweep's moment,
one `stillStep()` past t, with one path through `Session::still`; or
exactly t, with the sweep's moment restated to match. The update counts
that `render_file` and `test_capture` assert follow from that choice,
because moving `--frame` onto `Session::still` runs one more update.

A test should assert that `--frame --at 1 --scale 2` of `cascade` equals
the sweep plate at `--at 1` byte for byte, and, in process, that
`pinDensity(2)`, `setPolicy(Advance)`, `step(1)`, `still(2)` equals both.

## `material::kit::sparkle` sizes its cells in the shader's own units, so it cannot be an ink

Five of the six text paints read the run's `extent` from
`TextPaintParameters` and work in `(point - origin) / extent`. `Sparkle.sksl`
reads `origin` alone: its cells are a fixed 22 units of the coordinates the
shader is sampled in, and its points a fraction of a cell. A fill samples
a recipe in node pixels, so `sparkle(box)` as a fill twinkles as authored.
An ink samples it in the unit square — `Composer::Impl::textInkOf` maps
[0,1]² onto the passage's metric band — so the whole passage stands inside
one cell and at most one point of light shows, whatever bounds are
passed. The only way a sketch had to see it on type was an `SkShader`
local matrix scaling a pixel-sized field into the unit square.

The preset is evidently meant to be one of the six text paints, all of
which the header says are painted over a run: its cell size should be a
fraction of `extent`, as the other five scale their fields, so
`sparkle(unit, t)` as an ink twinkles across a word as `sparkle(box, t)`
does as a fill.

A test should ink a word with `sparkle(SkRect::MakeWH(1, 1), t)` and
assert that light lands on several separate glyphs, and that the same
material as a fill over a 220 × 70 box is unchanged. Wanted by
`text_paints` (its SPARKLE OVER A BASE cell shows the base alone);
`stock_materials` shows it only as a fill.

## A block after the first takes its first-line indent one line late under the optimizing breaker

`IntervalSequence` fetches source lines lazily and insets each one as it
is fetched, with the first-line indent only where `m_lineInBlock == 0`.
Under `TextWrap::Pretty` (Knuth–Plass) the breaker asks for the interval
past a block's last line while that block is still open, so the line the
next block will start on is fetched and inset as a continuation line of
the block before. `openBlock` then resets `m_lineInBlock`, and the NEXT
fetched line — the new block's second — takes the first-line indent. The
greedy breaker (`TextWrap::Auto`) fetches no further than it places and
indents every block's first line.

`textIndent` evidently means the first line of every block, whichever
breaker sets it; the fetched-but-unplaced line should be re-inset (or
fetched afresh) when a block opens, together with the pitch, lead and
grid step `openBlock` hands over, which the same early fetch sets from
the previous block.

A test should set two blocks with `textIndent(20)` and
`textWrap(TextWrap::Pretty)` and assert that each block's first line
starts 20 px in and its second line at zero, matching the same passage
under `TextWrap::Auto`. Wanted by `text_paints` (its long run shows the
second paragraph's indent on its second line).

## Every sketch that hyphenates English builds the same pattern hyphenator

`weave::kit::englishHyphenationPatterns()` is the pattern text, and each
sketch that hyphenates wraps it in a function-local static
`std::make_shared<const weave::kit::PatternHyphenator>("en", …)` of its own
— `paragraph_sheet`, `manuscript/manuscript.cpp` and `text_paints` carry
the same five lines — because `HyphenationOptions::patterns` holds a
shared hyphenator and the kit ships only the table.

The kit evidently means the one set it carries to be the ready choice: a
held instance beside the table (`weave::kit::englishHyphenator()`, the
name the owner's to pick) would let `hyphens({.patterns = …})` be one
expression, and would give every passage the same pointer, which is what
the paragraph setting compares by.

A test should assert that two calls return the same instance and that
`hyphens({.patterns = englishHyphenator()})` breaks "specimen" where the
hand-built table does. Wanted by the three sketches named.

## A connection's vitals have no value of their own, so every Data sketch snapshots and prints them by hand

`data::Connection` answers `generation()`, `dropped()`, `undecodable()`,
`closed()`, `address()`, `sender()` and `error()` one call at a time, and
nothing in SigilData or the sketch kit gathers them. So every sketch that
shows a door's state declares its own comparable struct of those fields,
a function that fills it from the connection, a "did it change" test that
decides whether to describe again, and a readout of name–figure rows:
`feed_sky` (`Vitals`, `vitalsOf`, `Door::read`, `readout`), and a `Reading`
or equivalent in `phone_sky`, `webrtc_sky`, `osc_desk`, `feed_events`,
`grpc_watch`, `midi_pads`, `serial_sensor`, `artnet_lights`,
`channel_bind` and `feed_vitals` — eleven copies of one shape, each
naming and ordering the rows its own way.

The connection evidently means its own words to be what a reader shows:
one comparable value taken off a connection (the fields above, equality
by value) would make the change test `now != shown`, and one sketch-kit
component over it (a readout of that value in the theme's `caption` and
`.readout` registers, the error row standing in for the sender where
there is one; `connectionReadout` is a provisional name, the owner's to
pick) would take the struct, the fill and the rows out of every sketch
named.

A test should take the value off a connection before and after a
recorded arrival and assert that it compares unequal exactly when a
field moved, and a Harness case should assert the component's rows for
an open door, a closed one and one whose URI failed to open. Wanted by
the eleven sketches named.

## An initial letter's own style reaches its shaping and not its paint

`InitialLetter::style` is documented as a partial over the opening's
style. `layout/InitialLetter.cpp` overlays it into `capStyle` and uses that
for the cap's size and face, but the positioned run it inserts takes
`cap.styleIndex = plan.styleIndex` — the opening word's span — and the
painter resolves a run's paint through `spans[run.styleIndex]`
(`paint/Paint.cpp`). So a colour, a decoration or a paint layer the
initial states is dropped and the cap is drawn in the passage's own ink:
`InitialLetter(style=Type(color=teal))` sets a paper-coloured cap.

The partial is evidently meant to style the cap wholly, as it already does
its shaping fields: the cap run should carry a style of its own (appended
to the layout's spans, or resolved by the painter from the plan) so its
paint is the overlaid one.

A test should set `initialLetter({.lines = 3, .style = {.color = red}})`
over a white passage and assert that the cap's pixels are red and the
body's white. Wanted by `python_type_atelier` (its teal cap) and
`bullets_dropcap` (whose cap asks for the palette's figure colour).

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

## SigilWeaveKit's line-edge tables are empty modules in Python

`src/common/python/weave/Tables.cpp` registers `sigil.weave.kit.hanging`
and `sigil.weave.kit.kinsoku` and binds nothing into them, so
`weave::kit::hanging::latin()` and the kinsoku sets are unreachable, though
`HangingTable`, `HangingEdge` and `KinsokuTable` are bound as values and
`ParagraphBlock(hanging=…)` takes one.

The modules evidently mean to carry the stock tables `kit/LineTables.h`
ships. A Python passage that wants optical margin alignment today states
its own few edges.

A test should assert that `ParagraphBlock(hanging=weave.kit.hanging.latin())`
equals the C++ table entry for entry, and likewise for each kinsoku set.
Wanted by `python_type_atelier` (its justified story states three edges).

## A text leaf in Python cannot be given a pattern hyphenator

`HyphenationOptions` is bound with `enabled`, `penalty`, `limits`,
`consecutiveLimit`, `zone` and `lastWordOfBlock`
(`src/common/python/weave/Type.cpp`) and without `patterns`, so `hyphens(…)`
on a node or a rule breaks a word only at the soft hyphens typed into it.
`weave.kit.PatternHyphenator` and `englishHyphenationPatterns()` are bound
and `layoutParagraph(…, hyphenator=…)` takes one, so the pattern hyphenator
reaches the paragraph engine directly and never the paragraph lane.

The field is evidently meant to be bound as the C++ one is, holding the
shared hyphenator.

A test should set `hyphens(HyphenationOptions(patterns=PatternHyphenator("en",
englishHyphenationPatterns())))` on a narrow justified leaf from Python and
assert a line ends in a hyphen. Wanted by `python_type_atelier` (its story
carries typed soft hyphens instead).

## `rem` measures against the composer's inherited font, not the root element's

`Composer::Impl::runCascade` resolves every `rem` length against
`rootFont`, which is what `Composer::setInherited` last stated
(`src/common/compose/core/Cascade.cpp`, `resolveLength` and the font
overlay), so a `fontSize` stated on the tree's root node — directly or by a
`rule(":root")` — changes what `1_em` means below it and leaves `1_rem` at
16 px. CSS's `rem` is the root ELEMENT's computed font size, and the
cascade chapter says "`1_rem` is the root's size".

It evidently means the root element: a sheet that states the one root size
on `:root` and writes its type scale in `rem` (the token sheet a Tailwind
scale is) should set every step against that size. Until then a scale in
`rem` is a scale against 16 px, and `black_watch` states its sizes in
pixels.

A test should state `fontSize(10)` on a root box, set a child leaf at
`fontSize(1.5_rem)`, and assert the leaf resolves to 15 px; and that
`setInherited` still decides it where the root states no size. Wanted by
`black_watch`; every sketch that takes the brief's `rem` scale meets it.

## A pattern made from a tile samples linear whatever the tile says

`material::pattern::clothTile` returns a `Tile` whose filter is nearest
("sampled nearest"), and `compose::Pattern(Tile)` keeps the tile but not
its filter: `Pattern::bake` samples through its own `m_sampling`, which
starts linear (`sigilcompose/core/Pattern.h`). So a cloth, a dither or a
grid line held as a `Pattern` is blurred across every thread edge when the
still is taken at a density the bake was not made at, unless the caller
states `.sampling(SkSamplingOptions(SkFilterMode::kNearest))` again.

The tile's own filter is evidently meant to carry through: a `Pattern`
built from a tile should start at the tile's filter, and `sampling()`
should override it.

A test should build `Pattern(clothTile(cloth, 2))`, bake its material and
assert its sampling is nearest; and that `.sampling(linear)` after it
wins. Wanted by `black_watch`, which restates nearest on every pattern it holds.
