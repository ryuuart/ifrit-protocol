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

Taken on the merged tree at the close of the session's work:

- Release build: zero errors, zero warnings. `ctest`: 3418 of 3418,
  every library's README compile-checked among them.
- CPU plate tier: 195 scenes byte-identical; the four sketches whose
  sliced inner-aligned strokes now paint were rebased with that cause,
  and six more with the caching causes below — `aero desktop`,
  `persona menu` and `y2k chrome` where a bake now holds the skirt an
  effect under it filters, `dunhuang_star_chart` and `minard_1869` where
  a bake now stands on the canvas's own grid, and `volatility_cost`,
  whose subject is the runtime's own caching verdicts.
- Device tier: 195 of 195 within the per-channel bar. Promotion tier:
  191 of 195 within the contract's three clauses; the rest is the entry
  below.
- ASan with UBSan and TSan: clean over the suite at the merge.
- Benchmark baseline retaken with every new arm; window-FPS baseline
  at an unlocked screen, none under the gate.

## Rulings

- The review's declines stand as closed. The last of the three is taken:
  the band a ribbon fills is SigilGeometry's, and `turnedArea` is its
  `signedArea`. The delegation is to `sweptRegion` and NOT to
  `bandRegion`, because the two are different constructions and only one
  of them can carry a join vocabulary or a width law keyed on the spine's
  direction — measured before the change, a `bandRegion` ribbon differed
  from the swept one at every corner (98 px on a rectangle, 24 on a
  circle, none on a straight run), which is the corner treatment and
  nothing else.
- The close-out's open items are done: the colour and effect helpers onto
  material's, bar `lain_navi`'s plate program — a photographed city
  defocused past recognition is a picture rather than a shape with
  props, and the library states that a consumer composing its own
  grained body writes its own noise or asks for a preset. It stands on
  the library's seam for a consumer program already; whether it becomes
  a preset is a ruling, not a fix.
- `findings/` is deleted once those land; the review's record lives
  outside the repository.

## Automatic texture promotion moves 4 of 195 plates past the contract

`sigil.py plates --tier promotion` renders every scene twice on the CPU —
once with promotion off, once EAGER (every node the promoter's rules
admit, baked from its first frame) — and differences the pair. The rule is
in `src/common/compose/README.md` and it is THREE CLAUSES, each differing
pixel judged by what it stands on: one code value where the held-off plate
is transparent black, two where it holds content, and forty where the
difference is confined to an antialiased edge BOTH plates draw — the
graze, where one step of a pixel's coverage costs whatever quantizer
stands at that coordinate.

    195 scenes, 191 within the rule, 0 failed. Four declare a nonlinear
    picture and stand under that; nineteen stand under the graze clause,
    naming their grazing figure and pixel count.

WHAT REMAINS, all four over CONTENT, which no clause admits:

    lain_navi 6 (6 over content) · winamp_base 5 (5) ·
    ksp_mapview 6 (3) · fallout2_charsheet 3 (3)

THEY ARE ONE SHAPE: a wash at another value, mean difference zero, the two
fields correlated to four decimal places, jittering by a few code values
per pixel over a large area. `lain_navi` is 83 407 pixels at 6 over the
near-white of type nine additive passes stand on, the promoted plate
darker at every one. `winamp_base` is 19 000 pixels in one panel's grain,
the two noise fields correlating at 0.9998 with a mean difference of
0.001. `ksp_mapview` is 537 305 pixels at 3, most of the plate.
`fallout2_charsheet` is 3 783 at 3 in one horizontal band.

FIVE ABLATIONS SAY WHAT THEY ARE NOT, each measured on all four: the bake
margin raised by 40 and by 300 moves none of them; disabling the split
bake moves none; disabling the promotion bake takes all four to zero, so
it is the bake and not the sweep; taking the bake at the canvas origin
moves none; and taking it at F16 instead of N32 takes `ksp_mapview` from 3
to 2 and `fallout2_charsheet` from 3 to FIVE, so it is not one arithmetic.

What has not been tried is counting the composites a pixel actually passes
through. `winamp_base`'s band stands under three surfaces — one promotion
bake and two split bakes, none nested inside another — and three
composites is one more than the second clause allows even where none of
them nests. The next probe is a bake that records how many blits each
pixel of the plate was under, differenced against the difference.

Assert once fixed: `--tier promotion` reports every scene within the rule,
and each cause gets a case in `compose_test` beside the thirteen that pin
the ones found so far
(`ComposeCache.APromotedLineKeepsTheInkThatStandsOutsideItsBox`,
`ComposeFaces.APromotedLineKeepsTheInkAFaceDrawsOutsideItsOwnMetrics`,
`ComposeCache.APromotedCurveKeepsTheCoverageItsLivePaintComputes`,
`ComposeCache.APromotedCurveFarFromTheOriginStandsOnTheLivePaintsGrid`,
`ComposeCache.APromotedNodeDropsTheRecordingItsBakeReplaced`,
`ComposeCache.APromotedNodeIsRebakedWhenTheClipThatCutItOpens`,
`ComposeCache.APromotedPhraseThatAddsLightKeepsTheGroundUnderIt`,
`ComposeCache.ADeviceBakeRasterisesOnItsLivePaintsRoute`,
`ComposeCache.ATracedBoundaryIsRetracedWhenTheScaleUnderItMoves`,
`ComposeCache.APromotedShapeKeepsTheInkItDrawsOutsideItsBox`,
`ComposeCache.APromotedNodeKeepsTheSkirtABlurredChildFilters`,
`ComposeCache.ANodeInsideABakeIsNotBakedAgain` and
`ComposePaintBounds.ADeclaredShapeBoundsEveryLayerTheNodeIsGiven`).

## The graze bar rests on one scene where it was measured off two

`scripts/sigil/plates.py` judges `graze <= 40`, and the forty was measured
off `minard_1869` at 31 and `dunhuang_star_chart` at 38. A device bake now
stands on the canvas's own grid, and dunhuang reports **4** where it
reported 38 — so the bar is measured off `minard_1869` alone, with
`sigillum_aemeth` at 35 the only other scene anywhere near it, then
`astral_tome` 15 and `vertigo_titles` 8.

Nothing crosses the bar, so nothing was changed. But a number derived from
two agreeing scenes and now resting on one is a derivation to retake, and
`minard_1869`'s own 31 is unexplained: the grid halved the pixels it
grazes on (157 127 -> 86 862) and left the worst exactly where it was, so
whatever it is, it is not the offset every other grazing scene turned out
to be.

Assert once settled: the bar names the scene it is measured off and that
scene's figure is explained, or the clause bounds the shape alone and the
ledger says so.
