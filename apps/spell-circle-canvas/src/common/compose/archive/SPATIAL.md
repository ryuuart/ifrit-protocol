# SigilCompose — the spatial question

> **ARCHIVED 2026-07-25.** A design conversation (2026-07-24), half
> source-verified and half argument — see its claims table. Its settled
> conclusions moved to `../DESIGN.md`: the 2D-affine boundary and
> dependency refusals (§Boundaries), the tile-bake primitive and order
> of work (§Direction), the markup refusal (§Boundaries). The ribbon
> analysis (§5) and glass-panel recipe (§4) remain here as the working
> notes for those features; their claims stay claims until built.

Written 2026-07-24 from a design conversation, not from building. Read
the claims table at the bottom first: roughly half of this is verified
against source and half is argument. Companion to `DESIGN.md`
(architecture), `ROADMAP.md` (what the studies asked for), `REVIEW.md`
(the first-principles pass).

The question that started it: should Compose take Eigen, Diligent Engine,
and a tiling renderer; does it still make sense as a C++ embedded DSL
rather than a markup language; and how do 2D layout and 3D scenes share
one library. The short answer is that four unrelated-looking asks converge
on **one missing primitive**, and none of them require Compose to learn
anything about 3D.

---

## 0. The convergence — one primitive, four clients

This is the finding worth keeping. Four separate requests want the same
thing:

| Client | What it wants |
|---|---|
| 3D panels | bake a subtree to a texture at the panel's screen-footprint density |
| Infinite canvas | tile cache keyed on `(tile rect, scale)` |
| High resolution | the same key |
| Ribbons | per-tile bake of an unrolled strip |

The primitive: **snapshot a live keyed subtree at a host-chosen density,
invalidated per tile.** The shared trap is already recorded in
`HANDOFF.md` — the y2k defect was *"bake recorded at 1x, replayed at 2x."*
Any of these four that fails to key on scale reproduces that bug once per
zoom level.

Existing pieces: `Cache::Group` (§30) is the bake; `node.bakeScale`
(`Paint.cpp:2115`) is the internal scale channel; `Element::bakeScale`
(`Compose.h:1220`) is the *authored* density factor, which is intent, not
footprint.

---

## 1. Dependencies

### Eigen — no

Eigen's value is dense/sparse arithmetic and decompositions. Nothing on
the roadmap wants that. §3 wants `t → (x,y)` sampling; §8b wants
tangent/normal frames and arc-length reparam; §2 wants per-instance
affines. That is 2/3/4-component math, where expression templates buy
nothing at runtime and cost compile time plus a second vocabulary at
every seam — and every seam in Compose is a Skia seam.

The vocabulary already ships in the installed Skia: `SkM44`, `SkMatrix`,
`SkPoint3`, `SkRSXform`, `SkVertices`, `SkMesh`.

What is actually missing is **computational geometry**, which Eigen does
not do either:

- robust polygon boolean and **offsetting** → `clipper2` (in vcpkg).
  `SkPathOps` already gives Op/Simplify/AsWinding (§14, used for
  parallel-rail repair); variable-width offsetting with miter control is
  the genuine hole.
- triangulation, if 3D lands → `earcut-hpp` / `cdt` / `libtess2` (all in
  vcpkg).

Eigen earns a place only given a real *solve*: stress-majorization graph
layout, least-squares Bézier fitting, Cassowary-style constraints. Real,
narrow, not now.

### Diligent Engine — no

It would be the fourth graphics abstraction in one process, and three of
them want to own the device: Graphite (Metal/Vulkan/Dawn, SkSL),
Ultralight's `WebGpuDriver`, Qt's QRhi, and Diligent (HLSL→SPIRV→MSL, its
own PSOs and binding model). Two shader languages, two resource models,
one queue to marry them. **There is no vcpkg port** — it would need a
custom port on the `choreograph` model. Verify Diligent's Metal backend
licensing before spending any further thought on it.

The need behind the ask is real: Graphite exposes no depth buffer, no
compute, no instanced draws, no texture arrays. **Texture arrays are the
one item on the original list that genuinely cannot be reached from
Skia at any layer** — which is the tell that the 3D question is upstream
of the tiling question, not beside it.

