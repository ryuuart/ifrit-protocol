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

Taken on the merged tree after the extractions campaign:

- Release build: zero errors, zero warnings. `ctest`: 3370 of 3370.
- CPU plate tier: 195 scenes; the five that moved in the campaign were
  rebased with their causes (`bg3_dice_roll` lands on a named face
  through the library pose; `chaucer_astrolabe` a hairline ring one
  pixel over from the projection value's arithmetic; `genesis_fire` a
  fused multiply-add landing differently in the library's translation
  unit; `dunhuang_star_chart` one code value from the precession
  rotation; `cde_motif` a mitred bevel corner's diagonal). Every other
  plate the eight seams touched is byte-identical.
- Device tier: 195 of 195 within the per-channel bar. Promotion tier:
  147 of 195 within the rule (one code value over clear pixels, two
  over content); the rest is the entry below.
- ASan with UBSan and TSan: clean over the suite at the merge.
- Benchmark baseline retaken with the campaign's new arms; window-FPS
  baseline at an unlocked screen, none under the gate. The FPS lane
  refuses a locked screen, which throttles an invisible window's GPU
  work.

## Rulings

- The file splits run now, one agent per library in sequence, one
  commit per file, the case listing byte-identical, no behaviour
  change.
- The seams' library growths without a sketch consumer yet stay, with
  their cases.

## Deferred, a campaign of its own

- The source-file splits by subject: `sketch/book/main.cpp` (1331),
  `book/SketchbookView.cpp` (896), `book/qml/Main.qml` (850); the
  1380-line `Composer::Impl::paint` in `compose/core/StackingPainter.cpp`
  and the compose files its review lists (`Element.h`, `Reconcile.cpp`,
  `Composer.cpp`, `ComposerImpl.h`, `Volatility.cpp`, `Instance.h`,
  `Layout.cpp`, `ComposeInternal.h`, `Derive.cpp`).

## Automatic texture promotion moves 48 of 195 plates past the contract

`sigil.py plates --tier promotion` renders every scene twice on the CPU —
once with promotion off, once EAGER (every node the promoter's rules
admit, baked from its first frame) — and differences the pair. The rule
is in `src/common/compose/README.md`: a promoted node paints the picture
its live paint paints, within one code value per channel where the
held-off plate is transparent black and two where it holds content, and
a scene that moves further is a defect in compose rather than a plate to
rebase. The tier reads both bars, judging each differing pixel by what
the held-off plate holds in it.

    195 scenes, 147 within the rule, 0 failed. Every mover is over
    CONTENT: no promoted node moves a pixel that stood on nothing.

FOUR SCENES NOW STAND UNDER A DECLARATION OF THEIR OWN, and it is not an
exclusion: a sketch whose picture ends on a step, a round, a gate or a
reciprocal has no bound between a difference in what went into it and
the difference it shows, so the tier is handed the gain rather than the
rounding. `ctx.nonlinearPicture()` says so, holds the automatic promoter
off that sketch in every host, and is printed on the scene's verdict
line and in the tier's summary. `spacejam_1996` (its view rounds each
channel to the six levels of the 1996 web cube and unpremultiplies to do
it, so its gain is 1/alpha and its steps are 51 — within ONE with the
view left out) and the three `eva_magi` plates (`phosphorBloom`, whose
bright pass is a threshold gate over a reduced layer laid back over the
sharp source — `eva_magi_interior` stands at 6 with the effect left out)
carry it, each with its ablation in its own header.

THE LAST NAMED PROBE CAME BACK: THE ROUTES AGREE. The same stroked curve
in plain Skia, into a `saveLayer` over the destination and into a fresh
premultiplied offscreen, is the SAME PIXELS; an offscreen already
holding the ground is the same pixels as the destination; and the
blitted offscreen stands exactly the one code value its second composite
costs. So the bake's surface, its alpha type, its coverage mode and its
bounds snapping are not where the sub-pixel graze comes from. Pinned by
`ComposeCache.ADeviceBakeRasterisesOnItsLivePaintsRoute`.

WHAT THE GRAZE IS: the float arithmetic of the integer device offset. A
bake is taken under the live matrix with an integer subtracted from its
translation, so the live paint rounds its sum in the binade of the
device coordinate and the bake rounds its own in the binade of the
offset one. The same arc, the same path, the same matrix, drawn live and
through the bake's construction, is byte-identical up to a few hundred
pixels from the canvas origin — where the integer cancels exactly — and
parts by half a float step of the device coordinate beyond it: nothing
along an edge that meets the grid squarely, a whole supersample bucket
where a curve runs nearly tangent to one. That is `minard_1869`'s 459
pixels and `dunhuang_star_chart`'s 57 on plates 2560 px across.

IT CANNOT BE REMOVED AT THE BAKE'S END. The error is the live paint's
own rounding, which the bake does not share, and it is independent of
the offset chosen — a different integer, a power-of-two one, an offset
folded into local space instead, all give the same number. The one
construction that removes it is a bake allocated from the CANVAS ORIGIN,
so no translation enters the layer's matrix at all, and that costs a
full-canvas surface for every promoted node; Skia's raster API has no
device origin to put under a smaller one. The contract in
`src/common/compose/README.md` now states this beside the shader
inversion it is the geometric twin of. WHAT IS UNDECIDED is what follows
from it: a third clause in the contract, or an origin-anchored
allocation for the bakes that can afford one.

WHAT REMAINS, max channel first, none of it ablated by this pass:

    sigillum_aemeth 122 · coverage_boundary 118 · kumiko_asanoha 89 ·
    paragraph_sheet 83 · material_child 66 · black_watch 65 ·
    thaumonomicon 43 · dunhuang_star_chart 38 · fx_scatter_mix 37 ·
    chevreul_circle 35 · stroke_atlas 34 · minard_1869 31 ·
    thunder_fulu 30 · chladni_tab1 28 · aero desktop 23 ·
    blur_falloff 17 · volatility_cost 17 · hit_slots 16 ·
    chrome_type 13 · …and twenty-nine more at 9 or less

The five heads of that list have never been ablated at all, and the
graze above accounts for tens rather than a hundred: `sigillum_aemeth`,
`coverage_boundary`, `kumiko_asanoha` and `paragraph_sheet` are the next
place to look, and each is a separate question from the offset's last
bit.

Assert once fixed: `--tier promotion` reports every scene within the
rule, and each cause gets a case in `compose_test` beside the seven that
pin the ones found so far
(`ComposeCache.APromotedLineKeepsTheInkThatStandsOutsideItsBox`,
`ComposeFaces.APromotedLineKeepsTheInkAFaceDrawsOutsideItsOwnMetrics`,
`ComposeCache.APromotedCurveKeepsTheCoverageItsLivePaintComputes`,
`ComposeCache.APromotedNodeDropsTheRecordingItsBakeReplaced`,
`ComposeCache.APromotedNodeIsRebakedWhenTheClipThatCutItOpens`,
`ComposeCache.APromotedPhraseThatAddsLightKeepsTheGroundUnderIt` and
`ComposeCache.ADeviceBakeRasterisesOnItsLivePaintsRoute`).
