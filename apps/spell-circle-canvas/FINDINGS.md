# Findings

Defects found while working, stated as what the code does, what it was
evidently intended to do, and what a test should assert once intent is
restored. A work queue: delete an entry when it is fixed, and delete this
file when it is empty.

## rota_convocationis is over its budget after its opening

`--bench --sketch rota_convocationis` passes on the sketch's own declared
moment (p99 11.3 ms of a 16.6 ms budget), and the twelve sub-seals no
longer add to the per-frame cost — the seal cycle grows the frame by 3 ms
where it grew it by 18. What the entry that stood here did not cover:
from about six seconds in the plate is over budget anyway, and by the
seal cycle's end it sits near 35 ms with no single node responsible.
`--bench --at 9`, `--at 12` and `--at 15` report p50 31.5 / 34.0 /
35.3 ms.

The cost is a long tail of full-canvas live paints — the emissive stacks
(`stella`, `arcus`, `star-lit`, `inner-lit`, the `rim-lit` and `nom-lit`
grades), each a `Baked` path filled at `kPlus` over the whole 1280 px
canvas, four per glow — plus three text rings that replay a picture every
frame (`vox`, `registrum`, `textura`) because their placement is driven
by a live path phase. The names' band was the fourth and is fixed: it is
turned as a body and baked. The same conversion on the other three is NOT
a win as things stand — a ring turned by a bound rotation is
`transformLive`, so its bake is held in local space and the blit resamples
a 650–1000 px image through the rotation, which costs more than the
replay it replaced.

Intended: either the emissive grades are cheap enough that twenty of them
fit in a frame, or a ring turned by a declared rotation can hold a bake
the blit does not resample. The second is the library half and the more
useful one: a device-space bake is refused to a node whose transform is
live, and yet a rotation about the node's own centre moves no pixel of
the bake's CONTENT — only where it lands.

Assert once fixed: `--bench --at 9`, `--at 12` and `--at 15` all verdict
PASS at 1280x1280, and the per-frame report shows no full-canvas live
paint in the emissive stacks.

## The weave kit's showcase corpus, palette and style shorthand reach nobody

`weave::kit::mixedScriptFiller()` (`sigilweave/kit/SampleText.h`) is
documented as the shared deterministic corpus every specimen sets, and no
sketch calls it. `weave::kit::palette` (`sigilweave/kit/Palette.h`) is
documented as the shared showcase palette, and no sketch reads it — it is
also `SkColor` where every consumer here works in `SkColor4f`.
`weave::kit::makeStyle()` (`sigilweave/kit/Labels.h`) is the documented
one-call style shorthand, and the sketches wrote 222 hand-rolled
`TextStyle` factories instead.

Intended: either these are what the sheets set their specimens in, or
their headers stop claiming a role nothing plays. The three are not one
decision — the corpus and the shorthand are plainly useful and simply
unreached, while the palette duplicates what `sketch::kit::Theme` now
carries in the right colour type.

Assert once fixed: a specimen sheet that needs filler text calls
`mixedScriptFiller()` rather than embedding a literal, and either
`weave::kit::palette` has a consumer or it is gone.

## Ring and grid placement is respelled where geometry already has it

`geometry::arrange::{along, onEllipse, onRing, cellAt, cellRect,
moduleSize, step}` (`sigilgeometry/path/Arrange.h`) is the canonical
ring-and-grid arithmetic, and `compose/kit/Placers.h` and
`compose/kit/Layouts.h` both state in their own file comments that they
must delegate to it rather than respell it. Two sketches reach for it;
sixty-two write the same arithmetic out with `std::cos` and `std::sin`
(184 sites).

Intended: one rounding. The two spellings do not agree to the pixel, so
this is not only duplication — it is the drift those file comments warn
against, and adopting `arrange` moves plates by sub-pixel wherever a
sketch is converted. It is therefore a per-sketch judgement with the
cause in each commit, not a sweep.

Assert once fixed: the converted sketch's placement is `arrange::`, and
its plate is rebased in the same commit that converts it.

## The house colours a paint program needs are still typed out

Six migrated sheets (`bullets_dropcap`, `decay_step`, `grid_layouts`,
`hub_reload`, `lane_retarget`, `ticker_lanes`) still declare one to three
of the house colours as file-scope constants, because the sites that use
them are inside `custom()` paint programs — which the kernel invokes
after the describe scope has ended, so they cannot read
`sketch::kit::theme()` and must capture a value.

Intended: the colour is read from the theme once while describing and
captured by the lambda, so a sketch under a bound theme paints in that
theme rather than in the house one it was written against.

Assert once fixed: no sketch under `src/sketch/sketches/` declares a
`SkColor4f` equal to a `houseTheme()` palette value.

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

## A promoted bake of spacejam_1996 is not the picture its live paint draws

`spacejam_1996` rendered `--headless` with automatic texture promotion on
draws one plate twelve times over, and rendered `--no-promotion` draws
another: the two differ by up to 213 code values on the outline of every
element on the page — every disc, every label, the logo and the starfield
alike — which is a picture shifted or resampled, not an antialiased edge
rounded once more. Promotion is decided by a measured cost, so which of
the two a machine draws depends on its load; the deterministic session
now holds promotion off, so a diffed capture sees only the live one, and
the discrepancy stands wherever promotion is on.

Intended: a promoted bake is taken in device space at an integer-snapped
rect and blitted back with the matrix reset, so under an upright matrix
it lands on the same pixels the live paint would have produced, with
only an antialiased edge free to differ by one code value. Something
about this page — its host scale, or a node promoted inside a recording
replayed under a fractional translation — carries a bake whose blit does
not land where the live paint did.

Assert once fixed: `--headless --no-deterministic --sketch spacejam_1996`
and `--headless --no-promotion --sketch spacejam_1996` differ by at most
one code value, and only on antialiased edges; and a compose test that
promotes a subtree under the plate's host scale reads back the pixels
the same subtree paints live.
