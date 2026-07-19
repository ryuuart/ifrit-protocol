# IfritCompose — the concrete API surface (proposal)

Companion to DESIGN.md. This is the surface as you would write it,
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
Element custom(PaintProgram program);                       // raw SkCanvas

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

// ---- shape (geometry: defines PaintContext::outline and clipping) ----
Element &corners(Corners);
Element &clip(bool = true); Element &clipPath(SkPath);

// ---- paint (ours; stacking per DESIGN.md) ----
// fill/stroke/shadow are DEFINED as sugar over the decoration slots:
// .fill(f) ≡ .background(f); .stroke(s) ≡ .foreground(PathFormat from s);
// .shadow(s) ≡ .background(shadow decoration). One painting system.
Element &fill(Fill); Element &stroke(Stroke); Element &shadow(Shadow);
Element &opacity(Animatable);
Element &blend(SkBlendMode);
Element &translateX(Animatable); Element &translateY(Animatable);
Element &rotate(Animatable /*degrees*/); Element &scale(Animatable);
Element &transformOrigin(float fx, float fy);   // fractions of own box
Element &zIndex(int);

// ---- identity, caching, transitions ----
Element &key(std::string_view);            // stable identity across renders
Element &cache(Cache);                     // Picture | Texture | None

// Implicit transitions — no property enum (properties are named once,
// by their setters). Node-level: reconciled changes to ANY animatable
// property on this node ramp instead of snap. Property-level: pass a
// Transition to the setter itself to scope or override.
struct Transition { std::chrono::milliseconds duration;
                    choreograph::EaseFn ease = choreograph::easeOutQuad; };
Element &transition(Transition);                       // node default
Element &opacity(float, Transition);                   // per-property
Element &fill(Fill, Transition);                       // colors lerp via
                                                       // choreograph
                                                       // Sequence<SkColor4f>

// ---- composition ----
Element &child(Element);
Element &children(std::span<Element>);

// Deferred description: fn (any invocable per ComponentFn — function,
// lambda, functor) runs only when props changed (operator==) since the
// last render — the data-driven cache.
template <ComponentProps P, ComponentFn<P> F>
Element memo(P props, F fn);
```

**One paint-program context.** Every paint-phase entry point — custom
leaves, decorations, contour-walk bodies — receives the same struct
(one shape to learn, one clock to trust):

```cpp
struct PaintContext {
  SkSize size;            // resolved layout size; draw in [0,0 .. size]
  SkPath outline;         // the node's shape (corners / clipPath applied)
  double elapsedSeconds;  // THE Ticker's FrameClock time — paused and
                          // time-scaled consistently with all bindings
  float contentScale;     // device px per layout px (2.0 on HiDPI)
  bool animating;         // whether the Ticker is currently active
};
using PaintProgram = std::function<void(SkCanvas &, const PaintContext &)>;
// Contract: canvas is translated to the node's origin (clipped when
// .clip() is set); matrix/clip restored after you return. You may use
// TextFlow, IfritImage, PaintShaders — anything.

