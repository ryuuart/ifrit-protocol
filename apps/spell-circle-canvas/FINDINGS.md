# Findings

Defects found while working, stated as what the code does, what it was
evidently intended to do, and what a test should assert once intent is
restored. A work queue: delete an entry when it is fixed, and delete this
file when it is empty.

## Two draw sketches pin their self-measured figures by hand

`ctx.measured` (`sigilsketch/canvas/Sketch.h`, and its twin on
`sigilsketch/draw/Draw.h`) is the `--stability` contract: a self-measured
number reaches the plate through it so a deterministic capture substitutes
a fixed value. `psx_doom_fire` and `genesis_fire` pin their own figures
against `ctx.deterministic` with a hand-written ternary and a `pinned`
member — correct, but the routing belongs to `measured`.

The draw runtime is why they cannot: `DrawSketch::draw` is handed the pen
alone, and the figures — a sim rate, a build time — are per frame, while
the `DrawContext` that carries `measured` exists only in `setup`. The
member is the sketch's copy of the one bit `measured` reads.

Intended: the draw runtime gives a frame a way to route a figure —
`measured` on the pen the frame is handed, or a per-frame hook that
carries the context — and the two sketches call it and drop the members.
That is a change to `sigilsketch/draw/Draw.h`.

Assert once fixed: `plate_ledger.py --stability N` holds over the two
scenes, and neither file names `ctx.deterministic`.

## A live layer effect re-filters a static subtree every frame

`blur_falloff`'s rack panel declares `Effect::blur(dofMap(), kMaxSigma)`
and binds `maxSigma`, so its Gaussian passes are held and only the mix
between them moves — and `--bench` still reports the panel as a live
paint of twenty milliseconds at 240×240. The effect is live, so the node
is volatile, so it paints live: the content is re-rasterized into a fresh
layer every frame, and a fresh layer is a new image, so Skia's filter
cache never hits and both Gaussian passes run again over it. The held
passes save the DAG's construction and nothing of its execution.

Intended: a live effect over a subtree that is itself static is applied
to the subtree's BAKE — the content rasterized once under `Cache::Texture`
with the effect left out of it, and the filter run over that one image at
every blit. The image's identity holds, so a held pass is a cache hit and
only the mix re-runs. That is a cache tier in the compose painter, beside
the live-material memo: "the effect is the only thing live about this
node".

Assert once fixed: `--bench --sketch blur_falloff` verdicts PASS, and a
compose_core_test case draws a static box under a blur whose sigma is
bound, changes the sigma, and finds one bake and no re-record.

## A one-dimensional OCIO view costs a full-canvas shader per frame

`daemon_console` graded its whole canvas through
`ocio::exponent(1.08f)` as the composer's view transform, and that view
alone cost twenty milliseconds a frame at 900×640 — three times the rest
of the console together — so the sketch failed the 60 FPS gate for a
grade the eye cannot separate from none. The grade is gone from the
sketch; the cost stands in the library.

Intended: a view transform that is one-dimensional per channel — an
exponent, a gamma, a contrast — lowers to a table colour filter, one
lookup per channel, rather than to a per-pixel program; a
three-dimensional transform keeps the program.

Assert once fixed: a material_ocio test resolves `exponent(1.08f)` to a
filter whose per-pixel cost is a table lookup, and a 900×640 sketch
carrying it as its view holds `--bench`'s budget with the view on.

## nightingale_coxcomb's entrance bakes each wedge four times

`--bench` reports p50 3.4 ms and p99 18 ms at 1900×1032, and the frames
over the budget are the right wheel's entrance, which the gate's 1.5 s
warm-up lands inside: each wedge is a `Cache::Texture` node whose
entrance scales it from nothing about the hub, and a bake under a moving
scale is held in local space at the coarse scale ladder, so a wedge
growing through 0.25, 0.5, 0.75 and 1 is baked four times in the first
hundred milliseconds of its entrance — the litho fill, four shaders
including a three-octave grain, over the sector each time — and three
bands a month with months a hundred milliseconds apart put two of the
largest bakes into one frame. It is not a cache being invalidated: the
steady plate blits.

Intended: an entrance that scales a baked node costs one bake. Either the
ladder bakes ONCE at the scale the motion is heading for when that scale
is declared (a `from(a).to(b)` on the scale slot names it), or the
entrance is a mask reveal rather than a scale, which the plate's author
decides.

Assert once fixed: `--bench --sketch nightingale_coxcomb` verdicts PASS
with p99 inside the budget, and the wedge bakes in the run number one per
wedge.

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

## rota_convocationis slows with every sub-circle it generates

Each sub-circle the sketch generates toward the end of its sequence
adds to the per-frame cost rather than to a settled subtree, so the
frame time climbs with the count: the last sub-circles are visibly
slower to appear than the first. Evidently intended: a generated
sub-circle is described once and then cached like the ring it joins, so
generating one more costs one record and nothing per frame after it.

Assert once fixed: `--bench --sketch rota_convocationis` verdicts PASS
with the frame time at the end of the sequence within the budget and
flat against the number of sub-circles generated (the per-frame report
shows no node whose cost grows with the count).
