# Merge-readiness review — the sketches

Branch `sigil/library-campaigns` against merge base `aabd3fe1b224`; scope
`apps/spell-circle-canvas/src/sketch/sketches/` (195 sketches, 3 of them
directories, plus `shared/`), read against `src/sketch/kit`,
`src/sketch/include/sigilsketch`, `src/common/compose/include/sigilcompose/kit`
and the geometry/material/motion/measure headers. Read-only: no build, no
run, no edit. Every `path:line` below was opened and read; paths are relative
to `apps/spell-circle-canvas/`.

What could not be verified from disk: the cpu-tier manifest
`build/plate_baseline_Release.sha256` exists in neither `build/` nor
`build-prev/` (both `plates_Release/baseline/` directories hold two entries),
so "no cpu-tier baseline" is judged from what the ledger *can* sweep, not from
a manifest. `build/plates_Release/cpu/` holds 161 plates — one per canvas and
set sketch, none for any draw sketch (see finding 1).

## Ranked top ten

1. should-fix | `scripts/sigil/plates.py:39` | `KINDS = ("canvas", "set")` — the plate ledger never opens a draw sketch, so the 34 draw sketches (`genesis_fire`, `hitman_verlet`, `psx_doom_fire`, 9 `brush_*`/`bristle_*`, 15 `observable_*`, 7 `p5_*`) have no cpu plate, no baseline and no self-difference check; Sketchbook already accepts `--kind draw` (`src/sketch/book/main.cpp:26`) and the draw runtime is seeded per session (`src/sketch/include/sigilsketch/draw/Draw.h`, "random starts from the same seed in every session") | the ledger should hash every kind it can step deterministically | add `"draw"` to `KINDS` and rebase.
2. should-fix | `src/sketch/sketches/import_native.cpp:139-147` | `settled()` polls `view.frameVersion()` against a `steady_clock` 15 s deadline with `sleep_for(16ms)` and returns `published > 0` either way, so a page the web engine has not finished laying out after 15 s of wall time is what the set wears and photographs | a plate must be a function of the declaration, not of how long the machine took | wait on a frame-version *count* (or the engine's own settled signal) with no wall-clock exit, and fail the render rather than return an unsettled frame.
3. should-fix | `src/sketch/sketches/ui_particles.cpp:440-444` | `ticker.add([this](double dt){ step(chips, dt, …); step(posts, dt, …); })` integrates positions by the frame delta (`p.x += v.dx * dt`, line 389); the sweep steps at a fixed size so the plate is stable, but the window, `--video` and `--bench` all run a different simulation | the framework's own rule (`Draw.h` on `addFixed`; `psx_doom_fire.cpp:730`, `xcom_battlescape.cpp:1902`, `genesis_fire.cpp:1539` follow it) is a fixed step with an interpolant | `ctx.ticker.addFixed(60.0, …, 8, &alpha)` and lerp the pool from previous/current.
4. should-fix | `src/sketch/sketches/rota_convocationis.cpp:2093` | `ctx.plate()` on a 1280×1280 looping animation (`update()` at 2384 steps `stepFire`/`stepEmbers` every frame; the comment at 2088-2092 says the mark is there because the emissive stack cannot hold 60 FPS) — `CanvasSpec::plateOnly` is documented as "a sketch whose subject is the size of the sheet it draws … never a timeout override" | a live scene that misses the frame gate is a finding against the scene or the renderer, not a declaration | remove the mark and either cut the emissive layers to what presents at 60 FPS or file the cost against the compositor.
5. should-fix | `src/sketch/sketches/xcom_battlescape.cpp:1690-1814, 1963-1974` | `printAudit()` writes a fifteen-`std::printf` verification block to stdout on every setup, and `update()` prints `composer.stats()` for three frames; the verdicts are hand-formatted strings, so a line reading "correct" cannot fail anything | `sigil::measure::Check`/`Table` (`src/common/measure/include/sigilmeasure/check/Check.h:53,201`) compute the line from the two values and can fail a build; five sibling studies already use them | build a `measure::Table`, render it through `sketch::kit::table`, and drop the stdout dump.
6. should-fix | `src/sketch/sketches/twoadvanced_v3.cpp:340-440` | `extractRivImages()` walks a `.riv` byte stream for PNG signatures and recovers asset names from printable runs — a container-format reader written inside a sketch | resource *access* is SigilIO's and image *meaning* is SigilImage's (CLAUDE.md boundary); a Rive-container asset reader belongs beside the other decoders | move it to `src/common/image` (or an io mount that exposes the container's images) and have the sketch ask for `res://…/mainstage.riv#cloud 12`.
7. should-fix | `src/sketch/sketches/hitman_verlet.cpp:516-560` | `verlet()`, `satisfyStick()`, `satisfyIneq()` (and `collideWorld/collideSticks/satisfy` at 623-687) re-implement the integrator, the distance constraint and the range constraint that `src/common/motion/include/sigilmotion/physics/Verlet.h:32` and `Constraints.h:69,90` (`distance`, `range`, `spring`, `pin`) already provide, without including either | motion primitives live in SigilMotion; if the paper's square-root stick form is what the study needs, that library grows a `stick(a, b, length)` in that form | include `sigilmotion/physics/*`, keep the paper's coefficients as props, delete the local copies.
8. should-fix | `src/sketch/sketches/shared/EvangelionUi.h:53-116` | `panel(PanelCorners)` hand-builds a rounded rectangle whose masked corners are replaced by an `SkVector` cut, with `SkPathBuilder` arcs | `src/common/geometry/include/sigilgeometry/kit/Corners.h:158,174` already has `chamfered(cut, mask)`/`notched(...)` and `rounded()`; what is missing there is a non-square cut composed with rounding of the uncut corners | grow `Corners.h` (`chamfered(SkVector cut, mask)` composable with `rounded`) and delete the copy; `MagiModule`/`MagiVoteLayout` stay in `shared/`.
9. should-fix | `src/sketch/sketches/minard_1869.cpp:2100`, `chaucer_astrolabe.cpp:2640-2982`, `black_watch.cpp:809`, `chevreul_circle.cpp:961`, `dunhuang_star_chart/dunhuang_star_chart.cpp:2357` | each builds a `measure::Check` run and then lays the rows out itself (`auditColumn`, `verify`, `buildVerifyTable`, `auditPanel`) — none calls `sketch::kit::table` or `kit::readout` (grep: 0 uses in all five) | the kit's `table(rows, {.columns…})` is "N columns each at its own width … a key, a cost, the tier it took and the condition that refused it" | one `sketch::kit::table` over `Table::rows`, with the study's own theme bound.
10. should-fix | `src/sketch/sketches/genesis_fire.cpp:293-335`, `hitman_verlet.cpp:337-382`, `slitscan_2001.cpp:350-392` (+ 46 sketches with a local `mono()/sans()/ui()/label()` text-style helper; the face lists `{"Hoefler Text","Baskerville"}` ×10, `{"Menlo","Courier New"}` ×9, `{".SF NS","SF Pro","Helvetica Neue"}` ×9 repeat verbatim) | three sketches carry the identical `monoFace/uiFace/heavyFace/faced/mono/monoB/ui/t` block, and most sketches restate a house face by fallback list | `sketch::kit::Theme::mono(size, colour, track)`/`sans(...)` and the theme's two held faces exist for exactly this; a repeated fallback list is a house face and belongs in the kit once | bind a `Theme` whose `type.mono`/`type.sans` are the resolved faces, replace the helpers with `theme().mono(...)`/`.sans(...)`, and put the three recurring face lists in the kit.

## 1. Determinism

Checked and cleared (stated so the next reviewer does not repeat it):
every `std::mt19937`/xorshift in a canvas or set sketch is seeded by a literal
(`ui_particles.cpp:196,374`, `volatility_cost.cpp:262`, `daemon_console.cpp:213`,
`bristle_current.cpp:75`, `genesis_fire.cpp:426,757,800,1512,1530`,
`psx_doom_fire.cpp:175`, `chladni_tab1.cpp:475`); `penrose_paving.cpp:417,454,455`
iterate `boost::unordered_flat_map`s but feed only max/count aggregates
(`worstVertErr`, `badVerts`, `interiorVerts`), so order never reaches
geometry; `genesis_fire.cpp:1567` reads `steady_clock` into `buildUs` and draws
it through `ctx.measured` at 1674; `tile_map.cpp:266-276` prints reconcile
counts and swaps the millisecond line under `ctx.deterministic`; no
`rand()`, `random_device`, `std::hash`, pointer-ordered sort, `getenv` or
directory iteration anywhere under `sketches/`; every draw sketch's clock
read is `pen.millis()`/`pen.frameCount`, which the session steps.

- should-fix | `src/sketch/sketches/import_native.cpp:139-147` | (top ten, 2).
- should-fix | `src/sketch/sketches/ui_particles.cpp:440-444` | (top ten, 3).
- nit | `src/sketch/sketches/slitscan_2001.cpp:1519` | `deterministic_ ? 0.0 : bakeMs` re-spells `ctx.measured` by hand (the member is set from `ctx.deterministic` at 1965) | route the figure through `ctx.measured(bakeMs)` where it is formatted | replace the ternary; drop `deterministic_`.

## 2. Re-implementing what a kit or library provides

- should-fix | `src/sketch/sketches/hitman_verlet.cpp:516-560` | (top ten, 7).
- should-fix | `src/sketch/sketches/shared/EvangelionUi.h:53-116` | (top ten, 8).
- should-fix | five audit columns | (top ten, 9).
- should-fix | text-style chassis and face lists | (top ten, 10).
- should-fix | `src/sketch/sketches/eva_magi_interior.cpp:263`, `eva_magi_deliberation.cpp:28` | each defines its own `SkColor4f hex(uint32_t, float)` | `sigil::compose::hex` (`src/common/compose/include/sigilcompose/core/Paint.h:95`) is the same function and 47 other sketches use it | delete both, `using sigil::compose::hex`.
- should-fix | `src/sketch/sketches/chaucer_astrolabe.cpp:2489-2512`, `minard_1869.cpp:2115-2141` | `titleStrip()` places title, subtitle, provenance and a far-ranged note as absolutely-positioned `text()` nodes plus a hand-drawn hairline | `sketch::kit::titleCard` is exactly this shape — eyebrow/title/subtitle with `notes` ranged at the far edge and an optional rule — and takes the study's own registers through a bound `Theme` | one `titleCard` call per study.
- should-fix | `src/sketch/sketches/penrose_paving.cpp:397-406`, `fallout2_charsheet.cpp:532-545`, `thunder_fulu.cpp:1975-2107`, `eva_magi_defense.cpp:922-983`, `eva_magi_interior.cpp:679-708`, `spacejam_1996.cpp:1358-1408` | each carries its own `Audit {…}` struct or hand-appended "heading"/pass lines (`penrose` counts `badVerts`; `fallout2` counts `passed/total`; `thunder_fulu` appends formatted strings to a console feed; the two MAGI plates and spacejam print tables) | `measure::Check`/`Table` (`Check.h:53,201`) with `Standing::Claim/Finding/Reading/Heading` is the seam for this and is already linked (`SigilMeasure` is PUBLIC on `SigilSketches`) | build a `Table`, render with `sketch::kit::table`, let `failures()` be the verdict.
- should-fix | `src/sketch/sketches/horizontal_flow.cpp:64-70, 136-150` | `caption()` and `panel()` hand-roll a captioned, grounded panel (`box().absolute().left().top().padding(22).column().fill(kPanel)`) on a paper palette (`kPaper/kQuiet/kCinnabar`, lines 43-47) | `sketch::kit::caption` + `well({.ground, .padding})` under a paper `Theme` (the kit README shows the two-colour copy) | bind `sheetTheme()` with `ground = kPaper, ink = kSumi, ash = kQuiet` and use the kit's caption and well.
- should-fix | `src/sketch/sketches/stroke_atlas.cpp:211-232, 265-300, 540-1151` | a 1171-line catalogue sheet (`Catalog · Type`) that includes only `Page.h` for `stage`, then hand-rolls `ringOf`/`frameRect` silhouettes (`SkPathBuilder` circle and inset rect — `shapes::circle()`/a padded `box`), `rule()`, and an absolutely-placed `sectionTitle()` where `sketch::kit::sectionHeader` exists; `describe()` is 612 lines of furniture around ~40 stroke specimens | a catalogue is "the algorithm plus a run of these calls" | `page` + `panelGrid` + `sectionHeader` + `caption`; the specimen list stays.
- nit | `src/sketch/sketches/geo_groups.cpp:138`, `pop_deform.cpp:129`, `pop_order.cpp:155`, `bound_lane.cpp:219` | `panel()` wraps `kit::caption` around `box().width().height().stroke(kFrame)` | `sketch::kit::well({.width, .height, .keyline = Fill::color(kFrame)})` is the ruled well ("a plate is a well with two more fields") | use `well` with `keyline`.
- nit | `src/sketch/sketches/vagrant_story_target.cpp:492-513` | `gauge(x, y, w, h, fraction, colour)` — a frame and a filled bar | `sketch::kit::meter` (a fraction along a bar) with the label/reading left empty, or the kit grows a bare-bar form | use `meter` inside the texture scene.
- nit | `src/sketch/sketches/loot_grid.cpp:356-372` | `well()` — the kit's well plus an inner shadow and a sunken `bevelPair` | the kit `Well` has no sunken variant; a "hole punched in the panel" is a general look | grow `Well` with a `sunken`/`depth` field rather than keep a local copy.
- nit | `src/sketch/sketches/thaumonomicon/thaumonomicon.cpp:257`, `xcom_battlescape.cpp:510` (`hash3`); `chladni_tab1.cpp:192`, `nightingale_coxcomb.cpp:228` (`polar`); `kumiko_asanoha.cpp:198`, `penrose_paving.cpp:208`, `dunhuang_star_chart.cpp:323` (`clamp01`); `chladni_tab1.cpp:392` (`struct Xorshift`) | integer hashes, polar point, clamp and an xorshift RNG re-defined per sketch | `sigil::core::noise::pcgHash/hash/xorshiftNext` (`src/common/core/include/sigilcore/compute/Noise.h:91,109`), `geometry::arrange::onRing` (`path/Arrange.h:71`), `std::clamp` | import the origin.
- nit | `src/sketch/sketches/shared/TwoAdvanced.h:110-140` | `SectionCycle::at()` computes a held step index from `fmod(elapsed - start, stops * hold)` | `motion::stepIndex(elapsed, hz)` (as `tile_map.cpp:298` uses it) plus a `values/Sequence.h` `Hold` track say the same thing in motion's words | spell the cycle over `stepIndex`.
- nit | `src/sketch/sketches/shared/VerticalSpecimen.h:87-103` | `specimen()` stacks a horizontal caption over a vertical column | `sketch::kit::caption(measure, label, note, body)` with `captionWhere` on a bound `Theme` | use the kit caption; keep the two palettes and `body()`/`label()` in `shared/`.

## 3. Sketches over ~1000 lines — the general problem inside each

Every one of the 31 sketches over 1000 lines names its subject; what
follows is the mechanism each one hides and where it should live. Lines
are the top-level definitions that carry it.

- should-fix | `src/sketch/sketches/twoadvanced_v3.cpp:340-440` | (top ten, 6) — a Rive-container image extractor → SigilImage/SigilIO.
- should-fix | `src/sketch/sketches/xcom_battlescape.cpp:669-977, 1194-1332` | `paintFloor/paintHullWall/paintTree/paintUnit/paintCursor/paintPlate/paintButtonGlyph` draw palette-indexed sprites straight onto an `SkCanvas` (`Ink`), then `bakeAtlas()` packs them | a palette-indexed sprite painter with a 2048-wide atlas packer — the general form of what `compose/kit/PixelType.h` does for glyph runs | grow `PixelType.h`/`Sprites.h` with a palette-indexed sprite bake and an atlas packer; the sketch keeps the tile tables.
- should-fix | `src/sketch/sketches/cde_motif.cpp:881-1069` | eleven icons as ASCII pixel maps (`icoHome`…`icoFolder`) rendered by `art()` into per-run boxes; also `bevel/highlight/stipple/pinStripeTile` at 583-676 | a pixel-map sprite element (a char grid + colour set → one baked image) — `Sprites.h`'s `dotSprite` is a soft blob, not this — and Motif bevels which `compose/kit/Chrome.h` (`ChromeBody/ChromeSliver`) is the home for | a `pixelMap(rows, palette, cell)` in `Sprites.h`; bevels into `Chrome.h`.
- should-fix | `src/sketch/sketches/thaumonomicon/thaumonomicon.cpp:670-851` | `Ink` + `drawGlyph(SkCanvas&, glyph, alpha)`: 160 lines of hand-plotted rectangles per icon, invoked from `custom()` paint programs at 854/860 | the same pixel-sprite problem as above, plus `thaumRoute@359` (stamped-art edge routing) which is a `Routers.h` router with an image stamp | same `Sprites.h` growth; a `stamped` router.
- should-fix | `src/sketch/sketches/spacejam_1996.cpp:999-1200, 1358-1408` | an HTML auto-table layout scheme (`LayoutInput`, resolved columns/rows compared to Chrome) implemented in the sketch | a layout scheme is compose's (`compose/kit/Layouts.h`, `core` layout); a table-auto scheme is reusable by every HTML-era reconstruction | move the scheme to compose kit `Layouts.h` with a test that asserts the Chrome numbers this sketch prints.
- should-fix | `src/sketch/sketches/bg3_dice_roll.cpp:376-503` | `icosaVertices/buildSolid/rotate/numberFaces/settleAttitude` — an icosahedron mesh by search plus an attitude solver that lands a chosen face up | `geometry/kit/Solids.h` (box, cylinderPanel, extrude, revolve, superellipsoid, torus) has no platonic solid; "which rotation puts face N up" is a mesh-kit question | grow `Solids.h` with `icosahedron()` (and the other platonics) and a face-up pose in `mesh/curve/Pose.h`.
- should-fix | `src/sketch/sketches/ksp_mapview.cpp:244-336, 355-420, 473-566` | `Conic` (r(ν) = p/(1+e·cos ν) with the focus at the primary, prograde/radial vectors, hyperbolic branch), `ringDot/inscribedCircle/diamond/chevron` silhouettes, navball and bright-pass SkSL | the conic is a `geometry/path` generator; the four silhouettes are `geometry/kit/Silhouettes.h` entries (the file says why `polygon(4,45)` and `circle()` are wrong — that is the case for adding them); the orthographic-sphere shader is a material stock value | move each to its origin.
- nit | `src/sketch/sketches/world_hud.cpp:192-342` | `boneFrame()` is 151 lines of one HUD frame | a `frame`/`well` with a bone keyline over `Ornament.h`'s carved-frame slices | compose over `sketch::kit::frame` + `kit::carvedFrameSlice`.

The rest, named without a separate finding (the mechanism, and its home):
`chaucer_astrolabe` (3157) — stereographic projection (`proj/projRA/almCy…@288-334`) and the ring scales of the limb (`limb@1618`) → a projection value in `geometry/path` and ring typography over `arrange` + path text; `brass/brassRamp@902-931` → material kit surface. `dunhuang_star_chart` (2924) — `mapOfRa/precMatrix@345-398` chart projection and `raRuler/scrollBand` → the same projection seam; the catalogue join (`Catalogue.cpp`) → `sigildata` join. `minard_1869` (2889) — `flowRibbon@1055` over `brush::Ribbon` is already library; what remains is `runAudits@2182-2765` (584 lines, the largest single function in the directory) → `measure::Table` + `kit::table` (top ten, 9). `twoadvanced_v4` (2675) — `singleBevel/doubleBevel@286-296` chamfered Flash chrome → `Chrome.h`; `SectionCycle` → motion. `rota_convocationis` (2465) — `arcRing/spokeRing/nodeRing@618-685` ring constructions → `arrange`; `bakeGlow@722` → material kit. `thunder_fulu` (2239) — `resample/smoothPath/pathLength@365-526` over a `Poly` while `sigilgeometry/path/Polyline.h` is included at 174 → use it; `inkStroke@667` → brush. `sigillum_aemeth` (2174) — `dedupeAA/walkFrom/heptChords@441-479` solver; `waxGround/grooveFill@622-670` → material kit. `slitscan_2001` (2122) — `fitAtK/measureExposure@808-947` exposure fit already over `measure::lineFit`; `describe()` 556 lines of sidebar furniture → kit `titleCard/caption/readout`. `cde_motif` (2001) — above. `fallout2_charsheet` (1885) — `well/raised@935-963` bevels → `Chrome.h`; `figure@608` positioned runs. `chevreul_circle` (1847) — `toLab/luminance@310-317` while `sigilmaterial/color/Color.h` is included at 124 → use its Lab; `verify@621` 309 lines → kit table. `genesis_fire` (1707) — `stepSim@516` (158 lines) a particle system with Reeves' attributes → `sigilmotion/physics/Points.h` grows; the sidebar chassis → kit. `eva_magi_interior` (1701) — `portraitStatic@1060` 205 lines; `panels@353` cut-corner panels → shared/Corners. `winamp_base` (1565) — `part@557`/`buildMaterials@452` bevel parts → `Chrome.h`; `mmssCells@443` LCD digits → `PixelType.h`. `black_watch` (1482) — `expand/clothAt/findMirrors@234-295` a sett→cloth weave → `material/pattern` (tartan as a pattern generator). `eva_magi_defense` (1402) — `funnelPath/ribbon@326-342`, `installation@993` one component rotated six times → fine as a sketch; `runAudit` → table. `ds2_bench` (1375) — routers already `Routers.h`; `pcb@328` trace material → material kit. `penrose_paving` (1252) — `buildField@264` de Bruijn pentagrid → `geometry/path/Lattice.h` grows a pentagrid; `verify@406` → table. `lain_navi` (1234) — `crtEffect/plateEffect@709-736` → material stock. `stroke_atlas` (1171) — above. `loot_grid` (1145) — `cellRect/gridPanel@346,538` → `compose/kit/Grid.h`. `astral_tome` (1070) — `linkPass/starEl@702-786` chart cells → fine. `kumiko_asanoha` (1067) — `buildSeams@517` (123 lines) mitred lattice joinery → `geometry/path/Ops.h`/`Lattice.h`.

## 4. Comments and leftovers

- should-fix | `src/sketch/sketches/xcom_battlescape.cpp:1692-1814, 1966-1973` | (top ten, 5): fifteen `std::printf` on setup plus three frames of `composer.stats()` to stdout.
- should-fix | `src/sketch/sketches/spacejam_1996.cpp:1376-1408, 1507` | `SkDebugf` dumps of resolved columns, rows, image rects and `composer.stats()` on every setup and once in update | the numbers are the layout scheme's test assertions | move to a test on the extracted scheme; delete the dump.
- should-fix | `src/sketch/sketches/eva_magi_interior.cpp:687-708, 1601-1623`, `eva_magi_defense.cpp:935-983`, `penrose_paving.cpp:1095-1200` | audit tables printed with `std::printf` on setup | (category 2 finding on `measure::Table`) | render, do not print.
- nit | `src/sketch/sketches/black_watch.cpp:1451-1458, 1469-1475` | two `fprintf(stderr)` lines of verdict and `composer.stats()` in setup and update | the verdict is already on the plate through `measure::Check`; the stats are the window's | delete both.
- nit | `src/sketch/sketches/slitscan_2001.cpp:2046-2055`, `rota_convocationis.cpp:2097-2106` | `fprintf(stderr)` of the fit/round-trip numbers and of the chained timeline on setup | same | delete; the plate carries the figures.
- nit | `src/sketch/sketches/genesis_fire.cpp:354, 1264`; `hitman_verlet.cpp:396` | "the entrance the column used to stagger", "what a slot used to buy" — history of the sketch's own earlier shape | state the constraint ("each panel carries its own delay") | rewrite.
- nit | `src/sketch/sketches/vertigo_titles.cpp:327-329` | "What used to be a fourth cell … what used to be a fifth" | say what the two bindings are | rewrite.
- nit | `src/sketch/sketches/matrix_rain.cpp:475-480` | "The number this caption used to carry was derived from a probe…" | the constraint is "a caption states the declaration, not a font probe" | rewrite without the history.
- nit | `src/sketch/sketches/black_watch.cpp:803` | "the panel no longer has to sniff a trailing OK" | history | rewrite.
- nit | `src/sketch/sketches/rota_convocationis.cpp:460` | "A caption no longer stands beside the seal it names" | history | rewrite.
- nit | `src/sketch/sketches/dunhuang_star_chart/dunhuang_star_chart.cpp:442` | "the same sums this file used to accumulate by hand" | history | rewrite.
- nit | `src/sketch/sketches/chrome_type.cpp:4, 121` | "the box they used to get" / "The box it used to get" — reads as API history | say "the style on the node's own box, beside the style on the glyph outline" | rewrite.
- nit | `src/sketch/sketches/astral_tome.cpp:564` | "see the perf story" — a citation to a document | state the constraint (ten Outputs shared by 93 primitives because a per-primitive Output is a per-frame write) | rewrite.

No `TODO`/`FIXME`/`HACK`, no `#if 0`, no commented-out code blocks were found (`astral_tome.cpp:69` and `pop_stamps.cpp:8-32` are quotations, not dead code); the `workaround:` marker is unused under `sketches/`.

## 5. `sketches/shared/` and duplicated helpers

- should-fix | `shared/EvangelionUi.h:53-116` | (top ten, 8) → `geometry/kit/Corners.h`.
- should-fix | `genesis_fire.cpp:293-335` = `hitman_verlet.cpp:337-382` = `slitscan_2001.cpp:350-392` (+ `penMono/penMonoB/penUi`, `panel()` at `genesis_fire.cpp:356` and `hitman_verlet.cpp:397` with the same 90/85 ms delay shell) | three instrument-sidebar chassis, byte-alike | either a bound `Theme` (faces, registers) or one `shared/Instrument.h` | dedupe; the pen-side setters can live beside the theme as `penStyle(pen, theme().mono(...))`.
- should-fix | 46 sketches with local `TextStyle` helpers; `label()` in 24 files; the three repeated face lists | (top ten, 10).
- nit | `shared/TwoAdvanced.h` `SectionCycle`, `shared/VerticalSpecimen.h` `specimen()` | (category 2 nits above).
- nit | `eva_magi_interior.cpp:263` / `eva_magi_deliberation.cpp:28` `hex()` | the pair of MAGI plates share `shared/EvangelionUi.h` yet each re-defines the same helper that compose already has | (category 2, should-fix) — listed here for the shared-header audit.

## 6. Theme use

- should-fix | `src/sketch/sketches/horizontal_flow.cpp:43-47, 58-70` | own `kPaper/kQuiet/kCinnabar` and a `sans()` helper over `weave::ports::face({"Iowan Old Style", …})` in a `Catalog · Type` sheet | a paper `Theme` copy (the kit README's own example) | bind and read `theme()`.
- nit | `src/sketch/sketches/card_flip.cpp:67-74` | `kGround/kPanel/kInk/kAsh` constants named for theme roles with their own values, zero `theme()` reads, titles set with `type(18, kInk, 3)` at 209 | a `Kit · Depth` sheet should read `theme().palette` or bind a copy | bind a `Theme` with these values.
- nit | `src/sketch/sketches/crossing_rule.cpp:52` (`kCellGround`), `hit_slots.cpp:87-88` (`kInk/kDim`) | parallel copies of `palette.cellGround`/`ink`/`ash` in `Kit · API` sheets that otherwise use the kit | read the token | replace with `theme().palette.*`.

Every other `Kit · API`/`Specimen` sheet either reads `theme()` or carries
only subject colours (a specimen's warm/cool/figure inks), which is what
the kit README allows.

## 7. Plates, capture moments, marks

- should-fix | `scripts/sigil/plates.py:39` | (top ten, 1): 34 draw sketches unswept.
- should-fix | `src/sketch/sketches/rota_convocationis.cpp:2093` | (top ten, 4): `ctx.plate()` on a live loop.
- nit | `src/sketch/sketches/eva_magi_deliberation.cpp:222-227` | declares no `captureAt`; the plate is static (no `animate`/`Output`/`update`), so the sweep's fallback time renders the same picture, but `CanvasSpec` asks every sketch to declare the moment | `ctx.captureAt(0.05)` | add it.
- nit | `src/sketch/sketches/brush_custom.cpp:79-85` | the only draw sketch with no `captureAt` (the other 33 declare one) | declare the moment the sheet is complete | add `context.captureAt(0.25)` like its siblings.
- `oversample` is declared once (`psx_doom_fire.cpp`, `oversample(2)` on a 27 Hz pixel automaton) and is a whole number, as required. `fx_scatter_mix.cpp:107` captures at 0.05 s under constant-progress tracks by design (comment at 105-106). No other sheet captured at ≤0.1 s carries an entrance.

## Appendix — every sketch

Columns: name, lines, kind, kit use (`yes` = calls a `sketch::kit` component;
`stage/theme only` = uses `stage`/`Theme`/`Provide` and nothing else from
the kit; `no` = none), and the longest hand-rolled section (the longest
top-level function or method, by line span). `Anchor.cpp` is the registry
anchor, not a sketch; `dunhuang_star_chart/Catalogue.cpp` is a unit of its
sketch (its longest section is `catalogue`, 50-98).

| sketch | lines | kind | kit | longest hand-rolled section |
|---|---|---|---|---|
| `aero_desktop` | 771 | canvas | stage/theme only | window (423-520, 98 lines) |
| `annotated_margin` | 247 | canvas | stage/theme only | describe (127-247, 120 lines) |
| `astral_tome` | 1070 | canvas | stage/theme only | setup (928-1070, 142 lines) |
| `axis_ripple` | 467 | canvas | stage/theme only | ripplePanel (242-328, 87 lines) |
| `beethoven` | 296 | canvas | stage/theme only | imprint (173-220, 48 lines) |
| `bg3_dice_roll` | 1476 | canvas | stage/theme only | die (593-698, 106 lines) |
| `black_watch` | 1482 | canvas | stage/theme only | build (631-808, 178 lines) |
| `blend_options` | 317 | canvas | yes | setup (256-317, 61 lines) |
| `blur_falloff` | 197 | canvas | yes | setup (146-184, 39 lines) |
| `border_weave` | 174 | canvas | yes | setup (97-174, 77 lines) |
| `bound_lane` | 414 | canvas | yes | setup (264-414, 150 lines) |
| `bousen` | 337 | canvas | stage/theme only | describe (165-337, 172 lines) |
| `bristle_bloom` | 161 | draw | no | draw (93-161, 68 lines) |
| `bristle_current` | 196 | draw | no | step (109-144, 36 lines) |
| `brush_botanical_study` | 265 | draw | no | draw (166-265, 99 lines) |
| `brush_custom` | 142 | draw | no | draw (99-142, 43 lines) |
| `brush_dynamics` | 137 | draw | no | draw (55-137, 82 lines) |
| `brush_engine_atlas` | 152 | draw | no | draw (80-152, 72 lines) |
| `brush_live_tutorial` | 345 | draw | no | hatches (192-241, 50 lines) |
| `brush_rain` | 92 | draw | no | draw (45-92, 47 lines) |
| `brushwork_currents` | 114 | draw | no | draw (44-114, 70 lines) |
| `bullets_dropcap` | 218 | canvas | yes | setup (133-218, 85 lines) |
| `card_flip` | 240 | canvas | stage/theme only | card (133-161, 29 lines) |
| `cde_motif` | 2001 | canvas | yes | fileManager (1225-1350, 126 lines) |
| `chaucer_astrolabe` | 3157 | canvas | stage/theme only | verify (2640-2982, 343 lines) |
| `chevreul_circle` | 1847 | canvas | stage/theme only | verify (621-929, 309 lines) |
| `chladni_tab1` | 914 | canvas | stage/theme only | setup (785-914, 129 lines) |
| `chrome_type` | 183 | canvas | yes | setup (109-148, 40 lines) |
| `cjk_rules` | 183 | canvas | yes | setup (126-183, 57 lines) |
| `codec_roundtrip` | 279 | canvas | yes | setup (142-279, 137 lines) |
| `compute_variant` | 219 | set | no | describe (169-219, 50 lines) |
| `contour_poses` | 312 | canvas | yes | setup (125-312, 187 lines) |
| `corner_notched` | 162 | canvas | yes | setup (84-162, 78 lines) |
| `cosmati` | 596 | canvas | yes | describe (380-587, 208 lines) |
| `coverage_boundary` | 203 | canvas | yes | setup (130-203, 73 lines) |
| `crossing_rule` | 269 | canvas | yes | setup (166-269, 103 lines) |
| `crt_bloom` | 209 | canvas | yes | setup (140-209, 69 lines) |
| `curve_shelf` | 173 | canvas | yes | setup (83-173, 90 lines) |
| `daemon_console` | 822 | canvas | yes | describe (607-822, 215 lines) |
| `dart_flight` | 158 | set | no | describe (92-158, 66 lines) |
| `decay_step` | 223 | canvas | yes | setup (136-223, 87 lines) |
| `deformed_cloud` | 161 | set | no | describe (106-161, 55 lines) |
| `ds2_bench` | 1375 | canvas | stage/theme only | uiFace (141-327, 187 lines) |
| `elastic_type` | 441 | canvas | stage/theme only | describe (356-416, 61 lines) |
| `ember_decode` | 327 | canvas | stage/theme only | describe (213-291, 79 lines) |
| `encode_write` | 222 | canvas | yes | setup (121-222, 101 lines) |
| `env_faces` | 269 | canvas | yes | setup (173-269, 96 lines) |
| `env_lanes` | 216 | canvas | yes | setup (125-216, 91 lines) |
| `env_theme` | 264 | canvas | yes | setup (184-264, 80 lines) |
| `eva_magi_defense` | 1400 | canvas | stage/theme only | installation (993-1092, 100 lines) |
| `eva_magi_deliberation` | 234 | canvas | no | information (150-196, 47 lines) |
| `eva_magi_interior` | 1699 | canvas | stage/theme only | portraitStatic (1060-1264, 205 lines) |
| `exact_tangent` | 176 | canvas | yes | setup (114-176, 62 lines) |
| `exr_channels` | 285 | canvas | yes | throughRoughnessSlot (198-267, 70 lines) |
| `fallout2_charsheet` | 1885 | canvas | stage/theme only | figure (608-722, 115 lines) |
| `field_shelf` | 243 | canvas | yes | setup (141-243, 102 lines) |
| `first_light` | 128 | set | no | describe (56-128, 72 lines) |
| `floating_panels` | 214 | canvas | yes | card (85-132, 48 lines) |
| `flourish` | 640 | canvas | stage/theme only | cartouche (295-411, 117 lines) |
| `formation_bands` | 246 | canvas | yes | setup (129-246, 117 lines) |
| `frame_grid` | 318 | canvas | yes | setup (127-318, 191 lines) |
| `frame_inputs` | 299 | canvas | yes | setup (185-299, 114 lines) |
| `fx_scatter_mix` | 170 | canvas | yes | setup (103-170, 67 lines) |
| `genesis_fire` | 1707 | draw | yes | stepSim (516-673, 158 lines) |
| `geo_groups` | 233 | canvas | yes | setup (159-233, 74 lines) |
| `gerstner_grid` | 465 | canvas | stage/theme only | describe (395-465, 70 lines) |
| `gif_frames` | 231 | canvas | yes | sampled (141-197, 57 lines) |
| `glow_trail` | 181 | set | no | set (61-123, 63 lines) |
| `grid_layouts` | 160 | canvas | yes | setup (114-160, 46 lines) |
| `half_float` | 221 | canvas | yes | setup (118-196, 79 lines) |
| `hello` | 134 | canvas | stage/theme only | describe (39-108, 70 lines) |
| `hit_slots` | 288 | canvas | stage/theme only | answer (194-254, 61 lines) |
| `hitman_verlet` | 2076 | draw | no | simulation (1175-1282, 108 lines) |
| `horizontal_flow` | 184 | canvas | stage/theme only | shapePassage (72-107, 36 lines) |
| `hub_reload` | 273 | canvas | yes | setup (120-221, 102 lines) |
| `import_native` | 241 | set | no | describe (204-241, 37 lines) |
| `karaoke_wipe` | 444 | canvas | no | describe (270-342, 73 lines) |
| `keeps_and_frames` | 264 | canvas | yes | options (195-264, 69 lines) |
| `key_light` | 150 | set | no | describe (113-150, 37 lines) |
| `kinetic_card` | 300 | canvas | yes | describe (196-300, 104 lines) |
| `ksp_mapview` | 1999 | canvas | stage/theme only | navball (1152-1409, 258 lines) |
| `kumiko_asanoha` | 1067 | canvas | stage/theme only | buildSeams (517-639, 123 lines) |
| `lain_navi` | 1234 | canvas | stage/theme only | describe (953-1167, 215 lines) |
| `lane_retarget` | 265 | canvas | yes | setup (146-211, 66 lines) |
| `lantern_room` | 197 | set | no | describe (114-197, 83 lines) |
| `live_settling` | 189 | canvas | yes | setup (103-189, 86 lines) |
| `loot_grid` | 1145 | canvas | yes | rarityColor (111-299, 189 lines) |
| `manuscript` | 374 | canvas | yes | describe (222-349, 128 lines) |
| `material_atlas` | 290 | canvas | yes | setup (184-290, 106 lines) |
| `material_child` | 364 | canvas | yes | describe (280-343, 64 lines) |
| `material_lab` | 370 | set | no | describe (337-370, 33 lines) |
| `matrix_rain` | 545 | canvas | stage/theme only | describe (415-501, 87 lines) |
| `matte_luma` | 280 | canvas | yes | setup (198-280, 82 lines) |
| `mawarikomi` | 194 | canvas | stage/theme only | describe (91-194, 103 lines) |
| `mesh_generators` | 222 | canvas | stage/theme only | setup (162-222, 60 lines) |
| `mesh_normal_bridge` | 249 | canvas | stage/theme only | setup (189-249, 60 lines) |
| `minard_1869` | 2889 | canvas | no | runAudits (2182-2765, 584 lines) |
| `net_policy` | 182 | canvas | yes | setup (104-182, 78 lines) |
| `night_network` | 558 | canvas | stage/theme only | describe (207-558, 351 lines) |
| `nightingale_coxcomb` | 899 | canvas | stage/theme only | describe (526-786, 261 lines) |
| `nine_slice` | 266 | canvas | yes | describe (175-236, 62 lines) |
| `noise_shelf` | 224 | canvas | yes | setup (114-192, 79 lines) |
| `observable_circle_packing_contained` | 102 | draw | no | draw (79-102, 23 lines) |
| `observable_circle_packing` | 92 | draw | no | grow (53-73, 21 lines) |
| `observable_fibonacci_rectangles` | 64 | draw | no | draw (28-64, 36 lines) |
| `observable_fibonacci` | 46 | draw | no | draw (22-46, 24 lines) |
| `observable_flowfield_1` | 43 | draw | no | draw (22-43, 21 lines) |
| `observable_flowfield_2` | 62 | draw | no | draw (32-62, 30 lines) |
| `observable_flowfield_3` | 64 | draw | no | draw (32-64, 32 lines) |
| `observable_grid` | 47 | draw | no | draw (22-47, 25 lines) |
| `observable_l_system_tree` | 81 | draw | no | draw (40-81, 41 lines) |
| `observable_l_system` | 91 | draw | no | draw (50-91, 41 lines) |
| `observable_noise_map` | 40 | draw | no | draw (20-40, 20 lines) |
| `observable_noise` | 58 | draw | no | draw (30-58, 28 lines) |
| `observable_random_walker` | 68 | draw | no | draw (27-68, 41 lines) |
| `observable_reaction_diffusion` | 102 | draw | no | advance (59-77, 19 lines) |
| `observable_reynolds_steering` | 137 | draw | no | update (63-107, 45 lines) |
| `ocio_view` | 220 | canvas | yes | setup (135-220, 85 lines) |
| `optical_kerning` | 174 | canvas | yes | setup (86-123, 38 lines) |
| `over_under` | 238 | canvas | yes | readings (173-212, 40 lines) |
| `p5_attractor_loom` | 137 | draw | no | trace (82-116, 35 lines) |
| `p5_flow_field` | 140 | draw | no | draw (85-140, 55 lines) |
| `p5_fractal_garden` | 168 | draw | no | draw (117-168, 51 lines) |
| `p5_hello` | 45 | draw | no | draw (31-45, 14 lines) |
| `p5_liquid_layers` | 154 | draw | no | draw (94-154, 60 lines) |
| `p5_mixed_forms` | 91 | draw | no | draw (66-91, 25 lines) |
| `p5_refractive_metaballs` | 288 | draw | no | glassEffect (64-130, 67 lines) |
| `paint_shelf` | 233 | canvas | yes | setup (101-233, 132 lines) |
| `painter_gpu` | 241 | canvas | yes | draw (131-185, 55 lines) |
| `paragraph_paints` | 238 | canvas | yes | column (111-142, 32 lines) |
| `paragraph_sheet` | 431 | canvas | yes | leadingPanel (176-220, 45 lines) |
| `path_booleans` | 160 | canvas | stage/theme only | draw (74-142, 69 lines) |
| `pattern_sequence` | 238 | canvas | yes | setup (122-238, 116 lines) |
| `penrose_paving` | 1252 | canvas | stage/theme only | setup (1085-1230, 146 lines) |
| `persona_menu` | 807 | canvas | stage/theme only | describe (699-807, 108 lines) |
| `pixfont_dotsprite` | 266 | canvas | yes | stamp (225-266, 41 lines) |
| `place_repeat_tiles` | 223 | canvas | yes | setup (88-149, 62 lines) |
| `pop_billboards` | 267 | canvas | yes | setup (151-267, 116 lines) |
| `pop_deform` | 278 | canvas | yes | setup (169-278, 109 lines) |
| `pop_math` | 269 | canvas | yes | setup (106-269, 163 lines) |
| `pop_order` | 219 | canvas | yes | setup (171-219, 48 lines) |
| `pop_prims` | 172 | canvas | stage/theme only | setup (114-172, 58 lines) |
| `pop_stamps` | 214 | canvas | stage/theme only | setup (153-214, 61 lines) |
| `psx_doom_fire` | 769 | draw | no | firePanel (362-438, 77 lines) |
| `reflection_lab` | 194 | set | no | describe (140-194, 54 lines) |
| `rich_slot_reserve` | 173 | canvas | yes | setup (117-173, 56 lines) |
| `rota_convocationis` | 2465 | canvas | no | wheel (1691-1898, 208 lines) |
| `routers_straight` | 166 | canvas | yes | setup (100-166, 66 lines) |
| `routes_probe` | 310 | canvas | yes | diagram (181-256, 76 lines) |
| `ruby_kenten` | 258 | canvas | stage/theme only | describe (126-258, 132 lines) |
| `scattered_model` | 179 | set | no | describe (127-179, 52 lines) |
| `scene_surfaces` | 408 | set | no | describe (313-408, 95 lines) |
| `sdf_star` | 208 | canvas | yes | setup (95-208, 113 lines) |
| `set_stagger` | 179 | set | no | describe (123-179, 56 lines) |
| `shape_tour` | 251 | canvas | yes | setup (141-251, 110 lines) |
| `shapeworks_lab` | 358 | canvas | stage/theme only | describe (191-298, 108 lines) |
| `shipping_forecast` | 968 | canvas | yes | ringPanel (377-496, 120 lines) |
| `sigillum_aemeth` | 2174 | canvas | stage/theme only | margin (1502-1764, 263 lines) |
| `slang_portable` | 312 | canvas | yes | setup (142-312, 170 lines) |
| `slitscan_2001` | 2122 | canvas | stage/theme only | describe (1566-2122, 556 lines) |
| `spacejam_1996` | 1521 | canvas | stage/theme only | artLogo (878-998, 121 lines) |
| `spacing_passes` | 176 | canvas | yes | setup (106-176, 70 lines) |
| `sticker_collection` | 210 | canvas | stage/theme only | setup (151-210, 59 lines) |
| `stock_materials` | 250 | canvas | yes | setup (123-250, 127 lines) |
| `stroke_atlas` | 1171 | canvas | stage/theme only | describe (540-1151, 612 lines) |
| `substance_swatches` | 225 | canvas | yes | setup (136-225, 89 lines) |
| `svg_silhouette` | 142 | canvas | yes | setup (83-142, 59 lines) |
| `tategaki` | 210 | canvas | stage/theme only | describe (104-210, 106 lines) |
| `text_paints` | 184 | canvas | yes | setup (110-184, 74 lines) |
| `threaded_story` | 255 | canvas | stage/theme only | setup (157-206, 50 lines) |
| `thunder_fulu` | 2239 | canvas | stage/theme only | marginColumn (1607-1821, 215 lines) |
| `ticker_lanes` | 239 | canvas | yes | setup (141-239, 98 lines) |
| `tile_map` | 318 | canvas | yes | describe (234-290, 57 lines) |
| `twoadvanced_equipment` | 458 | canvas | yes | contentFrame (326-382, 57 lines) |
| `twoadvanced_v3` | 1332 | canvas | stage/theme only | extractRivImages (340-440, 101 lines) |
| `twoadvanced_v4` | 2675 | canvas | yes | featureSystem (1387-1564, 178 lines) |
| `ui_particles` | 481 | canvas | stage/theme only | buildPostAtlas (301-386, 86 lines) |
| `usd_roundtrip` | 274 | canvas | yes | setup (165-274, 109 lines) |
| `vagrant_story_target` | 734 | set | no | hud (554-648, 95 lines) |
| `vertigo_titles` | 815 | canvas | yes | screenPanel (383-553, 171 lines) |
| `video_compose` | 180 | canvas | stage/theme only | setup (103-180, 77 lines) |
| `video_compositing` | 227 | canvas | stage/theme only | setup (149-227, 78 lines) |
| `volatility_cost` | 513 | canvas | yes | update (455-513, 58 lines) |
| `warichu_placeholder` | 243 | canvas | yes | setup (99-142, 44 lines) |
| `web_panel` | 294 | canvas | stage/theme only | scene (224-268, 45 lines) |
| `web_script` | 324 | canvas | yes | setup (147-268, 122 lines) |
| `winamp_base` | 1565 | canvas | stage/theme only | mainWindow (633-795, 163 lines) |
| `world_hud` | 1076 | set | no | boneFrame (192-342, 151 lines) |
| `xcom_battlescape` | 1981 | canvas | stage/theme only | panel (1518-1671, 154 lines) |
| `y2k_chrome` | 619 | canvas | yes | describe (303-619, 316 lines) |
| `yarn_marquee` | 296 | canvas | yes | setup (240-296, 56 lines) |
| `zellige` | 258 | canvas | stage/theme only | fesOchre (84-124, 41 lines) |
| `dunhuang_star_chart` | 2924 | canvas | stage/theme only | setup (2757-2909, 153 lines) |
| `passive_tree` | 896 | canvas | yes | ringColor (155-233, 79 lines) |
| `thaumonomicon` | 1742 | canvas | stage/theme only | drawGlyph (692-851, 160 lines) |

## Counts

- blocker: 0
- should-fix: 24 (unique findings; the ten ranked entries are included, and a finding cross-referenced from a second category is counted once)
- nit: 22

Of the 195 sketches: 86 call a kit component, 57 use only `stage`/`Theme`, 52 use no kit — 33 draw and 15 set sketches, whose contexts the kit's canvas-bound components do not reach, and four canvas sketches: `eva_magi_deliberation`, `karaoke_wipe`, `minard_1869` and `rota_convocationis`. The large reconstructions in category 3 mostly sit in the `stage/theme only` column: they take the stage from the kit and hand-roll every piece of furniture above it.
