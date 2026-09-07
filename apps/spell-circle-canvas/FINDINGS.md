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

Taken on the merged tree after the post-merge compose pass:

- Release build: zero errors, zero warnings. `ctest`: 3252 of 3252.
- CPU plate tier: 195 scenes; the six that moved in the post-merge
  pass were rebased with their causes (`volatility_cost` reads a
  composer of its own; the eva_magi plates, chaucer, dunhuang and
  minard by a few code values where a device bake now carries the
  margin a stroked curve's rasterisation route needs).
- Device tier: 195 of 195 within the per-channel bar. Promotion tier:
  126 of 195 within one code value, twenty more at the second value the
  contract allows; the rest is the entry below.
- ASan with UBSan and TSan: clean over the suite at the merge.
- Benchmark and window-FPS baselines retaken at an unlocked screen:
  195 sketches, none under the gate. The FPS lane refuses a locked
  screen, which throttles an invisible window's GPU work; one heavy
  first frame following another session (`dunhuang_star_chart`) stood
  down once in a sweep and measured at the display's rate alone.

## Rulings

- The promotion head is researched further along its named probe, one
  cause at a time with a pin each; the tail is judged after.
- The promotion tier's ceiling reads the contract as written: one code
  value over transparent black, two where the bake lands on content.
- rota's raster crest is the author's look and leaves this file.
- The extractions campaign is staffed one agent per library seam, in
  sequence: solids and pose with the conic and silhouettes (geometry),
  the projections (geometry path), the pentagrid and joinery
  (lattice), tartan (material), particles (motion), sprites and the
  atlas packer (compose kit), the HTML table layout (compose layouts),
  the Motif bevels (chrome); each rewrites the sketches that carried
  the hand-rolled version and names its plate movers.

## Deferred past the merge, each a campaign of its own

- The library extractions the sketch review names — the mechanisms
  hidden in the sketches over 1000 lines: palette-indexed sprites and an
  atlas packer (`xcom_battlescape`, `cde_motif`, `thaumonomicon`) →
  `compose/kit/Sprites.h`; an HTML auto-table layout scheme
  (`spacejam_1996:999-1200`) → compose kit layouts with a test; Motif
  bevels (`cde_motif`, `twoadvanced_v4`, `winamp_base`,
  `fallout2_charsheet`) → `Chrome.h`; a stereographic and
  a chart projection (`chaucer_astrolabe`, `dunhuang_star_chart`) → a
  projection value in geometry path; a tartan sett-to-cloth weave
  (`black_watch:234-295`) and the orthographic-sphere navball with the
  bright pass over it (`ksp_mapview:359-457`) → material pattern and
  material stock; a pentagrid
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

