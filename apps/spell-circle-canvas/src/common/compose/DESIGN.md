# SigilCompose — design canon

Data-driven, cacheable, animated drawable components for the Skia
canvases — layered typographic posters, live data panels, game-UI-grade
chrome — drawable into any `SkCanvas` next to everything else we render.
Implemented through the completeness round, the review arcs, and the
study program; the remaining open phase is authoring (FlatBuffers
producer + tree inspector, §Direction).

**This file is the one place design decisions live.** Every other doc
has a narrower contract:

| Doc | Contract |
| --- | --- |
| `API.md` | The surface manual — what to call and its traps. **Where it disagrees with the headers, the headers win.** |
| `ROADMAP.md` | The ledger of walls: open asks and closed findings from building real scenes. Entry numbers are stable, closed entries stay marked CLOSED in place; its preamble is a binding contract. |
| `STRESS_TESTS.md` | Dated measured numbers. Quote figures from here (or from a rerun), never from memory — several here were later re-measured. |
| `REFERENCES.md` | The reference-grammar data library every study cites. |
| `sketch/README.md` | The sketch-host manual: run, `--bench`, capture discipline. |
| `sketch/sketches/README.md` | The study index. |
| `archive/` | Completed campaigns (`REVIEW.md`, `EXTRACT.md`, `HANDOFF.md`, `SPATIAL.md`). Provenance, not spec — their API sections describe pre-landing designs. Citations elsewhere like "REVIEW.md §6.2" resolve there. |

A rule found anywhere else that is not in this file or the file its
contract assigns it to is either historical or a defect in this file.

## Problem

We lay out paragraphs beautifully (SigilWeave) and documents completely
(SigilScry/Ultralight). Missing is the middle: *box-level* composition
of SigilWeave-quality typography and arbitrary Skia drawing — sized by
flexbox rules with baseline alignment, layered with explicit z-order
and blending, cached like display lists, animated at scene rate,
refreshed from data without rebuilding the world. SigilCompose exists
for when the typography and the drawing are the product.

## Foundations — borrow the owners, build the glue

Every adoptable framework owns its text and render pipeline — adopting
one means fighting it exactly where SigilWeave and Graphite should win.
Each hard subproblem has a proven owner already in our stack:

- **Layout: Yoga.** Flexbox with measure/baseline callbacks made for
  text leaves; microsecond incremental relayout. (Spike lesson: flexbox
  `stretch` would override measured text height — text leaves default
  start-aligned.)
- **Animation: Choreograph + SigilMotion.** Phrases/sequences/motions
  over a pausable, time-scalable FrameClock and Ticker that reports
  "needs more frames" for event-driven hosts. (`sigil::weave::
  Choreograph` is the unrelated glyph-placement utility; namespaces
  disambiguate.)
- **Caching: SkPicture** display lists, textures under heavy effects.
- **Text and paint: ours** (SigilWeave, SigilImage, SigilScry frames,
  raw Skia).

What we build is deliberately thin: element values, a keyed reconciler,
memo, cache boundaries, a stacking painter. Glue, not a framework.

## The model — three roles

1. **Elements** — cheap immutable value descriptions built by a C++
   fluent embedded DSL (no markup, no parser; a FlatBuffers authoring
   schema is a *producer* of these values, never the API).
2. **Components** — free functions `Props → Element` over plain data.
   No base classes or lifecycles; state lives in the caller's model.
   `memo(props, fn)` skips the describe call when props compare equal.
3. **Composer** — the retained side: reconciles element trees by key
   into instances holding Yoga nodes, resolved styles, cache pictures,
   and live Choreograph bindings. A guest in the host's canvas:
   `render()` on data change, `draw(canvas)` inside whatever paint
   callback exists, honoring the current matrix/clip; owns no surface,
   loop, or thread.

## Element memory — a hot base, boxed rarities

