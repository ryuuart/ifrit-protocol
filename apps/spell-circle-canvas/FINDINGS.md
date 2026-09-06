# Findings

Defects found while working, stated as what the code does, what it was
evidently intended to do, and what a test should assert once intent is
restored. A work queue: delete an entry when it is fixed, and delete this
file when it is empty.

The queue is in two parts. The first is the merge-readiness review of
branch `sigil/library-campaigns` against `main` (merge base
`aabd3fe1b224`): nine read-only passes over the library groups, whose full
reports with every nit and every line number are the files under
`findings/`. What is listed here is every blocker and should-fix from
those reports, condensed, so that a fix pass can work from this file and
delete lines as it goes. The second part is the standing entries from
before the review, rewritten to what the tree holds now.

## Rulings for the fix pass

- The bar is everything in this file, nits included, except the two
  deferrals below, which are campaigns of their own after the merge.
- Deferred: the library extractions the sketch review names (pixel
  sprites and an atlas packer, the HTML auto-table layout, an
  icosahedron and a face-up pose, the conic generator, the chart
  projections, the tartan weave, the pentagrid, Reeves particles), and
  the big file splits (`material/skia/Paint.cpp`, `mesh/pop/Pop.h`,
  `book/main.cpp`, `book/SketchbookView.cpp`, the 1380-line
  `Composer::Impl::paint`). Test files split by subject only where they
  pass about 700 lines; one binary per library stands.
- `rota_convocationis` loses its `ctx.plate()` mark, keeps its look, and
  the emissive stack's cost is filed against the compositor.
- The plate ledger sweeps draw sketches too and takes their baselines.
- `SigilGeometryMeshPopDevice` is split out the way the mesh renderer's
  device executor is.
- The device mesh painter carries the environment, metallic and
  roughness terms the host applies, with a conformance case.
- The zip reader becomes an archive byte source in SigilIO; `Hub::probe`
  answers bytes and a registered `probe<T>()` answers meaning, with the
  image prober registered by SigilImage.
- Path and ellipse silhouettes take their exclusion margin from Skia
  path ops; the distance field stays for pixel coverage.
- Kit spellings: `swatchSide` for the length, an optional `Fill` for
  every ink, `column` on the scrollbar.
- `routes_probe` is rebased with commit 7a3ed160 named as the cause.
- The three long READMEs get chapter files beside them (material:
  colour and paint; core and motion: one per feature).
- Before the merge, on the fresh tree: the device tier, the promotion
  tier (which reports rather than gates until the glyph-edge cause is
  found), ASan and TSan over the suite, and the bench and app-FPS
  retake on an idle machine.
- Rules of the pass: a library pass builds its own targets and never
  edits a sketch; the sketch pass comes last and brings every sketch
  onto the new vocabulary; no agent runs the plate ledger — each names
  the scenes its change should move, and the closing pass rebases them
  with the causes.

## Verification state at the review

Taken on a fresh build directory (the old tree removed, `sigil.py setup
--config Release` from an empty `build/`):

- The Release build completes with zero errors and zero warnings after
  four fixes on the branch: a missing `<span>` include in the brush
  loader, two sketches returning references to temporaries, the `.geo`
  importer's unhandled big-integer case, and a Slang shader-directory
  variable that a fresh configure read empty because geometry is added
  before material (the shader workflow now names the directories
  itself).
- `ctest`: 3000 of 3000 pass. One case (`ReadGltf`) skips until
  `sigil.py assets` has fetched the demo assets into `build/assets`;
  after the fetch it runs and passes.
- CPU plate tier: 160 of 161 scenes byte-identical to the baseline.
  `routes_probe` moved (`3f8386ab9914` → `1485691082c4`) and reproduces
  stably on both trees. Its `custom("routes.live", …)` leaf is the shape
  commit 7a3ed160 changed (a paint program of its own now reads the
  backdrop), and the plate was not rebased with that cause. Confirm the
  new picture is the intended one and rebase it with the cause named.
- Not run on the fresh tree: the device tier, the promotion tier, the
  bench and app-FPS ledgers (both baselines still want an idle-machine
  retake), the sanitizer lanes, a manual Sketchbook launch.

Every library group has a report; SigilMaterial's and the two geometry
sub-passes arrived as delegated passes and are listed with the rest.

