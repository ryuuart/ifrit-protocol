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
