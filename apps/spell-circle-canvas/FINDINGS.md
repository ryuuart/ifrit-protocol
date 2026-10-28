# Findings

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

## The Library of Babel flips a few pixels by one level in about one run in five

`borges_library.py` flips two to five pixels by one level in about one
capture in five, alone as often as under a GPU load, and the 25-pixel,
seven-level move once seen under load did not reproduce in twenty-one
renders; Graphite snapshots are copies, the path atlas resets at
deterministic flushes and the sketch's random source is seeded, so the
cause is elsewhere. A capture is a pure function of scene time. A test
should take ten consecutive captures and find them identical, and the
original load condition should be re-run before more work goes in.

## A timed scene3d sweep frame can carry no pending wait under load

`DeviceSweep.ATimedScene3DFrameCarriesThePresentedPicturesWait`
(`src/sketch/plate/test/SweepTest.cpp`) failed in a parallel gpu run: under
load the frame carried no pending wait, while a timed scene3d frame is
meant to carry the presented picture's wait every time. The test should
hold under `ctest -j8` with the device loaded; the case asserts a wait,
not a duration, so the serial label is not its fix.

## A program over an undeclared effect gets no key from Skia

Skia lets a context declare one hundred runtime effects, of which the
stock effects take eleven; a sketch's own effects written down at exit
are declared on the next launch, and the store now names each sketch
whose programs run past the limit and go unkeyed. A program over an
undeclared effect carries a dynamic snippet identity for which this Skia
hands back no key, so keying such a program by its text hash wants a
Skia port patch. A test should key a program over an effect declared
past the limit and find it stood up on the next open.

## Two scene3d sessions stepped to one moment can differ under load

`Scene3DSession.TheSameMomentIsTheSamePicture` failed in two parallel
gpu runs beside a World bench: two sessions stepped to the same frame
read back different pictures, while a scene3d frame is meant to be a
pure function of scene time; this may share the cause of The Library
of Babel's one-level flips on the device, which also survive with its
texture bakes off and never appear on the raster. The two timed
PostChain cases also still fail under `ctest -j8` beside an external
load although they run serially. A test should assert the two runs
byte-equal under `-j8` with the bench running; the next step is to draw
the interior with antialiasing off and a fixed path strategy and diff.

## A buffer re-formed on another device's context starts transparent

A `Graphics` buffer now follows the device its host draws on, but a
picture standing on another device's context is not carried over, because
Graphite has no cross-context copy here and the host-less readback reads
only its current context; the headless window and `renderFile` still draw
their frames on the CPU, so a device stack is read back there once per
change. The picture is meant to carry across, and every host meant to draw
on the device it has. A test should draw a layer, install a different
device, and find the next composite still showing the marks; and a
headless capture of a stack pixel-equal to the window's.

## paint_shelf's swatch captions no longer state the window its code states

The captions still read the sweep window from three o'clock while the
code now states it from twelve, kept so the plate stays identical; a
caption is meant to state what the code states. The sketch pass rewrites
the captions and the plate moves with that cause.

## The triplet comment still explains shared TBB by a port that is gone

`cmake/triplets/arm64-osx.cmake` explains its shared oneTBB rule by the
vcpkg usd port, which the manifest no longer carries; the rule holds
because shared is the one linkage oneTBB supports, and the comment
should say so. Editing the triplet changes every port's ABI hash and
forces a full dependency rebuild, so the reword lands with the next
intentional triplet change.

## A face adopted by hand through a session's context outlives the session

`ctx.fonts.adopt(face, origin)` adopts into the context the host shares
across sessions and stays for the context's life, while every adoption a
session makes is meant to be released when it closes, as the ones its
files make now are. A test should find a face a session adopts by hand
through `ctx.fonts` not found by family after the session closes.

## A face arriving while two sessions stand is held by both

Sessions over one hub on one thread all receive a face that arrives, so a
face decoded while two stand gets a share for each, because the decoder
cannot tell which session asked, and the family lives until the older
session closes. A test should find a face loaded by one of two concurrent
sessions over one hub leaving when that session closes, whatever the
other did.

## vertigo_titles renders to more than one hash

The sketch's device plate has answered three different hashes across
sweeps of one tree, with the differences inside the hollow letters, a
text leaf whose ink is a clear material under stroke and underlay
effects; a plate is meant to be the same bytes on every render of one
tree. A test should find twenty renders of the sketch at its capture
moment hashing alike, or name the effect whose output varies.
## A layer's colour function on the CPU path clamps values past one