## SigilCompose, SigilSkia, SigilScry (findings/review-compose.md)

Should-fix (correctness): `compose/typography/TextAnnotations.cpp:61-84`
readings indexed by the per-line unit list, so a base broken across a
line takes the next reading; `typography/TextFxPainting.cpp:301-304` the
fx selection cache keyed on width alone while layout keys on width and
height; `skia/draw/Direct.h:43,54` with `compose/core/Instances.cpp:131`
promoted textures keyed on a bare `SkImage*` that a re-baked sheet can
reuse; `core/Derive.cpp:239-246` the thread guard compares the raw
measure but stores the sanitised one, so Yoga's NaN moves every round
until the budget is spent; `core/Coverage.cpp:72,167` a legal
`threshold(0)` traces the whole box; `scry/engine/CMakeLists.txt:56`
`SUITES SlotFillingTest` cannot match the instantiated
`SlotDoors/SlotFillingTest`, so those cases lose the `gpu` label and fail
on a device-less machine.

The remaining 78 should-fix items and 96 nits are in the report by
category. Two beyond the top ten bear on the merge: the README's compile
guard (`test/docs/api_doc_probes.py:83-88`) probes only qualified names,
which is why `compose/README.md:437` names `mul`/`lift` (which do not
exist), `:1397` names `scripts/setup.py`, and `:1133`/`:1464` name two
ledger scripts that do not exist; and `Composer::Impl::paint` is one
1380-line function (`core/StackingPainter.cpp:1200-2580`).

## Sketch framework (findings/review-sketch-framework.md)

Blockers:

- `src/sketch/set/Session.cpp:102-104`, `book/SketchCatalog.cpp:464-488`,
  `book/main.cpp:1169,1329` — the thumbnail worker renders set sketches
  through the process-wide runtime, which in the app is the Diligent
  device installed before the catalog exists, so a background thread
  drives the device and queue the render thread draws with, and at quit
  `releaseDevice()` runs before the worker is joined. Intended: a
  thumbnail is CPU-only whatever the process holds. Fix: the runtime
  becomes a per-session value and thumbnails open on `Runtime::cpu()`;
  until then stand `needsDevice()` kinds down while a device is installed
  and end the fill before the device is released. Assert: a set-kind
  thumbnail with a runtime installed never reaches it.
- `src/sketch/live/Host.cpp:456-457,278-287` — the skew guard stats the
  executable on disk at every compile; once the host is rebuilt while the
  old window runs, the file is newer than every header, the guard passes
  and the old process loads a dylib built against new headers. Fix:
  capture the stamp once at first call (or in `Host::Options`). Assert:
  moving the binary's mtime forward after construction keeps the refusal.

Should-fix:

- `book/SketchbookView.cpp:369,442,452` — `openSketch`/`publishMetrics`
  assign `m_orbitable`, `m_metrics`, `m_orbit` on the render thread
  outside `synchronize()` while the GUI reads them; carry the values in
  the queued lambdas.
- `plate/Sweep.cpp:415` vs `Sweep.h:98` — a failed GPU readback
  `continue`s with no plate and exits 0; return 1 as the raster path does.
- `README.md:623-627` vs `book/main.cpp:1037-1051` — README says `--video`
  on a set requires `--gpu`; the code encodes on the CPU executor; refuse
  or rewrite.
- `Host.h:36-45`, `SketchCatalog.h:44`, `main.cpp:736` — say the host
  stamp is in the thumbnail key; the code and README say it is not;
  delete the host clause.
- `live/Host.cpp:461-472`, `core/Crash.cpp:1018-1033` — shipped strings
  carry agent-workflow prose, file citations and a stale "dylib links its
  own libskia.a" claim; reduce both to the constraint.
- `live/Residency.cpp:35-36`, `live/Host.cpp:342`,
  `SketchbookView.cpp:344,657,760` — eviction runs under `hostMutex` on
  the render thread and `~Host` joins its compile; hand the evicted host
  back to be destroyed outside the lock.
- `kit/Rows.h:30,46`, `Legend.h:67`, `Ticker.h:75`, `Heading.h:39`,
  `Scrollbar.h:64` — `swatch` is a `Ground` in one struct and a `float`
  side in the next; `ink` is `Fill` on Timeline and `SkColor4f` on five
  others; `Scrollbar::horizontal` inverts the `column` polarity;
  `swatchSide`, `Fill` everywhere, `column`.