When 3D goes live, two honest paths, to be priced then and not before:

1. **Raw Metal behind a `Sigil3DDriver` seam** — the
   `UltralightMetalDriver` / `WebGpuDriver` pattern, already built and
   proven: share Graphite's `MTLDevice`/`MTLCommandQueue`, everything
   rides one queue so ordering is implicit, publish by blitting into
   ping-pong textures, wrap zero-copy as a Graphite-backed `SkImage`.
   Portability paid twice, later.
2. **Graphite-on-Dawn** — 2D and 3D share one device and one shader
   story, and compute/depth/texture-arrays arrive from the same API
   Chrome ships. Strategically the cleaner option if 3D is more than a
   maybe.

---

## 2. The boundary: Compose stays 2D and affine

**The boundary is already latent in the code and merely unnamed.**
`Paint.cpp:1546`, `:2009` and `:2115` all guard on
`!totalM.hasPerspective()` — Compose silently degrades its cache paths
under a perspective matrix today. Promoting that from an implementation
detail to a stated law is free; per §28 the law then wants a test rather
than a doc comment.

**Cheap experiment, worth an afternoon before designing anything.**
`SkCanvas::concat(const SkM44&)` exists (`SkCanvas.h:988`), as does
`include/utils/SkCamera.h`. Drawing a Composer under a projected `M44`
gives a perspective plate with **live** content — no bake, transitions
and bindings still running. The cost is exactly the guards above: the
promotion paths switch off and the plate paints live. That is a number,
not an argument, and the profiler already measures it. If it holds 60 fps
for panels-at-an-angle, a large class of cases never needs the texture
path.

**The one concession worth taking**: `Element::perspective(SkMatrix)` on
a `Texture`-cached subtree — paint-only, in-plane, already named at
`sketch/sketches/eva_magi_interior.cpp:212`. Not 3D; a projected bake,
and what shipped game UI actually does.

**What Compose must not gain**: depth, z in the model, imperative node
mutation, perspective in the kernel.

---

## 3. The split — a 3D library depends on Compose, never the reverse

Compose owns no surface, no loop, no thread, so guest-hood costs it
nothing. The seam is already built twice, in both directions, with
round-trip pixel tests: `WebView::frame()` is HTML→SkImage for a host;
`WebImage::paint()` lets a page display a canvas the host draws into.
3D is the third client — 3D-hosts-2D (panels in the world) and
2D-hosts-3D (a viewport leaf in a composition) are one mechanism with
opposite arrows.

**Surface-granular, not element-granular.** Do not crack one tree open to
place its elements individually. Plate granularity becomes a scene
authoring decision on the 3D side: N surfaces = N Composers = N textures,
each with its own geometry, dirty flag and bake density. Correct
per-plate depth then falls out of the depth buffer instead of out of a
sort, each plate keeps an independent layout, and Compose gains zero new
concepts.

**The ordering law falls out cleanly, and matches a law that already
exists**: within a surface, Compose's `(zIndex, declaration order)`
painter's rule; between surfaces, the 3D library's depth rule; a Compose
subtree cannot participate in 3D ordering. That is *"a component cannot
z-escape the site it was composed into"*, one level up.

**Cadence gets better, not worse.** The 3D host redraws every frame
because the camera moves; a panel's content is static for seconds.
`Composer::dirty()` already answers exactly that, and declared volatility
already computes it. In 3D the bake *is* the expensive part, so Compose's
caching model is worth more in the 3D client than in the 2D one.

**Naming**: extractable, so `Sigil`-prefixed, and it depends on Compose
while Compose depends on nothing new — which keeps `EXTRACT.md`'s story
intact. `SigilStage` reads better than `Sigil3D` and does not promise a
renderer that has not been chosen.

---

## 4. Translucency — pass the backdrop in

`Element::backdrop(Effect)` at `Compose.h:1074` already carries the wall
in its own doc comment:

> *Incompatible with `Cache::Texture` (the backdrop depends on the live
> destination)*

A 3D panel is **always** a texture, so that incompatibility is exactly
what a glass panel hits. Supplying the backdrop as an **input image**
instead of reading the live destination converts it into a compatibility.
The mechanism exists; it is missing a source that is not the canvas
underneath.

