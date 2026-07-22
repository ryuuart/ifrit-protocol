# SigilCompose — stress-test catalog

## Phase-1 status and reference numbers

Kernel landed: items 1–3 render in `compose_demo` (PNG panels), the
kernel semantics (layout, stacking, memo, keyed reorder, auto-caching,
transition retarget/unmount, bindings) are pinned by `compose_test`,
and `compose_bench` (Release, Apple Silicon dev Mac, 100 text rows,
800×2400 raster target) reads:

| Benchmark | Time | Notes |
| --- | --- | --- |
| render, nothing changed | 29 µs | 100 memo hits ≈ 290 ns/row |
| render, one row changed | 30 µs | one describe+patch ≈ +0.7 µs |
| render, cold mount | 226 µs | describe + mount + Yoga |
| draw, fully cached | 406 µs | 1 picture, 0 live nodes |
| draw, root-bound volatile | 375 µs | volatility partitions: 1 live node, rows stay cached |
| relayout on width change | 583 µs | ~200 paragraph re-measures |
| frame with 1 transition | 700 µs | ticker step + partial repaint |

**Cache::Texture (landed, measured):** dense text block (800×540
fully covered): picture replay 1224 µs → texture blit **600 µs (2.0×)**;
the sparse 100-row list texture-cached whole: 404 µs → 597 µs (48%
*regression* — blitting mostly-empty area). Both predictions held:
texture-cache dense or effect-heavy subtrees, leave sparse regions on
Auto. ComposeGallery (`--headless` smoke) adds the interactive vehicle
for the motion items: live scoreboard mutations with transitions,
Choreograph-bound headline + per-frame custom leaf, binding-driven
blend stack.

**Decorations + slots (landed):** the Decoration seam is live —
`background()`/`foreground()` take any `DecorationScheme` — with the
three primitives in `<sigilcompose/Decorations.h>`: `PathFormat`
(solid/dash/stamped strokes along the outline; item 9's dashes and
item 10's vine stamps render in compose_demo's chrome panel), `Slice`
(drawImageLattice nine-patch, pixel-tested on a synthesized asset —
item 8), and `ContourWalk` (arc-length walk with tangent-aligned
canvas, sample counts and volatility declaration tested; element
stamps still pending). `slot()`/`renderSlot()` covers item 4 with the
isolation guarantee pixel-tested: a slot update re-records only the
ancestor chain — sibling paint programs never re-run. Enabling fix:
caching is now **per-node** (clean children embed as nested
drawPicture refs in ancestor re-recordings) plus dirty-propagation up
the parent chain; re-measured — cached draw 412 µs, volatile 368 µs,
renders 30 µs: policy change is cost-free and buys partial
invalidation everywhere.

**Layer effects (landed, measured):** `.effect()` (node's own layer,
SkImageFilter or SkSL via `Effect::shader`) and `.backdrop()` (filters
beneath the node, backdrop-incompatible with texture baking and
guarded so) are in the kernel, pixel-tested (blur bleed, deterministic
backdrop color-invert, baked-texture filter). The crt_bloom demo panel
renders items 13/14: plus-blended scanline stacks brightening where
they overlap and a bloom halo (blurred plus-blended headline copy).
The decisive number: a blurred headline drawn per frame costs
**7.38 ms** via picture replay (filter re-executes) vs **80 µs**
texture-baked — **92×** — making Cache::Texture-under-effect the
mandatory pattern for static filtered content.

**Graphite re-measure (item 21, answered):** same benchmarks against
a Graphite Metal RenderTarget (wall / CPU): 100-row cached draw
**313 µs / 311 µs** (vs 412 µs raster — modest: replay still pays
per-op command translation on the CPU; rasterization moves to GPU);
bloom via picture replay **369 µs / 84 µs** (vs 7.38 ms raster — the
filter executes on GPU, 20× wall, 88× CPU); bloom texture-baked
**57 µs / 15 µs** (best on every axis). Verdict: Graphite absorbs
filter cost dramatically, texture baking still wins another ~6×, and
picture replay is cheaper but not free anywhere — the honest ceiling
is display-list translation. Also landed this round:
`<sigilcompose/Util.h>` (gradients, stroke/shadow sugar — background
decorations now correctly paint *beneath* the fill, the CSS
box-shadow ordering — and the three-line `Stage` bundle, all tested)
and `layout(LayoutScheme)` (item 6: the ~20-line user Grid places and
sizes cells via the bounded second layout pass, bounds-verified).

