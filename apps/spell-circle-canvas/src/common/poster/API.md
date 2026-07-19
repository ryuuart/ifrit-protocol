# IfritCompose — the concrete API surface (proposal)

Companion to COMPONENTS.md. This is the surface as you would write it,
header-level signatures plus complete usage in real canvas contexts.
Everything here is `namespace ifrit::compose` unless noted.

## The three answers up front

**Is it HTML / markup / an external DSL?** No. You write C++. The "DSL"
is a fluent builder over plain value types — an embedded DSL in the same
sense as SwiftUI or Flutter, not a parsed language. There is no string
templating, no codegen, no runtime parser. (A FlatBuffers authoring
schema can arrive later for the Python/TouchDesigner path — but as a
*producer of the same Element values*, a client of this API, never the
API itself.)

**Is it a component builder?** Components are **free functions** from
your data to an `Element` value. No base classes, no inheritance, no
lifecycle methods. State lives in your data model; the tree is a
projection of it.

**Is it abstracted?** The *bookkeeping* is abstracted — element diffing,
Yoga node lifetimes, cache invalidation, stacking order, saveLayer
management, dirty tracking. The *capabilities* are not: styling takes
real Skia types (`SkColor4f`, `sk_sp<SkShader>`, `SkBlendMode`,
`SkPath`), text takes real TextFlow types (`TextStyle`, `Paragraph`,
`ParagraphLayoutOptions`) and hands the resolved `ParagraphLayout` back,
animation takes real Choreograph objects (`ch::Output<float>`, phrases
on the Ticker's timeline), and `custom()` hands you the raw `SkCanvas`.
Every layer has a bottom you can reach.

---

## Values

```cpp
// Paint slots accept real Skia machinery; the named constructors are
// conveniences, shader() is the general case.
struct Fill {
  static Fill color(SkColor4f c);
  static Fill linearGradient(SkPoint from, SkPoint to,
                             std::vector<SkColor4f> colors,
                             std::vector<float> stops = {});
  static Fill radialGradient(SkPoint center, float radius,
                             std::vector<SkColor4f> colors,
                             std::vector<float> stops = {});
  static Fill shader(sk_sp<SkShader> shader);   // anything Skia can make
  static Fill none();
};

struct Stroke  { Fill fill; float width = 1.0f; };
struct Shadow  { SkColor4f color; SkVector offset; float blur = 0.0f; };
struct Corners { float radius = 0;  /* or per-corner overloads */ };

// Dimensions carry Yoga semantics: pixels, percent of parent, or auto.
struct Dim;                    // implicit from float (px)
Dim px(float);  Dim pct(float);  Dim autoDim();

// Any float-valued paint property can be a live Choreograph binding
// instead of a constant. Bound properties are paint-only by contract:
// animating them never triggers relayout.
using Animatable = std::variant<float, const choreograph::Output<float> *>;
```

## Elements

`Element` is a move-friendly value. Builders mutate-and-return; nothing
happens until a `Composer` reconciles the tree.

```cpp
// ---- factories (the leaf set = everything we already draw) ----
Element box();
Element text(std::u8string utf8, textflow::TextStyle style);
Element text(textflow::Paragraph paragraph,                 // full control:
             textflow::ParagraphLayoutOptions opts = {});   // spans, K-P,
                                                            // justification…
Element image(std::shared_ptr<const ifrit::image::ImageAsset> asset);
Element web(std::shared_ptr<ifrit::web::WebView> view);     // live frames
Element custom(CustomPainter painter);                      // raw SkCanvas

// ---- layout (Yoga, 1:1 semantics) ----
Element &row(); Element &column(); Element &wrapLines();
Element &gap(float px);
Element &padding(float all);            // + per-edge overloads
Element &margin(float all);
Element &width(Dim); Element &height(Dim); Element &aspect(float);
Element &minWidth(Dim); Element &maxWidth(Dim);              // + heights
Element &grow(float = 1); Element &shrink(float); Element &basis(Dim);
Element &alignItems(Align); Element &alignSelf(Align);       // Baseline!
Element &justify(Justify);
Element &absolute(); Element &inset(float all);               // + per-edge

// ---- paint (ours; stacking per DESIGN.md) ----
Element &fill(Fill); Element &stroke(Stroke);
Element &corners(Corners); Element &shadow(Shadow);
Element &opacity(Animatable);
Element &blend(SkBlendMode);
Element &translateX(Animatable); Element &translateY(Animatable);
Element &rotate(Animatable /*degrees*/); Element &scale(Animatable);
Element &transformOrigin(float fx, float fy);   // fractions of own box
Element &clip(bool = true); Element &clipPath(SkPath);
Element &zIndex(int);

// ---- identity, caching, transitions ----
Element &key(std::string_view);            // stable identity across renders
Element &cache(Cache);                     // Picture | Texture | None
Element &transition(Prop, double seconds,  // implicit: prop change animates
                    choreograph::EaseFn ease = choreograph::easeOutQuad);

// ---- composition ----
Element &child(Element);
Element &children(std::span<Element>);

// Deferred description: fn is only invoked when props changed (compared
// by operator==) since the last render — the data-driven cache.
template <typename Props>
Element memo(Props props, Element (*fn)(const Props &));
```

Custom leaves get the real canvas, pre-positioned:

```cpp
struct PaintInfo {
  SkSize size;            // resolved layout size; draw in [0,0 .. size]
  double elapsedSeconds;  // FrameClock-derived, for self-animating leaves
  float contentScale;     // device px per layout px (2.0 on HiDPI canvases)
  bool animating;         // whether the Ticker is currently active
};
using CustomPainter = std::function<void(SkCanvas &, const PaintInfo &)>;
// Contract: canvas is translated to the node's origin (and clipped when
// .clip() is set); matrix/clip state is restored after you return. You
// may use TextFlow, IfritImage, PaintShaders — anything.
```

## Composer — the retained side, and the whole canvas contract

```cpp
class Composer {
public:
  explicit Composer(ifrit::tick::Ticker &ticker);

  /** Layout viewport in canvas-space px (a poster's size, a panel's
   *  rect…). Percent dims resolve against this. */
  void setSize(SkSize size);

  /** Reconciles `root` against the retained tree by key/position:
   *  new nodes mount, matching nodes patch (starting transitions),
   *  missing nodes unmount. Call whenever your data changed — memo'd
   *  subtrees with equal props cost a hash check. */
  void render(Element root);

  /** True when content or layout changed since the last draw() —
   *  combine with ticker.active() for the redraw decision. */
  bool dirty() const;

  /** Lays out (if dirty) and paints, honoring the canvas's CURRENT
   *  matrix and clip — the composer draws INTO your drawing; it owns no
   *  surface, no loop, no thread. Cached subtrees replay SkPictures. */
  void draw(SkCanvas &canvas);

  // ---- escape hatches / queries ----
  /** Resolved layout rect of a keyed node (canvas space). */
  std::optional<SkRect> bounds(std::string_view key) const;
  /** The live TextFlow layout of a keyed text node — for glyph
   *  choreography, hit queries, decorations. Valid until next layout. */
  const textflow::ParagraphLayout *paragraphLayout(std::string_view key) const;
};
```

That is the entire integration contract: **construct with a Ticker,
`render()` on data change, `draw(canvas)` wherever you already draw.**

---

## Worked example 1 — static poster, headless (textflow_demo style)

```cpp
using namespace ifrit::compose;
namespace ch = choreograph;

Element poster(const EventInfo &info) {
  return box().column().padding(64).gap(24)
      .fill(Fill::linearGradient({0, 0}, {1080, 1350},
                                 {kInkDark, kInkPlum}))
      .cache(Cache::Picture)                    // static → record once
      .child(text(info.title, display96).key("title").zIndex(2))
      .child(box().row().gap(12).alignItems(Align::Baseline)
                 .child(text(u8"vol. 4", mono14))
                 .child(text(info.subtitle, serifItalic21)))
      .child(image(info.cover).absolute().inset(0).zIndex(0).opacity(0.35f));
}

// Host: exactly like a textflow demo panel.
ifrit::tick::Ticker ticker;               // unused motions? fine — inert
Composer composer(ticker);
composer.setSize({1080, 1350});
composer.render(poster(info));

sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1080, 1350));
composer.draw(*surface->getCanvas());     // one replayed picture
writePng(surface, "poster.png");
```

## Worked example 2 — data-driven table in a live canvas backend

```cpp
Element scoreRow(const RowData &r) {
  return box().row().gap(12).padding(8).corners({6})
      .fill(r.highlighted ? kAccent : kCard)
      .key(r.id)
      .transition(Prop::Fill, 0.25)                  // highlight fades in
      .child(text(r.name, mono14).grow(1))
      .child(text(formatScore(r.score), mono14Bold)
                 .key(r.id + "#score")
                 .transition(Prop::TranslateY, 0.3, ch::easeOutBack));
}

Element scoreboard(const Model &m) {
  auto list = box().column().gap(4).cache(Cache::Picture);
  for (const RowData &r : m.rows)
    list.child(memo(r, scoreRow));                   // untouched rows: hash check
  return list;
}

// Inside the existing render path (SkiaSceneBackend / SCKEngine style):
void Panel::onModelChanged(const Model &m) { m_composer.render(scoreboard(m)); }

void Panel::paint(SkCanvas &canvas) {                // your draw callback
  canvas.save();
  canvas.translate(m_panelRect.left(), m_panelRect.top());
  m_composer.draw(canvas);                           // next to SceneRenderer output
  canvas.restore();
}

bool Panel::needsFrame() {                           // event-driven contract
  return m_ticker.tick(m_clock.tick()) || m_composer.dirty();
}
```

## Worked example 3 — choreography + raw drawing inside layout

```cpp
// Explicit Choreograph: real Outputs bound as style values.
ch::Output<float> drop = -60.0f, fade = 0.0f;
ticker.timeline().apply(&drop).then<ch::RampTo>(0.0f, 0.7f,
                                                ch::EaseOutQuint());
ticker.timeline().apply(&fade).then<ch::RampTo>(1.0f, 0.5f);

Element hero =
    box().column().gap(16)
        .child(text(u8"IFRIT PROTOCOL", display96)
                   .key("headline")
                   .translateY(&drop).opacity(&fade)   // paint-only, no relayout
                   .cache(Cache::None))                // repainted while animating
        .child(custom([](SkCanvas &c, const PaintInfo &i) {
                 // Raw Skia inside a layout-managed box: rings, shaders,
                 // whatever — this is the "way more customized than UI" hatch.
                 drawSigilRings(c, i.size, i.elapsedSeconds);
               }).height(220).cache(Cache::None));

// Glyph-level motion via textflow::Choreograph, as a Ticker steppable:
ticker.add([&](double dt) {
  const auto *layout = composer.paragraphLayout("headline");
  if (layout) glyphRain.step(*layout, dt);     // stable glyph enumeration
  return glyphRain.active();
});
```

---

## What you have access to, by layer

| Layer | You touch | Abstracted away |
| --- | --- | --- |
| Structure | `Element` values, keys, `memo` | diffing, mount/unmount, node lifetimes |
| Layout | Yoga's model 1:1 (flex, %, absolute, baseline) | `YGNode` management, measure caching |
| Text | `TextStyle`, full `Paragraph` + options; resolved `ParagraphLayout` back out | shaping/layout invalidation, measure funcs |
| Paint | `SkColor4f`, `SkShader`, `SkBlendMode`, `SkPath`, per-node z/opacity/transform | paint order, stacking contexts, saveLayer, picture recording/invalidation |
| Animation | `ch::Output`, phrases/sequences on the Ticker timeline, implicit transitions | when to re-record caches, needs-frame aggregation |
| Escape | `custom()` = raw `SkCanvas` in a laid-out box; `bounds(key)` for drawing *around* nodes | nothing — this is the floor |

## Stress tests — composition, data, layout, mutation

Design answers to the hard cases, each with its API delta.

### Independent data sources updating at different rates

Three tiers, cheapest first:

1. **`memo` per source.** Describe functions are cheap; a root that
   composes two memo'd branches re-describes only the branch whose
   props changed. This covers "model A refreshed, model B untouched".
2. **Slots** — named mount points for truly independent update domains
   (React's portal/root lesson, minus the DOM):

   ```cpp
   // In the tree:  .child(slot("ticker"))
   composer.renderSlot("ticker", tickerStrip(feedModel));  // updates ONLY
   // that mount point — the surrounding tree is untouched, its caches
   // stay valid, and the slot content still participates in layout and
   // stacking exactly like an inline child.
   ```
3. **Multiple Composers** for fully separate systems (scene HUD vs
   poster): each is already a guest in the canvas; cross-composer
   ordering is simply draw-call order.

### Two elements composing on top of each other

New container — the overlap primitive (Flutter `Stack` / CSS grid-area
overlap, made explicit):

```cpp
Element stack();   // children share the stack's box; each positions
                   // itself via alignSelf/inset; painted in
                   // (zIndex, declaration order); sized to the largest
                   // child unless given explicit dims
```

Ordering and blending semantics stay the DESIGN.md stacking model, now
stated precisely for cross-component composition:

- Siblings from *different components* interleave in their **common
  parent's** paint order `(zIndex, declaration order)` — a component
  cannot z-escape the stacking context it was composed into, so two
  data-driven subtrees layered in a `stack()` are ordered where they
  meet, by the code that composed them. Ordering is always decidable by
  reading the composition site.
- `.blend(mode)` composites a node **as a layer against everything
  painted before it in its stacking context** — including siblings that
  came from other components/data sources. Multiply-ing a live table
  over a poster background is one `.blend(SkBlendMode::kMultiply)` at
  the composition site.

### Custom layout schemes (the lightweight grid)

Two levels, honoring "layout is just code":

1. **Grid as a component** — a free function that computes rects and
   emits absolutely-positioned children. Works with zero new API; fine
   for one-offs.
2. **`LayoutScheme` concept** — SwiftUI's `Layout` protocol, C++20-ified:
   a container can delegate measure + placement to your code instead of
   flexbox (implemented internally as a Yoga measure func + pinned
   children, so it nests freely inside flex and vice versa):

   ```cpp
   template <typename L>
   concept LayoutScheme = requires(const L &l, const LayoutInput &in) {
     { l.measure(in) } -> std::convertible_to<SkSize>;
     { l.place(in) }   -> std::ranges::range;   // of SkRect, one per child
   };
   // LayoutInput: constraints + each child's measured size (text leaves
   // already measure via TextFlow).

   struct Grid {                       // ~20 lines of user code
     int columns; float gap;
     SkSize measure(const LayoutInput &) const;
     std::vector<SkRect> place(const LayoutInput &) const;
   };
   Element layout(LayoutScheme auto scheme);       // container factory
   ...
   layout(Grid{.columns = 3, .gap = 12}).children(cells);
   ```

### Querying — allowed, but only on the resolved side

Elements are write-only descriptions; **reads target the Composer**
(React's ref lesson: you query the committed tree, never the
description — querying descriptions would invent a second identity
system). The read surface is post-layout and read-only:

```cpp
std::optional<SkRect> bounds(std::string_view key) const;
const textflow::ParagraphLayout *paragraphLayout(std::string_view key) const;
std::optional<std::string> hitTest(SkPoint canvasPoint) const;  // topmost key
```

That is enough to draw *around* nodes, attach scene geometry to them,
and do coarse interaction, without leaking node internals.

### Swapping children / updating data directly — the two write paths

The model admits exactly two ways to change what's on screen, priced
and policed differently:

1. **Describe** — structure and discrete state: call `render()` (or
   `renderSlot()`) with a new description. Keyed reconciliation IS the
   child-swap API: reordered keys move instances (transitions and glyph
   state intact), missing keys unmount, new keys mount. There is
   deliberately no `node.removeChild()` — imperative tree surgery is
   what reconciliation replaces, and it would corrupt memo/cache
   invariants silently.
2. **Bind** — continuous values: `ch::Output<float>*` (and friends)
   mutated every frame without any render call. Bindings are *declared*
   in the description, so the composer knows exactly which properties
   are volatile — bound properties are paint-only by contract, and a
   bound node inside a `Cache::Picture` boundary is either lifted out
   of the recording or the boundary demotes to `Cache::None`
   (declared volatility is what keeps caching sound).

Arbitrary direct mutation of retained nodes is rejected on principle:
it's the one door that, once open, makes every cache unsound and every
frame a full repaint "just in case".

## C++20 at the surface

Used deliberately, for errors and ergonomics rather than cleverness:

- **Concepts** gate the generic entry points with readable failures:
  `ComponentProps` (`std::equality_comparable` + `std::copyable` — what
  `memo` needs), `ComponentFn<F, P>` (invocable returning `Element`),
  `LayoutScheme` (above), `Steppable` (`invocable<double> -> bool`, for
  `Ticker::add`).
- **Defaulted `operator==`** makes any aggregate usable as props:
  `struct RowData { ...; bool operator==(const RowData&) const = default; };`
  — memo works with zero ceremony; **designated initializers** make
  call sites self-documenting (`Grid{.columns = 3, .gap = 12}`).
- **Ranges**: `children()` accepts any range of `Element`, so
  `column.children(rows | std::views::transform(scoreRow))` replaces
  the loop.
- **`std::chrono` durations** for time-valued API (revising earlier
  sketches): `.transition(Prop::Opacity, 400ms, ch::EaseOutQuint())` —
  no naked doubles-of-seconds.
- **UDLs** for dimensions: `width(50_pct)`, `padding(24_px)`.
- `std::span`/`std::string_view` at boundaries; `std::variant` for
  `Animatable`; no exceptions in the hot path.

## Non-goals (unchanged)

No input/focus/accessibility, no VDOM/scheduler (you call `render()`),
no markup language in the core, no ownership of surfaces or threads —
the Composer is a guest in your canvas, which is the entire point.