The 3D-side recipe is the standard grab-pass, terminating in Skia instead
of a shader:

1. render opaques, resolve scene colour
2. per glass panel, warp its screen footprint back into panel UV
3. hand that in as the backdrop image
4. Compose composites and emits an **opaque** plate
5. opaque plates sort by depth buffer — the sorting problem disappears

For a planar quad, step 2 is a homography: one perspective `SkMatrix`
image draw, which Skia does natively. That is *not* the affine limit
biting — the perspective is on the input image, not the element tree.
Curved geometry needs a render-to-UV pass on the 3D side instead.

**The decisive reason is not the sorting convenience — it is that the
blend vocabulary does not survive the trip.** Aero glass, y2k chrome,
`LayerStyles`, `Material::blend` flattening stacks into
`SkShaders::Blend`, the runtime blenders and the Photoshop deep cuts all
live in Skia blend modes. A 3D transparent pass hands back SrcOver and
Add. Composite inside Skia and the entire vocabulary survives.

### Costs, stated honestly

- **Glass panels are the expensive tier.** A live backdrop is content
  volatility by definition — the `Cache::Group` bake cannot hold, so a
  glass panel re-bakes every frame the scene behind it moves. Opaque
  panels stay nearly free. Make the backdrop input **declared**, not a
  side channel, so volatility stays decidable by the same law as
  everything else.
- **Residual sorting survives, much smaller**: only among glass panels,
  only where they overlap, only when one must see through another.
- **Resolution matching**, third client of the same `bakeScale`
  conversation. Frosted glass forgives a mismatch; clear glass does not.
- **Decide the grab point.** Mid-frame after the opaque pass is
  latency-free; last frame's colour buffer is simpler and costs a frame.

---

## 5. Ribbons — the concrete case, and it is 80% built

The motivating problem: a curvy or twisting spine, a *frame* built around
it rather than a stroke on it, carrying several frame patterns, portraits,
real layout, and marquees running the whole length — and a console log
filling the band.

**This already exists in 2D as `brushes::ArtBrush` / `artAlong`,
`Brushes.h:1155–1255.`** It takes an **`Element`** — a whole Compose
subtree — `measure()`s it, `snapshot()`s it to a 2x-oversampled texture,
walks the contour by arc length with `SkContourMeasureIter`, emits a
triangle strip at `pos ± normal·half`, sets texcoords sweeping `texW·f`
along and `0..texH` across, and issues one `drawVertices` per contour
(`Brushes.h:1246`). That *is* "unroll a Compose tree into a flat strip and
bend it onto a spine". Its doc comment makes the argument for it:

> curvature warps the art smoothly, where a stamp run breaks into rigid
> segments

So: a stroke genuinely cannot express this (§8b filed it — `Fill` is
node-local, not stroke-local), stamped patterns genuinely will not stay
consistent across a twisting band, and an unrolled strip texture is
genuinely the answer.

### Why tiles, and it is not mainly about size

- **Size**: Metal caps texture dimensions at 16384, so `arcLength ×
  density` has a hard ceiling that a long ribbon at legible density hits.
- **Invalidation — the real reason.** A console log changes at its
  *head*. One texture for the whole ribbon means every new line re-bakes
  the entire strip; tiles along `s` mean one tile re-bakes.

That split prices the content:

| Content | Route | Cost |
|---|---|---|
| Marquee (periodic) | animate the UV offset, bake once | free |
| Console log (non-periodic) | tile along `s`, re-bake the head tile | one tile/frame |

The UV-scroll route needs `SkTileMode::kRepeat` along `s`; today both axes
are `kClamp` (`Brushes.h:~1217`).

### The five gaps in `ArtBrush`, three already filed

1. **`texs.push_back({texW * f, …})` stretches one bake end-to-end.**
   Correct for an art cell, wrong for laid-out content, which wants 1:1 —
   texcoord = arc length × density, with a `drawVertices` per tile over
   arc ranges rather than one sweep.
2. **`cache->bakedFor = art.node().get()`** — raw pointer identity, so
   rebuilding the value re-bakes everything. That is **ROADMAP §16
   verbatim**, and it is *the* blocker for live content on a ribbon.
