# SigilCompose — design

Data-driven, cacheable, animated drawable components for the Skia
canvases — layered typographic posters, live data panels, game-UI-grade
chrome — drawable into any `SkCanvas` next to everything else we render.
Status: **implemented through the completeness round and the review
arcs that followed** (`REVIEW.md`) — kernel, decorations (element
stamps included), derive phase with routes (`connector`/`rail`),
effects, the Material paint layer, the Brush engine, instancing,
kinetic typography, caching tiers, and queries
(bounds/paragraphLayout/hitTest/routesAt/snapshot). Measured numbers
live in `STRESS_TESTS.md` (its dated sections record each arc), pinned
by ~190 unit tests, the ComposeGallery scenes, compose_demo panels,
and byte-exact capture diffs. The concrete surface lives in `API.md`.

`ROADMAP.md` is the list of things this library does NOT do yet, and
unlike a wishlist every entry came out of building something real: seven
reference-grounded gallery scenes and a set of study sketches under
`sketch/sketches/`, each reconstructing a named artefact. Entries are
ordered by how many independent studies asked for the same thing.

## Problem

We lay out paragraphs beautifully (SigilWeave) and documents completely
(SigilScry/Ultralight). Missing is the middle: *box-level* composition of
SigilWeave-quality typography and arbitrary Skia drawing — sized by
flexbox rules with baseline alignment, layered with explicit z-order and
blending, cached like display lists, animated at scene rate, refreshed
from data without rebuilding the world. SigilScry covers genuinely web
content but through WebCore's text stack, capped at 60 FPS, a thread hop
away. SigilCompose exists for when the typography and the drawing are
the product.

## Foundations — borrow the owners, build the glue

