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

- The review's declines stand as closed, bar three that are done now:
  the pan-only predicate's three spellings become one question in
  SigilMaterial's vocabulary; `Ribbon::band` and `turnedArea` delegate
  to geometry's `bandRegion` and `signedArea`; the Knuth-Plass prefix
  tables leave the DP function, with the inlining claim measured.
- The close-out's open items are done: the typography chapter names
  its eight public names; compose reports on one channel; the three
  hand-spelt radian factors; `Anchor`'s discriminator in the type; a
  `SpriteBatch` for the atlas draw; the four typography and brush
  files over 600 lines split by subject; the cases for `exactTangent`,
  `Annotation::reserve` and the text-reuse volatility branch; the
  stamped router in the kit; `boneFrame` onto the kit frame; the three
  colour and effect helpers onto material's; `SIGIL_BAKE_DUMP` stated
  or gone; a bevel under a span gate reads the silhouette.
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

## A bevel ring is lost while a span gate reveals its node

`kit::Bevel` and `styles::BevelPair` clip their ring to `ctx.outline` —
"the clip is the WHOLE outline, not the edge — an open edge encloses
nothing" — which is the same clip an Inner-aligned `PathFormat` makes.
An aligned stroke now clips to `PaintContext::silhouette` where an
adaptor or a span gate narrowed the outline to a contour bounding no
area; a bevel does not read it, so under `mask(by::spans(...))` or a
span-qualified pass its clip stands on the revealed run, which bounds
nothing, and the whole ring is discarded until the run closes.

Evidently intended: the same rule the alignment now keeps — the light
and shadow edges stand inside the shape along the part of the boundary
that is shown, and the settled reveal is the unspanned ring.

Not taken with the alignment's fix because no site in the tree spells a
bevel on a span-gated node, so the change would be unpinned by any
picture; the sketches that would exercise it do not exist yet.

Assert once fixed: a `kit::bevelled` panel under `spans::upTo` at a
fraction of one straight run draws that fraction of its ring inside the
silhouette, and fully revealed is the unspanned ring pixel for pixel.