**Derive phase (landed):** `text().flowAround(key, margin)` plumbs
resolved node bounds into SigilWeave `ExclusionFlow` shapes as the
design's bounded second layout pass (item 7: pixel-verified — the
excluded region stays text-free and displaced words push the paragraph
taller; self/descendant references are cycle-guarded and ignored), and
`connector(from, to, router)` makes relationships first-class elements
(item 12, core): the routed path (straight default, Router fn for
custom) arrives as the connector's PaintContext outline, so a
PathFormat foreground strokes it — endpoint moves re-route and
invalidate through the standard dirty chain, pixel-tested across a
re-render.

**Polish pass + the particles answer:** frame-loop hot paths cleaned
— the volatility walk now runs only on reconcile/animation frames
(plus one settling frame), stats tallies moved out of draw() into
stats(), trivial leaf boxes no longer earn pictures, text measurement
short-circuits Yoga's re-probes by (constraint, content-rev), and the
derive/custom-layout passes skip trees that contain none. `leaks`
reports **0 leaked bytes** for compose_demo and ComposeGallery.
"What would millions look like?" is now measured: UI-as-particles is
ONE `Cache::None` custom leaf over an EnTT registry (SoA pools stepped
as a Ticker steppable) batching into a single `drawAtlas` — the
textflow GlyphRSXformBatches pattern. CPU raster: ~300 ns/particle
flat (10k = 3 ms, 1M = 309 ms; per-particle drawCircle is 2.5×
worse). **Graphite: 3.7 ns/particle — 1,000,000 plus-blended sprites
in 3.7 ms/frame.** Millions are a leaf, not a tree; EnTT belongs on
the user side of the seam, exactly where the scene core already uses
it. Retained-tree instances themselves stay AoS until profiles say
otherwise (the tree walks are no longer per-frame).

**Completeness round (landed):** the remaining catalog gaps closed in
one pass — measured Release, Apple Silicon dev Mac, raster unless
noted:

- **Structural prune without memo** — the reconciler now proves
  value-equality on comparable descriptions (plain boxes, fills,
  text runs, images; anything carrying an incomparable callable stays
  conservative): re-rendering an unchanged non-memo tree patches 0
  nodes, records 0 pictures, and leaves `dirty()` false, so hosts can
  skip the frame entirely. Keyed reorders/mounts/unmounts still
  invalidate through an explicit structure check.
- **hitTest (item 5)** — paint-order, transform- and shape-aware
  (star-arm gaps miss; rotated bars hit in their visual place;
  keyless nodes resolve to the nearest keyed ancestor; clips bound
  their subtrees): **1.2 µs** per query over a 50-blob rotated
  scatter. Demoed in `query_hits.png` (host draws bounds() ring +
  hitTest probe row) and live in the gallery's organic scene.
- **Per-edge chrome (item 9)** — `shapes::edges()` extracts exact
  sub-contours by arc-length classification (rounded corners split at
  the diagonal, bisection-refined) and `onEdges(mask, decoration)`
  retargets ANY primitive at them; pixel-tested both on rects and
  rounded rects.