Compose applies a layer's filter as an image filter, and on Skia's CPU
rasteriser any image filter over a layer reads values past one held at
one; a layer also takes the destination's format, so an 8-bit destination
clamps before the filter on any backend. A colour function over float
content is meant to scale the stored value. A test should find
brightness(0.2) on an image element holding an F16 texel of 4.0 reading
0.8 on a raster F16 target, which wants a pure colour function on the
layer paint's colour filter rather than its image filter.

## A painted map's device texture can outlive its device

A PaintedMap handed a device destroys its texture through that device when
its last copy goes, so a material lowered and kept past the device's life
calls into a destroyed device; compose's TextureScene has the same shape.
The painting is meant to be released while its device stands or forgotten
once the device has gone. A test should find a lowered material worn by a
World and destroyed after its device touching no device.

## A device-painted data map rounds its straight alpha differently

On the device a data slot's straight values are divided before the 8-bit
quantisation, while the host route quantises premultiplied values and
converts on upload, so a translucent data map reads differently by route.
One picture is meant whichever route paints the map. A test should find a
translucent roughness channel painted on the device and on the host
reading within one level per texel.
## A filtered extent moves when content reaches past the anchor's reach

A layer effect's extent is the node's anchor grown by the filter's reach,
out on a 64-unit grid; content that reaches past that grows the extent by
grid cells on that side only, so a bleed or a child larger than the slack
still moves the picture, while a filter's picture is meant never to
depend on how far the marks reach. Growing the extent symmetrically about
the anchor still differs on the device, turned and under the device clip.
A test should find a blurred panel at a stroke bleed of one and of seventy
giving the same pixels on the device.

## The engine's clock can be written by any holder of a copy

`Engine::time()` answers a const reference, but a copy of an `Animatable`
shares its cell and is writable, so `Animatable<float> clock =
engine.time(); clock = 5.0f;` writes the engine's own cell until the next
tick, and an animation started on the copy writes the same cell the engine
overwrites each frame; Python's `engine.time().set(3)` is the same. Only
the engine is meant to write the clock; a holder reads it and binds over
it. A test should find `engine.time().value()` equal to `elapsed()` after a
copy is assigned, and an animation on a copy leaving what `time()` reads
unchanged.

## Role rules carry palette colours by value

`Theme::styleSheet()` writes `palette.ink`, `ash` and `figure` into the
`h1`, `label`, `caption` and `.readout` rules as values, so a stage sheet
that sets `palette-ink` recolours wells and fills but not headings or
captions, and a sketch that wants another ink must stay its own theme. The
role rules are meant to ink with `var("palette-…")`, so a palette variant
is rules at the stage. A test should find a stage with `.sheet =
rule(":root").var("palette-ink", red)` setting an `h1` under it in red.

## A scope program cannot read properties

`compose::drawWith` takes `(Pen&, const Scope&)` with no paint context, so
a program under the stage captures the figure colour by value where a pen
program reads `PaintContext::vars`. A program under the stage is meant to
read the properties in force. A test should find a `drawWith` program
under a node that sets `var("x", red)` resolving `var("x")` to red.

## A reading's ink is a colour value

`compose::kit::Reading::ink` is an optional `material::Color`, so a lit
row cannot name `var("palette-ink")` and a readout reads the theme at
describe time, while `Bars::inks` takes a `Fill`. A test should find a
reading whose ink is `var("x")` setting its lines in x where x is in force.

## A whole-canvas Python draw gets no paint context

A sketch's `draw(self, pen)` under `@sketch(theme=…)` is handed no
context, so it rebuilds the theme each frame to colour the pen, while a
pen program reads the stage's properties through `context.vars`. A test
should find `draw(self, pen, context)` under a themed sketch finding
`palette-ink` in `context.vars`.

## No colour can be derived from a property

A `Fill` or `ColorTerm` holds a `var` reference but nothing derives from
one, so a bar at half the figure colour's alpha keeps a theme read; CSS's
relative colour syntax says the same property at a stated alpha. A test
should find a fill of property p at alpha 0.5 painting p's colour at half
alpha where p is in force.

## A text leaf in a column shrinks below its lines

