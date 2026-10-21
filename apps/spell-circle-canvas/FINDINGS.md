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

## The reporting seam stops at Compose, Material and Weave

Every warning in the core, geometry, motion, protocol, Scry, Skia and
sketch libraries goes through `sigil::core::report(library, message)`,
while Compose still prints through `SkDebugf` with its own prefixes,
Material through `SkDebugf` and `fprintf`, and Weave as before. A
reporter installed with `installReporter` should hear every library's
warning and nothing should reach stderr. A test per library should
install a reporter and find the warning there.

## The window lane refuses a locked screen for a reason that no longer holds

`scripts/sigil/bench.py` refuses to run the fps lane behind a lock screen
because a hidden window's work ran several times slower after half a
minute; the lane now keeps the process user-facing for the whole run, and
rows taken behind the lock screen read the refresh rate at normal
priority. Either a covered or locked window's row is as good as the
unlocked one and the refusal goes, or the refusal states its real reason.
A test should present one cheap page unlocked and locked and find the two
rows within the band, or find the lane naming why a locked row does not
count.

## The window lane's window is not the size asked for on this display

With the main display at 3840 x 1080 and one pixel a point, a window
asked for at 900 x 700 at ratio 2 came back as `window=900x600@2`, and
one asked for at 1440 x 900 as `1440x900@2`; the row reports what the
window really was and the lane says when that differs from the ask, but
a window inside the display's pixel bounds is meant to come back at the
size asked. A test should ask for a window within the display's bounds
and find the row reporting that size.

## A filter over an ink-sized leaf takes the box, so a blurred stroke on a page-sized box pays a page-sized layer

A node whose marks stay on its outline is bounded by its ink, except that a
layer effect over it measures the box, because a device blur's result moves
with the extent its layer is given: a layer at the ink grown by the filter's
reach keeps the speed (`BM_Draw_BlurredStrokes` on a page-sized box with the
layer at the ink runs as the fitted box does) but moves rota_convocationis by
one level and painted_fields by three. The layer is meant to be the ink
grown by the reach and the picture meant to be the box's; the way there is
a layer whose origin stands on the grid the box's layer stands on. A test
should blur a stroke on a page-sized box with the layer at the ink and at
the box and find the pixels byte-equal at blur 12, 24 and 48.

## A device page is read back to host memory by any caller that only wants its size

`media::deviceImage(frame, nullptr)`, and so `Texture::frameAt` and
`material::skia::image(texture)`, read a device-standing page back to host
memory through `skia::readbackPixels`, which waits by polling with
`sleep_for`; `world::samplingOf` no longer calls them, but any caller that
wants only metadata still pays a readback with sleep granularity on top.
The doors are meant for pixels and a readback meant to block only as long
as the device needs. A test should ask a device-standing texture for its
size and device image and find `DeviceBinding::image` never called, and a
readback of a finished paint returning within one paint's device time.

## A Python sketch's context drops the session's bake runtime and textures

`State::update` in `src/sketch/python/host/Session.h` copies neither
`bakeRuntime` nor `textures` from the `Scene2DContext` it is handed, and
`State::context()` rebuilds the context without them, so a Python sketch's
`bakeScene3D` draws on the process's runtime even in a session opened on
its own, and the session's value-keyed texture set is out of its reach. A
Python scene2d sketch is meant to bake where a C++ one in the same session
bakes, and to reach `ctx.texture`. A test should open a Python sketch in a
session with its own runtime and find its bake drawn through that runtime,
and hand `ctx.texture` two equal trees and find one paint and equal
textures.

## A keyed operator compares equal on its key alone, so the texture door cannot tell two programs apart

`drawWith(key, …)` and `stamp::ByLane{.key = …}` compare equal on the key
through `Operator`'s type-erased equality, and a keyed text effect carrying
an ad-hoc program does the same, so `compose::Textures` hands one texture
to two trees whose operators run different programs under one key. The
door is meant to share only what it can prove is one picture. A test
should hand the door two trees that differ only in the program behind one
`drawWith` key and find two textures; the way there is a seam on
`Operator` and on the text-effect key saying its equality is the author's
word, which the door treats as equal only to the same node, as it does for
a keyed `custom`.

## The stock strokes understate a mitred corner's reach

`PathFormat::bleed()` answers half the width for a centred stroke and the
width for an outer one, and `Border`, `LayeredBrush` and a mitred
`lines::Line` answer the same way, while their default join is a mitre at
the renderer's limit of four, which reaches twice the width centred and
four times outside, and a square cap reaches a further seven tenths of
the width; `detail::CoverageMark` in `core/paint/MaterialEffects.h`
declares it stays on the outline while its stroke step's bleed is the
width under the same mitre. A bleed is meant to hold the mark's widest
reach, as the material stroke and the ribbon already do, so a bake or a
layer holds the mitre tips and the marks can stand on the outline under
their defaults. A test should stroke a star whose tips touch its box with
`brush::solid` under `Cache::Texture` and find it pixel-equal to its live
paint, and find a faded shaped node with an acute corner wearing a
material stroke opening a layer that holds the tip; the fix grows bake
extents and moves plates, with the cause stated.

## A contour walk states no extent

`ContourWalk` has neither `bleed()` nor `reach()`, so a walk's stamps are
clipped by any cache sized from the box once they overhang it; it is
meant to declare its reach as `Scatter` does with `bleedPx`. A test
should walk art wider than the box's margin under `Cache::Texture` and
find it pixel-equal to the live paint.

