# Findings

Defects found while working, stated as what the code does, what it was
evidently intended to do, and what a test should assert once intent is
restored. A work queue: delete an entry when it is fixed, and delete this
file when it is empty.

## rota_convocationis is over its budget after its opening

`--bench --sketch rota_convocationis` passes on the sketch's own declared
moment (p99 11.3 ms of a 16.6 ms budget), and the twelve sub-seals no
longer add to the per-frame cost — the seal cycle grows the frame by 3 ms
where it grew it by 18. What the entry that stood here did not cover:
from about six seconds in the plate is over budget anyway, and by the
seal cycle's end it sits near 35 ms with no single node responsible.
`--bench --at 9`, `--at 12` and `--at 15` report p50 31.5 / 34.0 /
35.3 ms.

The cost is a long tail of full-canvas live paints — the emissive stacks
(`stella`, `arcus`, `star-lit`, `inner-lit`, the `rim-lit` and `nom-lit`
grades), each a `Baked` path filled at `kPlus` over the whole 1280 px
canvas, four per glow — plus three text rings that replay a picture every
frame (`vox`, `registrum`, `textura`) because their placement is driven
by a live path phase. The names' band was the fourth and is fixed: it is
turned as a body and baked. The same conversion on the other three is NOT
a win as things stand — a ring turned by a bound rotation is
`transformLive`, so its bake is held in local space and the blit resamples
a 650–1000 px image through the rotation, which costs more than the
replay it replaced.

Intended: either the emissive grades are cheap enough that twenty of them
fit in a frame, or a ring turned by a declared rotation can hold a bake
the blit does not resample. The second is the library half and the more
useful one: a device-space bake is refused to a node whose transform is
live, and yet a rotation about the node's own centre moves no pixel of
the bake's CONTENT — only where it lands.

Assert once fixed: `--bench --at 9`, `--at 12` and `--at 15` all verdict
PASS at 1280x1280, and the per-frame report shows no full-canvas live
paint in the emissive stacks.

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
