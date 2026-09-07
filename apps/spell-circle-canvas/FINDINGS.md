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
- Device tier: 190 of 195 within the per-channel bar; four over
  (`aero desktop` mean 21.95, `winamp_base` 20.58, `chevreul_circle`
  7.81 with p99 153, `nine slice` p99 205) and `brushwork_currents`
  crashes on the GPU lane — filed below.
- Promotion tier: 111 of 195 within one code value; the 84 outside are
  the standing entry below, which reports rather than gates.
- Sanitizers and the bench and app-FPS retake: run after this state was
  taken; their outcome is recorded in the standing entries if they find
  anything.

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

## Sketch framework, left by the fix pass

- `sketch::device()` and `sketch::painterRuntime()` are still
  process-wide where a set's runtime is now per-session, so a CANVAS
  sketch that stands a mesh up in space (`floating_panels`,
  `painter_gpu`) still draws its background thumbnail through whatever
  device the process installed. Intended: a still is CPU-only whatever
  the process holds, for every runtime. Assert: a canvas thumbnail with
  a painter runtime installed never reaches it.
- `book/SketchbookView.cpp` — `keyAs` (the p5 key table) is an
  anonymous-namespace function in the app and untested; its input is a
  `Qt::Key` and `core/` is Qt-free. Either it moves with a seam of its
  own or the table is pinned where it stands, from the book's script
  tests.

## Compose, left by the fix pass

- Test-gap nits: `drawInkedImage`'s recorded-region and non-invertible
  paths, `Region::oval` (constructed in a brush case, asserted about
  nowhere), `balanceThroughLine`.
- `Track::over` against `Annotation::unit` — the better rename touches
  `kit/Typeset.h`, `core/Element.h` and TYPOGRAPHY.md; a vocabulary pass
  takes it.
- Named and declined with a reason in the fix report, kept here so the
  review is not re-run: the three pan-only predicates are three
  questions; `Ribbon::band` is a documented join vocabulary;
  `turnedArea` wants a span overload in SigilGeometry first; the
  ornament's Bernstein and the two `Hatch` constructions would move
  plates for a nit with no defect behind them.

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

## rota_convocationis cannot hold 60 FPS through its own emissive stack

The scene draws one charged disc: every lit band, seal, star and rim
flame is an emissive fill laid over the whole disc, and the composite
misses the 60 FPS gate through the second half of the cycle. The sketch
carried `ctx.plate()` to be judged as a still instead, which
`CanvasSpec::plateOnly` states is for a sketch whose subject is the size
of the sheet it draws and never a timeout override. The mark is gone and
the look stands, so the scene now presents as what it is.

What the compositor does: each emissive layer is a full-disc fill drawn
into its own layer and composited, so the per-frame cost scales with the
number of lit elements rather than with the area any of them covers, and
nothing coalesces layers that share a blend and a clip.

Intended: a run of emissive fills over one disc is one composite pass,
whatever it costs to build, so a scene's frame cost tracks the pixels it
touches rather than the count of nodes that touch them.

Assert once fixed: `--bench` on `rota_convocationis` holds 60 FPS across
the whole loop on a raster surface, and a case in `compose_bench` pins
the cost of N emissive fills over one shape as flat in N past the first.

## The device tier reads four scenes outside its bar

`sigil.py plates --tier device` renders every scene on the GPU and
differences it against the CPU plate of the same run, per channel, with
a bar of mean 12 and p99 128. Four scenes stand outside it:

    aero desktop     mean 21.95  p99 204  max 255
    winamp_base      mean 20.58  p99  97  max 204
    chevreul_circle  mean  7.81  p99 153  max 183
    nine slice       mean  4.67  p99 205  max 215

`winamp_base` and `chevreul_circle` moved on the CPU in the same pass
(a colour mix in linear light; a check run rendered through the kit's
table), so the first question for each is whether the device path
mixes or lays out differently from the host, or whether the CPU move
merely exposed a difference that was already there. `aero desktop` and
`nine slice` did not move on the CPU, so their difference is the
device's alone.

Intended: a device plate is the CPU plate within the bar, and every
scene renders on both. Assert once fixed: `--tier device` reports every
scene within the bar and none failed.