## material::over crashes on an operand with no recipe

`composed()` in `src/common/material/core/Combine.cpp` asks an operand's
recipe whether it has a target without checking for a recipe, so
`over(program, from(Color).surface({...}), noise)` dereferences null. An
operand with no recipe is meant to have no body for any target, so the
composition is skipped and the stack falls back to the slotted `over`
recipe. A test should call `over()` with a `from(Color)` operand and find
a material.

## A gradient given as a surface channel reads flat on a World body

A `linearGradient` passed as `.metallic` or `.roughness` through
`.surface({...})` on a `cylinderPanel` renders as one value, while the
same gradient painted as a texture and slotted at the two channels shows
its sweep. A material channel is meant to vary over the body's texture
coordinates. A test should give a World body a gradient roughness channel
and find different highlight widths at its two ends.

## An emission map ignores its repeat and scale

A texture built from `pattern::gridLines(...).texture()` with
`.tile(Repeat).uv(Scale(...))`, wrapped in `material::image()` as a World
mesh's emission map, draws one tile stretched over the whole card whatever
the scale, while the floor's base-colour slot honours the same settings.
A test should give an emission map a 5 by 7 repeat on a unit quad and
count 35 cells.

## A colour function over half-float pixels clamps before it scales

`Filter::brightness(0.2)` on an image element holding an F16 image made
from `halfFloatPixels` draws the same picture as on the 8-bit readback,
because the stored values past one are clamped before scaling. A colour
function over float content is meant to scale the stored value. A test
should apply brightness(0.2) over an F16 texel of 4.0 and read 0.8.

## A closed contour's band carries a spur at its start vertex

`bandRegion` over a closed hexagon leaves a radial tick at the start
vertex at every formation, which contour_poses shows in its rail rows. A
closed contour's band is meant to close with no seam. A test should take
the band of a closed hexagon at each formation and find no pixel outside
the band's offset envelope at the start vertex.

## A class read only by an operator is warned about as unused

`styleClass("chosen")`, read only by `outline::Hull`, triggers the warning
that it names a class no rule speaks about. A class consumed by an
operator's `styleClass` field is meant to be a legitimate selector. A test
should assert that a class an operator reads raises no warning.

## hitTestable(false) does not take a subtree with it

A keyless leaf under a node opted out with `hitTestable(false)` still
answers a hit test with the node's inherited key, so composer_queries'
marker under the probe point answered as the probe itself. The verb is
documented as CSS `pointer-events: none`, which inherits; an opted-out
slot or overlay is meant to take its keyless contents with it. A test
should put an unkeyed dot inside an opted-out slot over a keyed target and
find the hit test answering the target.

## Content given to a slot before its tree mounts is dropped

A Python `setup` that calls `composer.renderSlot("readings", ...)` and then
returns the tree holding `slot("readings")` is warned that no slot has that
name and the content is dropped, because the returned tree is rendered
after setup; python_instrument_board renders its page first to avoid it.
Content given to a slot before the tree that holds it mounts is meant to be
kept and shown once it mounts. A test should fill slot "x" in setup, return
a tree containing it, and find the content at frame zero.

## The declared fill type omits a described colour animation

`Element.fill` is declared as `MotionFillLike`, which leaves out
`ColorAnimatable`, so `fill(motion.animate(from_=colour, to=colour, ...))`
fails strict typing although the binding accepts a described one. The
declaration is meant to list what the binding accepts, or the binding to
refuse it as it refuses a live colour. A typing fixture should fill a box
with a described colour animation and check clean while a live
`ColorAnimatable` stays an error.

## Flocking weights are in incomparable units

`applyFlock` in SigilMotion's `Forces.cpp` sums separation as the inverse
of distance, alignment as a velocity difference and cohesion as an offset,
so the default `Flocking{1.5, 1, 1}` leaves separation around a hundredth
of cohesion and flocking.cpp needs separation weights in the hundreds to
hold birds apart. The three are meant to be weights on one another whose
ratios shape the flock. A test should step a flock under default weights
and find the minimum pair spacing above a fraction of the reach.

## A family name is still read by hand in four places

`src/sigilweave/testing/Plate.cpp`, `src/common/compose/core/style/FaceChoice.cpp`
(twice), `src/common/draw/Text.cpp` and `src/common/python/skia/Values.cpp`
each call Skia's `getFamilyName` themselves rather than
`weave::fonts::familyName`, so a face adopted under another family answers
its file's name there. Every reader is meant to answer the one name. A
test per site should find it answering what `fonts::familyName` answers;
`Face::familyName` in the style target below the fonts target stays as it
is.

## A clip shape that throws leaves a stack's composite frame open

`Stack::remake` in `src/common/draw/Stack.cpp` opens the composite's
frame and `drawLayer` pushes the pen before it runs a layer's clip shape,
so a shape that throws escapes with the frame open and the push unpopped;
the Python binding catches inside the shape and is safe. The remake is
meant to end the frame and rethrow, as `Layer::bake` does. A test should
find `Stack::image` throwing for a layer whose clip throws, and after
`noClip()` the next composite succeeding with the pen's push and pop
balanced.

## The triplet comment still explains shared TBB by a port that is gone

`cmake/triplets/arm64-osx.cmake` explains its shared oneTBB rule by the
vcpkg usd port, which the manifest no longer carries; the rule holds
because shared is the one linkage oneTBB supports, and the comment
should say so. Editing the triplet changes every port's ABI hash and
forces a full dependency rebuild, so the reword lands with the next
intentional triplet change.

