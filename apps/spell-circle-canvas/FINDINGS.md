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

## daemon_console does not hold 60 FPS at its own canvas

`--bench` reports p50 ≈ 32.8 ms at 900×640, and the frame is drawing
rather than describing: the full-screen scanline-and-refresh-band overlay
is one live SkSL shader over every pixel of the canvas, re-run each
frame, and it is the only node in the sketch that costs anything like
that. The rest of the console — the feed's row mounts, the bound meters,
the caret — prices in fractions of a millisecond, exactly as its own
header claims.

Intended: a sketch holds 60 FPS at the canvas it declares, and a scanline
overlay whose only per-frame input is a phase is a texture crept by a
bound translate rather than a per-pixel program. The sketch already does
that for its CRT vignette.

Assert once fixed: `--bench --sketch daemon_console` verdicts PASS, and
no node in the report is a full-canvas live paint.

## eva_magi_defense spikes past the budget on its re-describe frames

`--bench` reports p50 2.8 ms and p99 22.9 ms at 1920×1080 — a frame in
twenty costs eight times the median. The front advances by re-describing
the funnel slot six times a second, and that re-describe re-records the
funnel's ribbons and re-bakes what stood under them, which lands whole
inside one frame.

Intended: a sketch holds 60 FPS at the canvas it declares, and work
scheduled at a fixed rate is spread or made cheap rather than paid in one
frame.

Assert once fixed: `--bench --sketch eva_magi_defense` verdicts PASS,
with p99 inside the budget rather than the median alone.

## Two of the historical plates do not hold 60 FPS at their own canvas

`chaucer_astrolabe` reports p50 ≈ 37.7 ms at 2400×1600 and
`nightingale_coxcomb` p50 7.1 ms with p99 27.7 ms at 1900×1032 — the
second is a spike rather than a level, so one frame in twenty costs four
times the median.

Intended: a sketch holds 60 FPS at the canvas it declares. Both are
large sheets over expensive material stacks, and the plate ledger already
carries a per-sketch timeout override for `chaucer_astrolabe`, so the
cost is known — but a declared canvas the sketch cannot present at is a
different statement from a sweep that takes a while.

Assert once fixed: `--bench` verdicts PASS for both, `nightingale_coxcomb`
with p99 inside the budget rather than the median alone.

## A cube map in a container file cannot be loaded

`material::EnvironmentMap::fromCubeMap` takes an ordinary image — a
cross or a strip — because that is what SigilImage decodes. DDS and KTX,
the two container formats that hold six faces and a mip chain in one
file and the two every capture tool writes, decode nowhere in this tree,
so a cube map has to be unpacked to a sheet or to six files by hand
before it reaches the library.

Intended: the six named sources the design lists reach the value from
the forms they actually ship in.

Both containers are already readable by a dependency this build
installs: DiligentTools' `Image` names `IMAGE_FILE_FORMAT_DDS` and
`IMAGE_FILE_FORMAT_KTX`, and `CreateTextureFromFile` reads them. It is
reachable only from a target that links Diligent, and the material kit
is asserted by a boundary probe to link no renderer — so the door is in
`world/diligent` or in SigilImage's decode feature beside the
OpenImageIO backend, not in the value itself.

Assert once fixed: a six-face cube written to a DDS and to a KTX loads
to the same panorama a sheet of the same faces does, texel for texel at
the six face-centre directions.

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

## A pass's stamps are cooked inside the draw, every frame, on both tiers

`world/frame/CpuGeometry.cpp`'s `drawStamps` and
`world/diligent/Geometry.cpp`'s stamp block both call `cook(Stamped{...})`
once per named point set per frame, inside the rasteriser. The device
tier then mints a fresh artefact number per frame
(`kStampArtefact | (gpu.frame << 8u)`) precisely so the upload cache
cannot hold the result — so a stamped point set re-instances on the host
and re-uploads its whole vertex buffer every frame, however still it is.

Intended: a stamped set is formed once per distinct (cloud, stamp) and
uploaded once, the way every other geometry artefact is. The stamping
itself is a point operator — a Copy/Instance with a kernel, written into
the device vertex buffer directly on the tier that has one, its host
executor generated from the same Slang source — which is what removes
both the re-cook and the artefact-id trick rather than caching around
them.

Assert once fixed: draw a pass whose point set does not change across
three frames and count the instancings and the vertex-buffer uploads; the
second and third frames must do neither. And: cook one chain through the
stamping kernel and through the host executor and compare the vertex
buffers bit for bit, the way the chain cook and the swept rings already
are.

## Two authoring verbs each mean two different random fields

`points::jitter` (`mesh/pop/Modifiers.cpp`) walks one stateful
`pcgUnitNext` sequence; `pop::Jitter` (`kernels/Pop.slang`) is stateless
and seeds per index. `points::displaceNoise` sums three taps of
`noise::value3` (a trilinear lattice); `pop::Noise` (`mesh/pop/Cook.cpp`)
sums six library sines. Each pair carries one verb's name and answers
with a different field, and nothing marks either half superseded.

Intended: one verb, one field. The chain op is the current spelling and
the pre-chain modifier is what a caller reaches for without a chain, so
the modifier should be the op's arithmetic applied directly.

Assert once fixed: displace the same cloud through the modifier and
through a one-op chain and compare the points bit for bit, for both
verbs.

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
