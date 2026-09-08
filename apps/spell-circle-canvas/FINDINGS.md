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
  146 of 195 within the rule (one code value over clear pixels, two
  over content); the rest is the entry below.
- ASan with UBSan and TSan: clean over the suite at the merge.
- Benchmark baseline retaken with the campaign's new arms; window-FPS
  baseline at an unlocked screen, none under the gate. The FPS lane
  refuses a locked screen, which throttles an invisible window's GPU
  work.

## Rulings

- SigilMotion's force lane is the caller's to pre-load: `Verlet::step`
  accumulates onto it and clears it after integrating, with a case that
  pre-loads a force and sees it move the points.
- The file splits run now, one agent per library in sequence, one
  commit per file, the case listing byte-identical, no behaviour
  change.
- The promotion tier's sub-pixel graze gets its last probe (the
  rasterisation route, layer against opaque surface, in plain Skia);
  spacejam and the eva_magi plates declare their own nonlinearity so the
  tier reads them under it, the way volatility_cost reads its own
  composer.
- The seams' library growths without a sketch consumer yet stay, with
  their cases.

## Deferred, a campaign of its own

- The source-file splits by subject: `material/skia/Paint.cpp` (1573),
  `Paint.h` (771), `Effect.cpp` (748); `geometry/mesh/pop/Pop.h` (1131),
  `mesh/pop/Cook.cpp` (792), `mesh/codec/Geo.cpp` (601),
  `geometry/path/Ops.cpp` (604); `sketch/book/main.cpp` (1331),
  `book/SketchbookView.cpp` (896), `book/qml/Main.qml` (850); the
  1380-line `Composer::Impl::paint` in `compose/core/StackingPainter.cpp`
  and the compose files its review lists (`Element.h`, `Reconcile.cpp`,
  `Composer.cpp`, `ComposerImpl.h`, `Volatility.cpp`, `Instance.h`,
  `Layout.cpp`, `ComposeInternal.h`, `Derive.cpp`).

## Automatic texture promotion moves 49 of 195 plates past the contract

`sigil.py plates --tier promotion` renders every scene twice on the CPU —
once with promotion off, once EAGER (every node the promoter's rules
admit, baked from its first frame) — and differences the pair. The rule
is in `src/common/compose/README.md`: a promoted node paints the picture
its live paint paints, within one code value per channel where the
held-off plate is transparent black and two where it holds content, and
a scene that moves further is a defect in compose rather than a plate to
rebase. The tier reads both bars, judging each differing pixel by what
the held-off plate holds in it.

    195 scenes, 146 within the rule, 0 failed. (126 before this pass,
    under the one-value bar the tier used to apply everywhere.)

TWO MORE CAUSES WERE FIXED, each with a pin.

FOUR — A BAKE HELD THE CLIP THAT CUT IT. Every device bake carries the
canvas's own clip into its layer, so what it holds is the node's paint AS
THAT CLIP LEFT IT — and a clip narrows and widens for reasons the node's
own bounds cannot see: an ancestor's layer standing over the box while it
fades in, a panel opening, a window growing. Nothing else a bake is
compared against moves with it, so a bake taken under the narrow clip
blitted the CUT for as long as the node's content stood still, and the
marks the clip removed never came back. On `minard_1869` a title's first
line, its scale bar with graduation and caption, two footnotes and a date
line vanished; on `dunhuang_star_chart` the same shape. Every device bake
— the promotion tier's, the split's, the group's and the device-space
local one — is now stamped with the clip it was taken under and remade
when that clip differs, exactly as a recording holding a device blit
already was. Pinned by
`ComposeCache.APromotedNodeIsRebakedWhenTheClipThatCutItOpens`, which
fails by 255 without it. `minard_1869` 163 → 31,
`dunhuang_star_chart` 216 → 38, `chaucer_astrolabe` 108 → 9.

