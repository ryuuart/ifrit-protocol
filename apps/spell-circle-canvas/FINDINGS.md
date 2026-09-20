# Findings

## Group ruby is fixed in the library and `ruby_kenten` still asks for words

Compose has a selection unit now: `weave::Unit::Selection` numbers one unit
per extent a selector addressed, `TextAnnotations.cpp` associates one
reading with that whole unit, and a base broken across columns partitions
the reading across its fragments instead of repeating it. Two extents that
touch are still two units.

`src/sketch/sketches/ruby_kenten.cpp` has not moved. Its GROUP specimen
(line 149) and its SPLIT specimen (line 173) still pass
`weave::Unit::Word`, and word units divide `書物` and `国語辞典` into
several units, so those two panels still show one reading repeated over
each piece of the compound. The file's own header comment (line 19) and
its GROUP panel caption still name the word unit.

The specimens should ask for the unit that means what they demonstrate.
Moving those two call sites to `weave::Unit::Selection`, with the comment
and the caption, and rebasing the plate — GROUP and SPLIT move, MONO,
JUKUGO and KENTEN must not — closes this entry. The library behaviour is
already asserted by `ComposeAnnotate` and `TextVertical`; what a test
cannot see is which unit a specimen names, so the plate is the check.

## An outward rail doubles back at every arc join

`src/common/geometry/path/Contour.cpp` writes a join at the distance
`Contour::corners` bisects to, and `bisect` answers the far end of its
bracket, so the corner's recorded distance lands just past the real
vertex. The sample at the vertex is therefore emitted before the join's
entering point, and `swallowedByJoin` drops a sample only inside a
MITER's reach — an arc join, which is what a turn away from the offset
side opens, stands for no sample at all. A constant outward rail
(`parallel` on a clockwise polygon) and a varying one (`profileOffset`,
which shares the same body) both zigzag by well under a sample step at
every corner, and the rail self-crosses there.

A join stands for its vertex, so a sample at that vertex is a place the
join has already answered and must be dropped whether the join is a
miter or an arc. A regression should offset a clockwise polygon outward,
at several scales, with a constant profile and with a varying one, and
assert the rail has no self-crossing at any corner. Closing it moves
every constant rail's point list — `operations::offset`,
`edges::insetOutline` and every constant band — so the plate tier is the
cost of the fix.

## A corner sharper than a right angle keeps a spur the miter window misses

`swallowedByJoin` drops samples within `join.radius` either side of a
miter, while the miter overshoots the vertex along the contour by the
offset divided by the tangent of half the interior angle. The two are
equal at a right angle, smaller above it and larger below, so a corner
sharper than 90° leaves samples the miter already answered for standing
in the rail, and each one closes a small loop toward the offset side. A
four-point zigzag with sharply acute corners shows one loop per turn, on
a constant rail and on a varying one alike.

The window should be the reach the join actually takes, which
`offsetJoins` already computes as `reach` when it strikes the miter
point. A regression should offset a polyline whose interior angles run
from obtuse through 90° to sharply acute, on both sides, and assert no
corner loop at any of them, with the rails at 90° and wider unchanged to
the bit. Only plates holding corners sharper than a right angle move.

## The chord between two of a strand's contours is walked as a strand

`flat()` in `src/common/geometry/path/Crossings.cpp` repeats a contour's
last point at a break, and the comment beside it says this lets the
segment loop skip the join. It skips half of it: the duplicated pair is
zero length and is dropped by the `r.length() <= 1e-6f` guard, but the
pair that follows runs from the previous contour's last point to the
next contour's first and is intersected like any other segment. What
suppresses a hit on that chord today is the run rule in `changesSides`,
which refuses a probe that cannot step past an open contour's end;
where the contour before the break is CLOSED the suppression is instead
the seam wrap, which probes geometry that has nothing to do with the
chord.

A chord between two contours is part of neither mark and should never be
intersected. The run indices `Flat::Run` records make the skip a test on
the segment index. A regression should hand `discoverCrossings` a
two-contour strand whose contours are closed and whose join chord is met
by another strand running nearly along it, and assert no crossing on the
chord while the crossings on both contours still answer. The comment in
`flat()` overstates what the duplicate point buys and should be
corrected whether or not the segment is skipped.

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

## `SurfaceParameters` takes sRGB numbers into fields documented as linear

`material::kit::SurfaceParameters` is introduced as "The
metallic-roughness ABI. Colours are LINEAR"
(`src/common/material/include/sigilmaterial/kit/Pbr.h`), and its
`baseColor`, `emissive` and `absorption` are `material::Color`, which
documents itself as "four straight (not premultiplied) components in
sRGB" (`include/sigilmaterial/color/Color.h`). Every writer supplies
sRGB: `SurfaceParameters::metal(tint, roughness)` and `::dielectric`
take a colour and store it unchanged, and the Python setters in
`src/common/python/world/World.cpp` read a CSS string through
`python::color` into the same field, so
`SurfaceParameters(baseColor="#804000")` writes sRGB-encoded numbers
into a linear field with no report. The stock values are linear —
`gold()` carries gold's linear reflectance — and
`kit/shaders/Surface.sksl` multiplies `baseColor` by the map sample, so
an authored colour and a sampled texel are meant to be in one space.

