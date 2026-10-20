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

## Syphon's message receiver can be freed while a queued message is still delivered to it

A process whose Syphon publisher stops as a subscriber leaves can crash
in Syphon's message return callback: the receiver's port context carries
no retain or release and `invalidate` does not drain the shared
messaging queue, so a departure message still queued calls into the
freed receiver; load widens the window. A receiver should outlive every
callback queued for it. A draft port patch giving the port context
retain and release and releasing the port on invalidate waits in the
session's scratchpad (`tests-syphon/syphon-receiver-outlives-its-port.patch`)
for the registry; once it lands, `io_frames_delivery_in_two_processes`
and repeated `FramesDelivery` runs under load should never crash.

## A timed scene3d sweep frame can carry no pending wait under load

`DeviceSweep.ATimedScene3DFrameCarriesThePresentedPicturesWait`
(`src/sketch/plate/test/SweepTest.cpp`) failed in a parallel gpu run: under
load the frame carried no pending wait, while a timed scene3d frame is
meant to carry the presented picture's wait every time. The test should
hold under `ctest -j8` with the device loaded; the case asserts a wait,
not a duration, so the serial label is not its fix.

## Effects past the declaration limit get no key

Skia lets a context declare one hundred runtime effects; the effects a
sketch's own setup compiled, written down at exit and declared on the
next launch after the stock ones, are dropped with a message past that
limit and stay unkeyed, so their programs build on every first frame.
Every program a sketch draws is meant to be recorded and stood up. A
test should fill a store past the limit and find it saying which
sketches go unkeyed; the share of the hundred the stock effects use is
not yet measured.

## The window's input door has no test

`WindowInput` answers `session.pointer` and `session.key` on the window
as the headless host does, and nothing tests it, since a test needs a
running window. A `--shot` window run driven over the endpoint should
fire a function file's `mousePressed`.

## A change of bake density re-traces every coverage silhouette

`setBakeDensity` marks every node's paint dirty, so a zoom settle
traces each coverage silhouette again on the processor although the
trace is taken at the host's scale and the masks are now cut on the
device; the settle should leave the coverage trace alone. A test should
alternate the bake density and find nothing re-traced.