Descriptions are built, copied, and compared every render, so their
layout is a design rule. `ElementNode` keeps inline only fields every
kind touches; everything rare or kind-specific lives in out-of-line
value-semantic `Box<T>` blocks — absent costs one null pointer.
`Animatable` applies the same rule per property: a compact class, not a
variant; the fat `Transitioned` payload boxes out-of-line. A
`static_assert(sizeof(ElementNode) <= 768)` in `Composer.cpp` enforces
it structurally: **new rare or kind-specific state goes in a block,
never the base.** (2752 → 688 bytes at the §15 split; 744 today with
the stroke block, guard ≤ 768; STRESS_TESTS.)

## The pipeline — five phases, procedural entry at each

Declarative and procedural are not two APIs; they are early-phase and
late-phase entries into one dataflow:

| Phase | Input → Output | Procedural entry |
| --- | --- | --- |
| **Describe** | data → elements | components, `memo`, ranges |
| **Layout** | constraints → rects | `LayoutScheme`; SigilWeave measure |
| **Derive** | resolved geometry → more content | `flowAround`, `connector`/`rail`, `ContourWalk` stamps |
| **Paint** | geometry + canvas → pixels | `DecorationScheme`, `Effect`s, `custom()`, SkSL |
| **Frame** | time → values / next data | Choreograph outputs, steppables, host feedback |