The struct should hold linear values, because that is what the shader
multiplies. A test should assert that a base colour written as an sRGB
mid-grey shades identically to a flat map whose sampled value is that
grey's linear equivalent — exactly one transfer conversion on the way in
— and that a colour read back comes out in the space it was written in.
Either the struct takes a colour type of its own that says it is linear,
or the setters and the two static helpers convert; the two header
sentences cannot both stand.

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

## A live host cannot run a compiler spelled as more than one word

`compileLine` and `linkLine` in `src/sketch/live/Host.cpp` wrap
`options.compiler` in `shellArgument`, which single-quotes the whole
string, so a compiler given as a command prefix reaches `popen` as one
argv word. The case
`SketchHostBuildDirectory.TwoHostsInOneProcessNeverLinkOverEachOther`
hands the host a stub compiler spelled `/bin/sh <script>` — the comment
above `stubCompiler` says a command prefix "is all the host ever does
with the compiler it is given" — so every build in that case fails with
`sh: /bin/sh …/compiler.sh: No such file or directory`, the case finds
zero linked dylibs where it expects four, and it fails on every run. The
guard against two hosts in one process linking over each other is
asserting nothing.

A compiler is a command prefix: the host should split it into words
before quoting each one, or the option should be stated and checked as
one executable path and the stub rewritten as one. A regression should
build through a compiler given as a multi-word prefix and assert the
object and the dylib land, and the two-hosts case should then count its
four distinct libraries again.

## The device sweep's shader-error sink collects without a lock

`skgpu::ShaderErrorHandler::compileError` is called from the pipeline
pool now, because `GraphiteContext` sets `fExecutor` and Graphite runs
each pipeline creation task on it — several at once for a scene whose
stages fail together. `ErrorSink` in
`src/common/material/test/MaterialGpuTest.mm` is a process-wide handler
that appends to a bare `std::string` with no lock, and the SigilSkia
README names SigilMaterial's device sweep as what that handler is built
on. It survives only because `shadeOnGpu` draws and submits one shader
at a time; the contract it was written against no longer holds.

A collector called from a pool must guard what it collects into, as
`PipelineLog` in `src/common/skia/graphite/test/GraphiteTest.mm` already
does. A test should call the sink's `compileError` from several threads
at once and assert that every report arrives whole.

## A sketch's first frame still waits for the programs a chain of stages needs

Sketchbook now stands its device programs up before the canvas draws:
every stock body is declared before the first Graphite context exists,
the programs a run builds are written down under the platform cache
location with the descriptions they were built under, and the next
launch replays that set on a worker while the canvas holds its frames.
Opening `crt_bloom` in the window twice over one store takes the
programs a draw builds from nine to one, which `sketch_pipeline_warmup`
asserts.

What is left is the FIRST open on a machine, which is still the one an
effect pass does after every shader edit. Nothing can be replayed then,
and what stands up instead is only the effect bodies that declare no
child: a described paint is expanded into every combination it allows,
so offering a child as an image makes a two-child body hundreds of
programs — the device is asked to hold hundreds it may never draw, the
driver stops compiling once its compiled variants no longer fit, and the
warm-up never returns. A composed stack, which is what the MAGI studies
wear and what costs the most, cannot be described ahead at all: the
backend inlines the whole chain into one program and which chain a
sketch wears is not known until the sketch has been read.

Two routes are worth trying, and neither was. A sketch declares what it
draws before it draws it, so the host could describe the stacks a
SELECTED sketch wears — from its own description rather than from the
catalogue — and precompile those, which is the only way to reach a
composed program ahead of its first frame. And the sweep and `--frame`
lanes neither declare nor record, because their device context is
created before the material warm-up joins; a batch run has no frame
anyone watches, but it draws the same programs, and letting it fill the
store would make the first interactive open of a machine the cheap one.

`sketch_pipeline_warmup` covers the second LAUNCH and not the second
open inside one process, deliberately: within one process the programs
are already standing in the context's own in-memory cache, which is what
the tally counts as found, so a second open of the same sketch builds
nothing whether or not a recorded set exists. Nothing there is this
feature's to remove. What the in-process case would show is a different
subject — the browser flipping between sketches, where each new sketch
brings a stack the context has never built — and that is what the route
of describing a SELECTED sketch's stacks ahead of opening it would
serve. A test for it belongs with that route rather than with this one.

A test should open a sketch in the window with a cold store and assert
that the stack's programs are standing before its first frame, and that
a headless run of the same sketch leaves a store the window run then
replays.

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
