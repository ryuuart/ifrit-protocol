# IfritCompose — design

Data-driven, cacheable, animated drawable components for the Skia
canvases — layered typographic posters, live data panels, game-UI-grade
chrome — drawable into any `SkCanvas` next to everything else we render.
Status: **design complete, reviewed through five stress rounds; ready
for phase 1**. The concrete surface lives in `API.md`; the
implementation-validation catalog in `STRESS_TESTS.md`.

## Problem

We lay out paragraphs beautifully (TextFlow) and documents completely
(IfritWeb/Ultralight). Missing is the middle: *box-level* composition of
TextFlow-quality typography and arbitrary Skia drawing — sized by
flexbox rules with baseline alignment, layered with explicit z-order and
blending, cached like display lists, animated at scene rate, refreshed
from data without rebuilding the world. IfritWeb covers genuinely web
content but through WebCore's text stack, capped at 60 FPS, a thread hop
away. IfritCompose exists for when the typography and the drawing are
the product.

## Foundations — borrow the owners, build the glue

Survey conclusion (React, Flutter, SwiftUI/Compose, Dear ImGui,
RmlUi/Slint/QML/LVGL, Rive, Skia's own machinery): every adoptable
framework owns its text and render pipeline — adopting one means
fighting it exactly where TextFlow and Graphite should win. But each
hard subproblem has a proven owner already in our stack:

- **Layout: Yoga** (vcpkg 3.2.1). Flexbox with measure callbacks made
  for text leaves; baseline callbacks for typographic alignment;
  microsecond incremental relayout. Validated by `compose_spike_test`:
  measure funcs drive `textflow::layoutParagraph`, width constraints
  reach the line breaker, `align-items: baseline` works via
  `LineMetrics::baseline`. (API lesson from the spike: flexbox's
  `stretch` default would override measured text height — text leaves
  default start-aligned.)
- **Animation: Choreograph + IfritTick.** sansumbrella's Choreograph
  (sigil-vcpkg-registry port) supplies phrases/sequences/motions;
  IfritTick (`src/common/tick`) is the backing ticking engine —
  pausable, time-scalable FrameClock and a Ticker that steps the master
  timeline plus steppables and reports "needs more frames" for
  event-driven hosts. (Nameclash note: `textflow::Choreograph` is the
  in-repo glyph-placement utility; `choreograph::` is the tween library
  — namespaces disambiguate, and they compose: glyph effects register
  as Ticker steppables.)
- **Caching: SkPicture** display lists (record/replay), textures for
  effect-heavy layers.
- **Text and paint: ours** (TextFlow, IfritImage, IfritWeb frames,
  PaintShaders, raw Skia).

What's left to build is deliberately thin: element values, a keyed
reconciler, memo, cache boundaries, a stacking painter. Glue, not a
framework.

## The model — three roles

1. **Elements** — cheap immutable value descriptions built by a C++
   fluent embedded DSL (no markup, no parser; a future FlatBuffers
   authoring schema is a *producer* of these values, never the API).
2. **Components** — free functions `Props → Element` over plain data.
   No base classes or lifecycles; state lives in the caller's model.
   `memo(props, fn)` defers and skips description when props compare
   equal.
3. **Composer** — the retained side: reconciles element trees by key
   into instances holding Yoga nodes, resolved styles, cache pictures,
   and live Choreograph bindings. Owns layout, stacking paint, cache
   invalidation. A guest in the host's canvas: `render()` on data
   change, `draw(canvas)` inside whatever paint callback exists,
   honoring the current matrix/clip; owns no surface, loop, or thread.

## The pipeline — five phases, procedural entry at each

The architecture's spine. Declarative and procedural are not two APIs;
they are early-phase and late-phase entries into one dataflow:

| Phase | Input → Output | Procedural entry |
| --- | --- | --- |
| **Describe** | data → elements | components, `memo`, ranges |
| **Layout** | constraints → rects | `LayoutScheme` concept; TextFlow measure |
| **Derive** | resolved geometry → more content | `flowAround`, `connector`, `ContourWalk` stamps |
| **Paint** | geometry + canvas → pixels | `DecorationScheme`, layer `Effect`s, `custom()`, SkSL |
| **Frame** | time → values / next data | Choreograph outputs, steppables, host feedback |

One law bounds arbitrary recursion: **within a frame, information flows
forward only.** Backward influence — paint affecting layout — is either
the declared, cycle-checked exception (`flowAround`'s second layout
pass, the DTP float-wrap approach) or crosses frames through ordinary
data (this frame's `bounds()` feeding next frame's describe), which the
event-driven loop already models. Recursion is closed under the model:
contour-walk stamps are element subtrees whose decorations may walk
their own contours, whose custom leaves may draw nested Composers —
every level reconciled, cached, and animated by the same rules.

## Stacking and compositing

CSS's model, simplified and explicit:

- Within a parent, children paint in `(zIndex, declaration order)`,
  stable-sorted; `stack()` is the overlap container (children share the
  box, position via alignment/insets).
- A node establishes a stacking context when it sets zIndex, opacity
  < 1, a blend mode, a transform, a clip, or a layer effect; children
  cannot interleave outside it. Consequence for composition: a
  component **cannot z-escape the site it was composed into** — layered
  order is always decidable by reading the composition site.
- `.blend(mode)` composites the node as a layer against everything
  painted before it in its stacking context — additive stacks
  (`kPlus`/`kScreen`) are how overlapping translucent layers brighten.
- **Transforms and opacity are paint-only** — never relayout (the
  web's compositor-property lesson; the cheap-to-animate set).
- Layer **effects** post-process stacking contexts: `.effect(...)`
  filters the node's own rendered layer, `.backdrop(...)` filters what
  lies beneath (CSS backdrop-filter) — both `SkImageFilter`-based,
  including SkSL runtime image filters.

## Animation — two write paths, two price tags

Exactly two ways to change what's on screen:

1. **Describe** — structure and discrete state: call `render()` /
   `renderSlot()`. Keyed reconciliation *is* the child-swap API
   (reorder moves instances with transitions and glyph state intact).
   No imperative node mutation — the door that would silently make
   every cache unsound.
2. **Bind** — continuous values: `ch::Output<float>*` declared in the
   description and mutated per frame with no render call. Bound
   properties are paint-only by contract; declared volatility is what
   lets caching stay sound (bound/animated nodes demote from picture
   caches while active).

Implicit transitions (the SwiftUI lesson) ride path 1:
`.transition({.duration, .ease})` on a node turns reconciled changes to
its animatable properties into Choreograph ramps instead of snaps, with
per-property scoping via the setters themselves — no property enum
(properties are named exactly once, by their setters). Property price
tags: paint props (opacity/transform/color) animate freely; layout
props re-measure — visible in the API, chosen deliberately.

## Caching

Cache as much as possible — automatically, because it is provable:
declared volatility (bindings, `with()` transitions, `animated()`
schemes, live leaves) means "static" is a decidable property of a
subtree, not a heuristic. The reconciler prunes structurally-equal
descriptions wholesale (with or without `memo` — `memo` only saves
calling the describe function); provably-static subtrees are
picture-cached automatically (record once, replay; Flutter's
RepaintBoundary meets Skia display lists, minus the manual boundary);
`.cache()` is an override — `None` to opt out, `Texture` to rasterize
under heavy effects. Volatility partitions the tree: an animating node
demotes itself, never its static siblings. Invalidation is the
reconciler's job alone.

## Independent data domains

Three tiers: `memo` per source (unchanged branches cost a hash check);
named **slots** (`slot("ticker")` in the tree,
`composer.renderSlot("ticker", …)` updating only that mount point while
its surroundings' caches stay valid); multiple Composers for fully
separate systems, ordered by draw-call order in the shared canvas.

## Queries

Elements are write-only descriptions; reads target the **Composer**,
post-layout, read-only: `bounds(key)`, `paragraphLayout(key)` (live
TextFlow layout for glyph choreography and decoration), `hitTest(pt)`.
Querying descriptions is rejected — it would invent a second identity
system (the React ref lesson).

## Kernel and extensions

The review rounds grew the surface; this boundary keeps the original
goal — *draw and control, quickly and robustly, without framework
weight* — honest. Three tiers. The **kernel** is what phase 1 builds
and what every user must understand: `Element`/component functions/
`Composer`, Yoga flex + `stack()`, stacking paint with
zIndex/opacity/blend/transform, `Fill::color/shader`, the
text/image/custom leaves, `key` + `memo`, `PropValue`/`Transition`,
`ch::Output` bindings, and automatic caching. **Util**
(`<ifritcompose/util.h>`) holds deliberately-demoted sugar a user
could write themselves — gradient constructors, stroke/shadow
helpers, the `Stage` host bundle — depending only on the kernel and
optional by definition. Everything else is an **extension that plugs
into a kernel seam** and ships as its own header, with the kernel
depending on none of it: `LayoutScheme`, `PathFormat`/`Slice`/
`ContourWalk`, `Effect`/backdrop, the derive phase (`flowAround`,
`connector`), slots, `Cache::Texture`. A user who reads only the
kernel section of API.md has a complete, sound mental model;
extensions add power without ever changing kernel semantics. That is
the design's weight budget, enforced structurally.

## Naming

Library **IfritCompose**, namespace `ifrit::compose`, directory
`src/common/compose/`. "Poster" survives as a use case, not a name.

## Phasing

1. Element values + keyed reconciler + Yoga integration + stacking
   painter + the four decoration primitives that are pure Skia wrappers
   (golden-image tests; `STRESS_TESTS.md` items marked P1).
2. Cache boundaries (`Picture`, then `Texture` under effects) + memo +
   slots.
3. Choreograph bindings, implicit transitions, layer effects.
4. Derive phase: `ContourWalk` stamps, `flowAround`, `connector`;
   remaining leaves (image/web/custom already sketched).
5. ComposeGallery (Qt, TextFlowGallery-style) hosting the stress-test
   catalog interactively; compose_bench for the per-frame costs.
6. Authoring: FlatBuffers schema, Python/TouchDesigner path.

## Risks

- Yoga re-probes measure functions with loose modes — text measure
  cache keyed on (content revision, width, mode); the known sharp edge.
- Animating text-affecting layout props re-measures per frame — priced
  visibly; prefer transforms for motion.
- `saveLayer` cost for opacity/blend/effect groups on Graphite — the
  CRT/bloom stress tests exist to measure exactly this; stacking
  contexts keep it opt-in.
- Measured (phase 1): on raster targets picture replay re-rasterizes,
  so auto-caching bounds CPU at rasterization cost (~4 µs/text row);
  `Cache::Texture` is the raster-surface pixel win, Graphite targets
  get cheap replay. See STRESS_TESTS.md reference numbers.
- `flowAround`'s second layout pass must stay bounded (single re-pass,
  cycles rejected at reconcile).
