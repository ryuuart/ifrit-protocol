# IfritCompose — stress-test catalog

Every use case raised during design review, kept as the implementation's
acceptance suite. Each item names what it exercises and how it will be
validated: **golden** = headless golden-image test (textflow_demo-style
deterministic render), **gallery** = interactive scene in ComposeGallery
(the planned Qt host, TextFlowGallery-style), **bench** = compose_bench
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
   floated hero frame (TextFlow `ExclusionFlow` from the frame's
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
    `textflow::Choreograph` reading `paragraphLayout(key)` as a Ticker
    steppable surviving a data refresh of a sibling. Exercises:
    bindings, steppables, glyph-state stability. *gallery + golden
    (frozen time)*
18. **Transition choreography** — list insert/remove/reorder with
    per-key staggered transitions; removed keys unmount cleanly
    mid-flight. Exercises: reconciler ↔ timeline interaction. *gallery*

## Integration & recursion (P4–P5)

19. **Web leaf in a composition / composition in a page** — a live
    `WebView` frame as a leaf (Cache::None), and a Composer drawn into
    an IfritWeb `WebImage` slot — both directions with IfritWeb.
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
- **ComposeGallery** (P5): Qt app in the TextFlowGallery mold — one
  scene per catalog item above marked *gallery*, with live controls
  (time scale, pause, data mutation buttons) for eyeballing motion and
  interaction that goldens can't capture.
- **compose_bench** (P5): google-benchmark target for the *bench*
  items, alongside textflow_bench/web_bench.
