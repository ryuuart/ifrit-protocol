# Findings

Defects found while working, stated as what the code does, what it was
evidently intended to do, and what a test should assert once intent is
restored. A work queue: delete an entry when it is fixed, and delete this
file when it is empty.

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

## Automatic texture promotion still moves 23 plates past one code value

`scripts/plate_ledger.py --tier promotion` renders every scene with
automatic texture promotion held off and again with it on and differences
the pair. The rule the promoter is held to is now stated in
`src/common/compose/README.md`: a promoted node paints the picture its
live paint paints, within one code value per channel, and a scene that
moves further is a defect in compose rather than a plate to rebase.

Over the twenty-five filed scenes, judged again on one binary with five
jobs, twenty-three still move. Worst channel first:

    spacejam_1996 213 (mean 0.58, p99 11) · eva_magi_interior 189 ·
    blur_falloff 70 · ksp_mapview 68 · lain_navi 62 · chladni_tab1 28 ·
    ds2_bench 16 · thaumonomicon 13 · winamp_base 5 · twoadvanced_v4 5 ·
    kumiko_asanoha 4 · sigillum_aemeth 3 · fallout2_charsheet 3 ·
    vertigo_titles 3 · twoadvanced_v3 3 · aero desktop 2 ·
    chevreul_circle 2 · daemon console 2 · floating_panels 2 ·
    gerstner grid 2 · path_booleans 2 · pop_stamps 2 · thunder_fulu 2

`eva_magi_deliberation` (253, and the only mean that moved) came within
the rule when the ink clip stopped being computed in a recording's own
space, which is a different defect: the UNPROMOTED render was the wrong
one, losing whole blocks of every bake blitted inside a recording, and
its plate was the picture of that. `world hud` reports 0 on this run.

WHAT THE PROMOTER'S NUMBERS DEPEND ON. Promotion fires on a stopwatch —
a node must cost more than a millisecond to replay for eight consecutive
frames — so an idle machine promotes nothing and the tier reads 0 for
every scene. Every number here was taken with the machine loaded, which
is what five concurrent jobs do, and a scene's max moves by a few code
values between runs because a different set of nodes crosses the bar.
The tier is therefore a floor on the drift, never a measurement of it,
and nothing headless can pin a scene until the promoter can be asked to
promote everything it is allowed to.

WHAT IS ESTABLISHED ABOUT `spacejam_1996`, the largest that is left, and
the shape the rest are likely to share. A per-key kill switch on the
promoter (temporary, not committed) attributes ALL of its difference to
whole-subtree promotion: with every promotion refused the plate is byte
identical to the held-off one, and with only the `starfield` node
refused the difference collapses from 1.26 M pixels to 188 k, all of it
inside the `table` node's own rect. The starfield's bake is taken at
device rect 0,0,2400,3000 under a matrix identical to the live one —
scale 1.875, translation zero, so not even an integer offset separates
them — and its pixels still differ by up to 42 where the tile it samples
is partially transparent, and not at all where the tile is opaque. The
diff is a ring around every star, repeating once per 416 px tile: the
same difference in every repeat of one image, which is a different read
of that image rather than noise.

Two cases in `core/test/ComposeTestKernel.cpp` pin the shape that does
NOT reproduce it — a magnified tile of hard-edged marks under a
promoted node, at a fractional host scale and over the plate's own
warm-then-photograph path, both exact. So the next experiment is the
node itself: dump the bake's PREMULTIPLIED pixels (a PNG round-trip
loses them wherever alpha is partial, which is exactly where the
difference is) and compare against the same node painted live into a
raster surface of the same size.

`volatility_cost` is not a defect and wants an exclusion by name: the
study DRAWS the runtime's own caching verdicts, so a promoted run is
meant to read differently. It is left out of the list above.

Assert once fixed: `--tier promotion` reports every scene within one
code value, and each cause gets a case in `compose_test` beside the six
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
