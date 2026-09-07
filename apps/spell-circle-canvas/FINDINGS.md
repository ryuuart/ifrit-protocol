# Findings

Defects found while working, stated as what the code does, what it was
evidently intended to do, and what a test should assert once intent is
restored. A work queue: delete an entry when it is fixed, and delete this
file when it is empty.

The merge-readiness review of branch `sigil/library-campaigns` against
`main` (merge base `aabd3fe1b224`) and the fix pass that followed it are
done; the full review reports are the files under `findings/`. What
remains of them is below, then the standing entries.

## Verification state at the merge

Taken on a fresh build directory at the head of the branch:

- Release build: zero errors, zero warnings. `ctest`: 3247 of 3247.
- CPU plate tier: 195 scenes, every mover of the pass named by the
  pass that moved it and rebased with the cause; the sweep no longer
  fills the frames it discards, so `chaucer_astrolabe` renders in
  seconds and every plate is byte-identical to before.
- Device tier: 195 of 195 within the per-channel bar after two causes
  were found on the Vulkan path (a blending draw painted over what was
  described after it; a fenced pass resolving multisamples over the
  pass before it).
- Promotion tier: 121 of 195 within one code value; the remainder is
  the entry below.
- ASan with UBSan and TSan: 3247 of 3247 each, no report.
- Benchmark and window-FPS baselines retaken at an unlocked screen and
  committed: 195 sketches, none under the gate. The lane refuses a
  locked screen, which throttles an invisible window's GPU work. One
  sketch, `dunhuang_star_chart`, stood down inside the sweep (no frame
  presented within its slot after the sketch before it) and was
  measured alone at the display's rate and merged; a heavy first frame
  following another session is the shape to watch if it recurs.

## Rulings for the post-merge pass

- The promotion tier's head (flourish, beethoven, dunhuang, the eva_magi
  plates, spacejam, lain_navi, minard, tile map) is researched now, one
  cause at a time with a `compose_test` pin each; the tail is judged
  after it. Three causes are found and fixed and five scenes are left,
  with the ablations that narrow them: the entry below.
- rota keeps its look; the library question is taken and answered: a
  settled node with a static layer effect is baked without it and the
  effect is run OVER that bake, with the identity test. Not at the blit,
  which the measurement refused — see the entry below.

## Deferred past the merge, each a campaign of its own

- The library extractions the sketch review names — the mechanisms
  hidden in the sketches over 1000 lines: palette-indexed sprites and an
  atlas packer (`xcom_battlescape`, `cde_motif`, `thaumonomicon`) →
  `compose/kit/Sprites.h`; an HTML auto-table layout scheme
  (`spacejam_1996:999-1200`) → compose kit layouts with a test; an
  icosahedron and a face-up pose (`bg3_dice_roll:376-503`) →
  `geometry/kit/Solids.h` and a mesh pose; a conic generator and four
  silhouettes (`ksp_mapview:244-566`) → geometry path and
  `Silhouettes.h`; Motif bevels (`cde_motif`, `twoadvanced_v4`,
  `winamp_base`, `fallout2_charsheet`) → `Chrome.h`; a stereographic and
  a chart projection (`chaucer_astrolabe`, `dunhuang_star_chart`) → a
  projection value in geometry path; a tartan sett-to-cloth weave
  (`black_watch:234-295`) → material pattern; a pentagrid
  (`penrose_paving:264`) and mitred lattice joinery
  (`kumiko_asanoha:517`) → `Lattice.h`/`Ops.h`; Reeves particles
  (`genesis_fire:516`) → `sigilmotion/physics/Points.h`. Each is a
  rewrite to the seam, never a verbatim move.
- The source-file splits by subject: `material/skia/Paint.cpp` (1573),
  `Paint.h` (771), `Effect.cpp` (748); `geometry/mesh/pop/Pop.h` (1131),
  `mesh/pop/Cook.cpp` (792), `mesh/codec/Geo.cpp` (601),
  `geometry/path/Ops.cpp` (604); `sketch/book/main.cpp` (1331),
  `book/SketchbookView.cpp` (896), `book/qml/Main.qml` (850); the
  1380-line `Composer::Impl::paint` in `compose/core/StackingPainter.cpp`
  and the compose files its review lists (`Element.h`, `Reconcile.cpp`,
  `Composer.cpp`, `ComposerImpl.h`, `Volatility.cpp`, `Instance.h`,
  `Layout.cpp`, `ComposeInternal.h`, `Derive.cpp`).

## Automatic texture promotion moves 69 of 195 plates past one code value