- **Element stamps (items 10, 20)** — `snapshot()` (one-shot
  reconcile→layout→record at intrinsic size, bindings sampled,
  liveOnly) plus `ContourWalk::stamp`: unit-proven record-once
  (stamp's describe runs exactly once while ~40 samples replay),
  level-2 recursion (a stamp whose own decoration walks its contour)
  and a nested-Composer custom leaf both pixel-tested. A stamped
  border card draws in **74 µs**.
- **Tile maps (items 15, 16)** — `image(atlas).region(rect)` with
  strict source constraint; childless image leaves no longer earn
  per-node pictures (a chunk records flat drawImageRects). 24 memo'd
  chunks × 100 tiles (2,400 tiles, 960×640): **2.49 ms** steady-state
  replay, one-chunk mutation costs **+80 µs** over steady state
  (only that chunk re-records — unit-asserted ≤3 recordings).
  *Item 16 verdict inverted on CPU raster*: the single SkSL
  atlas-sampling fill costs **5.52 ms** — per-pixel runtime-effect
  eval loses to 2,400 cached drawImageRects by 2.2×. Take the SkSL
  route only on GPU targets (Graphite executes the effect on-device).
- **Direct leaf blending** — fill-only leaves route blend/opacity
  onto the fill paint instead of a device-clip-sized saveLayer
  (guarded: live opacity and texture bakes keep the layer). A
  100-blob plus-blended field draws in **539 µs**; the gallery's
  organic scene dropped 300 ms → 88 ms (Debug) from this alone.
- **Transform-replay caching** — paint-only volatility (bound/
  transitioning transforms and opacity) no longer blocks the node's
  own content picture: a spinning 7-point star with a stamped border
  replays at **95.8 µs/frame** with zero re-records (ancestors still
  demote, so nothing above bakes the motion). Fill lerps, animated
  decorations, Cache::None, and animated image frames remain content
  volatility and paint live.
- **Kernel/API completions** — `wrapLines()`, per-edge
  padding/margin, per-corner `Corners` (`{tl,tr,br,bl}`),
  `autoDim()` + `_px`/`_pct` literals, full-control
  `text(shared_ptr<Paragraph>, ParagraphLayoutOptions)` (pointer
  identity = change signal), honest `PaintContext::contentScale`
  (host canvas scale; texture bakes report their bake scale),
  `PaintContext::fonts`, and the intrinsic-size root rule
  (`setSize({})` → content-sized, the snapshot path). The organic
  extension headers — `Shapes.h` (polygon/star/squircle/blob +
  `rounded()` + `edges()`), `Layouts.h` (Radial/AlongPath/Scatter),
  `Routers.h` (straight/orthogonal/arc), `Web.h` (`web()` leaf,
  engine-tested in compose_web_test both directions) — all plug into
  existing seams; the kernel depends on none of them. 56 unit tests
  green; gallery grew the `organic` scene (everything above, live)
  and the tile maze; compose_demo gained `organic.png`,
  `tile_maze.png`, `query_hits.png`.

**Finding (assumption revised):** on a *raster* target, SkPicture
replay re-rasterizes — cached and volatile draws cost the same ~400 µs
because pixels dominate, and the cache's win is confined to describe/
layout/traversal. The pixel-level win arrives with (a) Graphite
targets, where replay is cheap command re-recording on the GPU, and
(b) `Cache::Texture` (phase 2), which rasterizes once — now justified
as the *raster-surface* optimization, not just an effects host. At
~0.4 ms per full 100-row redraw the raster numbers are comfortably
inside frame budget regardless; item 21 re-measures on Graphite.

Every use case raised during design review, kept as the implementation's
acceptance suite. Each item names what it exercises and how it will be
validated: **golden** = headless golden-image test (weave_demo-style
deterministic render), **gallery** = interactive scene in ComposeGallery
(the planned Qt host, WeaveGallery-style), **bench** = compose_bench
measurement. Phases refer to DESIGN.md's plan.

## Composition & data (P1–P2)

1. **Static typographic poster** — headline + baseline-aligned mixed
   sizes on one row + cover image behind (`zIndex`), gradient fill;
   whole tree `Cache::Picture`; drawn to PNG. Exercises: text leaves,
   baseline alignment, absolute positioning, static caching. *golden*
2. **Data-driven scoreboard** — rows via `memo`, keyed reorder
   (shuffle preserves per-row transition state), score changes ride
   implicit transitions, highlight fill fades. Exercises: reconciler,
   memo, keys, `transition`. *golden (steps) + gallery (live)*
3. **Multiple tables with layered effects** — two scoreboards overlaid
   in a `stack()`, upper one `.blend(kMultiply)` + `.opacity(0.8)`.
   Exercises: cross-component stacking, blend-as-layer semantics.
   *golden*
4. **Independent data domains** — static poster containing
   `slot("ticker")`; ticker strip re-rendered at 10 Hz while the
   poster's caches stay warm (assert no re-record of the static
   subtree). Exercises: slots, cache stability. *golden + bench*
5. **Query-driven surround** — host draws scene geometry around a
   node via `bounds(key)`; `hitTest` picks the topmost key at a point.
   Exercises: Composer query surface. *golden*

## Layout (P1, P4)

6. **Lightweight grid** — `LayoutScheme` grid (`Grid{.columns, .gap}`)
   with flex content nested inside cells and the grid nested inside a
   flex column. Exercises: custom layout protocol, Yoga interop both
   directions. *golden*
7. **Text flowing around frames** — article text `.flowAround()` a
   floated hero frame (SigilWeave `ExclusionFlow` from the frame's
   resolved outline; second layout pass; cycle rejection unit test).
   Exercises: derive phase, exclusion plumbing. *golden*

