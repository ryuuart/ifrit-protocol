# Sketch refinement

## Completed visual changes

- Evangelion defense: cap-height-based label fitting, wider Helvetica numerals, corrected diagonal T-module placement, restored support-line crossbars, stronger alarm capsules, continuous phosphor bloom and subtler tube texture.
- Evangelion voting plates: serif MAGI wordmark, regular computer labels, oblique BALTHASAR numeral, heavier Japanese Mincho fallback, and mint panels that retain their colour under bloom.
- Shared Evangelion visual choices use existing Skia Gaussian filters and SigilMaterial bright-pass and CRT primitives.
- Removed the private sparse-texture clipping grid. It cut the interior plate's frame and circular bus into fragments when the warmed canvas was captured at a fractional scale. Local textures now draw as one image.
- Removed three tests of the deleted clipping implementation and added a visual regression asserting that the cached frame and ring retain their bright pixels at a fractional capture scale.

## Validation

- Release compose_test and Sketchbook built.
- 97 ComposeCache and ComposeCaching cases passed, including the new fractional-capture regression.
- All three Evangelion built-in sketches rendered at their declared moments with the ledger path. The corrected interior frame was visually inspected after warm-up.
- Scoped formatting and git whitespace checks passed.
- Current Evangelion GPU artifacts: `/tmp/sketch-directory-sweep/plate_eva_magi_defense.png`, `/tmp/sketch-directory-sweep/plate_eva_magi_deliberation.png`, `/tmp/sketch-directory-sweep/plate_eva_magi_interior.png`.
- Defense reference mean absolute RGB error at 1920x1080 decreased from 30.94 to 20.83 before the final support-line correction. This is a comparison aid, not a claim of exact reproduction.

## Further refinement

- All 31 oversized sketch implementations now use cohesive files beside a named directory entry. The complete sketch tree has no C++ source or header above 1000 lines, including after formatting.
- The catch-all shared directory is removed; its helpers now belong to genesis_fire, twoadvanced_v3, tategaki and eva_magi_interior. Local quoted includes are watched across owner directories.
- Removed more than 500 lines of unreachable MAGI portrait artwork. The infection now uses one raw numeric image for both coverage and rendering; its actual rendered coverage passes the square-module audit. Repeated captures also retain the audit overlay and underlying artwork after warm-up was changed to a full-size non-rasterizing canvas.
- Continue catalog-wide refinement and extract useful repeated algorithms into their owning libraries.

## Directory ownership pass

- The first 18 of the original 31 oversized sketches use focused files: the two large Evangelion studies, genesis_fire, twoadvanced_v3, loot_grid, black_watch, astral_tome, stroke_atlas, penrose_paving, bg3_dice_roll, cde_motif, lain_navi, ds2_bench, kumiko_asanoha, xcom_battlescape, thaumonomicon, world_hud and spacejam_1996.
- Genesis and 2Advanced v3 use separate compiled units for simulation/artwork/panels and assets/frame/sections. Their entry files are 211 and 179 lines before formatting.
- 10 source discovery and reload tests passed, including cross-directory ownership, nested headers, missing headers and cycles.
- Release Sketchbook built for the Evangelion/Genesis/2Advanced split; all three rendered and were visually inspected in `/tmp/sketch-owners-final/`. All 31 directory sketches subsequently built together in the complete Sketchbook target.
- All 196 source registry entries remain present and every feature-coverage link resolves. Formatting and QML checks pass. The full Vulkan GPU sweep rendered all 196 plates successfully. All seven catalogue contact sheets were visually reviewed. The directory reload test, named registration reload and three-frame viewport integration test pass; Genesis live-compiled all four units and rendered.

The final 13 splits are chevreul_circle, thunder_fulu, chaucer_astrolabe,
minard_1869, fallout2_charsheet, hitman_verlet, ksp_mapview,
rota_convocationis, sigillum_aemeth, slitscan_2001, twoadvanced_v4,
winamp_base and dunhuang_star_chart. Their units separate the mathematical
model, reference artwork, explanatory panels and scene lifecycle.

## Capture and video integration

- Capture warm-up uses the declared viewport instead of an 8x8 raster. Three fractional-scale MAGI captures preserve the frame, modules and audit table. A live integration fixture checks the viewport during warm-up and capture.
- Metal texture imports refuse non-Metal recorders before constructing backend textures. Video caches an unsupported import for each frame and recorder, then uses its CPU conversion fallback. Native Metal decode/wrapping still passes.
- Ten video decode/device cases and nine Skia texture-wrap cases passed. The Vulkan video plate rendered without repeated YUVA wrapping warnings and was visually inspected.
- The named sketch reload test now points to the world_hud directory entry.

## Catalogue readability

- The brush engine atlas compares one pass with six passes at legible pressure and weight. Each labelled tool now has a visible sample, with enough room for its pigment spread.
- Fibonacci rectangles fit their actual drawn bounds and stay centered at different sequence lengths; the pen already supports signed rectangle sizes.
- The data_scales sketch demonstrates every Scale transform through one Mapping component. The catalogue now contains 197 entries and the feature coverage map includes the new example.
- Dunhuang uses the existing checked, dynamically sized formatter instead of a local fixed buffer.
- Decay, elastic typography and the scale showcase now use the existing curvePlot component instead of repeating curve sampling and drawing loops. The elastic reference keyframe marks remain visible; the spring plot includes its overshoot crest.
- The data README probes and catalogue/stem checks pass. The Dunhuang formatter change is pixel-identical in RGB on the GPU.

## Preview ownership

- Thumbnail keys now follow the same quoted-include dependency closure as live reload, including nested headers owned by another sketch. Unrelated neighboring headers do not invalidate a borrowed dependency.
- Nine ThumbnailStore cases pass, including the bare-file and directory-sketch regression. Release Sketchbook and sketch_test rebuilt successfully.

The findings queue is empty after the scoped visual, ownership and integration review. The full GPU sweep covers the original 196 entries; data_scales was added and GPU-rendered separately. Subsequent plot refinements were rendered on raster and visually inspected. This pass did not repeat the full sanitizer, benchmark, window or multi-tier plate lanes.
