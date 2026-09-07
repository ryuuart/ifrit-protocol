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
  after it.
- `volatility_cost` is made to agree: the sketch draws its verdicts in a
  way that reads the same promoted or not; no exclusion anywhere.
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

## Automatic texture promotion moves 74 of 195 plates past one code value

`sigil.py plates --tier promotion` renders every scene twice on the CPU —
once with promotion off, once EAGER (every node the promoter's rules
admit, baked from its first frame) — and differences the pair. The rule
is in `src/common/compose/README.md`: a promoted node paints the picture
its live paint paints, within one code value per channel over transparent
black and two where the bake lands on content, and a scene that moves
further is a defect in compose rather than a plate to rebase.

    195 scenes, 120 within one code value.
    (chaucer_astrolabe was the one the sweep could not judge inside its
    ceiling; it renders in seconds now and moves by 108.)

ONE CAUSE IS FIXED and it was the one this entry was written about: a
text leaf reported the band of its lines as its paint bounds, and the ink
a face draws outside that band — a comma's tail, an accent — was cut by
the bake. That is why so many unrelated scenes reported the SAME maximum:
they share the sketch kit's chrome type. The class it accounted for (90
on five scenes, 87 on eight) is gone; `half_float`, `spacing_passes`,
`encode_write`, `axis_ripple`, `winamp_base`, `tategaki`,
`nightingale_coxcomb`, `twoadvanced_*`, `cde_motif` and `env_lanes` are
now within the rule, and `annotated_margin` (221 → 2), `ruby_kenten`
(211 → 2), `mawarikomi` (213 → 12), `y2k chrome` (111 → 2), `chladni_tab1`
(195 → 28), `paragraph_sheet` (217 → 83), `black_watch` (199 → 82) and
`stroke_atlas` (190 → 105) each lost a whole cause.

A SECOND CAUSE IS FIXED: a device bake was allocated to exactly what the
node paints, and Skia decides whether a path needs its CLIPPED
rasterisation from the path's control-point bounds — which for a curve
stand outside the ink it draws, about a hundredth of the curve's own
extent for the cubics a stroker approximates an offset with. So every
stroked curve was baked on Skia's clipped route and painted live on its
unclipped one, and the two do not answer the same antialiased coverage:
tens of code values along the whole length of the curve, which on a
high-contrast plate is the difference between the two inks. Every device
bake now carries a margin (a thirty-second of its larger side, never less
than two pixels), so what bounds the drawing is the clip the bake carries
in and never the allocation. Pinned by
`ComposeCache.APromotedCurveKeepsTheCoverageItsLivePaintComputes`. On the
narrowed head: `flourish` 244 → 8, `eva_magi_defense` 174 → 1 (within),
`eva_magi_deliberation` 188 → 2; `beethoven`, `dunhuang_star_chart`,
`eva_magi_interior`, `lain_navi`, `minard_1869` and `spacejam_1996` did
not move and are a different cause.

WHAT REMAINS, max channel first:

    flourish 244 · volatility_cost 228 · beethoven 228 · nine slice 221 ·
    dunhuang_star_chart 216 · eva_magi_interior 190 ·
    eva_magi_deliberation 188 · spacejam_1996 185 · lain_navi 176 ·
    eva_magi_defense 174 · minard_1869 163 · tile map 161 ·
    sigillum_aemeth 125 · coverage_boundary 118 · stroke_atlas 105 ·
    kumiko_asanoha 89 · hit_slots 86 · paragraph_sheet 83 ·
    black_watch 82 · thunder_fulu 77 · material_child 66 ·
    blur_falloff 55 · thaumonomicon 43 · fx_scatter_mix 37 ·
    chevreul_circle 35 · astral_tome 35 · chladni_tab1 28 ·
    aero desktop 23 · …and forty-six more at 16 or less, thirty-one of
    them at 6 or less

Two of those are known and not this entry's: `volatility_cost` DRAWS the
runtime's own caching verdicts, so a promoted run is meant to read
differently and it wants an exclusion by name; `nine slice` draws its
difference on purpose and has its own ruling.

WHAT TO LOOK AT NEXT. The tail — thirty-one scenes at 6 or less — is a
different shape from the head: on `svg_silhouette` (6) the differing
pixels are the APEX of a filled triangle and one pixel of a rule beside
it, an antialiased corner resolving one way in the bake and another live,
where the glyph cut was a whole coverage value. The head (flourish,
beethoven, the eva_magi trio, dunhuang) is untouched by the type fix and
has not been read yet.

Assert once fixed: `--tier promotion` reports every scene within the rule,
and each cause gets a case in `compose_test` beside the two that pin the
type one (`ComposeCache.APromotedLineKeepsTheInkThatStandsOutsideItsBox`
and `ComposeFaces.APromotedLineKeepsTheInkAFaceDrawsOutsideItsOwnMetrics`).

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
