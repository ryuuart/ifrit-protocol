# Findings

Defects found while working, stated as what the code does, what it was
evidently intended to do, and what a test should assert once intent is
restored. A work queue: delete an entry when it is fixed, and delete this
file when it is empty.

## A node carrying rotate(±90°) bakes at the wrong resolution

`eva_magi_defense`'s `art()` promotes each mark to `Cache::Texture`
individually, except at ±90°, where it falls back to `Cache::Auto`
because the bake comes back non-uniformly resampled — on the LEFT SIDE
BARRIER pill (196×33, rotate −90) the type is destroyed. 0, 45 and 180
are all clean, so the failure is exactly the quarter-turn.

Intended: a texture bake samples at the device resolution the node is
drawn at, whatever transform stands above it.

Assert once fixed: bake a node at each of 0/45/90/180/−90 and compare
each against the same node drawn uncached; the quarter-turns must agree
to the same tolerance as the others. `eva_magi_defense` then drops its
`bakeable()` guard.

## Six mutable function-local statics survive a reload

`fallout2_charsheet` writes six function-local statics from `setup()` and
reads them from every describe. A second instance, or a reload that
reuses a loaded dylib's statics, sees another sketch instance's probe
results. `winamp_base` has the same shape in `monoEm()`, `boldEm()` and
`pushSlots`'s `lastNow`/`lastSel`; `cde_motif`'s `g_liveColors` is a
mutable global written in `setup()`.

Intended: a sketch's state lives on the sketch instance, so two live
sessions of one sketch cannot see each other's.

Assert once fixed: open two sessions of one sketch in one process with
different probe inputs and render both; neither plate may change when the
other is opened.

## web_panel's settled() is a real wall-clock deadline in setup()

`steady_clock::now() + 15s`, `sleep_for(16ms)`, 8 stable ticks. On a
machine where the engine takes longer than 15 s the sketch renders
`unavailable(...)` — a different plate from the same code, decided by
machine speed. The header explains why a settled page is reproducible and
says nothing about this.

Assert once fixed: the settle loop counts engine ticks rather than
seconds, and a run under an artificially slowed engine produces the same
plate as a fast one.

## ds2_bench quantises onto a capture boundary

`quantizeTime(6.0f)` lands on multiples of 1/6 s, and its declared moment
(2.5 s) is one of them, as is the quick tier's uniform 2.0 s. The scene
clock is a sum of steps of 1/60, which is not exactly a multiple of 1/6
in float, so a tie-break decides whether `uTime` is one quantum or the
next — a shift of every scanline by 1.17 px across the 984×690 panel and
two cards. Stable across three renders of one build; not stable by
construction.

Assert once fixed: the declared moment is not a multiple of the
quantiser's period, and the plate holds under `--stability`.

## Numbers a sketch measures about itself are not routed through ctx.measured

`ctx.measured` (`sigilsketch/canvas/Sketch.h`) has zero users anywhere in
`sketches/`, while `chevreul_circle` draws pixel-readback deviations into
its own plate, `psx_doom_fire` draws a measured draw rate, `matrix_rain`
draws a measured glyph count, `substance_swatches` draws the machine's
SDK version, `genesis_fire` and `hitman_verlet` pin their own µs figures
against `ctx.deterministic` by hand, and `minard_1869` prints numbers
measured off its own geometry.

Intended: a self-measured number reaches the plate through `ctx.measured`
so a deterministic capture substitutes a fixed value.

Assert once fixed: a deterministic render of each of those sketches is
byte-identical to a second one taken on a machine of a different speed.

## Each public compose header is not known to be self-sufficient

`spacejam_1996` carried an include-order workaround: `Material.h` first,
because `Decorations.h`'s `Wash` holds a `Material` by value while
`Compose.h` only forward-declares it. `brush/Decorations.h` now includes
`core/Material.h` itself, so the workaround is gone — but nothing pins
the property.

Assert: a translation unit that includes exactly one public compose
header, first and alone, compiles. One test per header, generated from
the header list.

## blur_falloff does not hold 60 FPS at its own canvas

`--bench` reports p99 ≈ 18.8 ms at 1080×372, and one node is all of it:
the rack-focus panel re-runs a 240×240 blur every frame because its
sigma breathes, at 17.4 ms of a 18.0 ms frame. The other three panels
hold a fixed sigma and cost hundredths of a millisecond each, so the
cost is a full-resolution Gaussian at up to 14 px of sigma re-evaluated
per frame rather than anything about the sheet.

Intended: a sketch holds 60 FPS at the canvas it declares, and a blur
whose only changing input is one scalar does not re-read every source
pixel to answer it.

Assert once fixed: `--bench --sketch blur_falloff` verdicts PASS, and
the rack panel's per-frame cost tracks the panel's area rather than its
area times the sigma.
