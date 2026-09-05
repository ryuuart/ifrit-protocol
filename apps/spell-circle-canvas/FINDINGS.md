# Findings

Defects found while working, stated as what the code does, what it was
evidently intended to do, and what a test should assert once intent is
restored. A work queue: delete an entry when it is fixed, and delete this
file when it is empty.

## A node carrying rotate(±90°) under a recording ancestor bakes in local space

`eva_magi_defense`'s `art()` promotes each mark to `Cache::Texture`
individually, except at ±90°, where it falls back to `Cache::Auto`
because the bake comes back non-uniformly resampled — on the LEFT SIDE
BARRIER pill (196×33, rotate −90) the type is destroyed. 0, 45 and 180
are all clean, so the failure is exactly the quarter-turn.

Resolution is not the cause: every raster decision reads the transform's
singular values, and a quarter turn reports a scale of one. The cause is
the SPACE the bake is held in. The device-space bake, exact at any angle,
is taken only at recording depth zero; the local-space bake bakes the
node's local bounds and blits them through the rotation with linear
sampling. Eva's pills stand under `camera(...)`'s `box().inset(0)` and
`art()`'s `box().inset(0)`, and any static container with children takes
the automatic picture branch and bumps the recording depth, so every pill
falls to the local bake. At −90° the local axes swap onto device axes and
rotating a 196×33 rect about its centre (98, 16.5) puts a half-texel
offset onto the axis carrying the type's stroke detail; 0 and 180 land it
on the low-detail axis, and 45 shares it.

Intended: a texture bake samples at the device grid the node is drawn on,
whatever transform stands above it.

Awaiting the owner's decision: should a subtree containing an explicit
`Cache::Texture` veto its ancestor's AUTOMATIC `Cache::Picture`, so the
node reaches recording depth zero and takes the device-space bake? The
alternative is to expand the local bake rect to the pre-image of an
integral device rect. Separately worth the owner's eye: the device-space
bake is unreachable for almost any nested node.

Assert once fixed: a text node at each of 0/45/90/180/−90, NESTED ONE
LEVEL UNDER A STATIC CONTAINER, compared against the same node drawn
uncached; the quarter-turns must agree to the same tolerance as the
others (a root-level node passes today and proves nothing). Then
`eva_magi_defense` drops its `bakeable()` guard and its plate is rebased.

## Eight mutable function-local statics survive a reload

