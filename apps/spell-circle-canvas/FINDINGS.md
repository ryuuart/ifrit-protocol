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
- Benchmark baseline retaken and committed. Window-FPS baseline
  committed from the last sweep at an unlocked screen; nothing reads
  under the gate at the head (a per-sketch verification), and the two
  rows the ledger still shows under it (`pop_math`, `kumiko_asanoha`)
  were fixed after that sweep — one `sigil.py bench --lane fps --rebase`
  at an unlocked screen collects them. The lane refuses a locked
  screen, which throttles an invisible window's GPU work.

## Rulings for the post-merge pass

- The promotion tier's head (flourish, beethoven, dunhuang, the eva_magi
  plates, spacejam, lain_navi, minard, tile map) is researched now, one
  cause at a time with a `compose_test` pin each; the tail is judged
  after it.
- `volatility_cost` is made to agree: the sketch draws its verdicts in a
  way that reads the same promoted or not; no exclusion anywhere.
- rota keeps its look; the library question is taken: a settled node
  with a static layer effect is baked without it and filtered at the
  blit, with the identity test.
- The FPS ledger is retaken now at the unlocked screen.

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

## rota_convocationis misses the raster gate on pixels, not on layers

The scene draws one charged disc, and `--bench` fails on it at the
moments of the cycle where the disc is fully lit — the RASTER lane only.
Presented in the real window across the whole loop it holds well over the
gate, with p99 inside half the budget, and the app-FPS lane reports it
within band.

WHAT THE COMPOSITOR ALREADY DOES, contrary to what this entry said
before: none of the emissive elements composites through a layer. A
fill-only leaf routes its blend and opacity onto the fill paint
(`leafDirectBlend`) and a `Cache::Texture` node routes them onto its blit
(`deferBlendToBlit`), so a lit element costs one additive blit of its own
bake and nothing else. `BM_Draw_ChargedDisc_*` in `compose_bench` prices
that shape: the cost is linear in the count of lit elements because each
one is a distinct blit of a distinct bake, and there is no layer to
coalesce — every stack carries a gain of its own, so no two of them can
share a bake either.

WHERE THE RASTER FRAME ACTUALLY GOES, from `COMPOSE_PROF` and from
ablating a copy of the sketch:

- the additive stack over the disc — one anonymous full-canvas group
  whose own paint is the run of blits — is about three quarters of the
  frame at the moments the disc is fully lit;
- `nomina`, the ring of names, is the rest. Its bake is taken once and
  held, and the bench's node table reports the ONE profiled frame, which
  is a frame that re-bakes it — so the row that named it is a bake and
  not a per-frame cost. Removing its `styles::textGlow` alone takes that
  node from tens of milliseconds to under one, and a sigma of 0.5 costs
  nearly what a sigma of 6 does: it is the layer the filter needs over
  that node's whole band, not the blur's own arithmetic.

So a frame's cost already tracks the pixels the scene touches. What is
left is a LOOK decision the sketch's author owns — how much of the disc
is lit at once, and how large a band wears a glow — beside one library
question worth its own measurement: whether a held bake can wear a STATIC
layer effect at its blit the way a deferred one wears a moving one, which
would make a glow-wearing node that re-bakes pay the bake alone.

Assert if that is taken: a node whose content is settled and whose layer
effect is static is baked with the effect left out and filtered at the
blit, and its picture is identical to the same node filtered inside the
bake.
