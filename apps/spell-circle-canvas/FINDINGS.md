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
- `winamp_base` and `lain_navi` are rebased with the colour mix moving
  into linear light named as the cause — they are the only two scenes
  that mixed two colours through compose's own verb, and `lain_navi`'s
  default still does not show it.
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

The blockers, the correctness should-fixes and the README guard are
fixed; what is left of this group is `Composer::Impl::paint`, one
1380-line function (`core/StackingPainter.cpp`), which the rulings above
defer, and the nits the fix pass's report names as left.

## Sketch framework (findings/review-sketch-framework.md)

Both blockers, every should-fix and every nit but one are fixed: the kit
spellings and look defaults, the README drift, the shipped strings, the
`ThumbnailQueue` lift and the cases the report named. The file splits
`book/main.cpp`, `book/SketchbookView.cpp` and `book/qml/Main.qml` are
deferred by the rulings above. What is left:

- `sketch::device()` and `sketch::painterRuntime()` are still process-wide
  where a set's runtime is now per-session, so a CANVAS sketch that stands
  a mesh up in space (`floating_panels`, `painter_gpu`) still draws its
  background thumbnail through whatever device the process installed.
  Intended: a still is CPU-only whatever the process holds, for every
  runtime. Assert: a canvas thumbnail with a painter runtime installed
  never reaches it.
- `book/SketchbookView.cpp` — `keyAs` (the p5 key table) is still an
  anonymous-namespace function in the app and untested. It was left in
  place: its input is a `Qt::Key`, and `core/` is Qt-free, so moving it
  there would either bring Qt into the library or hard-code Qt's
  numbers. Either it moves with a seam of its own or the table is
  pinned where it stands, from the book's script tests.

## The sketches (findings/review-sketches.md)

Every should-fix and every nit is fixed except the library extractions
the rulings defer, which are the paragraph below, and five entries the
fix pass's report names as left with their reasons: `stroke_atlas` keeps
its absolute placement (a rule specimen is positioned against the shape
it is shown on, which the file states as its own constraint), and four
findings that were stale when written — `chaucer_astrolabe` and
`minard_1869` render their check runs through `kit::console` already,
`dunhuang_star_chart` has no check run to render, and `xcom_battlescape`,
`thaumonomicon` and `kumiko_asanoha` already stand on the origin the nit
names.

Deferred, each to its own campaign — the mechanisms hidden in the
sketches over 1000 lines: palette-indexed sprites and an atlas packer
(`xcom_battlescape`, `cde_motif`, `thaumonomicon`) → `compose/kit/
Sprites.h`; an HTML auto-table layout scheme (`spacejam_1996:999-1200`) →
compose kit layouts with a test; an icosahedron and a face-up pose
(`bg3_dice_roll:376-503`) → `geometry/kit/Solids.h` and a mesh pose; a
conic generator and four silhouettes (`ksp_mapview:244-566`) → geometry
path and `Silhouettes.h`; Motif bevels (`cde_motif`, `twoadvanced_v4`,
`winamp_base`, `fallout2_charsheet`) → `Chrome.h`; a stereographic and a
chart projection (`chaucer_astrolabe`, `dunhuang_star_chart`) → a
projection value in geometry path; a tartan sett-to-cloth weave
(`black_watch:234-295`) → material pattern; a pentagrid
(`penrose_paving:264`) and mitred lattice joinery (`kumiko_asanoha:517`)
→ `Lattice.h`/`Ops.h`; Reeves particles (`genesis_fire:516`) →
`sigilmotion/physics/Points.h`.

## SigilGeometry path ops (findings/review-geometry-path-ops.md)

Deferred by the rulings: the file split by subject — `path/Ops.cpp` (604:
the pathops booleans, the offset family, the corner treatments, the
resample distorts).

## SigilGeometry mesh, point operators, kit, device (findings/review-geometry-mesh-pop.md)

Deferred by the rulings: the file splits by subject — `Pop.h` (1131),
`mesh/pop/Cook.cpp` (792), `mesh/codec/Geo.cpp` (601).

## SigilWorld, SigilUsd, SigilSubstance, SigilImage (findings/review-geometry-material-world.md)

Every blocker, should-fix and nit this report names is fixed, with the
tests it asks for.

## SigilWeave and SigilDraw (findings/review-weave-draw.md)

Every blocker, should-fix and nit of this group is fixed except three,
which are decisions rather than defects and are named in the fix pass's
report: merging `Selector::Kind::Words` into `Word` (the switch that reads
it is SigilCompose's), choosing one spelling of `Unit` against `unit::`,
and lifting the Knuth-Plass prefix tables out of the DP function (the
lambdas over them are stated to inline).

## SigilCore, SigilData, SigilMeasure, SigilMotion, SigilIO, SigilVideo, the product, the build (findings/review-core-io-build.md)

Every blocker, should-fix and nit this report names is fixed, with the
tests it asks for.

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
