# Findings

Defects found while working, stated as what the code does, what it was
evidently intended to do, and what a test should assert once intent is
restored. A work queue: delete an entry when it is fixed, and delete this
file when it is empty.

## rota_convocationis is over its budget after its opening

`--bench --sketch rota_convocationis --at 9`, `--at 12` and `--at 15`
report p50 21.4 / 19.8 / 25.4 ms and p99 24.7 / 26.6 / 30.9 ms against a
16.6 ms budget, so the plate holds 60 FPS on its own declared moment and
nowhere in the second half of the loop. That is down from 32.6 / 34.0 /
35.3 ms p50, and the two things that came off it are done: a bake is now
blitted only where its ink is, and each lighting group is one bake the
gain rides rather than four additive fills of the sheet.

The entry that stood here named a cause that measurement does not
support, and the correction is worth keeping. A ring turned by a bound
rotation was NOT being re-baked per scale rung — a rotation cannot move
the ladder, and the bake was taken once. Nor can such a node hold a bake
the blit does not resample: a rotation moves every pixel of the content
it turns, and the only bake that is not resampled is one pinned to the
device rect, which a turning node leaves every frame. What was true is
the cost: the resample ran over the whole square of a bake whose ink is a
band, which is what the ink grid now skips.

What is left is a long tail with no single owner. At 15 s the whole frame
is 23 ms over about two hundred painted nodes, of which the largest are
the name ring's blit (2.4 ms), three text rings replaying a picture every
frame because a live path phase re-places their glyphs (`monogramma`
1.6, `registrum` 1.3, `textura` 1.2), and the star compound's two visible
morph steps, each an additive stroke of a twelve-pointed compound over
the sheet (2.3 ms each at 9 s). Nothing there is a defect; halving it is
a pass over the sketch, not a fix.

Assert once fixed: `--bench --at 9`, `--at 12` and `--at 15` all verdict
PASS at 1280x1280.

## Ring and grid placement is respelled where geometry already has it

`geometry::arrange::{along, onEllipse, onRing, cellAt, cellRect,
moduleSize, step}` (`sigilgeometry/path/Arrange.h`) is the canonical
ring-and-grid arithmetic, and `compose/kit/Placers.h` and
`compose/kit/Layouts.h` both state in their own file comments that they
must delegate to it rather than respell it.

Every RING under `src/sketch/sketches/`, and every GRID that indexes a
cell or sizes a module from its container, now reaches for it. What is
left is the third kind the same header owns: a run of `n` values evenly
spread over an extent, with no circle and no cell —
`t = (float)i / (float)n`, `x0 + (x1 - x0) * i / (n - 1)`,
`extent / count` — which is `arrange::step` and `arrange::along` with
`Turn::Open` or `Turn::Closed`. About seventy such sites stand in these
fifty-one files:

    axis_ripple, black_watch, blend_options, bound_lane, bristle_bloom,
    bristle_current, brush_botanical_study, brush_dynamics,
    chaucer_astrolabe, chevreul_circle, dart_flight, decay_step,
    dunhuang_star_chart, elastic_type, eva_magi_interior,
    floating_panels, frame_grid, frame_inputs, genesis_fire, half_float,
    hit_slots, import_native, ksp_mapview, lain_navi, lane_retarget,
    material_lab, matte_luma, mesh_generators, minard_1869,
    observable_l_system_tree, ocio_view, p5_fractal_garden,
    p5_liquid_layers, painter_gpu, rota_convocationis, scene_surfaces,
    sigillum_aemeth, slitscan_2001, spacejam_1996, sticker_collection,
    stroke_atlas, thaumonomicon, thunder_fulu, ticker_lanes,
    twoadvanced_v3, twoadvanced_v4, vagrant_story_target,
    vertigo_titles, winamp_base, world_hud

Intended: one rounding. The two spellings do not agree to the pixel, so
this is not only duplication — it is the drift those file comments warn
against, and adopting `arrange` moves plates by sub-pixel wherever a
sketch is converted. It is therefore a per-sketch judgement with the
cause in each commit, not a sweep.

