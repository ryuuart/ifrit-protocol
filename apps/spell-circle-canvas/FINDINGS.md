# Findings

Defects found while working. Each entry states what the code does, what
it was evidently intended to do, and what a test should assert once
intent is restored. Delete an entry when it is fixed; delete the file
when it is empty.

## A texture-cached fx track draws nothing for a face read from a file

**What the code does.** A `text()` leaf carrying both `.cache(Cache::Texture)`
and an `.fx(track)` paints no pixel at all when its style names a typeface
loaded through `SkFontMgr::makeFromFile` (the instrument faces under
`src/test/assets/`). The same leaf with the cache alone, or the track
alone, paints; the same leaf with all three in the machine's default face
paints. Observed on a raster host at 36, 40 and 41 px with a one-glyph run.

**What it was intended to do.** Paint the run through the cached texture
with the track applied, whatever face the style names — the texture path
and the fx path each already do for that face.

**What a test should assert.** `ComposeTextFx.ATrackReachKeepsAWideThrowInsideTheCull`
(`src/common/compose/typography/test/ComposeTestContentText.cpp`) set in
`whiteStyle(40)` — the instrument face — instead of `machineStyleAt(40)`,
with the `fonts` label and its `SUITES` entry in
`src/common/compose/typography/CMakeLists.txt` removed.

## A sketch that prints a number it measured differs from itself

`ctx.measured` — the pin that makes a self-measured number reproducible
under a capture — is used by five sketches. Thirty-three format numbers
with raw `snprintf` and fifty-three with `std::to_string`. Any of those
printing a value derived from the sketch's own execution (a build time, a
node count, a frame counter) draws a different plate on two runs of the
same binary, and a byte-identity sweep reports it as moved by a patch
that moved nothing.

Intended: every printed number that came from the sketch's execution
rather than from its data goes through `ctx.measured` first — and so does
every ORDER a measurement decides, which is the same defect one level up.
A table ranked by a stopwatch is a picture of the machine even when every
number printed in it is pinned.

One order decided by a stopwatch sits in the runtime rather than in any
sketch: the composer's automatic texture promotion bakes a node whose
paint measures over a millisecond for eight frames, and a promoted
node's antialiased edge lands 1 LSB from its live paint. A bare
`--headless` run opened its session deterministic but left promotion on,
so `chladni_tab1` (figure 1's star), `chevreul_circle` (the medallion's
rim), `ksp_mapview` and `eva_magi_deliberation` (a full-canvas child
inside its `phosphor` bake, spread by the bloom) each drew two plates
from one binary, while `--no-promotion` drew one; `spacejam_1996` drew
one plate over twelve renders. The session now holds promotion off
whenever it is deterministic, which is what the plate ledger already
asked for by flag. The five are verified once the host is rebuilt and
four bare headless renders of each are one digest.

Assert once fixed: two headless sweeps of the same binary produce
byte-identical plates for every canvas sketch, and each plate equals the
one that sketch renders alone.
