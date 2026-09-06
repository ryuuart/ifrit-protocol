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

## Automatic texture promotion moves 84 of 161 plates past one code value

`sigil.py plates --tier promotion` renders every scene twice on the CPU —
once with promotion off, once EAGER (every node the promoter's rules
admit, baked from its first frame) — and differences the pair. The rule
is in `src/common/compose/README.md`: a promoted node paints the picture
its live paint paints, within one code value per channel over transparent
black and two where the bake lands on content, and a scene that moves
further is a defect in compose rather than a plate to rebase.

The eager policy is what makes these numbers a measurement rather than a
floor: the cost rule is a stopwatch, so before it the tier reported
whatever the machine's load happened to promote — nothing at all on an
idle one. Every number below was taken on one binary, hashed either side
of the run, five jobs, Release.

    161 scenes, 48 within one code value.
    Two causes fixed → 77 within. Eighty-four still move.

The worst that remain, max channel first:

    flourish 244 · axis_ripple 237 · beethoven 228 · volatility_cost 228 ·
    annotated_margin 221 · winamp_base 218 · paragraph_sheet 217 ·
    dunhuang_star_chart 216 · mawarikomi 213 · ruby_kenten 211 ·
    nightingale_coxcomb 206 · black_watch 199 · chaucer_astrolabe 199 ·
    sigillum_aemeth 196 · chladni_tab1 195 · minard_1869 194 ·
    twoadvanced_equipment 194 · stroke_atlas 190 · eva_magi_interior 190 ·
    eva_magi_deliberation 188 · spacejam_1996 185 · eva_magi_defense 174 ·
    lain_navi 164 · tile map 161 · twoadvanced_v4 143 ·
    kumiko_asanoha 137 · twoadvanced_v3 128 · tategaki 119 ·
    coverage_boundary 118 · y2k chrome 111 · cde_motif 102 ·
    thunder_fulu 96 · cjk_rules 90 · encode_write 90 · half_float 90 ·
    spacing_passes 90 · svg_silhouette 90 · substance_swatches 89 ·
    env_lanes 87 · exact_tangent 87 · …and forty-three more at 87 or less

`volatility_cost` is not a defect and wants an exclusion by name: the
study DRAWS the runtime's own caching verdicts, so a promoted run is
meant to read differently.

WHAT IS LEFT IS ALMOST ALL TYPE, and that is the next thing to find. On
`half_float` (90) the difference is confined to the text — the title, the
subtitle, the row of notes, every cell's label, the caption — and every
picture on the sheet is byte identical. The differing pixels are glyph
EDGES, tens of code values apart on a few of them, which is a glyph
rasterized from a different mask rather than a glyph moved. The same
number recurs across unrelated scenes (90 on five, 87 on eight), so it is
one drawing shared by the sketch kit's chrome rather than a per-sketch
accident.

What has been ruled out, each pinned in `core/test/ComposeTestKernel.cpp`
where it passes: a line of type promoted as a node of its own, over an
opaque ground, at a plate's own view scale and fractional host
translation, is exact — near the canvas origin and two thousand device
pixels into it alike. So it is not the bake's integer offset, and not the
magnitude the glyph positions are computed at.

Assert once fixed: `--tier promotion` reports every scene within the rule,
and the cause gets a case in `compose_test` beside the ones that already
pin it.

## Two causes of the same shape are fixed, and a third of it is open

Both were the same defect: SOMETHING COMPOSITES WITH THE CANVAS AND THE
LIBRARY COULD NOT SEE IT, so the node was baked and the blend resolved
against the layer's transparent black.

  · A `custom()` leaf is handed the canvas and may draw with any blend
    mode, and `picture()` is built on one, so a recorded picture holding a
    plus-blended glow was baked away from the page beneath it. Fixed: a
    node holding a paint program of its own reads the backdrop
    (`core/Volatility.cpp`). Twenty-nine scenes came within the rule.
  · A decoration paints through a blend mode of its own — a soft-light
    wash, an additive halo on a layered brush, a multiply scanline — and
    nothing declared it. Fixed: `Decoration::blends()`, beside
    `isAnimated()`, `bleed()`, `reach()` and `borrows()`. It closed the
    hole; it moved no scene in the eight-scene subset it was measured on.

THE THIRD IS OPEN AND IS THE SAME SHAPE. A `Brush`, a `Silhouette`, a
`Material` program and a `LayerStyle`'s own painter are all callables the
same argument reaches: any of them may draw through a blend mode this
analysis cannot see. Only decorations and `custom()` declare it today.

Assert once fixed: a node whose brush, silhouette or material program
blends with the page is refused the automatic bake, and a case in
`compose_test` renders it promoted and live and finds the two identical.

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
renders more than one scene per process. Today the plates verb opens
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