3. **`SkSurfaces::Raster`** — the bake is always CPU, so every re-bake is
   an upload. A per-frame head tile needs the Graphite recorder seam
   (`GpuImage.h`).
4. **`half` is a constant** — no `halfWidth(s)`, so no taper and no §8b
   cross-section.
5. **`SkVector normal{-tan.fY, tan.fX}`** — 2D perpendicular only.

### Two hazards, one of which will definitely bite

- **Frenet frames flip at inflection points.** The normal is undefined
  where curvature passes through zero, so a naive 3D frame spins and a
  console log goes upside down mid-ribbon. Use **rotation-minimizing
  frames** (double-reflection, Wang et al. 2008 — about fifteen lines of
  float3 math, and note again that Eigen contributes nothing), or a fixed
  up-vector when the ribbon must not roll. Layer an authored `twist(s)`
  on top.
- **The inner fold.** `pos ± normal·half` self-intersects wherever the
  radius of curvature is smaller than the half-width — present in the 2D
  code today, unchecked. A wide ribbon on a tight curve is degenerate,
  not merely ugly. Either enforce `half < minRadius` or let it fold
  deliberately (a real ribbon folds), but decide, and per §28 test the
  limit rather than documenting it.

Minor: `kTriangleStrip` texcoords interpolate affinely per triangle, so a
foreshortened quad warps. `stationPx` is the 2D mitigation; handing the
strip to a real 3D pipeline gets perspective-correct interpolation free.

### Keep the stamps

Illustrator ships Art Brush **and** Pattern Brush precisely for this, and
Pattern Brush has named tiles for start / end / side / inner-corner /
outer-corner. Some pieces must not bend — corner cartouches, rivets, end
caps, the portrait frames themselves. The factoring is **continuous
laid-out content on the strip texture, rigid ornament stamped over it**,
and "several frame patterns along one path" is Pattern Brush's tile
vocabulary. Worth stealing outright.

### Where it lives, and the payoff

"First a stroke determines the path, then the ribbon is built around it"
is literally the **derive** phase — resolved geometry → more content,
where `ContourWalk`, `connector` and `rail` already live. The ribbon is
`ContourWalk`'s continuous sibling: one unrolled surface instead of
discrete element stamps at stations. The recursion law already covers it,
because the strip's content is an element subtree like any other.

And inside the unrolled strip, **all of Compose works unchanged**. Yoga
lays out along `s` and across `t`. `console()` streams in it. Portraits
are image leaves. Frame ornament is decorations. Marquees are transforms.
It is a very long thin box. The complicated ribbon therefore needs zero
new Compose *features* — `artAlong` generalised on five axes, plus the
§0 primitive.

---

## 6. The markup question — right pain, wrong organ

**Where the instinct is right.** Phase 6 (`DESIGN.md`) has been open since
day one. TouchDesigner and the Python path are already sitting there
wanting to describe a tree. And `ROADMAP.md`'s own headline finding is
that **four features existed, were correct, and were worth nothing** —
discoverability at the call site is the measured #1 defect.

**Where it goes wrong.** QML's value is not its syntax; it is (a) a
dependency-tracked reactive binding engine, (b) a runtime type system with
tooling, and (c) hot reload. Compose already has a *stricter and faster*
(a) — `ch::Output` plus declared volatility, where QML's dynamic property
graph is a trade down. It already has (c) in ComposeSketch. What is
missing is (b), tooling, and that is not a language problem.

**The fatal objection.** Markup can only name pre-registered values. The
entire vocabulary here is C++ values and callables — `custom()`,
`outline()` lambdas, `Material::sksl`, the `LayoutScheme` concept, brushes
as `GeometryOp`s. A parser can select from a registry; it cannot express a
new one. `eva_magi_interior.cpp` is 1,945 lines with real reasoning in the
comments: the authoring surface *is* code.

`DESIGN.md:71` already states the correct architecture — a FlatBuffers
authoring schema is a **producer** of Elements, never the API. Keep that
line. Building markup as the API costs a parser, an expression language, a
binding evaluator, error reporting and a debugger; QML is fifteen years
and a Qt-sized team, and Slint and RmlUi exist because that is expensive.

**Therefore**: build the **tree inspector** first — a live view of the
retained tree with resolved rects, volatility and cache state — then the
FlatBuffers producer. That is the fix for the measured defect.