- `kit/Panel.h:71-77`, `Legend.h:128`, `Ticker.h:71`, `Cells.h:108-124`
  — corner radii, bezel and tick reach are look defaults hard-coded in
  props rather than `Theme::spacing`; `columns()` is `panelGrid` with
  `columns = cells.size()`; optional props read from `Spacing`, fold
  `columns` into `panelGrid`.
- `book/main.cpp:1029-1035` — `--thumbnails` never creates the shared
  web engine scope, so a web sketch's still is its "unavailable" card.
- `book/main.cpp:211` = `SketchbookView.cpp:80` — two identical `fonts()`
  each leaking a `FontContext`; one owner.
- `kit/README.md:365-368`, `kit/CMakeLists.txt:12-19` — "links no device
  and no runtime" while the target links `SigilSketch` PUBLIC; split
  `core/` into its own target or state the link.
- Files by subject: `book/main.cpp` (1331), `book/SketchbookView.cpp`
  (896), `kit/test/SketchKitTest.cpp` (1221), `book/qml/Main.qml` (850).
- Tests: the thumbnail worker's queue and cancellation (lift into a
  Qt-free `ThumbnailQueue` under `plate/`); a set-kind thumbnail case;
  the rebuilt-while-running skew case.

## The sketches (findings/review-sketches.md)

Should-fix:

- `scripts/sigil/plates.py:39` — `KINDS = ("canvas", "set")`: the 34 draw
  sketches have no plate, no baseline and no self-difference check though
  Sketchbook accepts `--kind draw`; add `"draw"` and take their baseline.
- `src/sketch/sketches/import_native.cpp:139-147` — `settled()` polls a
  wall-clock 15 s deadline and returns either way, so an unsettled page
  can be photographed under load; wait on a version count and fail rather
  than capture.
- `ui_particles.cpp:440-444` — integrates by frame delta; the window,
  `--video` and `--bench` run a different simulation from the sweep; use
  `addFixed` with an interpolant as the other simulations do.
- `rota_convocationis.cpp:2093` — `ctx.plate()` on a looping animation to
  pass `--bench` by declaration; remove the mark, keep the look, and
  file the emissive stack's cost as a compose entry here.
- `xcom_battlescape.cpp:1690-1814,1963-1974` and
  `spacejam_1996.cpp:1376-1408,1507`, `eva_magi_interior.cpp:687-708,
  1601-1623`, `eva_magi_defense.cpp:935-983`, `penrose_paving.cpp:
  1095-1200` — audit tables printed to stdout on setup; build a
  `measure::Table` and render it through `sketch::kit::table`.
- `twoadvanced_v3.cpp:340-440` — a Rive-container PNG extractor inside a
  sketch; image meaning is SigilImage's, access SigilIO's.
- `hitman_verlet.cpp:516-560` — own Verlet integrator and stick/range
  constraints beside `sigilmotion/physics/Verlet.h` and `Constraints.h`;
  include and delete the copies.
- `shared/EvangelionUi.h:53-116` — hand-built cut-corner rounded rects;
  grow `geometry/kit/Corners.h` with a non-square cut and delete.
- `minard_1869.cpp:2100`, `chaucer_astrolabe.cpp:2640`,
  `black_watch.cpp:809`, `chevreul_circle.cpp:961`,
  `dunhuang_star_chart.cpp:2357` — each lays a `measure::Check` run out
  by hand; one `kit::table` over `Table::rows`.
- 46 sketches with local `mono()/sans()/label()` helpers and three
  recurring face lists (`genesis_fire.cpp:293-335` =
  `hitman_verlet.cpp:337-382` = `slitscan_2001.cpp:350-392`); bind a
  Theme whose registers are the resolved faces and put the house lists in
  the kit once.
- `eva_magi_interior.cpp:263`, `eva_magi_deliberation.cpp:28` — a private
  `hex()` beside `compose::hex`.
- `chaucer_astrolabe.cpp:2489-2512`, `minard_1869.cpp:2115-2141` —
  hand-placed title strips that are `kit::titleCard`.
