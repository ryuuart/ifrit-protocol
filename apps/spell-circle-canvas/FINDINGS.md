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

## A registry run of the window lane reads a sketch's rate off whatever the last selection left running

`Grimoire --window-bench` over the whole registry, one process for every
sketch, reads a cheap static page at anything from one frame a second to
the refresh rate with under two milliseconds of work, and the same page
alone reads the refresh rate every time; the lane driving it
(`scripts/sigil/bench.py`, `subprocess.run` with its output captured)
also stood down 107 sketches as presenting no frame in thirty seconds
that present at once on their own. A row is meant to be the sketch on
screen and nothing else. A test should select a cheap static page twice
in a row in one process and find both rows within the band of the page
alone, and a lane run over a handful of stems should stand none of them
down.

## A narrowed plate run clears the plates it did not render

`sigil.py plates --tier device --scenes a b` empties
`build/plates_Release/device/gpu/` before it renders, so one narrowed run
deletes the plates another run just took. A narrowed run is meant to
leave what it did not render. A test under `scripts/test` should run a
narrowed sweep beside a plate it did not name and find that plate still
there.

## A value-keyed texture shares a tree whose custom node is keyed

`compose::Textures` judges two trees the same through `sameDescription`,
which treats two `custom(key, …)` nodes with one non-empty key as equal
because that is the reconciler's per-position contract, so two different
maps that draw through the same custom key in the same shape would be
handed one texture. The door is meant to paint once per distinct picture.
A test should hand the door two trees that differ only in the program
behind one custom key and find two textures, or find the door refusing to
share a tree holding a keyed custom.

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

## The triplet comment still explains shared TBB by a port that is gone

`cmake/triplets/arm64-osx.cmake` explains its shared oneTBB rule by the
vcpkg usd port, which the manifest no longer carries; the rule holds
because shared is the one linkage oneTBB supports, and the comment
should say so. Editing the triplet changes every port's ABI hash and
forces a full dependency rebuild, so the reword lands with the next
intentional triplet change.