## Chrome & decoration (P1, P4)

8. **Ornate frame via `Slice`** — lattice with mixed
   stretch/repeat cells (9-slice as the 3×3 case). *golden*
9. **Per-edge treatments** — top edge lattice strip, bottom edge dashed
   `PathFormat`, left edge stamped `ContourWalk`; right edge bare.
   Exercises: sub-contour extraction, primitive composition. *golden*
10. **Vine border** — `ContourWalk` stamping an *element subtree*
    (ornament with its own decoration) around a rounded card; verify
    stamp picture is recorded once and replayed per sample. Exercises:
    element stamps, recursion level 1, walk caching. *golden + bench*
11. **Procedural border** — SkSL `PathFormat` paint + an `animated()`
    SkSL border that demotes its node from picture cache while active,
    re-promotes when stopped. Exercises: declared volatility.
    *golden (frozen time) + gallery*
12. **Connectors** — `connector("a","b")` routed between two moving
    cards, formatted with a flow-field SkSL fill along the path
    (mirrors the scene schema's Edge-between-Points). Exercises: derive
    phase relationships, endpoint tracking. *gallery + golden*

## Effects & compositing (P3)

13. **CRT stack** — several semi-transparent tiled scanline layers,
    `.blend(kPlus)`; brightness accumulates where layers overlap; plus
    a `backdrop()` barrel-distortion SkSL over the stack. Exercises:
    additive stacking contexts, backdrop effects, saveLayer cost
    (measure!). *golden + bench*
14. **Bloom, foreground and background** — bright-pass + blur
    `Effect` on a headline (foreground) and on a background sigil
    layer; verify `Cache::Texture` pays the filter once for static
    content. Exercises: layer effects, texture-cache-under-effect.
    *golden + bench*

## Tiling (P2–P3)

15. **Tile-map background** — describe-phase component selecting atlas
    regions (`image(atlas).region(...)`) from a rule/noise function,
    chunked into `Cache::Picture` boundaries; mutate one chunk's data,
    assert only that chunk re-records. Exercises: atlas regions,
    chunked caching, data-driven generation. *golden + bench*
16. **Pure-SkSL tiled background** — one fill sampling the atlas
    procedurally; compare cost against 15. *bench + gallery*

## Animation & motion (P3)

17. **Choreographed headline** — `ch::Output` bindings for drop/fade
    (paint-only: assert zero relayouts), plus glyph rain via
    `sigil::weave::Choreograph` reading `paragraphLayout(key)` as a Ticker
    steppable surviving a data refresh of a sibling. Exercises:
    bindings, steppables, glyph-state stability. *gallery + golden
    (frozen time)*
18. **Transition choreography** — list insert/remove/reorder with
    per-key staggered transitions; removed keys unmount cleanly
    mid-flight. Exercises: reconciler ↔ timeline interaction. *gallery*

## Integration & recursion (P4–P5)

19. **Web leaf in a composition / composition in a page** — a live
    `WebView` frame as a leaf (Cache::None), and a Composer drawn into
    an SigilScry `WebImage` slot — both directions with SigilScry.
    *gallery*
20. **Recursive stamps** — a `ContourWalk` stamp whose own decoration
    walks its contour (2 levels), and a `custom()` leaf drawing a
    nested Composer. Exercises: recursion closure, forward-only law.
    *golden*
21. **Sustained animation load** — N cached cards with transform
    bindings at 60 FPS: per-frame describe/layout/paint costs, memo hit
    rates, saveLayer counts. The perf gate for phase 3. *bench*

## Vehicles

- **Golden tests**: headless, deterministic (frozen FrameClock time,
  fixed seeds), PNG-compared like the textflow demo panels.
- **ComposeGallery** (P5): Qt app in the WeaveGallery mold — one
  scene per catalog item above marked *gallery*, with live controls
  (time scale, pause, data mutation buttons) for eyeballing motion and
  interaction that goldens can't capture.
- **compose_bench** (P5): google-benchmark target for the *bench*
  items, alongside weave_bench/scry_bench.

## The GPU re-measure (2026-07-21) — the 60 fps floor holds everywhere

`ComposeGallery --headless --gpu` sweeps all 26 scenes on a Graphite
Metal surface with each frame serialized to completion
(`SyncToCpu::kYes`), so "work ms" is the honest worst-case CPU+GPU
cost — a pipelined host does better. Phase columns (recon/layout/
volat/paint) come from `Composer::Stats`; on every scene, slow or
fast, the retained machinery rounds to 0.00–0.01 ms — frame cost is
paint, i.e. pixels.

| scene (the former CPU offenders) | CPU raster | Graphite GPU |
|---|---|---|
| daemon console | 53.7 ms (19 fps) | **0.69 ms** (1458 fps) |
| flourish | 14.9 ms | **1.9 ms** |
| persona menu | 9.2 ms (p99 61.9) | **0.44 ms** (p99 0.61) |
| aero desktop | 9.7 ms (p99 54.4) | **0.86 ms** |
| passive tree | 9.4 ms | **2.6 ms** |
| ui particles | 7.6 ms | **0.43 ms** |
| y2k chrome | 5.5 ms | **6.3 ms** (GPU-heavy blur stack) |

Worst GPU scene = y2k chrome at 159 fps headroom; 24 of 26 scenes are
under 2 ms.

**Re-measured 2026-07-22 after the reference-grounded rebuild** (22
scenes; the generic "rpg hud" retired for a Veloren-grounded "world
hud", four studies added — loot grid, gerstner grid, cosmati, and the
passive tree rebuilt on Path of Exile's real tree export). The floor
still holds: every scene is above 150 fps on Graphite, 18 of 22 under
2.2 ms, and the retained phases still round to 0.00–0.01 ms.

| scene | Graphite GPU | CPU raster |
|---|---|---|
| y2k chrome | 6.64 ms (151 fps) | 5.9 ms |
| loot grid | 6.08 ms (165 fps) | 20.9 ms |
| passive tree | 4.52 ms (221 fps) | 20.4 ms |
| world hud | 3.87 ms (259 fps) | 37.6 ms |
| zellige | 2.20 ms | 6.7 ms |
| flourish | 2.09 ms | 15.0 ms |
| cosmati | 1.77 ms | 8.3 ms |
| gerstner grid | 1.68 ms | 17.4 ms |
| everything else | < 1.2 ms | — |

The four newest scenes invert the usual ratio — they are cheap on GPU
and expensive on raster — because they lean on SDF materials,
instances() and generated patterns, all of which are per-pixel shader
work the software backend evaluates in the interpreter. That is the
same conclusion as before, arrived at from the other direction. The CPU-raster p99 spikes were quantized live materials
hitting their re-resolve frame (one full-screen software SkSL eval);
on GPU they vanish. Conclusion pinned: full-screen live materials are
GPU-tier content — the CPU-raster path re-rasterizes them per pixel
per frame in software, and no tree/layout/kernel change alters that.

**Two Graphite findings (both fixed at the shared seams):**

1. **Graphite performs no implicit raster-image uploads.** Any draw
   sampling a non-Graphite SkImage consults the Recorder's client
   `ImageProvider` — absent one, the draw is silently DROPPED
   (`KeyHelpers.cpp` "Couldn't convert SkImage…"). Every generated
   atlas/nine-slice/tile draw was being dropped on GPU.
   `SkiaGraphiteContext::makeRecorderOptions()` now installs a caching
   provider (promote on first use, keyed by uniqueID+mipmapped) at
   every recorder-creation site, all backends.
2. **`kRGBA_F32` textures are not linearly filterable on Apple GPUs**,
   so an F32 OCIO LUT cannot be promoted and the whole graded
   composite drops. LUTs now bake to F16 (ample for display LUTs in
   [0,1], filterable everywhere).

Hosts: ComposeGallery's interactive view already rendered through
Graphite (QQuickRhiItem + shared context, Metal); ComposeSketch now
does the same (the CPU-raster QQuickPaintedItem host is gone — it was
where "the framework is slow" impressions came from). Both fall back
to raster-and-upload off Metal, and `Composer::purgeCaches()` handles
backend switches (GPU-minted cache images must not replay onto the
next canvas).

**instances() (2026-07-21, `<sigilcompose/Instances.h>`):** the flyweight
repeat layer — Atlas of element-tree cells + user-owned SoA Pool + one
`drawAtlas`. 10k instances: 36.8 ms CPU raster (3.7 µs/sprite — raster
replay re-rasterizes, Data≈Live there) vs **0.18 ms Graphite Live**
(18 ns/sprite, ~200×) — masses are a GPU play, as designed. Data mode
prunes on (atlas, pool, revision); Live is the Cache::None particle path.

**ElementNode split (2026-07-21) — SUPERSEDED; the sizes below are the
figures as of that date, not current.** A later arc took `PaintProps`
856 → 256 B and `ElementNode` 1288 → **688 B**, and tightened the guard
from 1400 to **768**. That correction is 40 lines down, under
"PaintProps compaction", and this entry is left in place because this
document is a dated LOG — but a benchmark table is read for its numbers
and not for its date, so the pointer belongs at the top rather than
below. `Composer.cpp` is the authority: `static_assert(sizeof(ElementNode)
<= 768)`.

The monolithic ~46-field description
struct became a lean base (hot fields: kind/key/layout/paint/corners/
decorations/children) + seven out-of-line `Box<T>` blocks (Text, Image,
Custom, Derive, Fx, Material, Memo — value-semantic deep-copying boxes,
so the COW clone in `NodeHandle::operator->` still works). Measured:
sizeof 2752 → **1288 B** (−53%); describe benches all improved —
unchanged 100-row render 30.7 → 26.5 µs, one-changed 32.3 → 27.3,
decorated-unchanged 130 → 112, cold mount 273 → 254 µs. Behavior
preservation proven two ways: 190 tests green both configs AND a
pixel-exact capture diff (26/26 gallery scenes identical pre/post split;
the headless captures now render with the FPS overlay OFF so they are
deterministic and diffable). A `static_assert(sizeof(ElementNode) <=
1400)` guards regrowth — new rare/kind-specific state goes in a block.
Next size target if ever needed: PaintProps is 856 B of the base (8 fat
PropValue variants) — slimming PropValue is a public-API change, parked.

**Brush-arc tail (2026-07-21):** the last three seams-audit items landed.
`brushes::ArtBrush`/`artAlong()` — Illustrator's Art Brush proper: one art
cell baked once (2x), each contour walked into a triangle-strip ribbon,
one `drawVertices` warps the art CONTINUOUSLY around curvature (rigid
stamp runs can't) — 0.22 ms/frame live on CPU raster for a ~1500 px
S-curve at 6 px stations, and it caches when settled. `lines::hatch()/
crosshatch()` — Sk2D lattice fill clipped to the outline (0.98 ms live
for a 400 px blob). `styles::gloss()/GlossContour` — the PS Satin/Gloss
Contour curve: blurred coverage through a 256-entry ring table, clipped
inside the shape (blur→TableARGB on one image-filter chain; same math as
SkTableMaskFilter, composable). Night network shows the first two
(ARTLINE vine + SALTMARSH hatch — twelve constructions now); the y2k gel
orb wears the gloss. 3 pixel tests.

**ui_particles on instances() (2026-07-21):** the scene's hand-rolled
atlas surfaces + RSXform arrays + drawAtlas leaf replaced by
`instancing::Atlas` cells (element trees registered directly) + two
Pools synced from the EnTT sim each frame + two `instances(...,
Mode::Live)` leaves. Same look, counts, and seeds; 8.05 ms CPU raster /
**0.54 ms GPU**. Port friction folded back into the layer: all-white
tints now skip the colors lane (kSrcOver), and the stamp reuses
thread-local scratch instead of allocating three vectors per frame.

**VariationDrive (2026-07-22):** `text(...).variationDrive("GRAD", &out)`
— a variable-font axis driven at DRAW time, paint-only (no reshape, no
relayout). The gate is per-font: `FontContext::axisIsAdvanceInvariant`
samples every glyph advance at the axis extremes; advance-variant axes
(wght on most fonts) are REFUSED with a warning and the text draws at
its shaped coordinates — GRAD is the advance-invariant weight. The
weave side is `ParagraphLayout::LiveVariations` (bucket typefaces swap
for memoized varied clones at flush; positions untouched). Probe result
memoizes per (instance, content). Pinned by two tests incl. a
bounds-hold + pixel-thicken check on the SF face.

**PropValue slimming (2026-07-22):** `PropValue<T>` is no longer a
std::variant — the fat Transitioned payload (~100 B: from/waypoints/
spec) boxes out-of-line, plain constants and bindings stay inline.
PaintProps 856 → 256 B; **ElementNode 1288 → 688 B** (2752 B before
this arc, −75% total); the size guard tightens to 768 B. Describe
benches hold/improve (unchanged 26.3 µs, cold 250 µs). Behavior pinned
by the capture diff: 29/29 CPU scenes byte-identical (modulo the
intentional y2k sliver fix).
