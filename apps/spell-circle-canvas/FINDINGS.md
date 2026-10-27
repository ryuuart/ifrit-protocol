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

## A filtered extent still moves when a bleed crosses a grid line

A layer effect filters an extent grown out to a 64-unit grid laid from
the node's origin, so a bleed that grows within one cell leaves the
picture alone, but a bleed that pushes the extent's edge across a grid
line still moves it. A filter's output is meant not to depend on how far
the marks inside it reach. A test should find a blurred node whose box
edge stands five units short of a grid line giving the same pixels at a
stroke bleed of one and of ten.

## The moving hold's bake lays its texels from its own bounds

The live window's hold of a turning subtree bakes from the subtree's own
bounds' origin, while every other local bake now lays its texels on the
node's grid, so a bleed that grows moves the held pixels under the
resampled blit. A test should find a held, turning node giving the same
blit at a stroke bleed of one and of four.

## The volatility walk runs every frame under a timer alone

While the engine runs only timers, the composer walks the whole tree for
volatile values every frame, though a frame with no motion lane running
needs only a scan of released scalars. A test should find the walk's
time falling to the scan's on a timer-only frame of a still tree.

## A drop shadow read from CSS text has no currentcolor

`readFilter("drop-shadow(2px 2px 4px)")` refuses the text because the
reader has no ink to paint the unnamed colour with, while CSS paints it
in `currentcolor`. A test should find `.filter("drop-shadow(2px 2px
4px)")` on an element painting the same as the shadow with the element's
ink named.

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

## The percentage indent in force does not reach a guest

A guest inherits the paragraph block in pixels, but an indent stated as a
percentage of the measure is not carried across the guest boundary,
while everything inherited is meant to cross. A test should find a guest
under a pen standing under a paragraph with a percentage indent drawing
the same as the guest stating that indent itself.

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
