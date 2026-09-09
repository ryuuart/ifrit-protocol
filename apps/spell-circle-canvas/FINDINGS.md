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

- The six plates the declared-shape join moved beyond the two named
  are rebased with the same cause: ink a layer was deleting, and
  edges re-registered against a rect that grew.
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
    sigillum_aemeth 35 (3) · stroke_atlas 34 (7) ·
    thunder_fulu    30 (30) · blur_falloff 17 (17) ·
    fallout2_charsheet 7 (3) · lain_navi 6 (6) · ksp_mapview 6 (3) ·
    winamp_base 5 (5)

The two scenes whose CPU plates moved when the declared shape came to
bound every layer were re-measured here: `kumiko_asanoha` stands where it
did, 89 over a grazing edge on 35 645 pixels; `sigillum_aemeth` reads 35
where it read 25, with the same 3 code values over content. Six other
scenes' CPU pictures moved for the same cause and their promotion figures
were NOT retaken — `thaumonomicon`, `thunder_fulu`, `lain_navi` and
`ksp_mapview` are in the list above, `eva_magi_defense` and
`rota_convocationis` are not.

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

Assert once fixed: `--tier promotion` reports every scene within the rule,
and each cause gets a case in `compose_test` beside the ten that pin the
ones found so far
(`ComposeCache.APromotedLineKeepsTheInkThatStandsOutsideItsBox`,
`ComposeFaces.APromotedLineKeepsTheInkAFaceDrawsOutsideItsOwnMetrics`,
`ComposeCache.APromotedCurveKeepsTheCoverageItsLivePaintComputes`,
`ComposeCache.APromotedNodeDropsTheRecordingItsBakeReplaced`,
`ComposeCache.APromotedNodeIsRebakedWhenTheClipThatCutItOpens`,
`ComposeCache.APromotedPhraseThatAddsLightKeepsTheGroundUnderIt`,
`ComposeCache.ADeviceBakeRasterisesOnItsLivePaintsRoute`,
`ComposeCache.ATracedBoundaryIsRetracedWhenTheScaleUnderItMoves`,
`ComposeCache.APromotedShapeKeepsTheInkItDrawsOutsideItsBox` and
`ComposePaintBounds.ADeclaredShapeBoundsEveryLayerTheNodeIsGiven`).

## The IO hub's network cache lives under the temp directory

`sigil::io::defaultNetworkCacheDir()` answers the process temp directory
plus `sigilio-net-cache`, and macOS purges files under the temp
directory that have not been touched for three days. A plate sweep does
not fetch, so a scene whose assets are network-fetched (`twoadvanced_v4`,
`twoadvanced_equipment`) drops out of the sweep as SKIPPED on a machine
that has been up long enough, and comes back only after a live render
fetches again; the sweep's scene count silently narrows from 195 to 193.

Intended: a cache that a lane depends on outlives the OS's temp policy —
the platform cache location (`~/Library/Caches` on macOS), where
Sketchbook already keeps its thumbnails, is the door the hub should
take by default, with the temp directory only where no platform cache
exists.

Assert once fixed: the default network cache directory is under the
platform cache location, and a sweep on a machine that rendered those
scenes a week ago still renders them.