- `penrose_paving.cpp:397`, `fallout2_charsheet.cpp:532`,
  `thunder_fulu.cpp:1975`, `spacejam_1996.cpp:1358` — private audit
  structs; `measure::Check`/`Table`.
- `horizontal_flow.cpp:43-70,136-150` — own paper palette and captioned
  panel; a paper `Theme` with `kit::caption` and `well`.
- `stroke_atlas.cpp:211-1151` — 612 lines of furniture around forty
  specimens; `page` + `panelGrid` + `sectionHeader` + `caption`.
- Mechanisms hidden in the sketches over 1000 lines, each to its origin:
  palette-indexed sprites and an atlas packer (`xcom_battlescape`,
  `cde_motif`, `thaumonomicon`) → `compose/kit/Sprites.h`; an HTML
  auto-table layout scheme (`spacejam_1996:999-1200`) → compose kit
  layouts with a test; an icosahedron and a face-up pose
  (`bg3_dice_roll:376-503`) → `geometry/kit/Solids.h` and a mesh pose;
  a conic generator and four silhouettes (`ksp_mapview:244-566`) →
  geometry path and `Silhouettes.h`; Motif bevels (`cde_motif`,
  `twoadvanced_v4`, `winamp_base`, `fallout2_charsheet`) → `Chrome.h`;
  a stereographic and a chart projection (`chaucer_astrolabe`,
  `dunhuang_star_chart`) → a projection value in geometry path; a
  tartan sett-to-cloth weave (`black_watch:234-295`) → material
  pattern; a pentagrid (`penrose_paving:264`) and mitred lattice joinery
  (`kumiko_asanoha:517`) → `Lattice.h`/`Ops.h`; Reeves particles
  (`genesis_fire:516`) → `sigilmotion/physics/Points.h`.
