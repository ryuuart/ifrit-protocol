# SigilCompose — the concrete API surface

Companion to DESIGN.md. This is the surface as you would write it,
header-level signatures plus complete usage in real canvas contexts.
Everything here is `namespace sigil::compose` unless noted. Originally
the design proposal; now implemented well past the completeness round —
`<sigilcompose/Compose.h>` (kernel) plus the extension headers
`Decorations.h`, `Shapes.h`, `Layouts.h`, `Routers.h`, `Util.h`,
`Web.h`, `Material.h`/`Ocio.h`, `Sdf.h`, `Brushes.h`/`Lines.h`/
`LayerStyles.h`/`Pattern.h`/`Patterns.h`, `Kinetic.h`, `Console.h`,
`Instances.h`, and `GpuImage.h` are the real headers, and
STRESS_TESTS.md carries the measured numbers. Where this document and
the headers disagree, the headers win.

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
`SkPath`), text takes real SigilWeave types (`TextStyle`, `Paragraph`,
`ParagraphLayoutOptions`) and hands the resolved `ParagraphLayout` back,
animation takes real Choreograph objects (`ch::Output<float>`, phrases
on the Ticker's timeline), and `custom()` hands you the raw `SkCanvas`.
Every layer has a bottom you can reach.

---

## Values

```cpp
// The kernel Fill is two constructors — a color, or anything Skia can
// shade. Gradient conveniences are userland-shaped and live in
// <sigilcompose/util.h> (they are one-line wrappers over
// Fill::shader(SkShaders::LinearGradient(...))).
struct Fill {
  static Fill color(SkColor4f c);
  static Fill shader(sk_sp<SkShader> shader);
  static Fill none();
};

struct Corners { float radius = 0;  /* or per-corner overloads */ };

// Dimensions carry Yoga semantics: pixels, percent of parent, or auto.
struct Dim;                    // implicit from float (px)
Dim px(float);  Dim pct(float);  Dim autoDim();

// Property values — ONE wrapper scheme for every animatable property,
// no per-setter overload matrix: a constant, a constant that
// transitions when it changes, or a live Choreograph binding.
struct Transition {
  std::chrono::milliseconds duration;
  choreograph::EaseFn ease = choreograph::easeOutQuad;
  std::chrono::milliseconds delay;   // holds the current/from value first —
                                     // the stagger primitive
};
template <typename T> struct Transitioned {  // value + spec, plus optional
  T value; Transition spec;                  // mount-time `from` / keyframe
  /* from, waypoints */                      // waypoints (see below)
};
template <typename T> Transitioned<T> with(T value, Transition spec);
// Mount entrances: play from → to when the node first appears (the CSS
// animation-on-enter); afterwards behaves exactly like with(to, spec).
template <typename T> Transitioned<T> withFrom(T from, T to, Transition spec);
// Mount keyframe path: absolute (time, value) waypoints — the damped-
// overshoot entrances one ramp can't shape.
template <typename T> Transitioned<T> withKeyframes(
    std::vector<std::pair<std::chrono::milliseconds, T>> frames,
    choreograph::EaseFn ease = ...);

template <typename T> class PropValue;  // T: float, SkColor4f, Fill…
// Holds a plain T, a Transitioned<T>, a const ch::Output<T>*, or a
// SHAPED binding (below). Stored COMPACTLY, not as a std::variant:
// constants and bindings inline, the fat payloads (Transitioned's
// from/waypoints/spec; a binding's map) boxed out-of-line and sharing
// one pointer since they are mutually exclusive — eight PropValue<float>s
// ride every node's paint props, so this is the ElementNode
// hot-base/boxed-rarities rule applied to the property type itself
// (see DESIGN.md).
// Bound and transitioned properties are paint-only by contract:
// animating them never triggers relayout.

// ---- shaped bindings ----
// A bare `&output` lands on the property RAW. bind() remaps it on the way,
// so one Output in [0,1] can drive a translation in pixels, an eased
// opacity and a bar's scaleX at once — without a second Output per unit
// and without the easing living in the tick loop.
Bound bind(const ch::Output<float> *source);
// Three stages, always in this order:
//   1. from(lo, hi)   normalise the SOURCE range onto [0,1]
//   2. map(ease)      shape it — any ch::EaseFn, so all of ease:: fits
//   3. quantize(n) snaps to n discrete levels (period-authentic widgets:
//      Winamp's volume really is round(percent*28), not a sampled slider)
//   4. scale/offset/to/invert  — affine, composed in CALL ORDER;
//      clamp(lo, hi) always applies last, wherever it is written
  .translateX(bind(&phase).to(-70, 170))            // [0,1] → px
  .opacity(bind(&progress).map(ease::outBack()).clamp(0, 1))
  .scaleX(bind(&hp).from(0, maxHp))                 // a health bar
// `.scale(240).offset(-70)` is v*240-70; `.offset(-70).scale(240)` is
// (v-70)*240 — reading order is evaluation order.
// Prunes like anything else (same Output, same affine, same curve under
// the conservative easeEqual rule), so a re-describe that changes only
// the range actually repatches.
```

## Elements

`Element` is a move-friendly value. Builders mutate-and-return; nothing
happens until a `Composer` reconciles the tree.

```cpp
// ---- factories (the leaf set = everything we already draw) ----
Element box();
Element stack();                                            // overlap container
Element text(std::u8string utf8, sigil::weave::TextStyle style);
Element text(std::shared_ptr<sigil::weave::Paragraph> paragraph, // full control:
             sigil::weave::ParagraphLayoutOptions opts = {});    // spans, K-P,
                                     // justification… — pointer identity is
                                     // the change signal (reuse the shared_ptr
                                     // to keep shaping caches warm)
Element image(std::shared_ptr<const sigil::image::ImageAsset> asset);
Element web(std::shared_ptr<sigil::scry::WebView> view);     // live frames
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
Element &absolute(); Element &inset(float all);   // + per-edge, + Dim-valued
                                                  // (autoDim() = unpinned side)
Element &left(Dim); Element &top(Dim);            // pin ONE edge (implies
Element &right(Dim); Element &bottom(Dim);        // absolute) — corner badges
Element &centerAt(SkPoint);   // center an absolute node ON a parent-space
                              // point (resolved post-measure) — node graphs

// ---- shape (geometry: defines PaintContext::outline and clipping) ----
Element &corners(Corners);
// Custom silhouette: a path generator over the laid-out size. Overrides
// corners() as the node's shape — fill, clip(), and every
// outline-following decoration (PathFormat, ContourWalk) trace it.
// Spiky shout dialogs, scalloped seals, any non-rectangular chrome.
Element &outline(std::function<SkPath(SkSize)>);
Element &clip(bool = true);
// Trim Path (Lottie/sksg): reveal the painted outline over [start, end]
// fractions of arc length — fill surface AND every outline-following
// decoration, so a stroked border .trim(0, with(1.0f, {600ms})) DRAWS
// ON. All three take the full PropValue treatment; TrimMode::Wrap +
// an animated offset marches a fixed window around a closed outline
// forever (marching ants, orbiting comets). Paint-phase only: clipping
// and hit-testing keep the untrimmed shape.
Element &trim(PropValue<float> start, PropValue<float> end,
              PropValue<float> offset = 0.0f, TrimMode = TrimMode::Clamp);

// ---- paint (ours; stacking per DESIGN.md) ----
// .fill() is kernel; every setter takes a PropValue, so
// with(v, {300ms}) and Output bindings work uniformly everywhere:
Element &fill(PropValue<Fill>);            // colors/fills lerp via
                                           // choreograph Sequence.
                                           // ALSO takes a live binding:
                                           // fill(&out) where out is a
                                           // ch::Output<Fill> — "this
                                           // widget's colour IS its
                                           // value". Write the Fill
                                           // Output from the same
                                           // steppable as the number.
Element &fill(Material);                   // the richer authoring value —
                                           // see "Materials" below
Element &background(Decoration); Element &foreground(Decoration);
Element &overlay(Decoration);              // BETWEEN them: over the fill,
                                           // under the content and children
                                           // — a hazard stripe that does
                                           // not grey out its own label
Element &stroke(Decoration brush);         // foreground() named for what it
                                           // means: dress the OUTLINE
Element &style(LayerStyle);                // a decoration bundle (aquaGel(),
                                           // y2kChrome()…) in one call
Element &echo(SkVector offset, SkColor4f); // misprint re-stamp UNDER the
                                           // fill/text (P3R registration error)
Element &effect(Effect); Element &backdrop(Effect);
Element &opacity(PropValue<float>);
Element &blend(SkBlendMode);
Element &translateX(PropValue<float>); Element &translateY(PropValue<float>);
Element &rotate(PropValue<float>); Element &scale(PropValue<float>);
Element &skewX(PropValue<float>); Element &skewY(PropValue<float>);
                                           // degrees; the ATLUS diagonal —
                                           // paint-only like rotate/scale
Element &transformOrigin(float fx, float fy);   // fractions of own box
Element &transformOriginPx(SkPoint);            // px pivot for overlay zooms
Element &zIndex(int);
// gradient/stroke()/shadow() CONSTRUCTORS live in <sigilcompose/util.h>
// — pure sugar over PathFormat/Fill::shader, deliberately outside the
// kernel (see "Kernel, util, extensions").

// ---- text extras (text() leaves) ----
Element &textAlign(sigil::weave::TextAlignment);
Element &textFill(Material);   // glyph paint mapped to TEXT-METRIC space
                               // (unit square → cap band); chrome type
Element &glyphFx(GlyphFx);     // kinetic typography: per-glyph effect +
                               // stagger + PropValue master progress
                               // (presets in <sigilcompose/Kinetic.h>)
// VariationDrive: drive a variable-font axis at DRAW time from a bound
// Output — paint-only, no reshape, no relayout. Gated per font: the
// paint phase probes advance-invariance (every glyph advance sampled at
// the axis extremes, memoized per content) and REFUSES advance-variant
// axes with a warning — wght moves advances on most fonts; GRAD is the
// advance-invariant weight. Refused text draws at its shaped coords.
Element &variationDrive(const char (&tag)[5],
                        const choreograph::Output<float> *value);

// ---- identity, caching, transitions ----
Element &key(std::string_view);            // stable identity across renders
Element &cache(Cache);                     // OVERRIDE only — see Caching:
                                           // provably-static subtrees are
                                           // picture-cached automatically
Element &bakeScale(float);                 // Cache::Texture only, 0.1–1: bake
                                           // at a reduced raster scale, blit
                                           // up — for planes whose content is
                                           // soft anyway (blurred glass);
                                           // never for sharp text/hairlines
Element &transition(Transition);           // node default applied to any
                                           // plain-constant prop change
Element &staggerChildren(std::chrono::milliseconds each,
                         Stagger::From = Stagger::From::Start);
                                           // GSAP container stagger: child
                                           // subtrees' withFrom() entrances
                                           // delay by order·each (End =
                                           // bottom-up, Center = ripple)

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
  sigil::weave::FontContext *fonts; // the composer's fonts — what element
                          // stamps and ad-hoc SigilWeave drawing use
};
using PaintProgram = std::function<void(SkCanvas &, const PaintContext &)>;
// Contract: canvas is translated to the node's origin (clipped when
// .clip() is set); matrix/clip restored after you return. You may use
// SigilWeave, SigilImage, PaintShaders — anything.

// custom() is DEFINED as sugar: a box whose content is one paint
// program — custom(p) ≡ box().background(p). Custom leaves and
// decorations are the same concept in different slots.
Element custom(PaintProgram program);
```

### Transition semantics — declarative state, not commands

Transitions are not fire-and-forget animations; they are a standing
declaration: *changes to this property converge smoothly instead of
snapping*. The lifecycle rules, stated once:

- **One motion per (instance, property), always.** A reconciled change
  while a transition is mid-flight **retargets**: the motion continues
  from the property's *current* value toward the new target — this is
  Choreograph's own `Output` re-apply semantic, and matches CSS
  transitions. Nothing stacks, nothing queues.
- **Reset is just description.** Describe the value you want; the
  system converges toward the latest description. Changing back
  mid-flight retargets back; describing a plain value with no
  `with()`/node transition snaps. There is no imperative
  cancel/reset/cleanup API to call or forget.
- **Mount applies values directly** — plain values and `with()` don't
  transition on first appearance (the CSS rule). Entrances are
  EXPLICIT: `withFrom(from, to, spec)` plays its ramp on mount,
  `withKeyframes()` plays a waypoint path, and `staggerChildren()`
  cascades a container's entrances — afterwards all behave like
  `with()` (retarget-from-current). `snapshot()`/`measure()` render
  the settled end values.
- **Unmount cancels automatically**: the instance's `ch::Output`s are
  destroyed with it, and Choreograph disconnects a motion when its
  Output dies — removed list rows can't leak motions by construction.
- **Keys carry state**: a keyed node that moves in the tree keeps its
  instance, so mid-flight transitions survive reorders.

## Composer — the retained side, and the whole canvas contract

```cpp
class Composer {
public:
  /** fontContext outlives the composer; ticker drives transitions and
   *  (via its FrameClock, when attached) PaintContext time. */
  Composer(sigil::motion::Ticker &ticker,
           sigil::weave::FontContext &fontContext);

  /** Layout viewport in canvas-space px (a poster's size, a panel's
   *  rect…). Percent dims resolve against this. The ROOT element
   *  always fills the viewport (its own width/height are ignored,
   *  like the CSS root) — size content via children. An empty size
   *  means "intrinsic": the root takes its content size (what
   *  snapshot() uses). */
  void setSize(SkSize size);

  /** Feeds PaintContext::elapsedSeconds (one clock everywhere). Null
   *  freezes paint time at 0 — static content, goldens. */
  void setClock(const sigil::motion::FrameClock *clock);

  /** Output view transform (color management): applied to the whole
   *  output as the final stage — one saveLayer while set, zero cost
   *  cleared. Intended source: an OCIO display/view baked to a LUT
   *  (<sigilcompose/Ocio.h>); any Effect works. Post-cache. */
  void setView(Effect view);

  /** Reconciles `root` against the retained tree by key/position:
   *  new nodes mount, matching nodes patch (starting transitions),
   *  missing nodes unmount. Call whenever your data changed — memo'd
   *  subtrees with equal props cost a hash check. */
  void render(Element root);

  /** Updates only the named slot() mount point (independent data
   *  domains); the surrounding tree's caches stay valid. */
  void renderSlot(std::string_view name, Element content);

  /** True when content or layout changed since the last draw() —
   *  combine with ticker.active() for the redraw decision. */
  bool dirty() const;

  /** Lays out (if dirty) and paints, honoring the canvas's CURRENT
   *  matrix and clip — the composer draws INTO your drawing; it owns no
   *  surface, no loop, no thread. Cached subtrees replay SkPictures. */
  void draw(SkCanvas &canvas);

  /** Drops every per-node cache (auto pictures, Texture bakes, held
   *  live-material shaders) and marks a full repaint. GPU hosts call
   *  this on device loss or a backend switch — cached images minted by
   *  a dead context must not replay onto the next canvas. The retained
   *  tree, layout, and animations are untouched. */
  void purgeCaches();

  // ---- escape hatches / queries ----
  /** Resolved layout rect of a keyed node (canvas space). */
  std::optional<SkRect> bounds(std::string_view key) const;
  /** The live SigilWeave layout of a keyed text node — for glyph
   *  choreography, hit queries, decorations. Valid until next layout. */
  const sigil::weave::ParagraphLayout *paragraphLayout(std::string_view key) const;
  /** Topmost key at a canvas-space point (paint-order, transform- and
   *  shape-aware; see "Querying" below). */
  std::optional<std::string> hitTest(SkPoint canvasPoint) const;
  /** Keys of route elements (connector()/rail()) anchored on nodeKey —
   *  the edge store's back-index; see "Querying" below. */
  std::vector<std::string> routesAt(std::string_view nodeKey) const;

  /** Per-frame introspection: node/cache/memo counters plus per-phase
   *  wall time — reconcileMs (render()s since the previous draw),
   *  layoutMs, volatileMs, paintMs. The paint number is where per-pixel
   *  cost lives; the other three are the retained machinery, which
   *  rounds to zero on every measured scene (DESIGN.md, "paint is the
   *  frame"). */
  struct Stats { /* instances, memoHits, picturesLive, …,
                    reconcileMs, layoutMs, volatileMs, paintMs */ };
  const Stats &stats() const;
};
```

That is the entire integration contract: **construct with a Ticker and
a FontContext, `render()` on data change, `draw(canvas)` wherever you
already draw.**

---

## Caching — automatic wherever it is provable

"Cache as much as possible" is the policy, and the declared-volatility
rule is what makes it *safe to automate*: because every source of
change is declared (bindings, `with()` transitions, `animated()`
schemes, web/custom `Cache::None` leaves), "static" is a provable
property of a subtree, not a heuristic guess. So:

- **Reconcile prunes automatically.** Element trees are structurally
  hashed; a subtree whose new description equals its old one is skipped
  wholesale — no patching, no cache invalidation, *whether or not you
  used `memo`*. `memo`'s only job is cheaper still: don't even call the
  describe function. Use it for expensive describes over big data;
  everything else is already pruned.
- **Picture caching is automatic** for provably-static subtrees (no
  declared volatility anywhere below): the composer records them on
  first paint and replays until a reconcile dirties them. `.cache()` is
  an *override*, not a chore: `Cache::None` to opt out (memory-tight
  hosts, huge one-off trees), `Cache::Texture` to rasterize under
  heavy effects. The default path is: write nothing, get the caches.
- **Animation costs exactly its subtree.** A bound headline demotes its
  own node, not its parent's static frame — volatility partitions the
  tree; static siblings stay cached while neighbors animate. And it
  **Size the node to the thing that animates.** A bound opacity or blend
  allocates a `saveLayer` the size of the NODE, not the size of what is
  painted in it — so ten Outputs hung on twenty full-canvas groups cost
  eighteen megapixels of layer per frame to twinkle content covering 2%
  of it. Measured: **18.38 → 7.94 ms** by moving each binding onto its
  own primitive's tight box, same Outputs, same pixels. This is the same
  shape of trap as "a picture is not a pixel cache" — a true statement
  that reads as free, and is free only per pixel of the node's box.

  partitions by KIND: paint-only volatility (bound/transitioning
  transforms and opacity) keeps the node's own content picture and
  replays it under the live transform; content volatility (fill lerps,
  animated decorations, live materials, `Cache::None`, animated image
  frames) paints live.
- **`bakeScale(0.1–1)` is almost always the wrong lever, and this doc
  used to recommend it for the one case it is worst at.** It rasterizes
  the bake below device scale and blits it back up. So it makes the
  BAKE cheaper and every BLIT more expensive — the blit becomes an
  upscaling resample, and it pays that forever. It is a win only when a
  node re-bakes often and blits rarely, which is the opposite of what a
  static `Cache::Texture` node does: bake once, blit every frame. One
  study removed it from six nodes and went **mean 11.07 → 4.31 ms**.

  Reach for it only when something forces frequent re-bakes (a live
  material stepping at its own rate, a resizing node) *and* the content
  is soft enough to survive the resample. Sharp text and hairlines never
  belong under a reduced bake.
- **A `Cache::Texture` bake is taken in one of two spaces**, and the
  composer picks. A node **holding still** is baked in DEVICE space,
  snapped out to whole device pixels and blitted with the matrix reset —
  a literal copy of the pixels the uncached draw would produce, at any
  angle. A node **moving** keeps a LOCAL-space bake, taken at a coarse
  quantized scale step and blitted through its transform: one bake
  reused across a whole pinch-zoom or a whole spin, at the cost of
  resampling. Changing between the two costs one re-bake.

  "Holding still" is two independent measures and it is worth knowing
  they are not the same one: the node's own transform must not be
  *declared* animating, **and** the device rect it lands on must not have
  moved. A node that declares nothing still moves under a resizing
  window, a pinch zoom, a pan, or an uncached ancestor's live transform.

  A device-space bake is never taken while recording into a picture: a
  picture can be replayed under a different matrix than it was recorded
  at, and a bake pinned to one device rect is not matrix-independent. So
  a `Cache::Texture` node whose parent is a cacheable static subtree gets
  the local bake, frozen into the parent's recording.

  > **A bake ISOLATES, so a decoration's blend mode does not reach the
  > canvas.** The bake is taken into a transparent layer with plain
  > srcOver — that is what makes it a group — and the finished image is
  > then composited by the node. So a Texture-cached node whose
  > *decoration* paints `kPlus` (an additive glow, a bloom stroke) must
  > also carry `.blend(kPlus)` on the node itself, or the blit lands
  > srcOver and the glow paints flatly over the ground instead of adding
  > to it. Uncached, the same decoration composites directly and looks
  > right — which is why this shows up as "it broke when I cached it".
- **`purgeCaches()` is the host's one hook**: on GPU device loss or a
  backend switch, drop every per-node cache (pictures, texture bakes,
  held live-material shaders) — images minted by a dead context must
  not replay onto the next canvas. Tree, layout, and animations
  survive.

### A picture is not a pixel cache

This is the most expensive misunderstanding in this codebase, so it
gets its own heading. An `SkPicture` records the **draw calls**.
Replaying it re-runs every SkSL shader over every pixel, every frame,
forever. `picturesRecorded == 0` reads like "fully cached" and tells
you *nothing* about cost. Only a **bake** keeps pixels.

Sixteen of the twenty-five run-1 studies missed 60 FPS on exactly this,
and `kumiko_asanoha` is the argument in one line: **0 pictures
recorded, 5 nodes painted, 182 ms**. The picture cache was not failing.
It was succeeding, and not helping.

### Automatic texture promotion

The composer measures what each node's paint actually costs and, once a
node has been expensive for several consecutive frames, bakes that
subtree once into a raster image and blits it thereafter. On by
default; `Composer::setAutoTexturePromotion(false)` globally,
`.cache(Cache::Picture)` per node ("record, and never promote").

Three kinds of node are eligible:

1. A cached subtree whose picture replay is expensive.
2. **A leaf that never records a picture at all.** Bare boxes are
   excluded from picture recording on purpose — one `drawRect` beats a
   nested recording — and for a while the promoter watched only the
   replay path, so a full-canvas box carrying one grain shader was
   structurally invisible to it. That node is this corpus's single
   largest cost centre: 663 ms of a 697 ms frame in `chladni_tab1`.
3. **A node whose only volatility is a live material that has not
   actually moved since the bake.** `Material::quantizeTime(10)` steps
   the injected `uTime` ten times a second, so at 60 FPS five frames in
   six resolve to the *same shader* — and identical inputs mean
   identical pixels, so the previous bake is still exact. Stability is
   **measured**, not read off `quantizeTime`: a material bound to a
   continuous Output resolves anew every frame, never reaches the rate,
   and stays live; a bound Output that happens to be holding still is
   just as cacheable and gets the same treatment.

**It may not change a pixel**, and that is structural rather than
hoped for: promotion is refused unless the node maps to device space
upright, unmirrored and unskewed, and the bake is then taken in device
space at an integer-snapped rect and blitted with the matrix reset and
no resampling. An integer device translation cannot alter
rasterisation, so the blit is a literal copy.

**The refusal you will hit is the paper-grain idiom.** A leaf at
`opacity(0.13).blend(kSoftLight)` over the whole canvas is the most
expensive node in three studies and cannot be promoted: compositing a
bake applies the alpha to an already-rounded 8-bit colour where the
direct draw applies it to the shader's float output. The two agree to
within 1 LSB, which is not agreement. Ask for that one yourself with
`.cache(Cache::Texture)` — an author who types it has accepted the
rounding, and the library has not.

**Why a node was or was not baked is reported.** `NodeCost::promotion`
carries `Cheap` / `Warming` / `Promoted` / `AskedFor` / `OptedOut` /
`Volatile` / `Composited` / `Transformed` / `Filtered` / `TooBig`, and
`ComposeSketch --bench` prints it under every expensive node.
`Composer::promotionReason()` turns it into a phrase. Every refusal is
individually correct and individually invisible, which is precisely how
sixteen studies shipped over the gate.

> **Writing a cache test?** A static node under a *cacheable* parent is
> painted exactly once, into the parent's recording, and never visited
> again — so it never appears in the profile and assertions about it
> pass vacuously. Put `.cache(Cache::None)` on the wrapper. (This is
> also the real shape of the corpus: these leaves sit under a `stack()`
> with animated siblings, which is why their cost is paid every frame.)

## Kernel, util, extensions — where things live

Three layers keep the library lean and the call sites short:

- **Kernel** (`<sigilcompose/compose.h>`): elements/components/
  Composer, flex + `stack()`, stacking paint, `Fill::color/shader`,
  text/image/custom leaves, `key`/`memo`, `PropValue` + `Transition`,
  automatic caching. Complete mental model, smallest possible surface.
- **Util** (`<sigilcompose/util.h>`, depends only on the kernel —
  deliberately *demoted* sugar that users could write themselves):
  gradient Fill constructors, `stroke()`/`shadow()` decoration
  helpers, and `Stage` — the three-line host bundle for the common
  loop:

  ```cpp
  util::Stage stage({1080, 1350}, fonts); // owns FrameClock+Ticker+Composer
  stage.render(poster(info));             // on data change
  bool more = stage.frame(canvas);        // tick + draw + needs-more-frames
  ```

  Anything in util is by definition optional reading.
- **Extensions** (own headers, plug into kernel seams, kernel never
  depends on them): `LayoutScheme`, `PathFormat`/`Slice`/`ContourWalk`
  (`<sigilcompose/Decorations.h>`, element stamps included),
  `Effect`/backdrop, `flowAround`/`connector`, slots — plus the
  organic kit: `<sigilcompose/Shapes.h>` (polygon/star/squircle/blob
  outline generators, `rounded()`, per-edge `edges()`/`onEdges()`),
  `<sigilcompose/Layouts.h>` (`Radial`, `AlongPath`, `Scatter`
  layout schemes), `<sigilcompose/Routers.h>`
  (`straight`/`orthogonal`/`arc` connector routers;
  `polyline`/`octilinear`/`orbit` rail routers), and
  `<sigilcompose/Web.h>` (the SigilScry `web()` leaf; header-only, only
  targets that link SigilScry include it) — and the later shelf, on
  the same terms: `<sigilcompose/Material.h>`/`Ocio.h` (the polymorphic
  paint value + LUT bake tooling, "Materials" below), `Sdf.h` (IQ
  operators as one-pass materials), `Brushes.h`/`Lines.h`/
  `LayerStyles.h`/`Pattern.h`/`Patterns.h` (the brush/line/chrome
  vocabulary, "The Brush engine" below), `Kinetic.h` (glyph-effect
  presets), `Console.h` (the streaming log), `Instances.h`
  ("Instancing" below), `GpuImage.h` ("Drawing images portably"
  below), and `Debug.h` (assertions for GENERATED geometry — see below).

## What the study program added

Everything in this section came out of rebuilding real artefacts and
finding the library could not say something. `ROADMAP.md` carries the
full list with its citation counts; this is the surface.

```cpp
// ---- bindings you can shape ----
bind(&out).from(lo,hi).map(ease).quantize(n).scale(s).offset(o)
          .to(lo,hi).invert().clamp(lo,hi)   // any float property
fill(&fillOutput)                            // ch::Output<Fill>, live

// ---- paint order ----
Element &overlay(Decoration);   // over the fill, UNDER content and children
Element &sampling(SkSamplingOptions);        // image leaves; kNearest
Slice::filter, Pattern::sampling             // the same knob on those two

// ---- reveals ----
Element &wipe(float angleDeg, PropValue<float> fraction);
// A directional reveal at ANY angle, covering the node's decorations too.
// trim() walks the PERIMETER (a wedge round the outline, not a growing
// surface) and scaleX/scaleY SQUASH — a striped fill shows it instantly.
decorations::wash(Material, SkBlendMode, amount)   // material-valued
                                                   // decoration; comparable
compose::metrics(style, fonts)  -> {ascent, descent, capHeight, xHeight,
                                    lineHeight, capSlack()}

// ---- text on a path ----
TextPath{ .path, .at, .align, .offset, .autoFlip, .orient }
// .orient = Tangent (running lettering) | Radial (type radiating like a
// spoke — a dial, a compass rose, an astrolabe limb). EVERY contour of
// the path is walked, in order, as one arc-length coordinate.

// ---- shapes ----
shapes::circle()  shapes::annulus(innerRatio)  shapes::sector(a, sweep, inner)
shapes::inset(px, Decoration)  shapes::arrow(shaft, head)
shapes::star(points, innerRatio, waist)      // waist bows the arms concave
shapes::parametric(fn, t0, t1, samples, close)   // fn: t -> UNIT (x, y)
shapes::lissajous(a, b, deltaDeg, turns)     shapes::rose(k, turns)
shapes::harmonograph(a, b, delta, damping, precession, turns)
shapes::spiral(turns, logarithmic, growth)   shapes::trochoid(R, r, d, inside)
util::disc(centre, radius)                   // the ELEMENT form

// ---- lines and decorations ----
lines::radialHatch(fill, spokes, width, centre)   // a fan out of a point
lines::concentric(fill, rings, width, centre)     // …and its ring twin
PathFormat::dashPhaseBinding                      // marching dashes, bound
PathFormat::trimStart/trimEnd/trimOffset/trimPhase // its OWN trim window,
                                                   // composing with the node's
decorations::paintOn(canvas, ctx, path, decoration) // the brush vocabulary
                                                    // on geometry you built

// ---- materials ----
Material::glowUnit(centre01, radius01, stops)  // radius = INSCRIBED circle
Material::linearUnit / radialUnit              // radialUnit = HALF-DIAGONAL
ease::outBack(s) / inBack / inOutBack / outElastic / inElastic / outBounce

// ---- instancing ----
instancing::instances(atlas, pool, mode, blend)   // blend is PER SPRITE
pool.sizes()[i] = {sx, sy};                       // non-uniform, opt-in

// ---- motion ----
ticker.addFixed(hz, fn, maxCatchUp, &alphaOut);   // fixed step; alphaOut is
                                                  // the render interpolant

// ---- verifying generated geometry ----
debug::coverage(pieces, region, grid)  -> {uncovered, doubled, witnesses}
debug::endpointDegrees(pieces, tol)    -> degree per merged endpoint
```

Four of these exist because a study reported the opposite of the truth
and worked around it — `PathFormat`'s trim window, the bound `Fill`,
`decorations::paintOn`, and `onPath`'s multi-contour walk were all
already there and undiscoverable. When something seems impossible here,
read the header before believing it.

## Worked example 1 — static poster, headless (weave_demo style)

```cpp
using namespace sigil::compose;
namespace ch = choreograph;

Element poster(const EventInfo &info) {
  return box().column().padding(64).gap(24)
      .fill(util::linearGradient({0, 0}, {1080, 1350},
                                 {kInkDark, kInkPlum}))
      .cache(Cache::Picture)                    // static → record once
      .child(text(info.title, display96).key("title").zIndex(2))
      .child(box().row().gap(12).alignItems(Align::Baseline)
                 .child(text(u8"vol. 4", mono14))
                 .child(text(info.subtitle, serifItalic21)))
      .child(image(info.cover).absolute().inset(0).zIndex(0).opacity(0.35f));
}

// Host: exactly like a textflow demo panel.
sigil::motion::Ticker ticker;               // unused motions? fine — inert
sigil::weave::FontContext fonts;
Composer composer(ticker, fonts);
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

// Glyph-level motion via sigil::weave::Choreograph, as a Ticker steppable:
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
     { l.place(in) } -> std::convertible_to<std::vector<SkRect>>;
   };
   // LayoutInput: constraints + each child's measured size (text leaves
   // already measure via SigilWeave).

   struct Grid {                       // ~20 lines of user code
     int columns; float gap;
     std::vector<SkRect> place(const LayoutInput &) const;
   };
   Element layout(LayoutScheme auto scheme);       // container factory
   ...
   layout(Grid{.columns = 3, .gap = 12}).children(cells);
   // NOTE: `place()` is the whole concept. This block used to advertise an
   // optional `measure(in) -> SkSize` and the header never required or
   // called one, so a scheme cannot size its own container — the spacejam
   // study computes its table's height and then hardcodes `.height(870)`
   // beside it. Recorded in ROADMAP.md; the doc no longer promises it.
   ```

### Querying — allowed, but only on the resolved side

Elements are write-only descriptions; **reads target the Composer**
(React's ref lesson: you query the committed tree, never the
description — querying descriptions would invent a second identity
system). The read surface is post-layout and read-only:

```cpp
std::optional<SkRect> bounds(std::string_view key) const;
const sigil::weave::ParagraphLayout *paragraphLayout(std::string_view key) const;
std::optional<std::string> hitTest(SkPoint canvasPoint) const;  // topmost key
std::vector<std::string> routesAt(std::string_view nodeKey) const; // edges
```

That is enough to draw *around* nodes, attach scene geometry to them,
and do coarse interaction, without leaking node internals. hitTest is
paint-order aware (topmost first), transform-aware (rotated/scaled
nodes hit in their visual place), and shape-aware (custom outlines and
corner radii bound the region — the gap between a star's arms misses);
keyless hits resolve to the nearest keyed ancestor, and clipped
subtrees don't hit outside their clip.

routesAt is the graph query — "which edges touch this node" — answered
from the edge store's back-index in O(routes at that node): keys of
`connector()`/`rail()` elements anchored on the node, in tree order,
for hover highlights and pruned edge updates. Keyless routes are
anchored but unaddressable, so they're omitted — give routes keys to
see them here. Valid after `render()`.

Two more read-side primitives round this out — the one-shot render and
its sizing twin:

```cpp
/** Reconcile + lay out + record an element tree into a picture at its
 *  intrinsic size (or bounded by maxSize). Bindings sample at their
 *  current values; transitions don't run. The bake behind ContourWalk
 *  element stamps — and generally "an element tree as a brush". */
sk_sp<SkPicture> snapshot(Element root, sigil::weave::FontContext &fonts,
                          SkSize maxSize = {});

/** One-shot intrinsic measurement: the same reconcile+layout without
 *  painting — the sizing primitive behind content-fit chrome
 *  (marquees, tooltips, badges). Same sampling rules as snapshot(). */
SkSize measure(Element root, sigil::weave::FontContext &fonts,
               SkSize maxSize = {});
```

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
   bound node demotes itself from its ancestors' recordings while
   active: bound transforms/opacity still replay the node's own cached
   content under the live transform, while content volatility paints
   live (declared volatility is what keeps caching sound).

Arbitrary direct mutation of retained nodes is rejected on principle:
it's the one door that, once open, makes every cache unsound and every
frame a full repaint "just in case".

### Shapes and patterns worth knowing about

`<sigilcompose/Shapes.h>` generators return an `OutlineFn`, so any
element can BE the shape — fill, clip, every outline-following
decoration and `hitTest()` all trace it. Two easy ones to miss:

- `shapes::sector(startDeg, sweepDeg, innerRatio = 0)` — a CLOSED,
  fillable circular sector; `innerRatio > 0` gives the annular
  segment. `shapes::arc()` beside it is deliberately OPEN and has no
  fillable area, so it is the wrong tool for a pie wedge, a
  polar-area petal, a cooldown sweep, a radial-menu slice or a gauge
  fill. Skia's angle convention: 0° = +x, sweeping clockwise.

`<sigilcompose/Patterns.h>` has two noise fields and they are not
interchangeable:

- `patterns::noise(freq, octaves, seed, turbulence)` wraps Skia's
  Perlin shader, whose three channels are INDEPENDENT fields. That is
  what you want as a displacement source, and it is wrong for grain:
  composited over a coloured surface with `kOverlay` it hue-shifts
  rather than shades.
- `patterns::grain(freq, octaves, seed)` is the LUMINANCE one — equal
  channels, so a blend mode reads as light. Paper tooth, film grain,
  stone veining, worn metal, dither: this one.

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
order) → **the node's own fill** → `overlay()` decorations → content +
children (stacking rules) → foreground decorations.
`.fill()`/`.stroke()` become sugar for the built-ins below.

> **`background()` is UNDER the fill, not under the content.** This is
> correct — it is what "background" means in every box model — and it
> is also the single easiest way to lose work here: an opaque `fill()`
> hides every `background()` on the same node completely and silently.
> Three decorations vanished this way in one study before anyone
> noticed. If you want a decoration over the fill but under the
> children, that is `overlay()`; over everything, `foreground()`.

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
//    (N-patch, per-cell stretch/repeat; nine-slice is the 3x3 case).
//    Painted via gpuimg::drawLattice, never the native
//    SkCanvas::drawImageLattice — an empty stub on Graphite (see
//    "Drawing images portably").
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

> **The bloom recipe above is a performance trap on anything that
> moves.** Bright-pass → blur → `kPlus` measured **62 ms at 2 MP** on
> the raster host, and `bakeScale` does not help, because the
> `saveLayer` is allocated at full size regardless of what you
> rasterize into it. It is affordable only on content static enough to
> bake once. For a rim glow that animates, do not reach for a
> full-frame effect at all: use `brushes::LayeredBrush` (an additive
> stroke stack — that *is* what a glow is) or an `SkMaskFilter` blur on
> the shape itself. Both cost the shape's own area instead of the
> frame's.

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
| **Layout** | constraints → rects | `LayoutScheme`; text measure via SigilWeave |
| **Derive** | resolved geometry → more content | `flowAround`, `connector`/`rail`, `ContourWalk` stamps |
| **Paint** | geometry + canvas → pixels | `DecorationScheme`, `custom()`, SkSL |
| **Frame** | time → values / next data | Choreograph outputs, steppables, host data feedback |

The **derive phase** is the addition this round forces — the home of
everything whose *input is resolved layout*:

```cpp
// Text flowing around frames: SigilWeave's ExclusionFlow already exists;
// the composer plumbs resolved node outlines into it. Runs as a second
// layout pass (how DTP engines do float wrap); reference cycles are
// rejected at reconcile time.
text(article, body16).flowAround("hero-frame", 12 /*margin px*/);
// Call repeatedly to weave one paragraph between several elements
// (drop cap + corner ornaments; margin applies to all):
text(verse, ink).flowAround("dropcap", 14).flowAround("fne", 8);

// Borders between connections: a relationship, not a node property —
// a first-class element whose geometry derives from its endpoints'
// resolved bounds. The routed path arrives as the connector's
// PaintContext::outline, so the full primitive set dresses it (a
// PathFormat, an SkSL Fill along the path, a ContourWalk, a Brush…).
// Mirrors the scene schema's own Edge-between-Points model. Routers
// are values/fns (stock ones in <sigilcompose/Routers.h>), not an
// enum — write your own.
connector("node-a", "node-b", routers::orthogonal())
    .stroke(PathFormat{.paint = Fill::shader(flowFieldSkSL), .width = 4});

// The component that IS a line: a path threaded through an ordered run
// of anchors (normalized points on keyed nodes' bounds — a transit
// line through its stations), re-routed whenever an anchored node
// moves. trim() makes it draw itself.
rail({{"a", {1, .5f}}, {"hub", {.5f, .5f}}, {"b", {0, .5f}}},
     routers::octilinear())
    .trim(0, with(1.0f, {800ms}))
    .stroke(lines::cased(3, ink, 5));
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

## Materials — the polymorphic paint value

### The cost model, stated plainly

A static SkSL material's **shader** caches. Its **pixels do not**, and
automatic picture caching cannot help — a picture records the draw call,
not the result, so a full-canvas `patterns::grain` re-runs its shader on
every replay.

That is not a small effect. One such node cost **480 ms of a 624 ms
frame** in the tartan study; `.cache(Cache::Texture)` on two grain layers
took the frame **624 ms → 28 ms, 22×**.

So: a large node filled with a generated material wants `Cache::Texture`
explicitly, whenever its content is static. The rule of thumb is area —
a swatch does not care, a full-canvas wash does.

`Material` (`<sigilcompose/Material.h>`) supersedes the kernel's
three-case `Fill` as the *authoring* value for `fill()` — and for
`textFill()`, glyph paint mapped to text-metric space; `Fill` stays
the low-level {none, color, shader} carrier the reconciler stores. A
Material is a small tree of paint nodes that compiles to ONE shader —
layers via `SkShaders::Blend`, never stacked saveLayer:

```cpp
Material::solid(color);
Material::linear(a, b, stops);  Material::radial(...);  Material::sweep(...);
Material::linearUnit({0,0}, {0,1}, stops);   // the UNIT SQUARE, not pixels
Material::radialUnit({0.5f,0.5f}, 1.0f, stops);
Material::image(img, tileX, tileY, localMatrix);  // sprites: sub-rect matrix
Material::sksl(effect, {{"uSpeed", 2.f}});        // SkSL runtime effect
Material::blend({{base, kSrcOver}, {sheen, kScreen}});  // ONE flattened shader
material.uniform("uGlow", &output);  // bind a ch::Output → material is LIVE
material.quantizeTime(6.0f);         // step the injected uTime at 6 Hz
```

`linear()/radial()/sweep()` are in node-local PIXELS, which is right
when you wrote the box's size down and impossible when the layout
decides it — a tooltip as tall as its copy, a panel that grows with
its content. The `Unit` pair authors the same ramp in the node's unit
square instead: `{0,0}` is the box's top-left and `{1,1}` its
bottom-right, whatever the box turns out to be, and `radialUnit`'s
radius is a fraction of the half-diagonal so `{0.5,0.5}` r=1 reaches
the corners of any box. Same trick `textFill()` uses to map a
material onto text metrics. Any number of stops; geometry tier.

Three volatility tiers, decided by what the recipe reads:

- **Static** (solids, ramps, images, constant-uniform sksl): resolves
  eagerly, collapses to a `Fill`, caches and prunes on the kernel
  path. Materials compare STRUCTURALLY by recipe, so re-describing
  the same material prunes even though each describe minted a fresh
  shader.
- **Geometry-dependent** (the effect declares `uResolution`): resolves
  when the node records, caches between layouts, re-records on size
  change.
- **Live** (a bound `ch::Output` uniform, or the effect reads `uTime`
  or `uContentScale` — both change independently of the node, so
  *reading them IS the volatility declaration*): re-resolves every
  frame; the node paints live. `quantizeTime(hz)` steps the injected
  uTime (`floor(t·hz)/hz`) — declared choppiness as a MATERIAL
  property, not per-consumer ticker plumbing (the P3R sea rule: its
  caustics run at 6 Hz); quantized/held materials repaint at their own
  rate, not the frame rate — between steps the cached picture replays,
  and an expensive one is promoted to a pixel bake that is reused
  until the material actually ticks (see "Automatic texture
  promotion"). That is the whole payoff of declaring the choppiness:
  `quantizeTime(10)` on a 60 Hz draw pays the shader ten times a
  second instead of sixty.

**Do not hand-roll a gradient in SkSL.** `Material::linear/radial/
sweep` lower to Skia's SIMD gradient blitter; an equivalent `mix()`
chain in a runtime effect measured **~7× slower on the same ramp — 15
ms against 2 ms**. Reach for `sksl()` when the recipe is not a ramp,
not to spell a ramp differently.

A `blend()` inherits the highest tier among its layers (the flatten
defers to resolve time when needed). Full-screen live materials are
GPU-tier content (DESIGN.md, "GPU-first"). Color management is an
output stage, not a material concern: `Composer::setView()` takes any
Effect, and `<sigilcompose/Ocio.h>` bakes an OCIO display/view or
colorspace conversion to an F16 3D-LUT Effect — OCIO is bake/export
tooling; the realtime path carries only the LUT sample.

## The Brush engine (lines as expressive as fills)

A `Brush` is ONE comparable value: an ordered **geometry pipeline** over
the node's outline feeding ordered **paint legs** — Illustrator's brush
model (Calligraphic / Scatter / Pattern / Art) closed under composition,
grounded in REFERENCES.md §9 (leaflet/mapbox/QGIS/tldraw conventions):

```cpp
element.stroke(Brush{}
    .op(ops::Rounded{6})                      // geometry ops, in order
    .op(ops::Wave{.amplitude = 3, .wavelength = 30})
    .leg(lines::cased(3, ink, 5))             // any Decoration is a leg
    .leg(brushes::ScatterBrush{.art = spark(), .spacing = 40}));
```

- **Geometry ops** are values (`ops::Wave/Rounded/Sketchy/Offset` —
  designated-init structs with `apply(SkPath)`, optional `bleed()`).
  `GeometryOp` type-erases them exactly like `Decoration` does paint
  schemes — Skia seals `SkPathEffect` subclassing, so this is that seam
  as data. A raw `ops::PathOp` lambda still converts (never prunes).
- **Legs** are ordinary Decorations: `lines::Line` (parallel casings,
  terminal/mid caps with the tip-at-endpoint convention, railway ties,
  dash that stays phase-registered across rails), `LayeredBrush` stacks
  (filament/circuit/rope/pulse), `brushes::ScatterBrush` (an ELEMENT
  instanced along the path, seeded jitter + a `StampModFn` programmatic
  twist), `brushes::PatternBrush` (Illustrator tile semantics:
  integer-fit side tiles, corner/start/end tiles), `brushes::Ribbon`
  (taper / calligraphic nib), or any `PathFormat`.
- **The whole Brush compares** when its parts do — a styled connector
  prunes and caches as one value; animated legs declare volatility
  through; bleeds aggregate (pipeline reach + leg reach).

**Corners are their own problem, and `PatternBrush` now handles them
properly.** A corner tile reserves `cornerLength` of arc on each
adjacent run (so side tiles butt against the elbow instead of sliding
underneath it — set it whenever the corner art is bigger than the side
art), it lands on the **vertex** rather than up to half a detection
step past it, and it faces the **bisector** of the two legs by
default, so one piece of art serves all four corners of a rectangle.
`cornerAlign = Outgoing` is the other useful answer — a directional
marker turning a corner should keep pointing the way it is going.
`cornerAngleDeg` is a per-sample tangent break, so a gently *rounded*
corner deliberately takes no corner tile: there is no break there to
find.

**Every brush declares its reach.** A `LayeredBrush`'s additive stack
paints wide of the path by construction — `filament()` is a 14 px
envelope under an 8 px blur, i.e. 31 px — and a node's recording culls
at its own bounds, so a brush that declared nothing had its halo
clipped off. `bleed()` is `max(width/2 + 3σ)` over the layers. If you
write your own decoration scheme and it paints outside the outline,
declare `float bleed() const` or it will be cut.

### Borders and corners — the frame vocabulary

**A frame is not a 1 px rounded rect**, and this is the part of the
library that was least discoverable: `sketch/sketches/stroke_atlas.cpp`
exists because the vocabulary was there and nobody could see it. **Render
it and look at it before writing a stroke** — every specimen carries the
literal call that made it, which is faster than reading this section:

```sh
ComposeSketch src/common/compose/sketch/sketches/stroke_atlas.cpp \
    --frame atlas.png
```

```cpp
// the rules (Decorations.h) — all Border modes, all comparable values
decorations::border(width, fill, inset)                    // continuous
decorations::brackets(width, fill, arm, inset, angleDeg)   // corners only
decorations::gappedRule(width, fill, gap, inset, angleDeg) // corners omitted
decorations::weightedCorners(runW, cornerW, fill, arm, inset, angleDeg)
decorations::doubleBorder(outer, inner)                    // as a LayerStyle

// the rails (Lines.h) — per-rail offset, width, fill, dash and phase
lines::rails(count, width, fill, gap)      // n symmetric rails
lines::rails({{.offset=-3, .width=1.6f, .fill=ink},
              {.offset= 0, .width=0.6f, .fill=red, .dash={0.01f, 9.4f},
               .cap=SkPaint::kRound_Cap}, ...})   // heavy outer, dotted core
lines::cased / triple / railway / wavy / arrow / dottedCore

// the silhouettes (Shapes.h) — a Corner mask picks WHICH corners
shapes::chamfered(size, Corner mask)   shapes::notched(size, depth, mask)
```

**`cornerAngleDeg` is the one number that will surprise you.** It is the
tangent break that counts as a corner, and it defaults to 30°. A regular
n-gon turns 360/n per vertex, **so the default finds nothing above 12
sides** — a 20-gon turns 18° and its brackets render blank. Pass roughly
0.6× the turn angle. A gently *rounded* corner has no hard break and
therefore takes no bracket, which is correct and is the other half of the
same surprise. When a scan finds nothing, the library prints the sharpest
break it did see and what to pass.

**Rails vs `Line::parallels`.** Neither is a superset. `Line` spells the
symmetric cases (`cased`, `triple`) in one value; `Rails` gives every rail
its own offset, width, fill, dash and phase — heavy-outer-plus-hairline,
solid outer with a dotted core, unequal gaps — none of which parallels can
express. Both are geometrically exact at hard corners: the offset routine
finds real vertices and joins them (arc on the outside of a turn, miter on
the inside) rather than chording across.

**Dashing happens in the CENTRELINE's arc space, then displaces.** That is
what keeps a dashed multi-rail set in register through curvature — every
rail is measured in one parameterisation. A consequence worth knowing:
`{0.01f, N}` gives round dots via the cap, but a 0.01-long dash under a
thin stroke has very low peak coverage and can read as absent against a
bright ground. If dots are faint, widen the rail or lengthen the "on"
interval — the offset is not the problem.

**Live pitch and angle.** `lines::Hatch` takes `spacingBinding` and
`angleBinding` (raw `const Output<float>*`, the same convention as
`PathFormat::dashPhaseBinding` and `Line::dashPhaseBinding`), so a moiré,
a tightening engraving or a rotating shade pass animates without leaving
the decoration.

`Element::stroke(brush)` attaches it; rails/connectors hand the routed
path to the same pipeline, so a transit line, a directed edge, and a
sketchy river are all `Brush` values on routes.

Alongside the pipeline, the line-and-chrome vocabulary — all ordinary
value decorations that compare, prune, and cache like `PathFormat`:

- **`brushes::artAlong(art, height, stationPx)`** (`ArtBrush`) — the
  Illustrator ART brush proper: ONE art cell (any element tree) baked
  once at 2×, each contour walked into a triangle-strip ribbon, one
  `drawVertices` warping the art CONTINUOUSLY around curvature — where
  a rigid stamp run breaks into segments. `stationPx` is warp fidelity
  (6 px follows tight metro curves).
- **`lines::hatch(fill, spacing, width, angleDeg)`** /
  **`lines::crosshatch(...)`** (`Hatch`) — Sk2D lattice hatching
  (`SkLine2DPathEffect`) clipped to the node's outline, so concave
  shapes hatch exactly; crosshatch adds the perpendicular pass. The
  engraving/drafting/blueprint fill for any silhouette.
- **`styles::gloss(color, sigma, offset, ringCenter, ringWidth)`**
  (`GlossContour`) — the Photoshop Satin / "Gloss Contour" curve: the
  shape's blurred coverage remapped through a 256-entry contour table
  (blur → TableARGB on one image-filter chain), tinted, clipped INSIDE
  the shape. The moving light band in gel and chrome that a plain
  gradient can't fake — it follows the shape's distance field, not a
  screen axis. Attach as a foreground.

## Instancing — thousands of things as one leaf

`<sigilcompose/Instances.h>` (namespace `sigil::compose::instancing`)
is the flyweight repeat layer: a template ATLAS baked once from
element trees, a user-owned SoA POOL, ONE atlas stamp per frame.
Node-graph nodes, inventory cells, confetti, tick arrays — masses are
a leaf, never N Yoga subtrees.

```cpp
auto atlas = std::make_shared<instancing::Atlas>(/*oversample=*/2.0f);
int gem = atlas->cell(gemCell(), {24, 24});    // frame index for the pool

auto pool = std::make_shared<instancing::Pool>();
pool->add({x, y}, gem, angleRad, scale, tint); // SoA: position / rotation /
pool->positions(); pool->tints(); /*…*/        // scale / tint / frame spans
pool->touch();                                 // bulk-mutated? bump revision

parent.child(box().width(w).height(h)          // the wrapper IS the
    .child(instancing::instances(atlas, pool,  // placement API
                                 instancing::Mode::Data)));

place::grid(*pool, count, columns, cell);      // data-level generators:
place::ring(*pool, count, center, radius);     // O(count) arithmetic, no
place::repeat(*pool, count, start, step, …);   // Yoga (skottie Repeater
                                               // law: exponential scale)
```

The contract: the **Atlas is a recipe** — cells register at a fixed
logical size, the sheet bakes once on first stamp (oversampled so
raster stamps never magnify; re-registering drops the bake). The
**Pool is yours** — mutate it directly or copy in from an EnTT view;
the ECS stays on your side of the seam. **Stamping is RSXform** —
rotation + uniform scale + translation only, by design (skew or
non-uniform cells are real elements). The leaf FILLS ITS PARENT
(absolute, inset 0): wrap it in a sized box and pool positions are
that parent's local px. Two modes, matching the kernel's two write
paths: `Mode::Data` prunes on (atlas, pool, revision) — mutate,
`touch()`, `render()`; `Mode::Live` is the `Cache::None` particle path
that reads the pool every frame. Past 2048 instances the stamp culls
against the local clip before building draw arrays. Measured: 10k
instances in 0.18 ms on Graphite (18 ns/sprite, ~200× CPU raster) —
masses are a GPU play.

## Drawing images portably — the GpuImage rule

For anyone writing custom decorations or paint programs that draw
lattices or sprite batches: **use `gpuimg::drawLattice` /
`gpuimg::drawSpriteAtlas` (`<sigilcompose/GpuImage.h>`), never the
native `SkCanvas::drawImageLattice` / `drawAtlas`.** In this Skia,
graphite's `Device` overrides those two ops with EMPTY bodies — the
draw silently vanishes on any Graphite canvas. And because compose
records subtrees into SkPictures, checking the live canvas is not
enough: a native op recorded on a raster canvas still vanishes when
the picture replays on Graphite. The portable forms are therefore
ALWAYS decomposed — lattice into per-cell `drawImageRect`s (NinePatch
alternating fixed/stretch bands), atlas into one `drawVertices`
sampling the sheet — and decomposed draws replay correctly on every
backend (raster's native drawAtlas lowers to the same vertices
internally anyway).

Both take a `gpuimg::Promoted &` cache: Graphite performs no implicit
raster-image uploads, so raster sources promote to textures once per
(image, recorder) — hold the cache where you hold the image (a Slice,
an Atlas). `Slice` and the instancing stamp already route through this
layer; images sampled by shaders/materials ride the recorder's
ImageProvider instead.

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
- `std::span`/`std::string_view` at boundaries; no exceptions in the
  hot path. (`PropValue` is deliberately NOT a `std::variant` — a
  compact class boxing the fat `Transitioned` payload out-of-line; see
  Values.)

## Non-goals (unchanged)

No input/focus/accessibility, no VDOM/scheduler (you call `render()`),
no markup language in the core, no ownership of surfaces or threads —
the Composer is a guest in your canvas, which is the entire point.