`sigil.py plates --tier promotion` renders every scene twice on the CPU —
once with promotion off, once EAGER (every node the promoter's rules
admit, baked from its first frame) — and differences the pair. The rule
is in `src/common/compose/README.md`: a promoted node paints the picture
its live paint paints, within one code value per channel over transparent
black and two where the bake lands on content, and a scene that moves
further is a defect in compose rather than a plate to rebase.

    195 scenes, 126 within one code value (120 before this pass).

THREE CAUSES WERE FIXED IN THE POST-MERGE PASS, each with a `compose_test`
case beside the two that pin the type one.

ONE — A DEVICE BAKE WAS ALLOCATED FLUSH AGAINST WHAT IT PAINTS. Skia
decides whether a path needs its CLIPPED rasterisation from the path's
control-point bounds, and its clipped and unclipped routes do not answer
the same antialiased coverage. A curve's control points stand outside the
ink it draws — measured at 1.68, 3.50 and 6.75 px for stroked arcs of
side 152, 340 and 676, about a hundredth of the curve's own extent — so a
bake sized to exactly what the node paints cut inside them, and every
stroked ring, arc or ornament was baked on one route and painted live on
the other. Tens of code values along the whole length of the curve, which
on a high-contrast plate is the difference between the two inks. Every
device bake now carries a margin, `bakeMargin(extent) = 2 + extent/32`.
Reproduced in PLAIN SKIA with no compose in it, which is what named the
mechanism. Pinned by
`ComposeCache.APromotedCurveKeepsTheCoverageItsLivePaintComputes`, which
fails by 62 without the margin. It moved `flourish` 244 → 8,
`eva_magi_defense` 174 → 1, `eva_magi_deliberation` 188 → 2,
`nine slice` 221 → 0, `stroke_atlas` 105 → 34, `hit_slots` 86 → 16,
`blur_falloff` 55 → 17, `thunder_fulu` 77 → 30 and `astral_tome` 35 → 9.

TWO — A BAKE LEFT A RECORDING BEHIND THAT WAS ONE FRAME OLD. A node stops
recording the frame its device bake is taken, and the recording it
already held was neither dropped nor staled: the content scalars that
separated the two become the bake's own, and a settled node is not dirty.
The bake is refused again the moment the matrix under it moves — which is
exactly what photographing a plate at its view scale does — and the stale
recording replays. On `beethoven` that showed as three of nine arcs
coming back a frame short of their reveal: the three whose transitions
settled while their neighbours were still running, so each was promoted
on the frame it landed. A promotion bake now drops the recording it
replaced, as the `Cache::Group` tier beside it already did. Pinned by
`ComposeCache.APromotedNodeDropsTheRecordingItsBakeReplaced`, which fails
by 228 without the drop. `beethoven` 228 → 1.