---

## 7. What Compose owes, in total

Three additions, none of which teach it anything about 3D, and two of
which the tiling work wants anyway:

1. **`snapshot()` of a live keyed subtree.** Today `snapshot()`
   (`Compose.h:1363`) is one-shot from an `Element` root and reconciles
   its own throwaway tree. There is no "bake this group of the *retained*
   tree to an image", which is what every client in §0 needs.
   `Cache::Group` already computes the bake internally; this exposes it.
2. **Host-supplied bake density**, multiplied against the authored
   `Element::bakeScale` (`Compose.h:1220`).
3. **A declared backdrop input image** — declared, not a side channel, so
   volatility stays decidable.

Already sufficient as-is: `bounds(key)`, `hitTest(SkPoint)`
(`Compose.h:1492` — canvas-space, so raycast → surface UV → composer px
composes cleanly), `dirty()`, `purgeCaches()`.

---

## 8. Order of work

Everything here is 2D work that the 3D client wants anyway; none of it
blocks on the graphics-API decision.

1. **Positioned leaf set** (§2) — N children with caller-supplied rects
   and no flex participation. Penrose pays 1,647 Yoga nodes for a scene
   with zero layout in it. This is the change that actually resolves
   "2D grid concerns mixed with everything else", and it is cheap.
2. **Comparable `Outline` values** (§3) — 43.4 of 43.5 ms on Chevreul was
   one un-prunable outline callable. Highest measured impact in the
   roadmap.
3. **Ribbon `(along, across)` as a paint space** (§8b) — expose what
   `artAlong` already computes. Closes §8b and §14's "true art brush"
   together.
4. **`ArtBrush` bake identity** (§16) — the blocker for live ribbon
   content.
5. **The §0 primitive**: subtree snapshot at host density, per-tile
   invalidation. Serves tiling, infinite canvas, ribbons and 3D panels at
   once.
6. **Tree inspector**, then the FlatBuffers producer. Not a language.
7. **Then** price Graphite-on-Dawn against raw Metal, and let texture
   arrays and the 3D pipeline fall out of that decision rather than drive
   it.

---

## Claims table

Per §28 — a documented limit is a claim until tested. Half of this
document is source-verified and half is argument; the difference is
recorded rather than smoothed over.

| Claim | Status |
|---|---|
| `hasPerspective()` guards at `Paint.cpp:1546/2009/2115` | **verified** (read) |
| `SkCanvas::concat(SkM44)` at `SkCanvas.h:988`; `utils/SkCamera.h` present | **verified** (read) |
| `SkM44`/`SkMesh`/`SkVertices`/`SkPoint3`/`SkRSXform` in installed Skia | **verified** (listed) |
| `backdrop()` incompatible with `Cache::Texture`, `Compose.h:1074` | **verified** (read) |
| `snapshot()` is one-shot from an `Element` root, `Compose.h:1363` | **verified** (read) |
| `Element::bakeScale` is authored intent, `Compose.h:1220` | **verified** (read) |
| `ArtBrush` behaviour, all five gaps, `Brushes.h:1155–1255` | **verified** (read in full) |
| No `diligent` port in vcpkg; `clipper2`/`earcut-hpp`/`cdt`/`libtess2`/`manifold`/`glm`/`eigen3` present | **verified** (listed) |
| A live Composer under a projected `M44` holds 60 fps for panels | **UNMEASURED** — the §2 experiment |
| `Cache::Group` bake cost for glass panels re-baking per frame | **UNMEASURED** — and `Cache::Group` itself is still untimed (`HANDOFF.md`) |
| Metal's 16384 max texture dimension | asserted, not probed on this machine |
| Frenet-frame flipping at inflections; RMF as the fix | standard result, not tested here |
| The inner fold self-intersects below `half < minRadius` | read from the code path; no failing case built |
| Diligent's Metal backend licensing | **unchecked** — verify before any further consideration |
| Text legibility under oblique sampling as the practical wall | argument, no capture built |

The rule this document is subject to, from `ROADMAP.md`'s own preamble:
**reproduce the wall before building the fix.** Nine entries across two
runs described a wall that was not there. Nothing above is exempt.