Survey conclusion (React, Flutter, SwiftUI/Compose, Dear ImGui,
RmlUi/Slint/QML/LVGL, Rive, Skia's own machinery): every adoptable
framework owns its text and render pipeline — adopting one means
fighting it exactly where SigilWeave and Graphite should win. But each
hard subproblem has a proven owner already in our stack:

- **Layout: Yoga** (vcpkg 3.2.1). Flexbox with measure callbacks made
  for text leaves; baseline callbacks for typographic alignment;
  microsecond incremental relayout. Validated by `compose_spike_test`:
  measure funcs drive `sigil::weave::layoutParagraph`, width constraints
  reach the line breaker, `align-items: baseline` works via
  `LineMetrics::baseline`. (API lesson from the spike: flexbox's
  `stretch` default would override measured text height — text leaves
  default start-aligned.)
- **Animation: Choreograph + SigilMotion.** sansumbrella's Choreograph
  (sigil-vcpkg-registry port) supplies phrases/sequences/motions;
  SigilMotion (`src/common/motion`) is the backing ticking engine —
  pausable, time-scalable FrameClock and a Ticker that steps the master
  timeline plus steppables and reports "needs more frames" for
  event-driven hosts. (Nameclash note: `sigil::weave::Choreograph` is the
  in-repo glyph-placement utility; `choreograph::` is the tween library
  — namespaces disambiguate, and they compose: glyph effects register
  as Ticker steppables.)
- **Caching: SkPicture** display lists (record/replay), textures for
  effect-heavy layers.
- **Text and paint: ours** (SigilWeave, SigilImage, SigilScry frames,
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

## Element memory — a hot base, boxed rarities

Descriptions are built, copied, and compared every render, so their
layout is a design rule, not an accident. `ElementNode` keeps inline
only the fields every kind touches — kind/key/layout/paint/corners/
decorations/children — and everything rare or kind-specific lives in
seven out-of-line, value-semantic `Box<T>` blocks (Text, Image, Custom,
Derive, Fx, Material, Memo): absent costs one null pointer, present
deep-copies, so the copy-on-write clone behind `Element`'s fluent
builders stays a defaulted copy. `PropValue` applies the same rule at
the property level: it is a compact class, not a variant — plain
constants and bindings stay inline while the fat `Transitioned` payload
(from/waypoints/spec, ~100 B for a float) boxes out-of-line, because
eight `PropValue<float>`s ride every node's paint block. Together the
two moves took the description from 2752 to 688 bytes (and improved
every describe bench), and a
`static_assert(sizeof(ElementNode) <= 768)` in `Composer.cpp` enforces
the rule structurally: new rare or kind-specific state goes in a
block, never the base.

## The pipeline — five phases, procedural entry at each

The architecture's spine. Declarative and procedural are not two APIs;
they are early-phase and late-phase entries into one dataflow:

| Phase | Input → Output | Procedural entry |
| --- | --- | --- |
| **Describe** | data → elements | components, `memo`, ranges |
| **Layout** | constraints → rects | `LayoutScheme` concept; SigilWeave measure |
| **Derive** | resolved geometry → more content | `flowAround`, `connector`/`rail`, `ContourWalk` stamps |
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

## The edge store — derive is flat

Relationships are not discovered by walking the tree. Each render
rebuilds — alongside the key index — the **edge store**: the routed
nodes (`connector()`/`rail()`) and flowing text nodes (`flowAround`)
as flat lists in tree order, plus a back-index from anchor key to the
routes anchored there. The derive pass iterates those lists — no tree
recursion, and a tree containing no derived content pays nothing.
The back-index doubles as the graph query: `routesAt(key)` answers
"which edges touch this node" in O(routes at that node), which is what
hover highlights and pruned edge updates want, without a scene walk.

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
   lets caching stay sound (content-volatile nodes paint live while
   active; purely paint-side volatility — bound transforms, opacity —
   replays the node's cached content under the live transform).

Implicit transitions (the SwiftUI lesson) ride path 1:
`.transition({.duration, .ease})` on a node turns reconciled changes to
its animatable properties into Choreograph ramps instead of snaps, with
per-property scoping via the setters themselves — no property enum
(properties are named exactly once, by their setters). Property price
tags: paint props (opacity/transform/color) animate freely; layout
props re-measure — visible in the API, chosen deliberately.

## VariationDrive — the probe rule

Driving a variable-font axis per frame is a bind-path special case
with one gate: the axis must be **advance-invariant**. The paint phase
draws varied glyphs at their already-shaped positions (no reshape, no
relayout — paint-only by contract), which is only honest if moving the
axis moves no advance. `FontContext::axisIsAdvanceInvariant` proves it
per font by sampling every glyph advance at the axis extremes; an
advance-variant axis — wght on most fonts — is refused with a warning
and the text draws unvaried. Drive GRAD, the advance-invariant weight,
or re-render discretely. The probe result memoizes per (instance,
content); on the weave side `ParagraphLayout::LiveVariations` swaps
bucket typefaces for memoized varied clones at flush, positions
untouched.

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
demotes itself, never its static siblings — and it partitions by KIND:
content volatility (fill lerps, animated decorations, live materials,
`Cache::None`) paints live, while paint-only volatility (bound or
transitioning transforms and opacity) keeps the node's content picture
and replays it under the live transform. Invalidation is the
reconciler's job alone; the one host-facing hook is `purgeCaches()` —
on GPU device loss or a backend switch, cached images minted by the
dead context must not replay onto the next canvas, so the host drops
every per-node cache while the retained tree, layout, and animations
survive.

## Where frames go — paint is the frame

`Composer::Stats` attributes wall time per phase — reconcile, layout,
the volatility walk, paint — precisely so a slow frame localizes at a
glance. The measured shape of every scene, fast or slow, is the same:
the retained machinery rounds to 0.00–0.01 ms and **paint is the
frame** — reconcile, Yoga, and the walks are microsecond-tier; pixels
are milliseconds. That is a discipline, not just a number: kernel and
tree optimizations serve describe-heavy hosts, but a slow scene is a
paint problem, fixed with caching tiers, materials, or the GPU — never
with tree surgery.

## GPU-first

The floor is 60 fps, and the honest way to hold it is to treat
Graphite as the primary target. Full-screen live SkSL — animated
materials, backdrop shaders — is GPU-tier content by definition: the
CPU-raster path re-evaluates the effect per pixel per frame in
software, and no tree, layout, or kernel change alters that (the
former CPU "offenders" all collapse to sub-millisecond frames on
Graphite — the GPU re-measure in `STRESS_TESTS.md`). CPU raster
remains the fallback and the deterministic test target, not the
performance story.

Graphite compatibility is a portability layer, not a per-call-site
concern (`<sigilcompose/GpuImage.h>`): this Skia's graphite `Device`
stubs out `drawImageLattice` and `drawAtlas` with EMPTY bodies, so any
such draw — direct, or replayed from a cached picture — silently
vanishes on GPU. The library therefore never records the native ops:
`gpuimg::drawLattice` decomposes to per-cell `drawImageRect`s and
`gpuimg::drawSpriteAtlas` to one `drawVertices`, on every backend —
always decomposed, because a picture recorded on a raster canvas must
replay correctly on a Graphite one, and a recorded native op would
still vanish there. The same layer owns texture promotion (Graphite
performs no implicit raster-image uploads; the shared recorder seam
additionally installs a caching ImageProvider), and GPU-hostile
formats are avoided at bake time (OCIO LUTs bake to F16 — F32 textures
are not linearly filterable on Apple GPUs).

## Independent data domains

Three tiers: `memo` per source (unchanged branches cost a hash check);
named **slots** (`slot("ticker")` in the tree,
`composer.renderSlot("ticker", …)` updating only that mount point while
its surroundings' caches stay valid); multiple Composers for fully
separate systems, ordered by draw-call order in the shared canvas.

## Queries

Elements are write-only descriptions; reads target the **Composer**,
post-layout, read-only: `bounds(key)`, `paragraphLayout(key)` (live
SigilWeave layout for glyph choreography and decoration), `hitTest(pt)`,
and `routesAt(key)` (the edge store's back-index — which routes anchor
on this node). Querying descriptions is rejected — it would invent a
second identity system (the React ref lesson).

## Kernel and extensions

The review rounds grew the surface; this boundary keeps the original
goal — *draw and control, quickly and robustly, without framework
weight* — honest. Three tiers. The **kernel** is what phase 1 builds
and what every user must understand: `Element`/component functions/
`Composer`, Yoga flex + `stack()`, stacking paint with
zIndex/opacity/blend/transform, `Fill::color/shader`, the
text/image/custom leaves, `key` + `memo`, `PropValue`/`Transition`,
`ch::Output` bindings, and automatic caching. **Util**
(`<sigilcompose/util.h>`) holds deliberately-demoted sugar a user
could write themselves — gradient constructors, stroke/shadow
helpers, the `Stage` host bundle — depending only on the kernel and
optional by definition. Everything else is an **extension that plugs
into a kernel seam** and ships as its own header, with the kernel
depending on none of it: `LayoutScheme`, `PathFormat`/`Slice`/
`ContourWalk`, `Effect`/backdrop, the derive phase (`flowAround`,
`connector`/`rail`), slots, `Cache::Texture` — and the shelf the
review arcs grew on the same terms: `Material`/`Ocio` (the polymorphic
paint value and its bake tooling), the Brush engine
(`Brushes`/`Lines`/`LayerStyles`/`Patterns` — the line-and-chrome
vocabulary), `Shapes`/`Layouts`/`Routers`/`Web`, `Sdf`, `Kinetic`
(per-glyph motion), `Console`, `Instances`, `GpuImage`. Each plugs an
existing seam (decorations, fills, layout schemes, routers, leaves); a
user who reads only the kernel section of API.md has a complete, sound
mental model, and extensions add power without ever changing kernel
semantics. That is the design's weight budget, enforced structurally.

Instancing is the weight budget's answer to "millions": masses are a
**leaf, not a tree**. `instances()` is a flyweight repeat layer — an
Atlas of element-tree cells baked once, a user-owned SoA Pool, one
atlas stamp per frame — so ten thousand things cost one custom leaf
and one draw, and the retained tree never has to become an ECS (EnTT
stays on the user's side of the seam, feeding the Pool). Its two modes
are the kernel's two write paths, not a third: Data mode rides the
describe path (pool revision → prune), Live mode the bind path
(`Cache::None`, read every frame).

## Naming

Library **SigilCompose**, namespace `sigil::compose`, directory
`src/common/compose/`. "Poster" survives as a use case, not a name.

## Phasing

1. ~~Element values + keyed reconciler + Yoga integration + stacking
   painter + the four decoration primitives~~ (landed).
2. ~~Cache boundaries (`Picture`, `Texture` under effects) + memo +
   slots~~ (landed; caching later grew the structural prune, direct
   leaf blending, and transform-replay tiers — STRESS_TESTS.md).
3. ~~Choreograph bindings, implicit transitions, layer effects~~
   (landed).
4. ~~Derive phase: `ContourWalk` stamps, `flowAround`, `connector`;
   remaining leaves (image regions, web)~~ (landed, plus
   `snapshot()`/`hitTest()` and the organic Shapes/Layouts/Routers
   kit).
5. ~~ComposeGallery + compose_bench~~ (landed; since rewritten around
   the reference-grounded studies — 29 scenes, GPU-hosted).
6. Authoring: FlatBuffers schema, Python/TouchDesigner path — the
   remaining open phase.

The review rounds after the completeness pass ran their own phased
plan (`REVIEW.md` §12) — file split + prune fix, Material/Brush,
geometry and routes (trim/rail), instancing/console/kinetic, then the
brush-engine and reference-grounding legs — all landed except the
authoring path above; `STRESS_TESTS.md`'s dated sections carry each
arc's measured numbers.

## Risks

- Yoga re-probes measure functions with loose modes — text measure
  cache keyed on (content revision, width, mode); the known sharp edge.
- Animating text-affecting layout props re-measures per frame — priced
  visibly; prefer transforms for motion.
- `saveLayer` cost for opacity/blend/effect groups on Graphite — the
  CRT/bloom stress tests exist to measure exactly this; stacking
  contexts keep it opt-in, and `Material::blend` flattens layer stacks
  into one shader (`SkShaders::Blend`) precisely to avoid it.
- Measured (phase 1): on raster targets picture replay re-rasterizes,
  so auto-caching bounds CPU at rasterization cost (~4 µs/text row);
  `Cache::Texture` is the raster-surface pixel win, Graphite targets
  get cheap replay. See STRESS_TESTS.md reference numbers.
- `flowAround`'s second layout pass must stay bounded (single re-pass,
  cycles rejected at reconcile).