Seven sites are deliberately NOT arrange's and say so in a comment where
they stand: a container measured back from its module and gaps is
`moduleSize` run backwards and the header does not carry that
(`loot_grid`, `substance_swatches`, `eva_magi_defense`), a measure that
takes the outer margins out of the width as well is not a module
(`card_flip`), a run that fills one row and then continues along the
second without wrapping again is not `cellAt` (`stroke_atlas`), an
analyser indexed column-major with its rows counting up from the floor
is neither thing `cellAt` answers (`winamp_base`), and five roots of
unity solved in double would move every rhomb of a tiling if rounded to
float (`penrose_paving`).

Assert once fixed: the converted sketch's placement is `arrange::`, and
its plate is rebased in the same commit that converts it.

## Automatic texture promotion moves 26 plates past the one code value it may cost

`scripts/plate_ledger.py --tier promotion` renders every scene with
automatic texture promotion held off and again with it on and differences
the pair. A promoted node is baked under the live matrix post-translated
by an integer, and inverting that matrix at a scale whose reciprocal is
inexact does not cancel the integer to the last bit, so a shaded pixel
may land ONE code value from the live paint and nothing may land
further. Over the registry, 134 of 160 comparable scenes stand within
that. Twenty-six do not, worst channel first:

    eva_magi_deliberation 253 (mean 31.34, p99 253) · volatility_cost 228
    · spacejam_1996 213 · eva_magi_interior 191 · aero desktop 127 ·
    thunder_fulu 88 · blur_falloff 70 · ksp_mapview 68 · lain_navi 62 ·
    chladni_tab1 28 · ds2_bench 16 · thaumonomicon 13 · winamp_base 5 ·
    twoadvanced_v4 5 · kumiko_asanoha 4 · vertigo_titles 3 ·
    twoadvanced_v3 3 · sigillum_aemeth 3 · fallout2_charsheet 3 ·
    world hud 2 · pop_stamps 2 · path_booleans 2 · gerstner grid 2 ·
    floating_panels 2 · daemon console 2 · chevreul_circle 2

`eva_magi_deliberation` is the one whose MEAN moves: the two pictures
differ over the whole frame rather than at a few pixels, which is a
different fault from the rest and the place to start.

`volatility_cost` is not a defect and should be excluded by name once
the rest are understood: the study DRAWS the runtime's own caching
verdicts, so a promoted run is meant to read differently.

Intended: a promotion is invisible but for rounding. What the tier
measures is the promoter's whole correctness surface, and until now
nothing headless exercised it at all — every plate is rendered with the
feature switched out.

Assert once fixed: `--tier promotion` reports every scene within one
code value, and each cause gets a case in `compose_test` beside the four
in `core/test/ComposeTestKernel.cpp` that already pin the rule.

## chaucer_astrolabe cannot finish a plate under the sweep's ceiling

Under the promotion tier's five concurrent jobs it was killed at the
300 s per-scene ceiling in the held-off pass, so it is the one scene the
tier could not judge. It renders alone, and the CPU tier's baseline
holds a line for it, so this is contention rather than a hang — but a
scene that only finishes when it has the machine to itself is a scene no
parallel sweep can gate.

Assert once fixed: `--tier promotion` and `--tier cpu` both render it
inside the ceiling at the default job count.

## slang_portable's plate draws a counter over the process's history

The sketch's "source that is not Slang" cell prints the compiler's own
diagnostic verbatim, and that text names the module it failed on:
`SigilProgram14.slang`. The name is built in
`material/slang/SlangCompiler.cpp` from a function-local `static uint64_t
serial` incremented once per `compileModule()` call, so the number counts
every module the PROCESS has compiled, not the three this sketch
compiles. It reads 14 only because eleven others were built before it in
that session.

The plate is judged on byte identity, so anything that changes how many
modules are compiled ahead of this one moves its hash without moving
anything the sketch is about: a warm-up gaining or losing a recipe, the
live host opening on a file after another sketch, or a sweep that ever
renders more than one scene per process. Today `plate_ledger.py` opens
one process per scene and the number is stable, which is the only reason
this is latent rather than a flapper.

Intended: a plate shows what the sketch demonstrates — here, that a body
which cannot compile says why. The renderer already has the pin for a
number a sketch measures about its own execution (`ctx.measured(value,
pinned)` under `ctx.deterministic`), and the module serial in a
diagnostic is exactly that kind of number.

Assert once fixed: the cell's text is identical whether the sketch is
rendered alone or after another sketch has compiled a module in the same
process — either by pinning the serial through `measured()` or by
stripping the generated module name out of the diagnostic before it is
drawn.