`fallout2_charsheet` writes six function-local statics from `setup()`
(`bodyEm`, `titleCondense`, `engravedCondense`, `bodyRise`, `titleRise`,
`engravedRiseFrac`) and reads them from every describe. A second
instance, or a reload that reuses a loaded dylib's statics, sees another
sketch instance's probe results. `winamp_base` has the same shape in
`monoEm()` and `boldEm()`, and `pushSlots`'s `static int lastNow,
lastSel` is a change-detector in the frame loop that misbehaves even in
one session after a reload.

Intended: a sketch's state lives on the sketch instance, so two live
sessions of one sketch cannot see each other's. The free helpers
(`bodySize()`, `engravedRise()`) become members; `lastNow`/`lastSel` join
`volSprite`/`balSprite` on the instance.

Assert once fixed: open two `CanvasSession`s of one sketch in one process
with different probe inputs and render both; neither plate may change
when the other is opened.

## Two sketches print numbers they measure about themselves, and two route them by hand

`ctx.measured` (`sigilsketch/canvas/Sketch.h`) is the `--stability`
contract: a self-measured number reaches the plate through it so a
deterministic capture substitutes a fixed value. `chevreul_circle` draws
pixel-readback deviations into its own plate with no deterministic branch
at all; `minard_1869` prints numbers measured off its own path-ops
geometry, which will move under a Skia upgrade. `psx_doom_fire` and
`genesis_fire` pin their own figures against `ctx.deterministic` with a
hand-written ternary — correct, but the routing belongs to `measured`.

Intended: chevreul and minard through `ctx.measured(value, pinned)`, with
chevreul's pass/fail colour pinned to the "exact" value rather than to
zero; psx and genesis replace the ternaries and drop the members.

Assert once fixed: `plate_ledger.py --stability N` holds over the four
scenes. Do not take this sweep in the same pass as the eva rebase above,
or a genuine mover and a pinned number are indistinguishable.

## blur_falloff does not hold 60 FPS at its own canvas

`--bench` reports p50 ≈ 21 ms at 1080×430, and one node is all of it: the
rack-focus panel binds `maxSigma` on `Effect::blur(dofMap(), 0)`. A
declared range of 0 holds no pyramid, so the bound sigma builds every
Gaussian pass at every paint — the one case the held pyramid does not
cover, and the header says so.

Intended: the sketch declares the largest sigma its binding reaches —
`Effect::blur(dofMap(), kMaxSigma)` — and the breathing sigma rides the
held passes.

Assert once fixed: `--bench --sketch blur_falloff` verdicts PASS, and the
rack panel's per-frame cost tracks the panel's area rather than its area
times the sigma.

## daemon_console does not hold 60 FPS at its own canvas

`--bench` reports p50 ≈ 30 ms at 900×640, and the frame is drawing
rather than describing: the full-screen scanline-and-refresh-band overlay
is one live SkSL shader over every pixel of the canvas, re-run each
frame, and it is the only node in the sketch that costs anything like
that. The rest of the console — the feed's row mounts, the bound meters,
the caret — prices in fractions of a millisecond, exactly as its own
header claims.

Intended: a sketch holds 60 FPS at the canvas it declares, and a scanline
overlay whose only per-frame input is a phase is a texture crept by a
bound translate rather than a per-pixel program. The shader is
`f(p.y, uTime)` and splits into a `Pattern::tile` scanline band with the
live `Pattern::offset(x, y)` that already exists, plus a 128-px gradient
box `.translateY(&sweep)` under `Cache::Texture`, both under `kScreen`;
`eva_magi_defense`'s CRT vignette is the idiom. No library gap.

Assert once fixed: `--bench --sketch daemon_console` verdicts PASS, and
no node in the report is a full-canvas live paint. The band's phase
quantises to a texel, so the plate moves; rebase with the cause.

## eva_magi_defense paints a full-canvas bloom live, and re-describes its funnel

`--bench` reports p50 785 ms at 1920×1080: `Effect::phosphorBloom` is
applied over the whole assembled picture, on a subtree that is live, so
twenty-four taps per pixel run over every pixel of the canvas every
frame. Underneath, the front advances by re-describing the funnel slot
six times a second with a fresh `rampMaterial(front)`, so `propsEqual`
misses and everything under the bloom dirties.

The owner wants the full-canvas glow KEPT, made cheap, with a hue-shifted
falloff — which the effect now carries: `phosphorBloom(radius,
threshold, intensity, chroma, hueDrift, tail)`, with a negative
`hueDrift` taking warm halos toward red and cool ones toward cyan-green,
and a layer effect on a `Cache::Texture` node bakes with it once.

Intended: the glow sources (rims, numerals, type) on their own node under
`Cache::Texture` with the bloom on that node, so the halo is paid once
per change rather than once per frame; and one static ramp panned
through `Material::offset`'s bound channel, so `renderSlot` runs only on
the five `fall` events. Riders for the owner: continuous front or the
6 Hz step (recommended continuous — the pan makes re-describes free).

Assert once fixed: `--bench --sketch eva_magi_defense` verdicts PASS with
p99 inside the budget; the declared moment (2.5 s) precedes the sweep
(3.0 s), so the plate must not move under the ramp change alone.

## Two of the historical plates do not hold 60 FPS at their own canvas

`chaucer_astrolabe` reports p50 ≈ 37.7 ms at 2400×1600 and
`nightingale_coxcomb` p50 7.1 ms with p99 27.7 ms at 1900×1032 — the
second is a spike rather than a level, so one frame in twenty costs four
times the median. Chaucer is a level: nine `Cache::Texture` passes under
one live `rotate(&reteRot)` over the whole rete subtree inside a clip
band, so no descendant bake is stable under it. Nightingale is a cache
being invalidated: the `custom()` raw-Skia leaf redraws unconditionally
over a base already cached.

Intended: a sketch holds 60 FPS at the canvas it declares. Both are
large sheets over expensive material stacks — but a declared canvas the
sketch cannot present at is a different statement from a sweep that
takes a while. Awaiting the owner: does "60 FPS at its declared canvas"
bind a plate whose subject is the sheet's size as strictly as a live
sketch? Recommended: no for chaucer (an explicit "plate, not live"
registry mark that `--bench`'s verdict reads, never a timeout override),
yes for nightingale (diagnose the re-bake; if it is a composer rule it
joins `ComposeTestContentCore.cpp`).

Assert once fixed: `--bench` verdicts PASS for `nightingale_coxcomb`
with p99 inside the budget rather than the median alone, and chaucer's
mark is read by the verdict.

## The environment's ground projection is carried and never read

`world::Backdrop::groundRadius` and `projectionCenter` are extracted
into the view and written to a USD dome light, and no tier reads either:
the sky is drawn at infinity on both. A set whose camera moves through
it therefore sees a horizon that does not shift, which is the one thing
ground projection exists to fix.

Intended: past zero, the panorama is treated as a sphere of that radius
standing on the ground rather than one at infinity, so the ray a pixel
reads is the intersection of the view ray with that sphere.

Assert once fixed: photograph one set from two camera positions a
radius apart with `groundRadius` set, and the horizon must land at two
different heights in the frame; with it at zero the two must agree.

## A variant re-draw is lit on the host tier and unlit on the device

`world/frame/CpuGeometry.cpp`'s variant overlay sets `over.lit = true`
and states in a comment that the pass's own lights are what the overlay
stands under; `world/diligent/Geometry.cpp` draws the same overlay with
`lit=false`, so the device shows a flat colour where the host shows a
shaded one. The gap widens now that the lit sum ends at a tone curve:
the host's overlay is curved and the device's is clipped.

Intended: one answer on both tiers, and the comment says which — a
variant surface is a colour laid over the bodies a selector names, and
it stands under the pass's lights.

Assert once fixed: render a Variant pass on both tiers over a body under
one directional light and compare the two plates within the tier
ceiling; the overlay's shading must agree, not just its coverage.

## The paragraph paint's whole-page render is judged by nothing

`weave_paint_test` rendered two thousand words under three runtime
shaders onto a 1200×900 surface and asserted that one pixel differed
from the background. The layout half of that case and the
preset-resolution half are now their own cases; the render left ctest,
because byte identity of a rendered page is the plate ledger's verdict
and not a unit test's. No sketch draws it yet, so nothing renders it.

Intended: a picture a test used to compare is compared by the tier that
owns pictures.

Assert once fixed: a canvas sketch lays a long paragraph under the
shader presets, and the full plate tier holds it byte for byte.

## Nothing checks that a README's sketch stems exist

Each library README points a reader at the sketches that draw the
feature beside it, and the stems it names are hand-typed: a rename in
the registry leaves a README pointing at a file that is not there, and
nothing fails. The compose README already gets this check for API
names; the stems get none.

Intended: a test resolves every sketch stem a README names against the
registry and fails on a stem with no file — a backticked
`^[a-z][a-z0-9]*(_[a-z0-9]+)+$` token that is not a test, bench or probe
target and whose paragraph mentions a sketch, study or studies must
resolve under `src/sketch/sketches/`; a cardinal within two lines of a
study list is refused (or the count deleted). It is the registry's test,
so it lives under `src/sketch/test/`, registered from
`src/sketch/CMakeLists.txt`, with a `--self-test` like the compose API
probe generator's.

Assert: the check passes over every README, FEATURES and TYPOGRAPHY
under `src/`, and fails when one stem is misspelt.

## A workspace row's runtime stays "not yet compiled" after it has compiled

`SketchCatalog` builds a row for a file opened by path with an empty
`kind`, because which runtime the file draws through is not known until
it has been built — and nothing fills it in afterwards: `learn()` takes
the canvas, the moment and the background off the running session and
leaves the runtime alone, so the inspector reads "not yet compiled" under
a sketch whose status strip says it is live. Evidently intended: a row
learns everything a session can tell it, the runtime included, the first
time that session runs. A test should open a file by path, drive one
frame, and assert the row's `kind` names the session's runtime.

## hitman_verlet's drag leader can never have a length

`stepPhysics` pins the hand to `dragTarget` as the last operation of the
constraint loop on every step of the drag phase. `dragTarget` itself is
not frozen — it eases from `dragFrom` to its destination over the 3.2 s
of the phase — but the pin puts the hand exactly on it each step, so the
hand and the target are the same point at every moment. The leader
`simulation()` draws between them — a wide casing, a narrow core and an
accent head over its last eighth, which the header names as one of the
things the pen draws — is a few pixels of round cap around the target
ring at every moment of the loop, and the accent head and the core's dash
are invisible inside it. The only surviving separation is `drawnWorld`'s
interpolant, a couple of pixels inside the 10 px casing.

Intended: the leader shows the pull, so it is drawn between where the
hand IS — its position before the pin resolves it, captured into a
member — and where the target has moved to.

Assert once fixed: at a moment inside the drag phase, the distance
between the captured hand position and `toStage(dragTarget)` is greater
than the casing's width, and the plate at that moment carries accent
pixels outside the target ring. Moves the plate; rebase with the cause.

## A sketch that fails to load leaves the window unable to load any other

Opening a sketch whose shaders fail to compile on the device (chaucer_astrolabe
before its latten body stopped naming a local `pos`) leaves Sketchbook
running but refusing every later sketch: the window stays up and nothing new
loads. The host evidently intends a failed load to be one session's failure,
with the next file opening as if the first had never been tried. A test in
the book's resident-session path should open a sketch that fails in its first
frame, then open one that does not, and assert the second renders.

## video_device_bench cannot open a hardware decoder for its own clip

Every arm of `video_device_bench` fails with FFmpeg's "Failed setup for
format videotoolbox_vld: hwaccel initialisation returned error" followed
by "no frame!", so `bench_ledger.py` reports the binary FAILED and the
baseline carries no entry for it, while `video_device_test` runs its
VideoToolbox case on the same machine. The bench evidently intends to
measure the native path on the encoder's own in-memory MP4.

Intended: the clip the bench encodes opens through VideoToolbox, or the
bench falls back to the worker path and reports the native percentage as
zero rather than failing.

Assert once fixed: `bench_ledger.py --benches video_device_bench` reports
the binary ran, and the native percentage it prints is non-zero on a
machine whose `video_device_test` VideoToolbox case passes.