- Comment history in `genesis_fire`, `hitman_verlet`, `vertigo_titles`,
  `matrix_rain`, `black_watch`, `rota_convocationis`,
  `dunhuang_star_chart`, `chrome_type`, `astral_tome` ("used to", "no
  longer", "see the perf story"); rewrite as the constraint.

## SigilGeometry path ops (findings/review-geometry-path-ops.md)

Blockers:

- `src/common/geometry/path/Ops.cpp:354` — `roundCorners` builds into a
  fresh `SkPathBuilder`, so an even-odd source comes back winding and a
  donut or a glyph with counters fills solid. The same class on every
  rebuilding operator: `overSamples` (50), `chamferCorners` (446),
  `displaceSquare` (571), `Edges.cpp:34`, `Band.cpp:141,208`. Seed each
  builder from `path.getFillType()`. Assert: an even-odd donut keeps its
  hole through each operator.
- `include/sigilgeometry/path/Cells.h:34` — `enum class Edge {Clamp,
  Wrap, Constant}` redefines `path::Edge` from `Edges.h:26`; the two
  public headers cannot share a TU. Rename to `Boundary` or nest it.
- `include/sigilgeometry/Geometry.h:9` — "every public header in one
  include" lists 8 of 29 `path/` headers; complete it after the `Edge`
  fix.

Should-fix (correctness): `Extremes.cpp:164` a failed conic chop emits
the curve twice; `Tidy.cpp:63` the duplicate guard is order-dependent
(a trailing zero-length piece survives, an all-degenerate contour
vanishes); `Polyline.cpp:482` `catmullRom` drops closure and never wraps
the seam; `Ops.cpp:385` `Zigzag` displaces an open contour's endpoint;
`Crossings.cpp:124,219` fixed 3 px probes and a 1.5 px merge box are not
scaled to the figure; `Neighbours.cpp:79` a non-finite coordinate makes
the bounds NaN and the floor undefined; `Scatter.cpp:216` a lattice over
a thin region answers nothing instead of fewer points; `Hull.cpp:84` an
open boundary chain is pushed as a closed ring; `Segments.cpp:199`
`startedAt` guards on segments instead of cycle pieces.

Should-fix (docs, comments, duplicates, API): `Polyline.cpp:246`
`sample()` is parameter-uniform and emits `count + 1` while the header
and README say arc length and `count`; `Fit.h:39` and README:427 claim a
closed run is fitted as a loop, `Fit.cpp:229` fits it open; `Ops.h:205,
214` `PuckerBloat`/`Twirl` warp per contour, not about the shape's
centroid; `Scatter.cpp:433,115` comments contradict the code (the clamp;
`nextBase` past 43 is not prime); `Band.cpp:127,199`, `Crossings.cpp:322`,
`Ops.h:21` history, a prose TODO and a file citation; `Noise.cpp:12` is a
byte copy of `core::noise::valueNoise`; `Crossings.cpp:80` re-flattens
beside `path::flatten`; `Symmetry.cpp:37,52`, `Frame.h:129` hand-spelt
`kTau`/`degrees()`/`kDegToRad`; `Polyline.cpp:93,485` `Polyline::
centroid()` and `Sampled lerp` referenced by nothing; `Lattice.h:29` the
one options struct without `operator==`; `Ops.h:186` `Roughen` cannot
take a Halton or stratified source (no `parameter`, 32-bit seed);
`Neighbours.h:80` only `within` has the `glm::vec2` overload; `Ops.cpp`
(604) and `bench/PathBench.cpp` (766) split by subject.

Tests missing: `chamferCorners`, `displaceSquare`, `Where::MaxCurvature`,
`relax()` with coincident points and `hold`, `TidyOptions::duplicates`
at front, middle and end, and the dropped `Blend.OklabMidGrayIsPerceptual`.

## SigilMaterial (findings/review-material.md)

Left: the README's chapter files (colour and paint).

## SigilGeometry mesh, point operators, kit, device (findings/review-geometry-mesh-pop.md)

Blockers:

- `include/sigilgeometry/Geometry.h:5`, `README.md:1039` — the umbrella
  claims every public header and omits about forty (the same finding
  the path-ops pass makes, extended to `kit/*`, pop, curve, render and
  device headers). Left to the path-ops pass, which renames the
  colliding `path::Edge` first.

Should-fix (API, build, docs, comments): `mesh/pop/device/Cook.cpp:355`
a refused kernel answers an empty cloud where the sibling executors fall
back to the host; `mesh/pop/CMakeLists.txt:64` the pop library links the
device PUBLIC, pulling Diligent and Vulkan into every consumer, against
the shape render has and README:1566,1582 state; `Pop.h:48,72` two
numberings of the builtin lanes; `Pop.h:1104`, `Sweep.h:235`,
`Stamp.h:170` "defined only with a device feature" is false;
`kit/Divisions.h:336` `chords()` insets open runs only; `kit/Shapers.h:209`
a kit header grows `path::profile`; `Pop.h:981,952` `deformFrame` and
`seedCustomNames` public without a true reason or a consumer;
`device/Resources.cpp:108` a staging texture per read against "made
once"; citations of non-existent files (`kit/Frame.h`) and the consumer
SigilWorld in `kit/Divisions.h:63`, `mesh/pop/Points.h:12`, `Pop.h:482`,
`Mesh.h:100`; README:895,1094 and `mesh/CMakeLists.txt:2` wrong about
where things live and what is exposed. Files by subject: `Pop.h` (1131),
`mesh/pop/Cook.cpp` (792), `mesh/codec/Geo.cpp` (601) — the three file
splits deferred by the rulings.

Nits: `device/Device.cpp:98,101,56` a `fprintf` where the sibling uses
`reportOnce`, an unsynchronised `static bool warned`, and a `setenv`
from a library entry point; `kit/Divisions.h:339,135` a missing
`<vector>` include and a dead ternary; `mesh/pop/kernels/Pop.slang:177` a
dead clamp; `mesh/pop/Points.h:114` `displaceNoise`'s `uint32_t` seed
against the operator's float; `Pop.h:576` a roadmap note;
`README.md:1673,869,1199` history, a broken clause and a wrong count;
`mesh/pop/Stamp.h:91` a vertex total that truncates past 2^32;
`kit/Shapers.h:63` `Wave::bleed()` and `Wave::max()` with identical
bodies.

## SigilWorld, SigilUsd, SigilSubstance, SigilImage (findings/review-geometry-material-world.md)

Blocker:

- `src/common/world/graph/Realise.cpp:67` — two masked post passes behind
  one geometry pass: the second overwrites the producer's `coverageOut`,
  the first's `coverageIn` is never written, `Targets::image()` answers
  null and `CpuPost.cpp:89` applies the op unmasked, silently. Intended:
  every masked pass reads coverage its producer paints. Fix: coverage as
  a vector of (name, selector) or one coverage per (producer, selector).
  Assert: `TwoMaskedPassesEachReadTheirOwnCoverage`.

Should-fix: `world/graph/Order.cpp:99,109` a reader declared before the
first writer gets no edge to the second and can read version 2 (assert
`AReaderDeclaredFirstStillRunsBeforeTheSecondWrite`);
`image/decode/Ktx.cpp:292-295,336-337,221` unchecked header products
overflow to a 2^62-float resize from a 100-byte file (cap dims, check
each product); `usd/read/Mesh.cpp:97`, `Primvar.h:42` unchecked face
indices and counts read out of bounds; `world/scene/Phases.cpp:356` a
per-frame `fprintf`; `world/frame/CpuGeometry.cpp:117`,
`world/diligent/Geometry.cpp:459` `targets.points(name)` inserts an empty
cloud per image name read; `substance/graph/Inputs.cpp:63` held images
never released; `substance/graph/Render.cpp:63,78` ignores the engine's
run result and answers by output count; `usd/write/Stamps.cpp:63`,
`Lights.cpp:48,64` normalise a zero direction into NaN orientations;
`world/scene/Draw.cpp:25,37` = `frame/CpuGeometry.cpp:22,77` identical
`painterLight()`/`dress()`; `world/frame/Targets.cpp:107` `stamped`
decides by a 64-bit fold alone against the store's "a hash is a bucket"
rule; `image/field/test/DistanceFieldTest.cpp` every case square (add
5×2, 2×5, 1×1, all-covered).

## SigilWeave and SigilDraw (findings/review-weave-draw.md)

Should-fix:

- `src/common/draw/brush/format/Zip.cpp:65-73` — resizes each entry to
  the size the central directory claims before reading; a tiny hostile
  zip allocates up to 2 GiB; cap the claimed size.
- `draw/brush/format/Photoshop.cpp:142-143` — allocates the raw bitmap
  (up to 128 MiB) from four header integers before any pixel byte is
  checked; bound by what the block can hold (×129 for PackBits).
- `draw/brush/format/test/FormatTest.cpp` — no truncated `.abr`, no
  section past the file, no huge or zero dimensions, no malformed
  `brush.json`, no zip claiming a huge size; one truncation-at-every-
  offset case per format plus one per hostile size field.
- `draw/brush/format/Native.cpp:242-298` vs `draw/README.md:459` — the
  native format drops the pressure envelope, `markerTip`, `bristles`,
  `blend`, speed/pressure/tilt responses, `sharpness`, `noise` on round
  trip while the README says it names the tool's own fields; write and
  read them with a field-by-field round-trip test. `Native.cpp:84-88`
  prints floats with `%.6g`, which does not round-trip; `%.9g`.
- `draw/brush/format/Zip.cpp` — a zip reader private to the brush
  formats while SigilIO owns resource access; an archive byte source in
  SigilIO.
- `src/sigilweave/layout/InitialLetter.cpp:245-272` — the notch is cut
  only on the initial's own block's bands, so a block with fewer lines
  than the sink runs the next block under the cap; carry the cut on or
  clamp the sink and report it.
- `InitialLetter.cpp:299-309`, `LineBreak.cpp:1889` — an initial on any
  block but the first gets `lineIndex = 0`, is inserted at `runs.begin()`
  (breaking logical order) and seats on the first block's metrics; carry
  the block's first line index and pitch on the plan.
- `InitialLetter.cpp:288-298` — in a column flow the initial is a
  horizontal blob whose baseline is the column's top edge; shape it
  vertically and seat its top at the column head.
- `layout/test/InitialLetterTest.cpp` — no case for the three above,
  `Wrap::kGlyph`, a negative sink, `graphemes` of 2 or more than the
  word holds, or a resumed pass not re-opening the initial.
- `sigilweave/layout/Flow.cpp:314-404` — a path silhouette's margin is
  answered by rasterising to A8 and a distance field with a 2048 px
  ceiling, and the comment never says what Skia offered; Skia path ops
  give the exact disc offset (fill ∪ round-stroked outline of width
  2·margin); keep the field for pixel coverage only.
- `sigilweave/include/sigilweave/layout/Flow.h:305` vs
  `LayoutOptions.h:186` — `setMinIntervalWidth` / `minimumIntervalWidth`;
  one spelling, no abbreviation.
- Unused parameters kept alive by casts: `InitialLetter.cpp:342`,
  `LineBreak.cpp:380`, `LayoutMetrics.cpp:94` (`glyphOutline(const
  Paragraph&)`, two compose callers), `examples/demo/weave_demo.cpp:26`.
- Comment rules: stale paths and document citations in
  `examples/demo/DemoScenes.h:3-4`, `DemoSupport.h:3`,
  `examples/gallery/src/scenes/SceneSupport.h:3,5`,
  `include/sigilweave/kit/SigilWeaveKit.h:5`; a campaign name and a
  "new features" history in `SceneNewFeatures.cpp` (rename to what it
  shows); shipped copy "no longer flows around circles alone" in
  `SceneShapes.cpp:30`; a prose TODO in `ports/SystemFontManager.cpp:27`.

## SigilCore, SigilData, SigilMeasure, SigilMotion, SigilIO, SigilVideo, the product, the build (findings/review-core-io-build.md)

Blocker:

- `src/common/core/include/sigilcore/compute/Chance.h:219-224` —
  `normal()` rejects until `0 < s < 1`, but a `stratified(0|1)` stream
  (documented valid) answers `fixed(0)` forever, so it never exits and
  `Gaussian::draw` hangs. Cap the rejection loop with a Box–Muller
  fallback, or refuse `stratified(<2)`. Assert: `normal()` over every
  documented source terminates.

Should-fix (correctness): `reconcile/Reconciler.h:146-152` a second
same-keyed sibling is destroyed without `m_host.destroy()`;
`core/hardware/GpuDevice.cpp:148` `frameIndex()` reads unlocked what
`beginFrame()` writes under the mutex; `io/hub/Network.cpp:95-101`
concurrent fetches of one URL share one `.part` and can commit a torn
file; `io/hub/Mounts.cpp:18-27` `tellg() == -1` becomes `resize(SIZE_MAX)`
and a directory reaches it; `data/decode/Csv.cpp:351-352` duplicate
header names lose a column silently; `io/hub/Selection.cpp:257-266` a
`./dir/*.ext` glob never matches; `core/cache/Volatility.cpp:27,41`
`samplesDestination` does not imply `readsBackdrop` as the header says;
`measure/time/Laps.h:32-37` stores a caller's `string_view`;
`compute/Field.h:441-452` a one-dimensional warped field reads three
dimensions; `compute/Noise.h:117-120` `pcgUnitNext` reaches exactly 1.0
against `[0, 1)`; `compute/Hash.h:63` `combine` narrows a 64-bit member;
`measure/stats/Fit.h:82-88` the subtractive `den` the README forbids and
a zero `rmsResidual` on the degenerate path; `Chance.h:98-161` a 64-bit
seed loses its high half; `hardware/Handle.h:24-33` cross-tag handles
compare equal; `compute/Curve.h:187-212` elastic eases at period 0
answer NaN; `Reconciler.h:102-105` `described` unread and `describedNodes`
counts visits.

Should-fix (API, boundary, docs, build): `io/hub/Cache.cpp:266-288` with
`Hub.h:115-124` `Hub::probe()` calls `probeImage` and answers an image
kind (IO owning meaning); `reconcile/Erased.h` a pure re-export that
makes `SigilCoreCache` link `SigilCoreReconcile`; `reconcile/Reconcile.h`
omits `Reads.h`; `compute/Hash.h:41,51` `fnv1a` not constexpr so `Env.h`
re-implements it with a different constant; `scripts/README.md` claims
`.clang-format-ignore` names the generated sources while the file is
empty; `GpuDevice.h:93-94` and `Field.h:20,385` doc counts wrong;
`video/decode/CMakeLists.txt`, `ui/CMakeLists.txt`, `spellcircle/mac`,
`spellcircle/qt` hand-roll ARC and framework links four ways (an `ARC`
flag and a `FRAMEWORKS` keyword on `sigil_library`).

Tests missing: decoders on empty, BOM, CRLF, unterminated quote,
duplicate headers, `+5`, NaN, deep JSON; the hub under retained leases
and concurrent fetches; physics at `dt == 0`, large `dt`, `stiffness >
1`, attract near zero; the reconciler with same-keyed siblings;
`normal()` over every source; video `finish()` twice and zero frames;
malformed UDP scenes.

Stray files to delete: `default/` (an empty untracked directory at the
app root) and `src/spellcircle/shared/schema/build/` (a stale CMake tree
inside the source directory); `.gitignore:20-21` entries that match
nothing; the unused `xcode` presets.

## Ring and grid placement is respelled where geometry already has it

`geometry::arrange::{along, onEllipse, onRing, cellAt, cellRect,
moduleSize, step}` (`sigilgeometry/path/Arrange.h`) is the canonical
ring-and-grid arithmetic. Every ring and every grid in the sketches now
reaches for it; of the evenly-spread runs (`t = i / n`, `x0 + (x1 - x0) *
i / (n - 1)`, `extent / count`) four sketch files still spell their own,
and the seven sites deliberately not arrange's say so in a comment where
they stand. The two spellings do not agree to the pixel, so converting a
sketch moves its plate by sub-pixel: a per-sketch judgement with the
cause in each commit, never a sweep.

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

What is left is almost all type, and that is the next thing to find. On
`half_float` (90) the difference is confined to the text and every
picture on the sheet is byte identical. The differing pixels are glyph
edges, tens of code values apart on a few of them, which is a glyph
rasterized from a different mask rather than a glyph moved. The same
number recurs across unrelated scenes (90 on five, 87 on eight), so it is
one drawing shared by the sketch kit's chrome rather than a per-sketch
accident. Ruled out, each pinned in `core/test/ComposeTestKernel.cpp`:
a line of type promoted as a node of its own, over an opaque ground, at a
plate's own view scale and fractional host translation, is exact.

Assert once fixed: `--tier promotion` reports every scene within the rule,
and the cause gets a case in `compose_test` beside the ones that already
pin it.

## A callable that blends with the page is not yet refused the bake

Two causes of one shape are fixed: a `custom()` or `picture()` leaf
holding a paint program of its own now reads the backdrop, and a
`Decoration` declares `blends()` beside `isAnimated()`, `bleed()`,
`reach()` and `borrows()`; `LayerStyles.h`, `Layered.h` and
`PixelStyles.h` declare it too and `core/Shape.h` requires it through a
concept. What remains to verify is the rest of the same shape: a `Brush`,
a `Silhouette` and a `Material` program are callables the same argument
reaches, and whichever of them does not yet declare a blend can still be
baked against transparent black.

Assert once fixed: a node whose brush, silhouette or material program
blends with the page is refused the automatic bake, and a case in
`compose_test` renders it promoted and live and finds the two identical.

## chaucer_astrolabe cannot finish a plate under the sweep's ceiling

Under the promotion tier's five concurrent jobs it was killed at the
300 s per-scene ceiling in the held-off pass, so it is the one scene the
tier could not judge. It renders alone in about four minutes, and the
CPU tier's baseline holds a line for it, so this is contention rather
than a hang — but a scene that only finishes when it has the machine to
itself is a scene no parallel sweep can gate.

Assert once fixed: `--tier promotion` and `--tier cpu` both render it
inside the ceiling at the default job count.

## slang_portable's plate draws a counter over the process's history

The sketch's "source that is not Slang" cell prints the compiler's own
diagnostic verbatim, and that text names the module it failed on:
`SigilProgram14.slang`. The name is built in
`material/slang/SlangCompiler.cpp:185-186` from a function-local `static
uint64_t serial` incremented once per `compileModule()` call, so the
number counts every module the PROCESS has compiled, not the three this
sketch compiles. The plate is judged on byte identity, so anything that
changes how many modules are compiled ahead of this one moves its hash
without moving anything the sketch is about. Today the plates verb opens
one process per scene and the number is stable, which is the only reason
this is latent rather than a flapper.

Intended: a plate shows what the sketch demonstrates — here, that a body
which cannot compile says why. The renderer already has the pin for a
number a sketch measures about its own execution (`ctx.measured(value,
pinned)` under `ctx.deterministic`), and the module serial in a
diagnostic is exactly that kind of number.

Assert once fixed: the cell's text is identical whether the sketch is
rendered alone or after another sketch has compiled a module in the same
process.
