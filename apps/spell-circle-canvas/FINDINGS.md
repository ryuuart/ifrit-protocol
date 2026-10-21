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

## The Syphon receiver patch waits for the registry push

The port patch `001-receiver-outlives-its-port.patch` for Syphon, which
gives the message receiver's port context retain and release and
releases the port on invalidate, is committed locally in the registry
checkout as syphon 2025-10-06#1 and is not pushed, and the baseline in
`vcpkg-configuration.json` is not bumped; the patched framework was
swapped into the build tree for the test runs and a manifest install
restores the unpatched one until the bump. Once pushed and bumped,
`io_frames_delivery_in_two_processes` and repeated `FramesDelivery` runs
under load should never crash.

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

## The decoration contract does not say which marks stand on the outline

The decoration contract in `sigilcompose/core/Shape.h` says every
decoration is drawn across an outline and measures bleed from the box,
while `Slice`, `Brackets` and `TickRail` paint against the box, and the
schemes that plainly stroke along the outline in brush and kit (the path
formats, `Brush`, `Stamps`, `Art`, the adaptors) do not declare
`staysOnOutline()`, so a shaped node carrying one still pays a box-sized
layer. The contract is meant to say a mark is either on the outline or on
the box, and every outline mark to say so. A test per scheme should find
the layer of a shaped node carrying it equal to the shape grown by its
bleed, and a shaped node carrying `Brackets` keeping its box.

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

## embossed_foil moved by a few scattered specular pixels before this pass

`embossed_foil` renders 87 pixels, five levels at most, away from its
device plate, and its hash differs from one render to the next, with every
mechanism this pass touched switched off one at a time; it was byte-
identical two days earlier. The plate is meant to be a pure function of
scene time. A test should render it ten times and find the renders
identical, and a bisection over the commits between should name the one
that moved it and pin its picture in that library.

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

## The triplet comment still explains shared TBB by a port that is gone

`cmake/triplets/arm64-osx.cmake` explains its shared oneTBB rule by the
vcpkg usd port, which the manifest no longer carries; the rule holds
because shared is the one linkage oneTBB supports, and the comment
should say so. Editing the triplet changes every port's ABI hash and
forces a full dependency rebuild, so the reword lands with the next
intentional triplet change.

