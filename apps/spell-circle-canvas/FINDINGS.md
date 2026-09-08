# Findings

Defects found while working, stated as what the code does, what it was
evidently intended to do, and what a test should assert once intent is
restored. A work queue: delete an entry when it is fixed, and delete this
file when it is empty.

The merge-readiness review of branch `sigil/library-campaigns` against
`main` (merge base `aabd3fe1b224`) and the fix pass that followed it are
done; the full review reports are the files under `findings/`. What
remains of them is below, then the standing entries.

## Verification state on main

Taken on the merged tree after the extractions and the file splits:

- Release build: zero errors, zero warnings. `ctest`: 3372 of 3372.
- CPU plate tier: 195 scenes byte-identical after the splits; the five
  that moved in the extractions were rebased with their causes, and
  `coverage_boundary` since, for the boundary now traced at the scale
  its plate is photographed at.
- Device tier: 195 of 195 within the per-channel bar. Promotion tier:
  147 of 195 within the rule (one code value over clear pixels, two
  over content), four scenes declaring their own nonlinearity; the rest
  is the entry below.
- ASan with UBSan and TSan: clean over the suite at the merge.
- Benchmark baseline retaken with the campaigns' arms; window-FPS
  baseline at an unlocked screen, none under the gate. The FPS lane
  refuses a locked screen, which throttles an invisible window's GPU
  work.

## Rulings

- A declared `shape()` bounds every layer a node is given — the
  group-opacity, effect and lifted-filter layers `recordBounds` sizes,
  as it already bounds the bake — with a pin; the two plates that move
  (kumiko_asanoha, sigillum_aemeth) are rebased with that cause.
- The promotion tier's remaining twelve stay recorded as the
  measurement with their next probes; the research resumes at the
  owner's word.
- The three git stashes in the checkout are the owner's to inspect.

## Automatic texture promotion moves 12 of 195 plates past the contract

`sigil.py plates --tier promotion` renders every scene twice on the CPU —
once with promotion off, once EAGER (every node the promoter's rules
admit, baked from its first frame) — and differences the pair. The rule is
in `src/common/compose/README.md` and it is now THREE CLAUSES, each
differing pixel judged by what it stands on: one code value where the
held-off plate is transparent black, two where it holds content, and forty
where the difference is confined to an antialiased edge BOTH plates draw —
the graze, where a promoted mark stands half a float step of its device
coordinate from its live paint and the quantizer at that coordinate (a
supersample bucket, a glyph's phase bucket) decides what it costs. The
graze bar is measured rather than chosen: `minard_1869` reports 31 and
`dunhuang_star_chart` 38 through the measure, and both sit inside one
supersample bucket at the contrast their curves stand at.

    195 scenes, 183 within the rule, 0 failed. Five declare a nonlinear
    picture and stand under that; thirty-four stand under the graze
    clause, naming their grazing figure and pixel count.

`--compare` answers which pixels are which, from the pixels rather than
from a list: a graze is a differing pixel where the picture varies by at
least the difference within a pixel of it IN BOTH PLATES, and so does
every differing pixel beside it. A mark that is gone leaves a flat ground
where it was and lands on `content` however antialiased the rest of the
page is. Pinned by `SketchCompare.TellsAGrazingEdgeFromAMarkThatIsGone`.

WHAT REMAINS, worst first, with the bar each crossed:

    kumiko_asanoha  89 grazing · paragraph_sheet 83 grazing ·
    black_watch     65 (59 over content) · thaumonomicon 43 (10) ·
    stroke_atlas    34 (7) · thunder_fulu 30 (30) ·
    sigillum_aemeth 25 (3) · blur_falloff 17 (17) ·
    fallout2_charsheet 7 (3) · lain_navi 6 (6) · ksp_mapview 6 (3) ·
    winamp_base 5 (5)

TEN OF THE TWELVE ARE OVER CONTENT, which no clause admits: a difference
where no edge explains it is a picture that moved, and the `content`
figure — not the max — is the number to chase. None of the ten has been
ablated.

THE TWO THAT ARE NOT are grazes over the measured bar, each with its own
next probe. `kumiko_asanoha` is TWO pixels of 4.2 M, each on a lattice
joint, each going from full ink to a ramp value — a coverage change of
about three quarters, far more than a supersample bucket, so it is either
the last bit through a corner or a conflation seam where two pieces meet
across a bake boundary. `paragraph_sheet` is ONE glyph of a page of type,
rendered at a different subpixel phase: Skia caches a glyph mask quantized
to a quarter of a pixel, and a coordinate whose last bit lands the other
way takes a different mask. Raising the bar to admit either would be
choosing a number to fit two scenes, which the derivation refuses.

AND ONE FINDING THIS PASS DID NOT TAKE. A node's declared `shape()` now
joins the rect a DEVICE BAKE is allocated to, because a bake allocated to
less than the ink cuts it. It does NOT join `recordBounds`, which sizes
layers — a group's opacity layer, an effect's, the surface a lifted filter
is run over — and joining it there moves pictures: `kumiko_asanoha`'s CPU
plate by 132 code values and `sigillum_aemeth`'s by 107, an effect's layer
growing over ink it had been cutting. Whether those layers should hold the
shape is the owner's call, not a verification pass's.

Assert once fixed: `--tier promotion` reports every scene within the rule,
and each cause gets a case in `compose_test` beside the nine that pin the
ones found so far
(`ComposeCache.APromotedLineKeepsTheInkThatStandsOutsideItsBox`,
`ComposeFaces.APromotedLineKeepsTheInkAFaceDrawsOutsideItsOwnMetrics`,
`ComposeCache.APromotedCurveKeepsTheCoverageItsLivePaintComputes`,
`ComposeCache.APromotedNodeDropsTheRecordingItsBakeReplaced`,
`ComposeCache.APromotedNodeIsRebakedWhenTheClipThatCutItOpens`,
`ComposeCache.APromotedPhraseThatAddsLightKeepsTheGroundUnderIt`,
`ComposeCache.ADeviceBakeRasterisesOnItsLivePaintsRoute`,
`ComposeCache.ATracedBoundaryIsRetracedWhenTheScaleUnderItMoves` and
`ComposeCache.APromotedShapeKeepsTheInkItDrawsOutsideItsBox`).
