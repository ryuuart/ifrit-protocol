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

## A bevel or inner shadow on a glyph or shape boundary blurs its band on every replay

Only `Boundary::Coverage` holds its band masks (`HeldCoverageMasks`); a
bevel or inner shadow on a glyph or shape boundary blurs its band again
at every replay, so a stationary edge treatment beside a changing ink is
cut again on each light-only update. Holding the masks for every outline
reuses the edge coverage across ink changes; every bevel plate moves
when it lands. A test should replay a bevelled leaf under a changing
shader ink and find the band blurred once.

## The sparkle stretched over a passage is one streak, not points of light

An ink maps its unit square onto the passage's box and the program
cannot see that box's aspect, so on a wide word a sparkle cell spans
about a third of the word and a box holds about ten cells; the SIGIL
wordmark shows one streak across its S where points of light across the
word were meant. The ink would have to hand the program its box size,
which `TextInk.cpp` pins to one by one; that is core or material work,
not the kit's. A test should ink the SIGIL wordmark with the sparkle at
its capture moment and find at least three separate lit regions on at
least three glyphs.

## Held coverage masks are remade on the CPU at each settle

`HeldCoverageMasks` remakes a part's cast mask on the CPU raster, blur
included, whenever the density changes, so every zoom settle
re-rasterises and re-blurs each held mask at full size on the processor
even on a device, where the mask should be made and blurred on the
device. A settle-style Graphite arm of `BM_Draw_CoverageSheet_TurningChild`
that alternates density should cost within a stated ratio of the steady
arm; the change moves every device plate that uses held coverage.

## The Library of Babel changes under GPU load

`borges_library.py` renders 25 pixels differently on thin antialiased
strokes of its right bookshelf, by at most seven levels, while another
process loads the GPU, and byte-identically alone; the bookshelf is
under a `Cache.Texture` canvas, so the compose texture bake or Graphite
under contention is where to look. A capture is a pure function of
scene time. A test should render the plate under a GPU load and find it
byte-identical. `chrome_type` moved by up to five levels in its bevel
glow during the same runs and should be read the same way.

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

