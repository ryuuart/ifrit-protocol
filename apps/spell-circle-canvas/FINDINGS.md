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
mutable global written in `setup()`; `stroke_atlas` keeps a process-wide
`Type&` singleton holding three typefaces, which additionally SHADOWS the
library's `type()` and forces every text call in the file to be spelled
`sigil::compose::text(...)`.

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

## The window ledger keys its rows by the first word of a display name

`Sketchbook --window-bench` prints `WINDOW <name> …` with the FILED name,
which carries spaces, and `scripts/app_fps_ledger.py` matches `(\S+)` —
so `aero desktop` is stored as `aero` and the rest of the name is parsed
as if it were key=value pairs. Fifteen of the hundred rows in
`bench/app_fps_Release.json` are truncated this way, and two sketches
whose filed names shared a first word would silently share one row.

Intended: a row is keyed by the registry stem, which is what `--sketch`
takes and what the file on disk is called, and carries no spaces by
construction.

Assert once fixed: every key in the baseline is the stem of a file in
`sketches/`, and a sketch filed under a two-word name round-trips
through the ledger under its stem.
