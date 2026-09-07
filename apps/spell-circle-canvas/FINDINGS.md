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

Taken on a fresh build directory after the fix pass (head 1e23e368 of
the pass, plus this file):

- Release build: zero errors, zero warnings. `ctest`: 3230 of 3230 pass
  (four cases skip for an SDK or data set the machine lacks).
- CPU plate tier: 119 scenes byte-identical, 42 moved, 34 draw scenes
  new to the ledger, none failed. Every mover re-renders identical to
  itself and traces to a pass that named it (the fix reports name them;
  the largest are `ui_particles` on a fixed simulation step,
  `horizontal_flow` and `penrose_paving` on the kit's table and well,
  `lain_navi` and `winamp_base` on a colour mix in linear light). The
  baseline was rebased from that sweep, draw scenes included.
- Device tier: 190 of 195 within the per-channel bar. All five that were
  not are fixed: `brushwork_currents` (which crashed the GPU lane), `aero
  desktop`, `nine slice` (which drew Skia's own lattice call in a trap
  cell and now draws SigilSkia's decomposition), and — on the fence that
  keeps a scene's painting order through a destination read —
  `winamp_base` and `chevreul_circle`. The tier judges every scene.
- Promotion tier: 111 of 195 within one code value; the 84 outside are
  the standing entry below, which reports rather than gates.
- ASan with UBSan: 3230 of 3230 pass, no report. TSan: 3232 of 3232,
  no report, after the schedule library began stating the fork and the
  join a divided range takes (oneTBB arrives uninstrumented, so the
  join edge was invisible to the sanitizer; two `workaround:` lines).
- The benchmark baseline was retaken from this tree on an idle machine
  and committed; the app-FPS baseline likewise (its lane presents every
  sketch in the real window).

## Rulings for the pre-merge pass

- `nine slice` changes rather than the ledger: the trap cell that calls
  Skia's own lattice draw goes (or draws through SigilSkia's
  decomposition), the `DEVICE_DIVERGENT` list is deleted, and the tier
  judges every scene again.
- The TSan compensation in the schedule library stands.
- The Graphite draw-order defect behind `winamp_base` and
  `chevreul_circle` is fixed before the merge, at its cause, with a test.
- The painter runtime becomes per-session like the set runtime, so a
  canvas thumbnail never reaches the installed device; the p5 key table
  stays as it is.
- `Track::over` against `Annotation::unit`: the rename is done with its
  docs; the compose items declined with a reason leave this file.
- Every standing entry is worked before the merge: `pop_math`'s gate and
  the four arrange sites; the slang serial and chaucer's ceiling; the
  promotion tier's glyph edges and the blend-declaring callables; rota's
  emissive stack in the compositor.
- After the merge: the extractions first, then the file splits.
- The merge itself is a merge commit on `main` once the closing chain
  is green again with a sound FPS lane.

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
    (chaucer_astrolabe is the one the sweep could not judge — its own
    entry below.)

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

## chaucer_astrolabe cannot finish a plate under the sweep's ceiling

Under the promotion tier's five concurrent jobs it was killed at the
300 s per-scene ceiling in the held-off pass, so it is the one scene the
tier could not judge. A scene that only finishes when it has the machine
to itself is a scene no parallel sweep can gate.

Where the time goes, measured. The sketch names its moment — `captureAt`
is 22.10 s of a 26 s cycle, the earliest frame of the state the plate is
about — and a named moment is reached by REOPENING the session and
stepping to it one painted frame at a time. That is 1326 full paints of
2400x1600 to photograph one of them. With automatic promotion held off,
as a deterministic capture holds it, each of those costs about 180 ms:
1326 × 180 ms is the four minutes, and nothing else in the run is worth
naming beside it.

There is no hot spot to cut. At the capture frame the sheet paints 150
nodes and replays 913 recordings; the largest single node is the vellum
grain, a full-sheet field already declared `Cache::Texture`, whose cost
is the soft-light composite of a 2400x1600 layer and not a re-evaluation
— then the mater's sheen at a quarter of it, and a long tail of
sub-millisecond nodes. Cutting every node over a millisecond would not
halve the frame.

Two things were tried and do not work. `Cache::Group` on the limb and on
the plate makes it three times WORSE: the plate's content changes every
frame — the day, year and Chaucer traces are drawn on it — so the group
can never hold and the attempt costs a layer per frame. Declaring more
bakes does not reach it either: the sketch already declares the passes
worth declaring, and the promoter, which the deterministic capture
switches off, finds one more node.

So the multiplier is the only lever, and there are two ways at it:

  * The sweep reaches a declared moment without paying for the canvases
    it throws away. Only the last of the 1326 is photographed, and the
    sketch is a function of its own elapsed time — compose's ramps are
    absolute-time and the ticker's fixed steps sub-step a jump — so a
    single advance to the moment is the same picture. What this would
    change is what compose's PAINT-time settle counters have seen by
    then, so it must be shown to leave every declared-moment plate
    standing before it lands.
  * The declaration moves, which moves the plate, and the moment is the
    subject: the trace of 12 March 1391 is the last state of the cycle
    and there is no earlier frame of it.

Assert once fixed: `--tier promotion` and `--tier cpu` both render it
inside the ceiling at the default job count.

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