Yoga floors a flex item at zero and compose adds no automatic minimum, so
a text leaf in an over-full column is laid out shorter than its lines
unless it states `flexShrink(0)`, while CSS floors an item whose minimum
is unstated at its content size. Giving a horizontal, non-clipping text
leaf that floor moves twenty device plates, so it waits on that adoption.
A test should find a leaf with its minimum unstated keeping its lines'
height and only `minHeight(0)` letting it shrink.

## A box in a column and text in a row shrink to nothing

A box in an over-full column and a text leaf in an over-full row shrink
to zero, while CSS floors a non-clipping item at its content, the longest
unbreakable word for text. A test should find a box with stated height H
in an over-full column keeping the smaller of H and its content height.

## A text leaf's alignment goes stale when only its parent changes

The rule that a text leaf aligned to stretch is laid out at its start
reads the parent's `alignItems` only when the leaf's own style is
written, so a describe that changes only the parent's alignment or
direction leaves the child where it was. A test should find a parent
changed from centred to stretched items moving its text child to the
start edge.

## The box page says a box's main axis is a row

`reference/pages/elements/box.md` says a box lays out as a row unless it
says otherwise, while `LayoutProps::direction` and the README say
column. A test should keep the page saying column.

## Three shader noises hash their lattice their own way

`material/.../Grain.slang`, `Noise.slang` and the compose kit's
`Sparkle.sksl` each carry a lattice hash of their own, while `Pop.slang`
reads the mixers core holds so that one noise means one picture on every
backend, which is the reason core holds them. A surface is meant to mean
the same noise wherever it is sampled. A test should find the three
bodies sampling the core mixer and a grained plate byte-identical across
the host and device paths.

## Core links measure, which links motion, which links core

`SigilCoreReconcile` links `SigilMeasure`, which links `SigilMotionTime`
only so `Stopwatch.h` can spell a chrono alias, so the three lowest
libraries form a cycle and no one of them can be built or read alone. A
duration is meant to be core's own word. A test should find the link
graph acyclic, with `core::Duration` spelled by measure and motion alike.

## Three world suites that need a device carry no gpu label

`Instancing`, `PresentedOnQueue` and `SurfaceRefusals` open the device
and are missing from world's gpu suite list, so a machine without one
fails them instead of skipping. A test should find every suite that opens
a device labelled gpu.

## Two wall-clock cases run in every plain ctest pass

`ComposeTextShadow.AChangingGlowCosts…` and
`SkiaGradient.AManyStopRampMaskCosts…` assert a time ratio and carry no
timed label, so a loaded machine fails them for no defect. A time claim
is meant to be a bench arm, or at least labelled timed. A test should find
no unlabelled case asserting on a clock.

## hangingPunctuation("allow-end") hangs as force-end does

Weave hangs a mark wherever it ends a line, while CSS's `allow-end` hangs
it only when the line would not otherwise fit. A test should find a line
that fits with its stop inside the measure not hanging the stop under
`allow-end`.

## fontVariationSettings merges axes where CSS replaces the list

The verb replaces only the axes it names, while CSS's
`font-variation-settings` replaces the whole list. A test should find a
later `fontVariationSettings("\"wdth\" 80")` under an inherited
`"GRAD" 50` dropping GRAD, or the merge stated as the rule in the verb's
own text.

## Weave has no automatic optical sizing

CSS's `font-optical-sizing: auto` sets a variable face's `opsz` to the
type size; weave has no such door, so `fontOpticalSizing` does not exist.
A test should find a face with an `opsz` axis set at 48 px shaping with
`opsz` 48 under the automatic setting.

## A key-index query inside a memo can read a freed node

`Composer::bounds`, `paragraphLayout`, `units` and the lookups beside
them read the key index directly, so a memo's component called during a
reconcile can be handed a node retired earlier in the same pass; only
`beatsOf` is guarded. A node that no longer stands is meant to answer
nothing. A test should find a memo calling `bounds("x")` on a sibling
retired in the same render getting nothing, clean under the address
sanitizer.

## Two memos that feed each other re-describe every frame

A memo that reads x and writes y beside one that reads y and writes x
re-describe once per frame for ever, bounded but unsaid, while a memo
that writes what it read is reported once; the same holds for a memo
whose own output moves the node its `beatsOf` query reads. A cycle
between memos is meant to be said once. A test should find such a pair
reported once and no longer re-describing.