One law bounds recursion: **within a frame, information flows forward
only.** Backward influence is either the declared, cycle-checked
exception (`flowAround`'s single second layout pass) or crosses frames
as ordinary data. Recursion is closed under the model: stamps are
element subtrees whose decorations may walk their own contours, whose
custom leaves may draw nested Composers — every level reconciled,
cached, and animated by the same rules.

**Derive is flat.** Each render rebuilds — beside the key index — the
edge store: routed nodes and flowing text as flat lists in tree order,
plus a back-index from anchor key to routes. Everything that asks
"where did that keyed node land" rides the SAME list — `flowAround`,
`connector`/`rail`, a band's borrowed spine (`around(key)`), a stroke
pass's `spans::fit(key)` — rather than growing a phase. No tree
recursion; a tree with no derived content pays nothing; `routesAt(key)`
answers in O(routes at that node).

## Stacking and compositing

CSS's model, simplified and explicit:

- Within a parent, children paint in `(zIndex, declaration order)`,
  stable-sorted; `stack()` is the overlap container.
- A stacking context forms on zIndex, opacity < 1, blend, transform,
  clip, or layer effect; children cannot interleave outside it — **a
  component cannot z-escape the site it was composed into.**
- `.blend(mode)` composites the node as a layer against everything
  painted before it in its stacking context.
- **Transforms and opacity are paint-only** — never relayout; layout
  props re-measure and are priced visibly in the API.
- `.effect()` filters the node's own rendered layer; `.backdrop()`
  filters what lies beneath (incompatible with `Cache::Texture` — a
  bake has no live destination to read; see §Direction for the
  declared-input fix).

## Animation — one engine, two write paths, named grammars

The substrate is single: choreograph Outputs stepped by one Ticker,
unified at every property by `Animatable`. Exactly two ways to change
the screen:

1. **Describe** — structure and discrete state: `render()` /
   `renderSlot()`. Keyed reconciliation *is* the child-swap API. No
   imperative node mutation — the door that would make every cache
   unsound.
2. **Bind** — continuous values: `ch::Output*` declared in the
   description, mutated per frame, no render call. Bound properties
   are paint-only by contract; declared volatility is what lets
   caching stay sound.

Everything the API offers is one of these, wearing a grammar. The map
(each documented in API.md):

| Grammar | Path | Owns |
| --- | --- | --- |
| `transition`/`animate`/`staggerChildren` | describe | reconciled state changes, entrances, staggers |
| bare `Output*` / shaped `bind()` | bind | continuous scrubbing, data-driven values |
| `isAnimated()` schemes, live materials (`uTime`, bound uniforms, `quantizeTime`) | bind (content volatility) | self-animating surfaces |
| `spans::upTo` on a stroke pass, `wipe` (`trim` condemned) | either | reveals |
| `glyphFx` + Animatable progress | either | per-glyph typography |
| `custom()` + `Cache::None` + `elapsedSeconds` | floor | immediate-mode escape hatch |
| pool `Mode::Live` / `Mode::Data` | the two paths verbatim | instanced masses |

Transition lifecycle: a standing declaration, one motion per (instance,
property), retarget-from-current, reset-is-description; mount applies
directly (entrances are explicit); unmount cancels; keys carry
mid-flight state. **There is deliberately no timeline object**:
multi-beat choreography is windowed bindings over one phase Output
(`bind().window()`), and the refusal (a Timeline/cue DSL saves zero
lines) is recorded in `Studio.h`.

**VariationDrive probe rule**: driving a variable-font axis per frame
is bind-path with one gate — the axis must be advance-invariant,
*proved per font* by sampling every glyph advance at the axis extremes.
wght is refused with a warning (text draws unvaried); GRAD is the
advance-invariant weight.

## Caching — automatic because provable

Declared volatility (bindings, transitions, `isAnimated()` schemes, live
leaves) makes "static" a decidable property of a subtree, not a
heuristic. The tiers:

- The reconciler **prunes** structurally-equal descriptions with or
  without `memo` — memo only skips the describe call.
- Provably-static subtrees are **picture-cached** automatically. **A
  picture is not a pixel cache**: replay re-runs every shader per
  pixel forever; only a bake keeps pixels.
- Volatility partitions the tree AND partitions **by kind**: content
  volatility paints live; paint-only volatility (bound/transitioning
  transforms, opacity) **replays the cached content under the live
  transform**.
- **Automatic texture promotion** bakes nodes measured expensive over
  consecutive frames (three eligible kinds, refusal reporting —
  API.md). **On by default only on CPU raster; OFF by default on
  Graphite/GPU** — the per-node profiler measures op *recording*, not
  GPU execution, and promotion measured inert there (ROADMAP §29).
- `.cache()` is an override: `None` opts out, `Texture` rasterizes,
  `Group` bakes a container + volatile children into one device layer
  held by an exact-compare subtree value memo, dropped the frame a
  binding ticks (ROADMAP §30 is the spec; **no performance number
  exists for Group yet — the 23× is a hypothesis from a different
  experiment; do not quote it**).
- Related mechanisms share a slogan, not a mechanism — split bake
  (§15), `scalarMemo` (§17), `leafDirectBlend` (§18, its
  `Cache::Texture` exclusion is load-bearing), `Cache::Group` (§30).
  **Read the source before merging any two.**
- **A bake isolates**: pixel standard is "byte-identical to
  compositing through a layer" — and pixel tests over opaque black are
  blind to isolation errors; test over a lit ground.
- Bakes take device space (holding still — two independent measures)
  or quantized local space (moving); never device-space inside a
  picture recording. `bakeScale` is almost always the wrong lever
  (cheapens the bake, taxes every blit).
- Invalidation is the reconciler's job alone; the one host hook is
  `purgeCaches()` (device loss / backend switch) — tree, layout, and
  animations survive.

**Paint is the frame.** On every measured scene the retained machinery
rounds to 0.00–0.01 ms; pixels are milliseconds. A slow scene is a
paint problem — fixed with caching tiers, materials, or the GPU, never
tree surgery.

## GPU-first

The floor is 60 fps and Graphite is the primary target; CPU raster is
the fallback and the deterministic test target, not the performance
story. Full-screen live SkSL is GPU-tier content by definition.
Portability is a layer, not a call-site concern (`GpuImage.h`): this
Skia's Graphite device stubs `drawImageLattice`/`drawAtlas` empty, so
the library **never records the native ops** — `gpuimg::` decomposes
on every backend (a picture recorded on raster must replay on
Graphite). The same layer owns texture promotion-to-GPU (Graphite does
no implicit raster uploads; a caching ImageProvider rides every
recorder) and format hygiene (OCIO LUTs bake to F16 — F32 is not
linearly filterable on Apple GPUs).

## Queries

Elements are write-only descriptions; reads target the **Composer**,
post-layout, read-only: `bounds`, `paragraphLayout`, `hitTest`,
`routesAt`. Querying descriptions is rejected — it would invent a
second identity system (the React ref lesson).

## Kernel, util, extensions — the weight budget

Three tiers, enforced structurally: the kernel depends on no extension;
a user who reads only the kernel section of API.md has a complete,
sound mental model; extensions plug kernel seams (decorations, fills,
layout schemes, routers, leaves) without changing kernel semantics.

The **kernel** is `Element`/components/`Composer`; Yoga flex +
`stack()`; stacking paint (zIndex/opacity/blend/transform/clip); the
text/image/custom leaves; `key` + `memo`; `Animatable`/`Transition` and
the reconciled-vs-bound write paths; automatic caching; the stroke
grammar (`shape`, the `stroke(where, what)` slot over `spans::`,
`band`/`across`, and the `Profile` seam) — plus the
element-surface conveniences that landed on `Element` itself (`trim`,
`wipe`, `echo`, `style`, `textFill`/`textStroke`, `glyphFx`,
`variationDrive`, `staggerChildren`, `hitTestable`, `sampling`).
`Material` is kernel-adjacent by signature (`fill(Material)`) though it
ships as a header — the polymorphic paint value compiling to ONE
shader (`SkShaders::Blend` flattening, never stacked saveLayers).
**Util** holds deliberately-demoted sugar a user could write (gradient
constructors, stroke/shadow helpers, the `Stage` host bundle — which
is nonetheless the canonical three-line host loop; start there).
**Studio** is the file prelude (hex/type/face/ramp/fmt) — spellings,
never decisions. Everything else is an **extension**:
`Shapes`/`Layouts`/`Routers`/`Web`, the Brush engine
(`Brushes`/`Lines`/`LayerStyles`/`Patterns`), `Sdf`, `Kinetic`,
`Console`, `Instances`, `GpuImage`. The **kit** is a tier of its own and
now a separate CMake library (`SigilComposeKit` — Frame, Divisions,
Legibility, PixelType, Strokes) whose only include path is compose's
PUBLIC headers, so the tier boundary is structural rather than
conventional (`kit/BoundaryProbe.cpp` is its negative control). PRESETS
are not kit: craft-named compositions belong in external loadable kits.

Decorations stay **primitives, not a zoo** at the seam — Fill,
PathFormat, Slice, ContourWalk over one `PaintContext` whose `outline`
is the load-bearing idea (routes and shapes share one dressing
vocabulary). The Brush/Lines/LayerStyles shelf is the *vocabulary
built over those primitives* — first-class values by later decision
(REFERENCES-grounded lines-as-fills), not a reversal of the seam. Over
them the brush vocabulary is closed and small: four KINDS
(`brush::solid`/`Pattern`/`Scatter`/`Art`) and two COMPOSITES
(`brush::layers`, `brush::weave` — formally ONE machine, two author
intents), with `.shaped()` the one geometry-deviation seam, `Profile` the
one width seam, and `CrossingRule` the one over/under seam.

**Instancing**: masses are a leaf, not a tree. Atlas of baked cells +
user-owned SoA Pool + one stamp per frame; EnTT stays on the user's
side of the seam. Its two modes are the kernel's two write paths, not
a third.

## Boundaries

- **Compose stays 2D and affine.** No depth, no z in the model, no
  perspective in the kernel; the `!hasPerspective()` guards in
  `Paint.cpp` are the (latent) boundary and want a test. The one
  concession on the table: perspective on a `Texture`-cached subtree —
  a projected bake, what shipped game UI does. (archive/SPATIAL.md.)
- **Guest-hood is absolute**: no surface, loop, or thread. A future 3D
  library (`SigilStage`) depends on Compose, never the reverse —
  surface-granular (N surfaces = N Composers = N textures), within a
  surface the painter's rule, between surfaces the depth buffer: the
  z-escape law one level up. Glass panels pass the backdrop IN as a
  declared input image (volatility stays decidable; the Skia blend
  vocabulary does not survive a 3D transparent pass).
- **Never a markup language.** Markup can only name pre-registered
  values; the vocabulary here is C++ values and callables. The
  authoring path is: tree inspector first (the measured #1 defect is
  call-site discoverability), then the FlatBuffers producer.
- **Dependency refusals**: no Eigen (the missing math is computational
  geometry — clipper2/earcut when needed — not linear algebra; every
  seam is a Skia seam); no Diligent (a fourth device owner). The 3D
  device question (raw Metal behind a seam vs Graphite-on-Dawn) is
  priced when 3D is real, not before.

## Growth rules

- **The extraction test**: a helper earns a home when it makes a CHORE
  cheap, never a CHOICE cheap (`Layouts.h` names decisions — zero
  study adoptions; `Util.h` names spellings — 166 uses of one helper;
  `stickerScatter` refused in writing, then deleted). The measured
  do-not-build list — frame/coordinate value, lattice resolver, ring
  band, ringLabel, leaderTo, tick ladder, legend row, icon set,
  palette/theme layer, timeline DSL, label() widget — lives with its
  evidence in archive/EXTRACT.md; inline `shape()` lambdas are the
  escape hatch *working*.
- **Anything read live must participate in reconciler equality** or a
  pruned node reads stale values forever; incomparable callables
  compare conservatively unequal and therefore never prune — prefer
  comparable value forms (shaper structs over raw `ops::PathOp`
  lambdas), and memoize where a callable is unavoidable.
- **A default that encodes a judgement about the caller's art cannot
  be changed compatibly** — the test is whether any existing caller's
  *output* changes (the `cornerAlign` doctrine, ROADMAP §27; audit
  recipe: flip it in a scratch copy and diff). Its conclusion, landed
  2026-07-26: such a default should not EXIST. Corner art and its
  alignment are one value (`brush::CornerArt{art, align}`) with a
  required constructor argument, so the un-thought-about state cannot
  be described. A diagnostic is what you ship when the type system
  could have refused.
- **The grammar names the author's intent, never the mechanism**
  (priority set 2026-07-25; ruling refined in ROADMAP §33). An author
  animating thinks one principle — a value over time on a property —
  and the first word at the call site names the OWNER of the motion:
  `animate()` is composer-manufactured — `animate(to(v), spec)` ramps
  on change, `animate(from(a).to(b), spec)` is a mount entrance; a
  DRIVEN property keeps the data spelling (`&out`, shaped `bind()`
  — the bare overloads are retained BY DESIGN: driven is data
  updating, animation a side effect); a surface that RUNS ITSELF
  declares `isAnimated()` — the `is` prefix is what makes it read as a
  QUERY, and there is no setter to confuse it with. Mechanism names do
  not stay internal either: `PropValue`, `with`/`withFrom`/
  `withKeyframes`, `outline()`, `Brush::op()`/`leg()`, `Pool::touch()`,
  `namespace brushes` and the `ops::` structs were all DELETED in R3
  (§33), because an alias kept forever is a second grammar. The grep
  test is two honest searches: `animate(` finds every authored motion;
  `bind(` and bound fields find everything data-driven. The wall and
  ruling are ROADMAP §32; the full-surface audit is §33. Its cheapest
  test, applied to every new name: **when a doc comment's job is to
  distinguish two names, that is the rename ticket.**
- **PERPENDICULAR SIGN — ONE CONVENTION, STATED ONCE HERE.** Positive
  `across` is to the **LEFT of travel**, which in screen space (y down)
  is OUTSIDE a clockwise path — SkPath's own direction for rects and
  circles, so `.outward()` exits the shape. Everything obeys it:
  `bandPointAt`, `Profile::across`, `strand::offset`, `TextPath::offset`,
  `lines::offsetAcross`, `lines::Rail::across`, `lines::Line::across` and
  `kit::brush::shapers::Offset`. The `lines::` family used to be the
  minority that meant right-of-travel (Mapbox's line-offset sign); R3
  flipped it, renaming each member so the compiler found every call site
  and negating every argument so no picture moved (§33 ruling 5). There
  is no second convention to look up.
- **Qt identifier ban** in every exported header (`emit`, `signals`,
  `slots`, `foreach`, `forever`, `Q_*`); the sketch rsp is Qt-free and
  blind — after editing headers, syntax-check a Qt TU.
- New scenes and studies are **reference-grounded**: every example
  reconstructs a named artefact citing a REFERENCES.md section;
  "generated, not drawn"; algorithms grounded in shipped
  implementations, not screenshots.

## Doctrine — measurement and process

Earned across the programs; the full case files are ROADMAP §§26–31.

1. **A documented limit is a CLAIM** — test it or label it. Read the
   source before building on any entry; reproduce the wall before
   building the fix (nine never-real walls across two runs). Citation
   count validates the symptom, never the cause.
2. **When a report names a cause, verify the cause is involved before
   fixing** — one A/B or pixel sample first (aero's innocent backdrop,
   black_watch's wrong beat).
3. **Controls must be things you BUILT.** A passing suite is a claim
   about a binary, not a source tree; before believing a negative
   result, check the thing you ran contains the change. Baselines are
   taken before/after back-to-back on a quiet machine, same commit and
   thermal state; a contended reading is quarantined, not averaged.
4. **A still is a claim about an animation.** Captures are
   deterministic (`--frame`, `ctx.captureAt`); a scene with beats
   declares its moment (52 of 56 plates photographed mid-motion when
   left to the default; the per-scene judgement is ROADMAP §31's open
   audit). Self-measured values route through `ctx.measured()`; the
   negative control is re-rendering the same binary.
5. **Zero steady-state cache writes predicts THRASH, not BANDWIDTH** —
   a sticking cache is not a free cache (fallout2: stuck perfectly,
   still regressed).
6. **The per-node profiler is blind on GPU** (it measures recording);
   trust scene-level GPU work-ms from synced sweeps; GPU visual QA
   must read GPU pixels — the raster sweep cannot see the
   silently-dropped-op class.
7. **The sibling-path failure family**: a fix must land on both
   siblings (corner scanners, doc tests covering one section, two-name
   identities contaminating their own guard — §§22/25/27). When two
   code paths express one rule, unify or test both.

## Direction

The near-term order of work (argued in archive/SPATIAL.md — its claims
table marks which half is source-verified vs unmeasured):

1. Positioned leaf set (N caller-supplied rects, no flex; Penrose pays
   1,647 Yoga nodes for zero layout).
2. Comparable `Outline` values (ROADMAP §3 — the highest measured
   impact: 43.4 of 43.5 ms on one un-prunable callable).
3. Ribbon `(along, across)` paint space — expose what `artAlong`
   computes (§8b/§14).
4. `brush::Art` bake identity (§16) — the blocker for live ribbon
   content.
5. **The one missing primitive, four clients** (3D panels, infinite
   canvas, high-res export, ribbons): snapshot a live keyed subtree at
   host-chosen density, invalidated per tile, keyed on scale (the y2k
   1x-bake-2x-replay lesson). `Cache::Group` computes the bake;
   `bakeScale` is the authored half of the density product.
6. Tree inspector, then the FlatBuffers producer.
7. Then price Graphite-on-Dawn vs raw Metal and let the 3D pipeline
   fall out of that decision.

Phase history: phases 1–5 (kernel through gallery) landed, as did the
review arcs (split, Material/Brush, geometry/routes,
instancing/console/kinetic, reference-grounding) — the record is
archive/REVIEW.md §12 and STRESS_TESTS' dated sections. Phase 6
(authoring) is open, shaped by the markup refusal above.

## Naming

Library **SigilCompose**, namespace `sigil::compose`, directory
`src/common/compose/`. Extractable, so Sigil-prefixed; "Poster"
survives as a use case, not a name.

## Risks

- Yoga re-probes measure functions with loose modes — text measure
  cache keyed on (content revision, width, mode); the known sharp edge.
- Animating text-affecting layout props re-measures per frame — priced
  visibly; prefer transforms for motion.
- `saveLayer` cost for opacity/blend/effect groups on Graphite —
  stacking contexts keep it opt-in; `Material::blend` flattens layer
  stacks into one shader precisely to avoid it.
- On raster targets picture replay re-rasterizes (~4 µs/text row);
  `Cache::Texture` is the raster pixel win; Graphite replays cheaply.
- `flowAround`'s second layout pass stays bounded (single re-pass,
  cycles rejected at reconcile).