// custom() is DEFINED as sugar: a box whose content is one paint
// program — custom(p) ≡ box().background(p). Custom leaves and
// decorations are the same concept in different slots.
Element custom(PaintProgram program);
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
      .transition({.duration = 250ms})               // highlight fades in
      .child(text(r.name, mono14).grow(1))
      .child(text(formatScore(r.score), mono14Bold)
                 .key(r.id + "#score")
                 .transition({.duration = 300ms, .ease = ch::easeOutBack}));
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
        .child(custom([](SkCanvas &c, const PaintContext &i) {
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

### Decorations — frames, 9-slice, patterned and procedural borders

`.fill()/.stroke()/.corners()/.shadow()` cover documents; game-style
chrome (textured frames, stamped vines, per-edge treatments, procedural
borders) needs decoration to be an **open protocol**, same move as
`LayoutScheme`. Prior art: Flutter's `Decoration`/`BoxPainter`, Godot's
`StyleBox` resources, UE Slate brushes, CSS `border-image` + the
Houdini Paint API — every mature system eventually makes "how a box is
dressed" pluggable. Ours plugs straight into Skia, which already owns
the hard cases.

**Layer model.** A node paints: background decorations (declaration
order) → content + children (stacking rules) → foreground decorations.
`.fill()`/`.stroke()` become sugar for the built-ins below.

```cpp
template <typename D>
concept DecorationScheme =
    requires(const D &d, SkCanvas &canvas, const PaintContext &in) {
      { d.paint(canvas, in) };
    };  // PaintContext: the ONE paint-program context (see Elements).
        // Optional `bool animated() const` → node repaints per frame —
        // the single declared-volatility rule shared by decorations,
        // effects, and bound properties alike.

Element &background(DecorationScheme auto);   // below content, stackable
Element &foreground(DecorationScheme auto);   // above children, stackable
```

**Primitives, not a zoo.** Concrete treatments (dashes, stamps, 9-slice
frames) are *data* over four general primitives — the kit deliberately
stops here:

```cpp
// 1. Fill — paint anything. Color and gradients are conveniences; the
//    general case is any SkShader, and SkSL runtime effects make that
//    "any function of position (and time)". Fills serve backgrounds,
//    stroke paint, and glyph paint alike.
Fill::shader(sk_sp<SkShader>);   // incl. SkRuntimeEffect-built shaders

// 2. PathFormat — format any stroke. A stroke is the outline path put
//    through a chain of path transforms (Skia path effects: dash
//    intervals, 1D path stamping, discretization, corner rounding,
//    your own SkPathEffect) and painted with a Fill. "Dashed border"
//    is a two-number PathFormat, not a type.
PathFormat{.effects = {...}, .paint = Fill::..., .width = 3};

// 3. Slice — map an image/asset onto a box through a lattice
//    (SkCanvas::drawImageLattice: N-patch, per-cell stretch/repeat).
//    Nine-slice is the 3x3 case.
Slice{.asset = frame, .xDivs = {...}, .yDivs = {...}};

// 4. ContourWalk — the general procedural border: walk the outline by
//    arc length (SkContourMeasure) and run a draw program at each
//    sample. Stamped vines, per-step images, "a different canvas as we
//    walk" — all the same primitive with a different body.
struct PathSample { SkPoint pos; SkVector tangent; float distance, fraction; };
ContourWalk{.spacing = 18,
            .draw = [](SkCanvas &c, const PathSample &s, const PaintContext &in) {
              /* anything — images, nested composed elements, SkSL */
            }};
// Element form: the stamp is a full element subtree — laid out once,
// cached as a picture, replayed per sample, animatable via bindings:
ContourWalk{.spacing = 24, .stamp = leafOrnament(seed)};
```

Per-edge treatments are composition, not a type: decorations receive
the outline, and `edges(outline, Edge::Top)` extracts sub-contours to
hand any primitive.

**Caching stays sound**: decorations are values in the description
(reconciled and hashed like everything else), painted inside the node's
`Cache::Picture` recording; ones declaring `animated()` — or carrying
bound `ch::Output` fields — demote their node to live painting while
active, exactly the declared-volatility rule bound properties follow.

### Layer effects — post-processing and backdrop treatment

Post-processing joins as the paint-phase counterpart of decorations:
effects operate on *rendered layers* at stacking-context boundaries,
the way Flutter's `ImageFiltered`/`BackdropFilter` and CSS
`filter`/`backdrop-filter` do — and like decorations, the surface is
two primitives, not an effect zoo:

```cpp
struct Effect {
  static Effect filter(sk_sp<SkImageFilter> f);   // blur, displacement,
                                                  // lighting, compose chains
  static Effect shader(sk_sp<SkRuntimeEffect> e,  // SkSL image filter: the
                       Uniforms u = {});          // node's layer is an input
};                                                // optional animated()

Element &effect(Effect);    // filters the node's own rendered layer
Element &backdrop(Effect);  // filters what's already painted beneath
                            // the node's bounds, then paints the node
```

Concrete looks are data built from these plus what already exists —
composed in user code or the stress-test catalog, not enumerated in the
API: bloom is a bright-pass SkSL `Effect::shader` blurred and re-blended
`kPlus` over the source (foreground *or* background nodes — effects
attach to any node in any layer); a CRT stack is semi-transparent tiled
layers with `.blend(SkBlendMode::kPlus)` accumulating brightness where
they overlap, a scanline SkSL fill, and a `backdrop()` distortion.

Cost model is explicit: an effect forces a `saveLayer` (its node is
already a stacking context), and pairs naturally with
`Cache::Texture` — a filtered subtree renders + filters once and stays
a cached snapshot until it dirties, so expensive post-processing on
static content is paid once, not per frame.

### Tiling and tile-map content

No new machinery — tiling resolves into existing phases, three tiers:

1. **Uniform tiling**: `Fill::shader` with `SkTileMode::kRepeat` and a
   local matrix (any image, `PaintShaders`, or SkSL motif).
2. **Procedural tile *selection*** (the tile-map case): a describe-phase
   component maps data/noise/rules over a grid and emits atlas-region
   image leaves — `image(atlas).region(SkRect)` is the one API addition
   (sub-rect of an asset, the sprite/atlas idiom). Chunk the grid into
   `Cache::Picture` boundaries the way game engines chunk tile maps;
   only chunks whose data changed re-record.
3. **Fully procedural tiling**: one SkSL fill sampling the atlas by a
   computed index — a single draw for backgrounds where per-tile
   identity doesn't matter.

### The pipeline — where procedural enters (one architecture, not two)

The generative cases (text flowing around frames, borders between
*connections*, draw programs walking contours, recursion) do not need a
second API path. They need the pipeline the declarative surface already
implies to be **explicit**, with a procedural entry point at every
phase — each entry point a function whose input is that phase's
resolved output:

| Phase | Input → Output | Procedural entry |
| --- | --- | --- |
| **Describe** | data → elements | components, `memo`, ranges — generation by ordinary code |
| **Layout** | constraints → rects | `LayoutScheme`; text measure via TextFlow |
| **Derive** | resolved geometry → more content | `flowAround`, `connector`, `ContourWalk` stamps |
| **Paint** | geometry + canvas → pixels | `DecorationScheme`, `custom()`, SkSL |
| **Frame** | time → values / next data | Choreograph outputs, steppables, host data feedback |

The **derive phase** is the addition this round forces — the home of
everything whose *input is resolved layout*:

```cpp
// Text flowing around frames: TextFlow's ExclusionFlow already exists;
// the composer plumbs resolved node outlines into it. Runs as a second
// layout pass (how DTP engines do float wrap); reference cycles are
// rejected at reconcile time.
text(article, body16).flowAround("hero-frame", 12 /*margin px*/);

// Borders between connections: a relationship, not a node property —
// a first-class element whose geometry derives from its endpoints'
// resolved bounds, drawn with the full primitive set (a PathFormat, an
// SkSL Fill along the connecting path, a ContourWalk…). Mirrors the
// scene schema's own Edge-between-Points model.
connector("node-a", "node-b")
    .route(routers::orthogonal)                // routers are values/fns
                                               // (Router concept), not an
                                               // enum — write your own
    .format(PathFormat{.paint = Fill::shader(flowFieldSkSL), .width = 4});
```

**Recursion is closed under the model**: a `ContourWalk` stamp is an
element subtree, whose decorations may themselves walk contours, whose
custom leaves may draw entire nested Composers — each level cached,
animated, and reconciled by the same rules. What keeps "insanely
procedural" from becoming "unsound" is one law: **within a frame,
information flows forward only** (describe → layout → derive → paint).
Backward influence — paint results affecting layout — happens either
through the one declared, cycle-checked exception (`flowAround`'s
second pass) or across frames through ordinary data (this frame's
`bounds()` feed next frame's describe), which the event-driven loop
already models. Same backend, same niceties, one dial: at which phase
your code runs.

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
  sketches): `.transition({.duration = 400ms, .ease = ch::easeOutQuint})` —
  no naked doubles-of-seconds.
- **UDLs** for dimensions: `width(50_pct)`, `padding(24_px)`.
- `std::span`/`std::string_view` at boundaries; `std::variant` for
  `Animatable`; no exceptions in the hot path.

## Non-goals (unchanged)

No input/focus/accessibility, no VDOM/scheduler (you call `render()`),
no markup language in the core, no ownership of surfaces or threads —
the Composer is a guest in your canvas, which is the entire point.