THREE — TWO SHEETS DREW THE RUNTIME'S OWN CACHING VERDICTS, so a promoted
run was meant to read differently: `volatility_cost` (the tier per node,
the split and `Composer::stats()`) and `tile map` (recordings held and
nodes painted live, beside the reconciler's counts). Both now read a
composer THE SHEET OWNS — the same tree, stepped on the same clock,
drawing into nothing, under a policy the sheet declares: `volatility_cost`
eager, so its map is what the promoter's RULES admit rather than what a
stopwatch reached, and `tile map` held off, which is the regime its memo
lesson is about. Neither is excluded anywhere. `tile map` is within the
rule and its CPU plate is byte-identical; `volatility_cost` fell 228 → 17
and ITS CPU PLATE MOVED (`929fbbff7af0` → `0f3a52faa0f2`), because the
tier column now names what the description admits instead of what the
host was opened with. Not rebased.

WHAT REMAINS, max channel first:

    dunhuang_star_chart 216 · eva_magi_interior 190 · spacejam_1996 185 ·
    lain_navi 176 · minard_1869 163 · sigillum_aemeth 122 ·
    coverage_boundary 118 · chaucer_astrolabe 108 · kumiko_asanoha 89 ·
    paragraph_sheet 83 · material_child 66 · black_watch 65 ·
    thaumonomicon 43 · fx_scatter_mix 37 · chevreul_circle 35 ·
    stroke_atlas 34 · thunder_fulu 30 · chladni_tab1 28 · aero desktop 23 ·
    volatility_cost 17 · blur_falloff 17 · hit_slots 16 · chrome_type 13 ·
    …and forty-six more at 9 or less, of which TWENTY sit at exactly 2.

THE TWENTY AT EXACTLY TWO are the second code value the contract already
allows — a node's own coverage composited twice where the live paint
composited once. The tier's ceiling is still one, which is
`sigil/plates.py`'s to change and not this entry's.

WHAT IS KNOWN ABOUT THE FIVE AT THE HEAD (`dunhuang_star_chart`,
`eva_magi_interior`, `spacejam_1996`, `lain_navi`, `minard_1869`). Two
ablations narrow them:

- ALL FIVE ARE THE WHOLE-SUBTREE PROMOTION BAKE, not the split. With the
  eager split disabled and eager promotion on, every one is unchanged;
  with eager promotion disabled and the split on, all five come within
  one code value.
- TWO OF THE FIVE ARE THE CLIP THE BAKE CARRIES. With `clipBakeLayer`
  made a no-op, `minard_1869` falls 163 → 31 and `dunhuang_star_chart`
  216 → 38, while the other three do not move at all. What minard loses
  is not a rounding: WHOLE MARKS VANISH — the scale bar with its
  graduation and its caption, and two of the five arithmetic footnotes —
  so the bake's clip cuts content the live paint keeps.

Two candidates for that clip were built and measured and neither moved a
plate, so neither was kept: a recording's CULL RECT read as a clip (a
recording canvas's device IS its cull, so an unclipped node inside one
reports the cull as its clip bounds — but replacing it with the clip the
replay stands under changed nothing, so these bakes are not inside a
recording), and the same with each inner clip edge released. What is left
to try is `getDeviceClipBounds()` at recording depth ZERO: what the clip
is when a promoted node's bake is taken under an open `saveLayer` or an
ancestor's `clipContent`, and why it is tighter than what the same node's
live paint is cut to.

`lain_navi` is a different shape and is not the clip: the promoted plate
loses the GLOW around its type — the live plate's cyan-white bloom reads
flat and green in the bake — which is a layer effect's reach rather than
a cut.

Assert once fixed: `--tier promotion` reports every scene within the rule,
and each cause gets a case in `compose_test` beside the four that pin the
ones found so far (`ComposeCache.APromotedLineKeepsTheInkThatStandsOutsideItsBox`,
`ComposeFaces.APromotedLineKeepsTheInkAFaceDrawsOutsideItsOwnMetrics`,
`ComposeCache.APromotedCurveKeepsTheCoverageItsLivePaintComputes` and
`ComposeCache.APromotedNodeDropsTheRecordingItsBakeReplaced`).

## rota_convocationis misses the raster gate on the halo it re-bakes

The scene draws one charged disc, and `--bench` fails on it at the
moments of the cycle where the disc is fully lit — the RASTER lane only.
Presented in the real window across the whole loop it holds well over the
gate, with p99 inside half the budget, and the app-FPS lane reports it
within band. At the sketch's own moment the raster lane passes
(p99 7.3 ms of the 16.6 ms budget).

THE LIBRARY QUESTION IS ANSWERED AND CLOSED. A settled node's static
layer effect is no longer rasterized inside its bake: the content goes
into one surface with the effect left out and the effect is one image
draw over it into the surface the node holds
(`src/common/compose/README.md`,
`ComposeCaching.AStaticEffectOverSettledContentIsRunOverItsBake`,
`BM_Draw_StaticGlow_*`). It is NOT applied at the blit the way a MOVING
effect is: the same arms measure a filter on a blit at a whole filter per
frame while the node turns — Skia answers one from its cache only while
the mapping that draw stands under holds still — against once per bake
here. rota's plate is byte-identical, and rota is the only scene in the
registry that takes the tier at all: the population is a node that holds
a LOCAL bake, and a node standing still bakes in device space, where a
filter would have to be re-expressed in device units to be lifted.

WHAT IS LEFT IS THE LOOK, and it is the author's. At the crest of
ignition the ring of names re-bakes on EVERY frame — its charge is a
memoized scalar and the scalar ticks — and each re-bake pays 9 ms of
content and about 70 ms of drop shadow over an 820x820 band. That
proportion corrects what this entry said before: the filter's own layer
dominates a small bake, and over a band this size the halo itself is the
cost. The run of additive blits over the disc — fringe, rays, flood, the
emblem — is about 15% of the same frame, and each of them is one blit of
its own bake with its own gain, which no compositor change can coalesce
(`BM_Draw_ChargedDisc_*` prices that shape: linear in the count of lit
elements).

So the two dials are the author's: how large a band wears a glow while
the charge is running, and how much of the disc is lit at once.