FIVE — TYPE THAT ADDS LIGHT DID NOT DECLARE THE BACKDROP IT READS. A
glyph pass carries an `SkPaint` of its own, so a phrase set additively —
the blend on the paint, which is where it belongs, since a blend on the
NODE opens a layer every frame — composites against what is under the
node exactly as a blended decoration does. Only the node's own blend and
its decorations' were counted, so such a phrase was promoted, baked
against transparent black, and blitted over the ground rather than added
to it: `lain_navi` lost the bloom around all of its type and read flat.
Every paint a text node can carry is now asked — the style's foreground
and its under- and overlays, its line decorations, each run of a
`RichText` value, and each span restyle — and a node carrying one, with
every ancestor, is refused the automatic bake and the memo hold. Pinned
by `ComposeCache.APromotedPhraseThatAddsLightKeepsTheGroundUnderIt`,
which fails by 135 without it. `lain_navi` 176 → 6.

WHAT REMAINS, max channel first. EVERY ONE OF THEM IS OVER CONTENT: no
promoted node moves a pixel that stood on nothing.

    eva_magi_interior 190 · spacejam_1996 185 · sigillum_aemeth 122 ·
    coverage_boundary 118 · kumiko_asanoha 89 · paragraph_sheet 83 ·
    material_child 66 · black_watch 65 · thaumonomicon 43 ·
    dunhuang_star_chart 38 · fx_scatter_mix 37 · chevreul_circle 35 ·
    stroke_atlas 34 · minard_1869 31 · thunder_fulu 30 ·
    chladni_tab1 28 · aero desktop 23 · blur_falloff 17 ·
    volatility_cost 17 · hit_slots 16 · chrome_type 13 ·
    …and twenty-eight more at 9 or less

TWO OF THE HEAD ARE A SCENE'S OWN NONLINEARITY AMPLIFYING A ROUNDING
THAT IS WITHIN THE RULE, and the ablation is decisive:

- `spacejam_1996` sets a VIEW TRANSFORM that quantizes each channel to
  the six levels of the 1996 web palette, unpremultiplying to do it — so
  its gain is 1/alpha and is unbounded on a near-transparent pixel. With
  the view left out the whole plate comes within ONE. The promoted
  picture is right; the quantizer is what turns a last-bit difference
  into a step of 51.
- `eva_magi_interior` wears a `phosphorBloom` over its whole picture: a
  bright pass through a smoothstep gate, gathered over a layer reduced by
  two and laid back over the sharp source. With the layer effect left out
  the plate stands at 6. The 190 is that 6 through the gate.

SO THE REAL REMAINDER IS SMALL AND THE SAME SHAPE EVERYWHERE: a promoted
mark stands a fraction of a pixel from its live paint, which reads as
nothing along an edge that meets the grid squarely and as tens of code
values where a curve GRAZES it. On `minard_1869` it is 459 pixels of two
donation stamps' ellipses; on `dunhuang_star_chart` 57. Four ablations
say what it is NOT: not the bake's margin (at twelve times the margin the
number does not move), not the integer device translation (baking at the
origin instead does not move it), not the clip's antialiasing (carrying
the clip in as an antialiased rect does not move it), and not nesting (a
bake inside a bake refused makes no difference). What is left to try is
the rasterisation route itself: the same path, the same matrix, drawn
into a fresh premultiplied layer and into the plate's own surface, in
PLAIN SKIA with no compose in it — the experiment that named the bake
margin.

Assert once fixed: `--tier promotion` reports every scene within the
rule, and each cause gets a case in `compose_test` beside the six that
pin the ones found so far
(`ComposeCache.APromotedLineKeepsTheInkThatStandsOutsideItsBox`,
`ComposeFaces.APromotedLineKeepsTheInkAFaceDrawsOutsideItsOwnMetrics`,
`ComposeCache.APromotedCurveKeepsTheCoverageItsLivePaintComputes`,
`ComposeCache.APromotedNodeDropsTheRecordingItsBakeReplaced`,
`ComposeCache.APromotedNodeIsRebakedWhenTheClipThatCutItOpens` and
`ComposeCache.APromotedPhraseThatAddsLightKeepsTheGroundUnderIt`).
