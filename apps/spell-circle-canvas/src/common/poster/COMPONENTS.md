# IfritCompose — data-driven drawable components (design)

Extends DESIGN.md (layout + stacking) with the component model: reusable,
data-driven, cacheable pieces that draw into any SkCanvas — "a front end
for canvas drawing", not a UI toolkit. Status: **design for review**;
animation infrastructure (Choreograph + IfritTick) already landed.

## What we surveyed, what we take, what we reject

| System | Take | Leave |
| --- | --- | --- |
| **React** | The element/instance split (cheap immutable *descriptions* reconciled against a retained tree), keys for identity, `memo` for skipping unchanged subtrees | VDOM generality, hooks, the fiber scheduler — our tree is small and we own the frame loop |
| **Flutter** | The three-tree lesson (Widget → Element → RenderObject: descriptions, identity, layout+paint are different objects) and **RepaintBoundary** — explicit cache boundaries in the tree | The framework itself; it owns text and raster pipelines we already own better (TextFlow, Graphite) |
| **SwiftUI / Compose** | Value-type styles; *implicit transitions* — when a prop changes, animate to it instead of snapping | The runtime/DSL machinery |
| **Dear ImGui** | Describe-per-frame ergonomics are great — and reconciliation gives us the same feel with retained identity underneath | True immediate mode: no identity → no caching, no animation continuity |
| **RmlUi / Slint / QML / LVGL** | Validation that markup-ish component trees work for canvas apps | All of them own their own text/render stack; adopting one means fighting it at every leaf where TextFlow/Skia should win |
| **Rive** | State-machine-driven *authored* motion assets — a possible future leaf type | Not a layout/component system; different problem |
| **Skia itself** | `SkPicture` record/replay is the native caching currency — resolution-independent display lists, replayable N times, snapshot-able to textures | — |

Conclusion: nothing off the shelf gives "React over *your* SkCanvas
leaves" without also seizing text and paint. But every hard subproblem
already has a proven owner in our stack — **layout is Yoga, animation is
Choreograph (driven by IfritTick), caching is SkPicture, painting and
text are ours** — so the part left to build is genuinely thin: element
values, a keyed reconciler, and cache boundaries. A few hundred lines of
glue, not a framework. That is the recommendation: build the thin layer,
borrow everything else.

## The model

Three roles, mirroring the survey's lessons:

1. **Elements** — cheap value descriptions, rebuilt freely:
   `box() / text() / image() / web() / custom()`, styled with the
   DESIGN.md properties (flex layout, zIndex, fills, transforms).
2. **Components** — pure functions `Props → Element` over plain data
   structs. `memo(props, fn)` skips describe+diff when props compare
   equal — the data-driven cache.
3. **Composer** — the retained side: reconciles element trees by key
   into instances holding Yoga nodes, resolved styles, cache pictures,
   and live Choreograph outputs. Owns layout, stacking paint, and cache
   invalidation.

```cpp
using namespace ifrit::compose;

// A component: pure data in, description out.
struct RowData { std::string name; int score; bool operator==(const RowData&) const = default; };

Element scoreRow(const RowData &row) {
  return box().row().gap(12).pad(8).corner(6)
      .fill(row.score > 0 ? kInk : kFaded)
      .key(row.name)                                  // identity across refreshes
      .child(text(row.name, mono14).grow(1))
      .child(text(std::to_string(row.score), mono14Bold));
}

Element scoreboard(std::span<const RowData> rows) {
  auto list = box().column().gap(4).cache(Cache::Picture);
  for (const RowData &row : rows)
    list.child(memo(row, scoreRow));                  // unchanged rows: no work
  return list;
}

// Host — any canvas drawing context:
ifrit::tick::FrameClock clock;
ifrit::tick::Ticker ticker;
Composer composer(ticker);

composer.render(scoreboard(model.rows));   // call when data changes (cheap values)
bool animating = ticker.tick(clock.tick());
composer.draw(canvas);                     // stacking-order paint; caches replay
```

## Caching — static, replayed, or live per frame

Flutter's RepaintBoundary + Skia's display lists, made explicit:

- `Cache::Picture` — subtree records into an `SkPicture` once, replays
  each draw. Vector-exact at any scale; the answer to "draw repeated
  complex items" (record one row style, replay per row via memo'd
  instances) and to static posters (whole tree cached → draw is ~free).
- `Cache::Texture` — subtree rasterizes to a texture; for effect-heavy
  layers (blurs, huge shadow stacks) where replaying draws is the cost.
- `Cache::None` — painted live every frame: choreographed glyph text,
  embedded WebView frames, custom leaves doing per-frame work.

Invalidation is the reconciler's job, not the caller's: a boundary
re-records only when its subtree's props/layout changed. A fully static
composition after `render()` costs one replay per draw; a fully dynamic
one degrades gracefully to live painting.

## Animation — Choreograph values as style inputs

No bespoke tween engine (superseding DESIGN.md's Timeline sketch):
[Choreograph](https://github.com/sansumbrella/Choreograph) (from the
sigil-vcpkg-registry) supplies phrases/sequences/motions; **IfritTick**
(`src/common/tick`) is the backing ticking engine — pausable,
time-scalable FrameClock plus a Ticker stepping the master timeline and
reporting "needs more frames" for event-driven hosts.

Two bindings into components:

- **Explicit choreography**: styles accept `ch::Output<T> *` alongside
  plain values — `.opacity(&fade.value)` — and motions are composed on
  `ticker.timeline()` with Choreograph's full vocabulary (ramps, holds,
  loops, ping-pong, cues, staggered delays). Output-bound properties are
  paint-only by design (transform/opacity/color), so choreography never
  triggers relayout.
- **Implicit transitions** (the SwiftUI lesson):
  `.transition(Prop::Opacity, 400ms, ch::EaseOutQuint())` — when
  reconciliation sees the prop change, the composer applies a ramp from
  current to new instead of snapping. Data refresh becomes animation for
  free.

Per-glyph effects stay on `textflow::Choreograph` (the in-repo glyph
placement utility — nameclash noted, namespaces disambiguate:
`textflow::` vs `choreograph::`), registered as Ticker steppables.

## What this is not

Not a widget toolkit (no input/focus/accessibility), not React (no VDOM,
no scheduler — `render()` is explicitly called when data changes), not a
replacement for IfritWeb (which remains the right tool for genuinely web
content). It is the missing middle: TextFlow-quality type and arbitrary
Skia drawing, composed by rules, cached like display lists, animated by
Choreograph, drawable into any canvas.

## Phasing

1. Element values + keyed reconciler over Yoga + the DESIGN.md stacking
   painter (golden-image tests).
2. `Cache::Picture` boundaries + memo.
3. Choreograph bindings: Output-valued props, implicit transitions.
4. Leaves: text (TextFlow), image (IfritImage), web (IfritWeb), custom.
5. Texture caching, FlatBuffers authoring schema (Python/TouchDesigner).
