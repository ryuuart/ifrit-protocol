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
// <sigilcompose/Util.h> (they are one-line wrappers over
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
// animate() is COMPOSER-MANUFACTURED motion: the composer runs the clock,
// as against a bound Output, where you do. THE ARGUMENT SAYS WHICH KIND:
//   to(v)          — RAMP ON CHANGE. No entrance; every later describe
//                    that differs ramps instead of snapping, retargeting
//                    from the current value.
//   from(a).to(b)  — a MOUNT ENTRANCE (the CSS animation-on-enter); after
//                    it plays, the property behaves exactly like to(b).
template <typename T> To<T> to(T value);
template <typename T> Transitioned<T> animate(To<T> t, Transition spec = {});
template <typename T> From<T> from(T value);          // .to(target) completes
template <typename T> Transitioned<T> animate(FromTo<T> ft,
                                              Transition spec = {});
// Keyframe path: absolute (time, value) waypoints — the damped-overshoot
// entrances one ramp can't shape; `ease` applies per segment, a leading
// time > 0 holds the first value. through() takes a float path with no
// template argument (a nested braced list is a non-deduced context, which
// is what forces the generic form's <T>); through<T>({...}) otherwise.
Waypoints<float> through(
    std::initializer_list<std::pair<std::chrono::milliseconds, float>>);
template <typename T> Transitioned<T> animate(Waypoints<T> w,
                                              choreograph::EaseFn ease = ...);
  .opacity(animate(to(dimmed ? 0.4f : 1.0f), {180ms}))   // ramp on change
  .opacity(animate(from(0.0f).to(1.0f), {400ms}))        // mount entrance
  .translateX(animate(through({{0ms, 40.f}, {200ms, -20.f}, {400ms, 0.f}})))
// `with(v, spec)`, `withFrom(a, b, spec)` and `withKeyframes<T>(f, e)`
// were the mechanism spellings of the three forms above. DELETED in R3
// (ROADMAP §33); animate() is the one verb.

template <typename T> class Animatable; // T: float, SkColor4f, Fill…
                                        // (`PropValue` was its mechanism
                                        //  name; DELETED in R3)
// Holds a plain T, a Transitioned<T>, a const ch::Output<T>*, or a
// SHAPED binding (below). Stored COMPACTLY, not as a std::variant:
// constants and bindings inline, the fat payloads (Transitioned's
// from/waypoints/spec; a binding's map) boxed out-of-line and sharing
// one pointer since they are mutually exclusive — eight Animatable<float>s
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
// Three stages, always in this order. The stage names are source/target;
// `from`/`to` used to name them too and were DELETED in R3 — they read as
// the endpoints of an authored ramp, which is a different idea entirely
// (ROADMAP §33 ruling 3):
//   1. source(lo, hi) normalise the SOURCE range onto [0,1]
//   2. map(ease)      shape it — any ch::EaseFn, so all of ease:: fits
//   3. quantize(n) snaps to n discrete levels (period-authentic widgets:
//      Winamp's volume really is round(percent*28), not a sampled slider)
//   4. scale/offset/target/invert — affine, composed in CALL ORDER;
//      clamp(lo, hi) always applies last, wherever it is written
//   5. wiggle(amount, frequency, seed, octaves, falloff) — smooth
//      procedural noise, added AFTER the affine chain and before clamp
  .translateX(bind(&phase).target(-70, 170))        // [0,1] → px
  .opacity(bind(&progress).map(ease::outBack()).clamp(0, 1))
  .scaleX(bind(&hp).source(0, maxHp))               // a health bar
// `.scale(240).offset(-70)` is v*240-70; `.offset(-70).scale(240)` is
// (v-70)*240 — reading order is evaluation order.
// Prunes like anything else (same Output, same affine, same curve under
// the conservative easeEqual rule, and every wiggle parameter), so a
// re-describe that changes only the range — or only the seed — actually
// repatches.

// ---- wiggle(): procedural noise (2026-07-29, lives in SigilMotion) ----
// After Effects' most-used expression, and the source of camera shake,
// handheld drift, organic jitter and turbulence. Two rulings:
//   • It reads NO CLOCK. The noise is a pure function of the NORMALISED
//     input (post source()/window(), pre map()), so "wiggle over time"
//     means binding a phase that ramps with time — which every animation
//     already has in hand. apply() stays a pure float→float map and
//     snapshot()/plate renders stay byte-reproducible for free.
//   • `amount` is in the PROPERTY'S OWN UNITS — px, degrees, alpha —
//     because the stage sits after the affine chain. It is a BOUND: the
//     noise is normalised to [-1,1] whatever the octave count, so the
//     value never leaves ±amount of the un-wiggled one, and clamp()
//     still applies last.
// The curve shapes the SIGNAL, not the schedule: .map()/.quantize() do
// not ease or stair-step the wiggle. Under window() the phase clamps
// with the input, so a wiggle scoped to a beat holds outside it.
wiggle(&out, amount, frequency, seed, octaves, falloff)  // = bind(&out)
                            // .scale(0).wiggle(…) — noise around REST,
                            // which is what a shake rig wants
  .translateX(wiggle(&seconds, 12.f, 7.f, 1))   // a camera shake: ±12 px
  .translateY(wiggle(&seconds, 12.f, 7.f, 2))   // @ 7 Hz, DIFFERENT seeds
  .rotate(wiggle(&seconds, 1.5f, 5.f, 3))       // — shared seeds would
                                                //   slide it on a diagonal
  .opacity(bind(&t).target(0.9f, 1).wiggle(0.4f, 20.f, 4, 3).clamp(0, 1))
                                    // 3 octaves = flicker, not drift
```

## Elements

`Element` is a move-friendly value. Builders mutate-and-return; nothing
happens until a `Composer` reconciles the tree.

```cpp
// ---- factories (the leaf set = everything we already draw) ----
Element box();
Element stack();                                            // overlap container
Element positioned();  // the positioned leaf set: children carry their OWN
                       // rects (.left/.top/.width/.height — px or pct, an
                       // open dim + opposing inset pins the far edge, text
                       // measures against its width) and the whole subtree
                       // skips Yoga — zero flex nodes below. For generated
                       // geometry (tilings, lattices, fields) that never
                       // wanted layout. Everything else is ordinary:
                       // decorations, masks, transitions, stagger, zIndex,
                       // hitTest, bounds(). The container itself is a normal
                       // box in its parent's flow and does NOT auto-size
                       // from its children. Unsupported inside: flex props
                       // (ignored), centerAt, layout() schemes, flowAround.
                       // Trap: a child with Auto dims and no opposing inset
                       // resolves 0x0 — size it or pin it.
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
// width()/height() are the flex BASIS, not a guarantee: `shrink` defaults
// to 1, so a width(150) child of an overflowing row gives some back and the
// failure is silent overlap. Pair with `.shrink(0)` when width(150) means
// "this IS 150". Faithful Yoga/CSS; it still costs an iteration each time.
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
// Custom silhouette: a Shape VALUE over the laid-out size. Overrides
// corners() as the node's shape — fill, clip(), and every
// outline-following decoration (PathFormat, ContourWalk) trace it.
// Spiky shout dialogs, scalloped seals, any non-rectangular chrome.
//
// A Shape is the seam's comparable value (§3 CLOSED): every shapes::
// generator is a ShapeScheme (`path(SkSize)` + `==`), so a shaped node
// PRUNES. A raw callable (`[](SkSize){…}`, OutlineFn) is still accepted
// — the escape hatch that never compares, so such a node re-patches
// every describe exactly as every shaped node used to; memo() it, or
// hold ONE Shape value (copies of one Shape compare equal).
Element &shape(Shape);                           // `outline()` DELETED (R3)
Element &clip(bool = true);
// Trim Path (Lottie/sksg) was `Element &trim(start, end, offset, TrimMode)`
// here. R4 DELETED it along with `wipe()`, and the `TrimMode` enum with it
// — see the fold table under "Masking" below, which is the same statement
// this section used to contradict. It revealed the fill surface AND every
// outline-following decoration at once, and that is why it moved: a reveal
// belongs to a PASS.
//   trim(a, b)          -> .stroke(spans::upTo(t), brush) for the marks,
//                          .mask(by::spans(...)) when the FILL must reveal
//   TrimMode::Wrap      -> spans::wrap(begin, end)
//   the offset argument -> spans::….offset(o)

// ---- paint (ours; stacking per DESIGN.md) ----
// .fill() is kernel; every setter takes an Animatable, so
// animate(to(v), {300ms}) and Output bindings work uniformly everywhere:
Element &fill(Animatable<Fill>);           // colors/fills lerp via
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
// Every unqualified mark slot takes an optional LOCAL name — the label
// `mask(parts::named(name), …)` addresses, same names and same law as
// stroke(Spans, what, name).
Element &background(Decoration, std::string name = {});
Element &foreground(Decoration, std::string name = {});
Element &overlay(Decoration, std::string name = {});
                                           // BETWEEN them: over the fill,
                                           // under the content and children
                                           // — a hazard stripe that does
                                           // not grey out its own label
Element &stroke(Decoration brush, std::string name = {});
                                           // foreground() named for what it
                                           // means: dress the OUTLINE
// The span-qualified slots: WHERE on the boundary, painted by WHAT. Twins
// — one claim ledger, two z-halves (see "The stroke slot" below).
Element &stroke(Spans where, Decoration what, std::string name = {});
Element &background(Spans where, Decoration what, std::string name = {});
Element &style(LayerStyle);                // a decoration bundle (aquaGel(),
                                           // y2kChrome()…) in one call
Element &echo(SkVector offset, SkColor4f); // misprint re-stamp UNDER the
                                           // fill/text (P3R registration error)
Element &effect(Effect); Element &backdrop(Effect);
Element &opacity(Animatable<float>);
Element &blend(SkBlendMode);
Element &translateX(Animatable<float>); Element &translateY(Animatable<float>);
Element &travel(MotionPath);               // a CURVE instead of two lanes
                                           // (see "The motion path")
Element &rotate(Animatable<float>); Element &scale(Animatable<float>);
Element &skewX(Animatable<float>); Element &skewY(Animatable<float>);
                                           // degrees; the ATLUS diagonal —
                                           // paint-only like rotate/scale
Element &transformOrigin(float fx, float fy);   // fractions of own box
Element &transformOriginPx(SkPoint);            // px pivot for overlay zooms
Element &zIndex(int);
// gradient/stroke()/shadow() CONSTRUCTORS live in <sigilcompose/Util.h>
// — pure sugar over PathFormat/Fill::shader, deliberately outside the
// kernel (see "Kernel, util, extensions").

// ---- text extras (text() leaves) ----
Element &textAlign(sigil::weave::TextAlignment);
Element &textFill(Material);   // glyph paint mapped to TEXT-METRIC space
                               // (unit square → cap band); chrome type
Element &glyphFx(GlyphFx);     // kinetic typography: per-glyph effect +
                               // stagger + Animatable master progress
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
                                           // up. ALMOST ALWAYS the wrong
                                           // lever (see Caching): only for
                                           // frequently RE-baking soft
                                           // content; never sharp text/
                                           // hairlines, never bake-once nodes
Element &transition(Transition);           // node default applied to any
                                           // plain-constant prop change
Element &staggerChildren(std::chrono::milliseconds each,
                         Stagger::From = Stagger::From::Start);
                                           // GSAP container stagger: child
                                           // subtrees' animate() entrances
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

*(Where these types LIVE, since 2026-07-29: `Transition`, `ease::`,
`Transitioned<T>`, the `animate()`/`from()`/`to()`/`through()` builders,
`bind()`/`Bound`/`BoundFloat` and `Animatable<T>` are defined in
SigilMotion, `<sigilmotion/Animation.h>`, and re-exported into
`sigil::compose` — every spelling in this manual is unchanged and stays
correct. What compose owns is the RESOLUTION described below: what a
described change means to a node. ROADMAP §37.)*

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
  EXPLICIT: `animate(from(a).to(b), spec)` plays its ramp on mount,
  `animate(through({...}))` plays a waypoint path, and `staggerChildren()`
  cascades a container's entrances — afterwards all behave like
  `with()` (retarget-from-current). `snapshot()`/`measure()` render
  the settled end values.
- **Unmount cancels automatically**: the instance's `ch::Output`s are
  destroyed with it, and Choreograph disconnects a motion when its
  Output dies — removed list rows can't leak motions by construction.
- **Keys carry state**: a keyed node that moves in the tree keeps its
  instance, so mid-flight transitions survive reorders.

### The motion path — `travel()` (2026-07-29)

After Effects' core motion model: a layer's position follows a SPATIAL
path through the comp, and its temporal easing is a separate concern.
`translateX`/`translateY` are two lanes; two lanes describe a POINT, not
a TRAJECTORY, and driving a curve through them means computing two
numbers a frame — the imperative door wearing the declarative one's
clothes.

```cpp
struct MotionPath {
  Shape path;                    // resolved against the PARENT's box
  Animatable<float> t = 0.0f;    // WHERE ALONG it, by arc length
  float lookAhead = 0.0f;        // auto-orient; 0 = leave rotation alone
};
Element &travel(MotionPath along);
```

The float-only ruling is not bent to do it. The lane is `t`, so the
whole `bind()` chain still applies — to the SCHEDULE rather than to the
geometry. The curve supplies the shape, the lane supplies the timing:

```cpp
choreograph::Output<float> phase{0};
ticker.timeline().apply(&phase).then<ch::RampTo>(1.0f, 6.0f);

box().key("comet").rect({0, 0, 14, 14}).fill(gradient(...))
    .travel({.path = shapes::circle(),                      // the shape
             .t = bind(&phase).map(&choreograph::easeInOutQuad)      // the schedule
                              .target(0, 2),                // two laps
             .lookAhead = 0.02f})                           // auto-orient
    .rotate(bind(&phase).target(0, 360));   // …and it still spins on top
```

Six rules, argued in DESIGN.md § The motion path:

- **The curve is resolved against the PARENT's box.** A `Shape` is a
  function of a size, and the size that makes a motion path mean
  anything is the FRAME the node moves in — `shapes::circle()` on a
  14 px comet inside a 400 px card is a 400 px orbit, not a 14 px
  twitch. A root node with no parent resolves against its own box,
  which is the canvas.
- **The transform ORIGIN is what rides the curve** — AE's anchor point,
  and already the pivot `rotate`/`scale`/`skew` turn about, so the point
  on the curve is fixed under all of them. Default (0.5, 0.5) rides the
  centre; `transformOrigin(0, 0)` rides the top-left.
- **PRECEDENCE: whatever the path drives, it drives outright.**
  `translateX`/`translateY` are IGNORED while a path is engaged — not
  blended, not an offset. Dropping the path hands the same lanes back,
  live. (A path that is absent, empty, or resolves to no measurable
  length is not engaged at all.)
- **Auto-orient ADDS to `rotate()`.** `lookAhead != 0` adds the angle of
  the chord `position(t + lookAhead) - position(t)` to whatever
  `rotate()` says — so a travelling node can bank AND spin, which is
  what AE does (auto-orient sets the base orientation, the Rotation
  property adds). Negative faces back down the curve. This is the one
  place the 2D port departs from `world::CameraPath`, and it departs
  towards that header's own `rollDeg` rule; the argument is in
  DESIGN.md.
- **WRAP on a closed curve, CLAMP on an open one.** `.target(0, 2)`
  reads as two laps with no extra API; negative `t` runs backwards; an
  open curve parks at its ends and HOLDS the last good chord there, so
  the orientation at the end of an L still points down the last leg.
  Closed means every contour the path resolved to is closed.
- **`t` is a fraction of TOTAL arc length across every contour** — the
  same coordinate `bandPointAt`, `spans::` and `SkTrimPathEffect`
  already speak. There is no `arcLength` flag: `SkContourMeasure`
  measures nothing else, and an `SkPath` has no native parameter to opt
  back to.

Paint-only, like the lanes it outranks: a travelling node never
relayouts, its content picture replays under the new transform, and
`bounds()` keeps reporting the LAID-OUT box. `hitTest` follows the
painted position (paint, bounds and the hit-test inverse share one
transform resolver).

Pruning follows the shape seam it is built from: `.travel({.path =
shapes::circle(), ...})` prunes while its fields hold, a raw
`[](SkSize){...}` never compares equal and keeps the node
conservatively un-pruned — the same contract, and the same cost,
`.shape()` documents.

**A relayout re-shapes the curve and leaves `t` alone.** The node slides
to the same fraction of the NEW curve rather than jumping phase or
freezing on the old one; on a proportional resize the motion looks
identical, just scaled. The arc-length table is cached against the two
inputs that determine it — the `Shape` VALUE and the size it was
resolved at — never behind a dirty flag.

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
change is declared (bindings, `animate()` transitions, `isAnimated()`
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
  tree, so static siblings stay cached while neighbours animate. And it
  partitions by KIND: paint-only volatility (bound/transitioning
  transforms and opacity) keeps the node's own content picture and
  replays it under the live transform; content volatility (fill lerps,
  animated decorations, live materials, `Cache::None`, animated image
  frames) paints live.
- **Size the node to the thing that animates.** A bound opacity or blend
  allocates a `saveLayer` the size of the NODE, not the size of what is
  painted in it — so ten Outputs hung on twenty full-canvas groups cost
  eighteen megapixels of layer per frame to twinkle content covering 2%
  of it. Measured: **18.38 → 7.94 ms** by moving each binding onto its
  own primitive's tight box, same Outputs, same pixels. This is the same
  shape of trap as "a picture is not a pixel cache" — a true statement
  that reads as free, and is free only per pixel of the node's box.
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
default **on CPU raster only — OFF by default on Graphite/GPU**, where
the per-node profiler measures op *recording*, not GPU execution, and
promotion measured inert (ROADMAP §29; `--no-promotion` for A/B).
`Composer::setAutoTexturePromotion(false)` globally,
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

> **The upright rule is about the ARITHMETIC, not about resampling, and
> the difference matters because the obvious relaxation is wrong.** It
> looks as though a device bake must be exact at any angle: it
> concatenates the full matrix into the layer and blits at an integer
> offset, so nothing resamples. Measured on a shader-filled 220 px box,
> promoted against the identical unpromoted render:
>
> | | differing pixels |
> |---|---|
> | `rotate(0)`, inside the canvas | 0 |
> | `rotate(0)`, bounds overflow the canvas | 0 |
> | `rotate(30)`, inside the canvas | 2, Δ1 |
> | `rotate(45)`, inside the canvas | 5, Δ1 |
> | `rotate(45)`, bounds overflow the canvas | **1157, Δ up to 40** |
>
> A shader's local coordinates come back through the INVERSE of the CTM,
> and the layer's CTM differs from the canvas's by an integer device
> translation. Inverting a rotation maps that integer through irrational
> entries and the cancellation is only approximate; an axis-aligned
> matrix maps it through ±1 and 0 and cancels exactly. Separately, a bake
> rect larger than the device clip gives Skia a different clip to
> rasterize antialiased edges against, which is worth tens of levels.
>
> A *constant* rotation is therefore refused just as an animated one is —
> which is expensive (one study lost 24.5 ms of a 29.9 ms frame to a
> scroll band tilted a constant −0.42°, because a scroll does not lie
> square). The remedy is `.cache(Cache::Texture)`: 1 LSB is not
> agreement, and an author who types it has accepted the trade. Two calls
> took that study from 29.92 to 5.81 ms with no visible change.

**A node's OWN paint can be baked apart from its volatile children.**
Volatility is declared per NODE, so a static full-canvas ground plane
carrying one moving child shares the child's verdict and loses — the
whole plane re-rasterizes every frame in order to redraw the child on
top of it. When the node's own paint is provably static and only its
children move, the composer bakes the own layer and paints the children
live over the blit. It reports as `CacheState::SplitOwn` /
`Promotion::SplitBaked`, and `--bench` prints
`[OWN PAINT baked, live children over the blit]`.

Nothing is required of you; it is measured on the node's OWN cost, so a
cheap plane is not baked merely for carrying an expensive child. Two
things are worth knowing:

- **A child with a non-srcOver blend or a backdrop filter is fine here**,
  unlike whole-subtree promotion where it is fatal. The blit lands
  *before* the children, so the child resolves against exactly the
  destination it would have found anyway.
- **A layer effect on the node is the one disqualifier** among the
  wrappers — a filter applies to the union of own paint and children.
  `clip()` and a whole-node `mask()` are fine: both halves take the
  identical clip.

The authoring workaround is still worth knowing for the cases this
cannot take (a rotated node, an effect): **lift the moving child out
into a sibling** so the expensive plane is a static leaf and promotes on
its own. It costs you the containment relationship that made you nest it.

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
`Volatile` / `Composited` / `Transformed` / `Filtered` / `ReadsBackdrop`
/ `TooBig` / `SplitBaked`, and `ComposeSketch --bench` prints it under
every expensive node. `Composer::promotionReason()` turns it into a
phrase. Every refusal is individually correct and individually
invisible, which is precisely how sixteen studies shipped over the gate.

**And it reports ALL of them, not just the first.** `promotion` is a
first-match verdict, so a node that is both volatile and clipped used to
report only `Volatile` — and an author who lifted the moving child out
then met a second refusal nobody had mentioned, and a third behind that.
`genesis_fire`'s ground plane has exactly three at once.
`NodeCost::refusals` is a bitmask and `NodeCost::refused(Promotion)`
reads it:

```cpp
for (const auto &row : composer.profile()) {
  if (row.cacheState != Composer::CacheState::Live) continue;
  printf("%s: %s\n", row.label.c_str(),
         Composer::promotionReason(row.promotion));       // the first
  for (auto also : {Composer::Promotion::Volatile,        // …and the rest
                    Composer::Promotion::Filtered,
                    Composer::Promotion::ReadsBackdrop})
    if (also != row.promotion && row.refused(also))
      printf("      …and: %s\n", Composer::promotionReason(also));
}
```

`promotion` stays the primary outcome and is DERIVED from the mask by
first-match, so the two cannot disagree. A node with nothing wrong with
it reports `refusals == 0`.

> **Writing a cache test?** A static node under a *cacheable* parent is
> painted exactly once, into the parent's recording, and never visited
> again — so it never appears in the profile and assertions about it
> pass vacuously. Put `.cache(Cache::None)` on the wrapper. (This is
> also the real shape of the corpus: these leaves sit under a `stack()`
> with animated siblings, which is why their cost is paid every frame.)

## Kernel, util, extensions — where things live

Three layers keep the library lean and the call sites short:

- **Kernel** (`<sigilcompose/Compose.h>`): elements/components/
  Composer, flex + `stack()`, stacking paint, `Fill::color/shader`,
  text/image/custom leaves, `key`/`memo`, `Animatable` + `Transition`,
  `env::` (the inherited value — below), automatic caching. Complete
  mental model, smallest possible surface.
- **Util** (`<sigilcompose/Util.h>`, depends only on the kernel —
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
  `<sigilcompose/Layouts.h>` (**seven** layout schemes: `Radial`,
  `AlongPath`, `ModularGrid`, `Diagonal`, `StickerSlot`,
  `BaselineGrid` — the only consumer of `LayoutInput::childBaselines`,
  and the reason that channel exists — and `Scatter`),
  `<sigilcompose/Routers.h>`
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

### `env::` — an inherited value, read where a component is COMPOSED

SwiftUI's `Environment` / React's context, for a library whose describe
phase is an ordinary C++ call tree. Passing `const Theme&` is idiomatic
for your own components; this exists for the ones you did not write — a
`console::`, a decoration four levels down — which would otherwise have
to be handed their colours by whoever composed them.

```cpp
Element scene(const Palette &p) {
  env::Provide<Palette> theme(p);        // binds for THIS SCOPE
  return box().child(panel());           // panel() -> … -> chip()
}

Element chip() {                          // handed nothing
  const Palette c = env::inheritedOr(Palette{});   // or inherited<Palette>()
  return box().width(20).height(20).fill(Fill::color(c.surface));
}
```

| Call | Means |
| --- | --- |
| `env::Provide<T> g(value)` | bind `T` for the scope of `g`. RAII, LIFO; an inner one shadows, other types are unaffected. Not copyable — it is a scope. |
| `env::inherited<T>()` | the nearest binding, or **nullptr** — the cue to use your own default, like a React context's default value |
| `env::inheritedOr<T>(fallback)` | the value, or `fallback` |
| `env::bound<T>()` | is one in scope? |

**Bindings are keyed by C++ TYPE, and there is no library-wide `Theme`.**
The key a library component uses is its own props type — `console::Style`,
which is why `console::console(ring)` (no style argument) is themed by
whoever composed it. A design-token layer is a header this library
refused twice (archive/EXTRACT.md §4.7); what is shipped is the channel.

**Three rules, and the second is the one that bites.**

1. **Reads happen during DESCRIBE, and that is why it is free.** The
   value lands in the reading node's own props, so the tree the Composer
   sees is environment-independent and `propsEqual` is already the exact
   dependency tracker: a theme change repatches the nodes whose props
   moved and nothing else; an unchanged theme prunes normally. Pinned in
   `ComposeEnv.*` — through four container levels, a colour change costs
   `patchedNodes == 1`.
2. **A callable the KERNEL invokes sees no scope.** `memo` is handled for
   you — it captures the ambient bindings, compares them alongside its
   props, and re-establishes them around the deferred call, so a memo can
   never serve a stale theme. Everything else that defers (a
   `ContourWalk` stamp, a `custom()` program, anything you store and call
   later) runs at derive or paint time with an empty stack and
   `inherited<T>()` returns null: **capture what such a lambda needs by
   value at the call site**, where the scope still exists.
3. **An inherited type is a comparable VALUE, and its equality means
   "describes to the same props".** Structural and exact — colours
   bitwise, typefaces by pointer — never perceptual. Materialise
   derivations INTO it: a theme carrying a `std::function` rule is
   incomparable and makes every memo below it a permanent miss. Run the
   derivation, store the colours.

A theme is a *describe-path* value by this design. If one property
genuinely must move at 60 Hz, that is still the bind path (a bound
`Fill`, a live `Material` uniform) — and it is priced: the CDE study
measured 40 bound `Fill` Outputs at 0.33 ms/frame steady against
0.033 ms as plain values. Bind the one thing that scrubs, not the theme.

## What the study program added

Everything in this section came out of rebuilding real artefacts and
finding the library could not say something. `ROADMAP.md` carries the
full list with its citation counts; this is the surface.

```cpp
// ---- bindings you can shape ----
bind(&out).source(lo,hi).map(ease).quantize(n).scale(s).offset(o)
          .target(lo,hi).invert().clamp(lo,hi)   // any float property
          // (from/to named these stages until R3 deleted them)
fill(&fillOutput)                            // ch::Output<Fill>, live

// ---- paint order ----
Element &overlay(Decoration, std::string name = {});  // over the fill,
                                // UNDER content and children
Element &sampling(SkSamplingOptions);        // image leaves; kNearest
Slice::filter, Pattern::sampling             // the same knob on those two

// ---- masks (the appearance-gating family — see "The masking family") ----
Element &mask(Gate with);                 // everything this node paints
Element &mask(Parts what, Gate with);     // …or some of it; stacked masks
                                          // INTERSECT, each with its own
                                          // animation
namespace parts { all, marks, surface, content, children, named(label) }
namespace by    { spans(Spans), edge(angleDeg, fraction),
                  shape(Region), outside(Region), alpha(Material) }
// Region: own / rect / oval / path — a COMPARABLE value, never a callable.
decorations::wash(Material, SkBlendMode, amount)   // material-valued
                                                   // decoration; comparable
compose::metrics(style, fonts)  -> {ascent, descent, capHeight, xHeight,
                                    lineHeight, capSlack()}

// ---- text on a path ----
TextPath{ .path, .at, .align, .offset, .autoFlip, .orient }
// .orient = Tangent  (running lettering — glyphs follow the curve)
//         | Radial   (type radiating like a spoke: a dial, a compass
//                     rose, an astrolabe limb)
//         | Upright  (glyphs stay LEVEL wherever they land — a calendar
//                     ring, a modern gauge; neither of the other two can
//                     express it)
// EVERY contour of the path is walked, in order, as one arc-length
// coordinate. `path` is a Shape, so TextPath compares structurally and a
// run on a comparable baseline PRUNES — 72 radial labels used to
// re-record every render() because this operator could not exist (§10e).
// A raw-callable baseline keeps the old never-prunable behaviour.

// ---- shapes ----
shapes::circle()  shapes::annulus(innerRatio)  shapes::sector(a, sweep, inner)
shapes::inset(px, Decoration)  shapes::arrow(shaft, head)
shapes::polygon(sides, rotateDeg)            // regular n-gon, inscribed
shapes::star(points, innerRatio, waist)      // waist bows the arms concave
shapes::chamfered(size, Corner)  shapes::notched(size, depth, Corner)
shapes::onEdges(Edge mask, Decoration)       // per-edge treatment (EdgeSlice)
shapes::parametric(fn, t0, t1, samples, close)   // fn: t -> UNIT (x, y);
                                             // incomparable — the escape hatch
shapes::parametric(key, fn, t0, t1, samples, close) // KEYED: a value; the key
                                             // + window is the identity
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

   A slot's NAME IS ITS KEY — one field — so `.key()` on a slot renames
   the mount and `renderSlot()` on the original name no-ops into a W × 0
   layout. Name a slot once, in `slot()`; both calls warn if you don't.

   **Slots are THE text-content idiom — counters, timers, tickers,
   any label whose STRING changes.** There is no binding path for text
   content: `PropValue` covers floats, colors and fills, never a
   string, so the obvious first hunt — a `text()` overload taking an
   Output — finds nothing (§9). Don't re-describe the whole tree for
   one number either; mount `.child(slot("fps"))` and
   `renderSlot("fps", text(...))` when the value ticks. It is genuinely
   cheap: one leaf patches, the surrounding caches hold.
3. **Multiple Composers** for fully separate systems (scene HUD vs
   poster): each is already a guest in the canvas; cross-composer
   ordering is simply draw-call order.

### Two elements composing on top of each other

New container — the overlap primitive (Flutter `Stack` / CSS grid-area
overlap, made explicit):

```cpp
Element stack();   // children share the stack's box; each positions
                   // itself via alignSelf/inset; painted in
                   // (zIndex, declaration order). SIZING TRAP: stack()
                   // makes children absolute, and absolute children
                   // never contribute intrinsic size (Yoga law) — an
                   // unsized stack is W×0 in flex, 0×0 absolute
                   // (measured, ROADMAP §10d). Give it dims or grow.
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

### Slicing one long bake into tiles

A strip longer than any texture — a marquee, a scrolling ribbon — is
authored as ONE element tree and baked once with `snapshot()`, which has
no size limit because a picture is vector. Cutting it into tile-sized
rasters is a clip and a translate; **there is no windowed bake and none
is needed** (ROADMAP §36 measured a bespoke region bake's ceiling at
zero). What goes wrong is the transform, so `tiles::` owns it:

```cpp
namespace tiles {
enum class Flow { Down, Across };        // which way the run marches
enum class Facing { Forward, Mirrored }; // pre-flip for mirrored sampling

/** The canvas transform bringing tile `index` into view. */
SkMatrix window(SkISize tile, int index, Flow flow = Flow::Down,
                Facing facing = Facing::Forward);

/** The same picture behind a bounding-box hierarchy, so each replay
 *  visits only its tile's ops. Pays from about four tiles on. */
sk_sp<SkPicture> sliceable(const sk_sp<SkPicture> &art);
}
```

```cpp
sk_sp<SkPicture> strip = tiles::sliceable(snapshot(box().child(art), fonts));
for (int k = 0; k < count; ++k) {
  SkAutoCanvasRestore restore(canvas, true);
  canvas->clear(SK_ColorTRANSPARENT);
  canvas->concat(tiles::window(tileSize, k, tiles::Flow::Down,
                               tiles::Facing::Mirrored));
  canvas->drawPicture(strip);
}
```

Two rules the door encodes rather than documents. **Author the strip in
the tiles' orientation** — a `column()` for tall tiles, a `row()` for
wide ones. `Flow` deliberately offers no transposing slice: a transpose
has determinant -1 and composes with whatever mirroring the consumer's
own sampling applies, at which point the bookkeeping is local to
neither side. And **`Facing` describes the CONSUMER, not the picture**:
`Mirrored` pre-flips across the strip so a surface that samples
backwards reads it the right way round. The surface's own bounds are
the clip, so neighbouring tiles share boundary texels and the seams
vanish.

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

`<sigilcompose/Shapes.h>` generators are comparable `ShapeScheme`
values (params + `path(SkSize)` + `==`), so any element can BE the
shape — fill, clip, every outline-following decoration and `hitTest()`
all trace it — and the node PRUNES (§3). Every generator is also
CALLABLE over a size, so it still converts to an `OutlineFn` wherever a
raw path-over-size function is wanted. The parametric family
(`lissajous`, `harmonograph`, `rose`, `spiral`, `trochoid`) carries its
identity in its parameters; the raw `parametric(fn, …)` cannot compare
unless you KEY it (`parametric("orbit-a", fn, …)` — the key plus the
sampling window is the identity, on the author's contract that one key
names one curve). Two easy ones to miss:

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
- `patterns::grain(freq, octaves, seed, contrast, stretch)` is the
  LUMINANCE one — equal channels, so a blend mode reads as light. Paper
  tooth, film grain, stone veining, worn metal, dither: this one.

  It has **five** parameters, not three, and the last two carry the
  interesting behaviour. `contrast` steepens the ramp around mid-grey
  (a tooth becomes a speckle). `stretch` is the anisotropy — it
  MULTIPLIES the y frequency, so brushed metal and rain streaks are one
  number, and **a large value aliases**: past roughly 8 the y period
  approaches the pixel and the field turns into a moiré that looks like
  a bug in the blend rather than in the noise. Drop `frequency` as you
  raise `stretch`.

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
        // Optional `bool isAnimated() const` → node repaints per frame —
        // the single declared-volatility rule shared by decorations,
        // effects, and bound properties alike.

Element &background(Decoration);   // below the FILL, stackable (the
Element &foreground(Decoration);   // type-erased Decoration wraps any
                                   // DecorationScheme); above children
```

**Primitives at the seam, a vocabulary on the shelf.** Concrete
treatments (dashes, stamps, 9-slice frames) are *data* over four
general primitives — the SEAM stops here. The Brush engine
(Brushes/Lines/LayerStyles/Patterns, below) is the first-class
vocabulary built *over* these primitives — a later, deliberate
decision (lines as expressive as fills), not a widening of the seam:

```cpp
// 1. Fill — paint anything. Color and gradients are conveniences; the
//    general case is any SkShader, and SkSL runtime effects make that
//    "any function of position (and time)". Fills serve backgrounds,
//    stroke paint, and glyph paint alike.
Fill::shader(sk_sp<SkShader>);   // incl. SkRuntimeEffect-built shaders

// 2. PathFormat — format any stroke. A stroke is the outline path put
//    through ONE path transform (dash intervals, 1D path stamping, or
//    any SkPathEffect of your own) and painted with a Fill or a
//    Material. "Dashed border" is a two-number PathFormat, not a type.
//
//    NOTE the field names. They are `strokeFill` and `effect`, not
//    `paint` and `effects` — and `effect` is SINGULAR because the type
//    holds one SkPathEffect, not a chain. Compose your own chain with
//    SkPathEffect::MakeCompose and hand it over whole.
PathFormat{.width = 3.0f, .strokeFill = Fill::color(kInk)};
PathFormat{.width = 1.0f, .strokeFill = ink, .dashIntervals = {4, 3}};
PathFormat{.width = 2.0f, .strokeFill = ink,
           .cap = SkPaint::kRound_Cap,      // default is kButt_Cap
           .join = SkPaint::kRound_Join};   // default is kMiter_Join
PathFormat{.width = 2.0f, .strokeMaterial = brass};  // supersedes strokeFill
PathFormat{.width = 1.0f, .strokeFill = ink,
           .stampPath = leaf, .stampAdvance = 12.0f}; // vines, chains
PathFormat{.width = 1.0f, .strokeFill = ink,
           .effect = SkDiscretePathEffect::Make(6, 2)}; // overrides both
// `util::stroke(width, fill, align)` is the short spelling of the first.

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
// The field is `position`, not `pos` — and `fraction` is PER CONTOUR,
// which matters the moment an outline has more than one (a ring with a
// hole, an edge set, a clipped trajectory). `distance` is the only
// coordinate that runs monotonically across the whole walk. It is the
// same trap the `Profile` seam's px key exists for: a law keyed to
// `fraction` under a mask is handed the REVEALED contour and slides
// as the reveal grows — identical in a still, divergent only in motion.
struct PathSample {
  SkPoint position;
  SkVector tangent;
  float distance = 0.0f;   // px along the whole walk
  float fraction = 0.0f;   // 0..1 WITHIN ITS OWN CONTOUR
};
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
`Cache::Picture` recording; ones declaring `isAnimated()` — or carrying
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
  static Effect directionalBlur(float sigma,      // the smear: sigma ALONG
                                float angleDeg,   // the axis at angleDeg,
                                float across = 0);// `across` perpendicular
  static Effect blur(Material sigmaMap,           // the blur whose sigma is
                     float maxSigma);             // a FUNCTION OF POSITION
  Effect &child(std::string name, Material src);  // a `uniform shader` slot
};                                       // optional isAnimated()
// `isAnimated()` is THE word — everywhere a scheme, an effect or a
// Material declares volatility. R3 deleted the other four spellings
// (`animated()`, R1's `animates()`, `Material::isLive()`, and the
// node-level "volatile"). The `is` prefix is what makes it read as a
// query: a cold read of `animates()` asks "animates WHAT?", and there is
// no setter to confuse it with (ROADMAP §33 ruling 13).

Element &effect(Effect);    // filters the node's own rendered layer
Element &backdrop(Effect);  // filters what's already painted beneath
                            // the node's bounds, then paints the node
```

`directionalBlur` is not a third primitive — it is a NAMED COMPOSITION
of the first one (ROADMAP §43.7's separate filing: four sketch sites
hand-built the same anisotropic `Blur`, one faked an animated version
with gradient ramps). At an axis-aligned angle it *is*
`SkImageFilters::Blur(x, y)` — bit-identical, which is what let the
four sites port unchanged — and at any other angle it is the rotate →
`Blur` → unrotate sandwich of existing filters; no new SkSL anywhere.
What the name buys over writing the chain yourself: a comparable
RECIPE, so a re-described equal `directionalBlur` prunes where a raw
`filter()` can only compare by pointer, and the named parameters
`"sigma"` / `"angle"` / `"across"` accept `.uniform(name, &output)` —
an animated smear angle rides the same live channel as a shader
uniform (an unknown name warns and is ignored, Material's guardrail).
It is a spatial filter, not motion blur — motion blur itself is
refused, §43.7.

`Effect::child(name, Material)` is `Material::child` on this seam, and
that is the whole of it: same name, same warn-and-ignore guardrails,
same tier inheritance (a live child makes the effect `isAnimated()`, by
calling Material's own recursion), same prune-signature participation.
`Effect::shader` fills exactly ONE child — `"content"`, the node's
rendered layer — so a second declared `uniform shader` had nothing to
bind it; now a Material fills it, resolved against THIS node's box, so
unit-space authoring (`linearUnit`, `glowUnit`) works here as it does on
a fill. A `filter()` has no child to fill, exactly as it has no uniform.

`Effect::blur(sigmaMap, maxSigma)` is what that channel is FOR
(ROADMAP §19: a depth-of-field falloff, a lens edge, a tube's
curvature had no spelling): the sigma map is a Material read as a
NUMBER — red channel × `maxSigma` is the radius at that pixel — so the
whole falloff is one unit-space ramp. `"maxSigma"` binds on the same
live channel; `child("sigma", other)` re-aims the map. HOW the library
spends that is deliberately not in the signature: a variable-sigma SkSL
kernel is not separable and must size its loop for the worst radius in
the node, and MEASURED (compose_bench, `VaryingBlur` arms, Release) it
costs 15× more when sigma quadruples where this costs 1.3×:

| arm (varying sigma)     | CPU raster 96² | Graphite 256² |
|-------------------------|---------------:|--------------:|
| pyramid, σmax 6         |       0.80 ms  |      0.66 ms  |
| naive kernel, σmax 6    |        167 ms  |      1.95 ms  |
| pyramid, σmax 24        |       1.04 ms  |      0.84 ms  |
| naive kernel, σmax 24   |       2521 ms  |      17.1 ms  |
| constant σ 24 (no vary) |       0.45 ms  |      0.60 ms  |

The last row is the floor: giving up on varying the parameter. The
feature costs ~2.3× that on CPU and ~1.4× on the GPU, against 20–2400×
for the kernel that produces the same picture by hand.

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
> full-frame effect at all: use `LayeredBrush` (an additive
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
    .stroke(PathFormat{.width = 4, .strokeFill = Fill::shader(flowFieldSkSL)});

// The component that IS a line: a path threaded through an ordered run
// of anchors (normalized points on keyed nodes' bounds — a transit
// line through its stations), re-routed whenever an anchored node
// moves. A span reveal makes it draw itself.
rail({{"a", {1, .5f}}, {"hub", {.5f, .5f}}, {"b", {0, .5f}}},
     routers::octilinear())
    .stroke(spans::upTo(animate(to(1.0f), {800ms})), brush)
    .stroke(lines::cased(3, ink, 5));
```

Stock routers (`<sigilcompose/Routers.h>`): pairwise `straight()`,
`orthogonal()`, `arc(bulge)`; rail `polyline()`, `octilinear()`,
`orbit(center)`, `manhattan(Bend, cornerRadius, chamferCut)` — and
`fromPairwise(Router)` adapts any pairwise router to `rail()`, stitching
the legs into one contour. The manhattan family (ROADMAP §8) adds what a
circuit graph needs: `Bend::MidX` is the stock Z, `Bend::HFirst`/`VFirst`
the two Ls (bend at the target/source column); collinear anchor runs
collapse to single segments (the zero-argument `orthogonal()` keeps its
degenerate verbs — frozen behavior); `chamferCut` cuts corners at 45°
where `cornerRadius` rounds them (`routers::chamfer(path, cut)` is the
treatment as a function, `kit::brush::shapers::chamfered(cut)` the same
cut for any brush pipeline).

#### The derive family — one name, six spellings, four shared laws

Everything above is ONE mechanism, and until §33 ruling 11 it had no
collective name — which is the audit's item 8 (six spellings, ~55 total
uses; the low churn *is* the symptom). The family is now gathered under
`derive::`, additively — nothing moved, and every existing spelling
still compiles:

| Member | What it borrows | Taught spelling |
| --- | --- | --- |
| flow text around a node | that node's resolved outline | `derive::flowAround(el, key, margin)` — or the method, which chains |
| a relationship between two nodes | both nodes' resolved boxes | `derive::connector(a, b[, router])` |
| a line through many nodes | each anchor's resolved box | `derive::rail(anchors[, router])` |
| a band's spine | a keyed element's resolved path | `band(derive::around(key), across(px))` |
| a stroke pass's gap | a keyed element's resolved box | `.stroke(spans::fit(key, margin), what)` |
| a weave strand's path | a keyed element's resolved path | `brush::Strand{strand::from(key), ink}` |

The last two keep their own concept namespaces (`spans::`, `strand::`)
because that is where an author is already looking; what `derive::`
gathers is the family's identity and its laws.

**The four laws they share** — one flat edge store, walked once per
render:

1. **An unknown key is SILENT.** `flowAround("typo")`,
   `spans::fit("typo")`, `around("typo")`, a connector to a node not in
   the tree — every one resolves to nothing and draws nothing. No
   diagnostic, by precedent and consistency: a warning would have to be
   the whole family's at once.
2. **ONE second pass, cycle-guarded.** Backward influence inside a frame
   is this declared exception and nothing else. A borrow that would close
   a cycle is dropped, not chased.
3. **The answer can lag one frame** where the borrowed node's own
   geometry settles during that layout. (The second `frame()` in the
   `fit`/`flowAround` tests is that, not a test artefact.)
4. **Flat, not recursive.** Routed nodes and flowing text are flat lists
   in tree order plus a back-index from anchor key to routes, so a tree
   with no derived content pays nothing and `routesAt(key)` answers in
   O(routes at that node).

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
explicitly, whenever its content is static. The rule of thumb is area
of **painted** pixels, not node box — a swatch does not care, a
full-canvas wash does, and a mostly-empty node is the exception that
proves it: texture-caching a sparse list REGRESSED 48% (blitting empty
pixels costs more than skipping them — STRESS_TESTS phase 1; leave
sparse regions on Auto).

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
Material::blend({{base, kSrcOver},                      // §5: layer STRENGTH —
    {grain.amount(0.30f), SkBlendMode::kSoftLight}});   // Photoshop opacity
                                                        // (composite, then mix
                                                        // back), recipe-equal
material.uniform("uGlow", &output);  // bind a ch::Output → material is LIVE
material.quantizeTime(6.0f);         // step the injected uTime at 6 Hz
material.child("uPalette", Material::image(lut, …));  // a SECOND source
```

**The child slot — two sources in one shader (§10f).** An `sksl()`
effect that declares `uniform shader NAME;` gets it filled with another
whole Material:

```cpp
// index texture read through a palette LUT — paletted shading, and the
// X-COM ramp arithmetic ((src & 0x0F) + shade) that has no other spelling
Material::sksl(paletteFx, {{"uShade", 1.0f}})
    .child("uIndex",   Material::image(indexTex, kClamp, kClamp,
                                       SkMatrix::Scale(20, 20),
                                       SkSamplingOptions(kNearest)))
    .child("uPalette", Material::image(lut, kClamp, kClamp, SkMatrix::I(),
                                       SkSamplingOptions(kNearest)));
```

`Effect::filter` has always had ONE child (`content`, the node's
already-painted layer, which is why a LUT over painted content always
worked); this is the door for the sources the node has NOT painted.
Any Material is a legal child, sksl ones included, and the whole tree
still compiles to one shader. Three rules:

- **kNearest for anything whose pixel VALUES are data.** An index
  texture sampled at kLinear is a blend of two unrelated palette
  entries.
- **The tier is inherited.** A live child makes the parent live; a
  child reading `uResolution` propagates the geometry tier — and it
  reads the PARENT NODE's box, because there is one box.
- **The child rides the prune signature.** Two materials with
  different children never compare equal; two with identical ones
  prune. (Image children compare by image POINTER, as everywhere else
  — hold the decoded LUT, do not re-decode it per describe.)

An undeclared child name warns and is ignored, exactly like an
undeclared uniform.

Effects carry the same live contract (§11):

```cpp
Effect::shader(fx, {{"uThreshold", 0.6f}})   // constants, as ever
    .uniform("uPhase", &phase);              // BOUND: resolved per paint,
                                             // node repaints while attached
Effect::directionalBlur(18, 0)               // same channel, same words:
    .uniform("angle", &angle);               // the sandwich rebuilds per
                                             // paint from the bound value
Effect::blur(focalRamp, 0)                   // …and a rack focus is the
    .uniform("maxSigma", &focus);            // SAME channel again (§19)
```

A parameter Material is live the other way round too: `Effect::blur(map,
14)` where `map` carries a bound uniform makes the EFFECT
`isAnimated()` — tier inheritance, so a bake can never sample the
parameter once and freeze it (the §41 frozen-matte failure class,
forbidden by construction here).

A static shader effect compares by RECIPE (runtime-effect pointer +
constant uniforms), so holding one `SkRuntimeEffect` process-wide and
re-describing prunes — a static `directionalBlur` compares by its
recipe too; a live effect never prunes, like a live material.
`then()` chains precompose when static and re-compose per paint when a
side is live.

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
- **Live** (a bound `ch::Output` uniform, a LIVE CHILD, or the effect
  reads `uTime`
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

## The stroke grammar — the words, and where a stroke goes

The five words, so the rest of this file can use them precisely
(ROADMAP §33; stage one shipped 2026-07-26):

| Word | Is | Spelled |
| --- | --- | --- |
| **shape** | the region an element occupies | `.shape(path)` |
| **line** | an element whose shape is an open path | `.shape(hline())` |
| **band** | a shape derived around a spine | `band(spine, across(px))` |
| **stroke** | the slot that dresses a boundary | `.stroke([where,] what)` |
| **brush** | what paints | `Brush`, `PathFormat`, `LayeredBrush`… |

"Frame" and "border" are not concepts — they are strokes of a boundary.
"Bounding box" is query-side vocabulary (`Composer::bounds`), never a
shape.

**`outline()` is now `shape()`.** The old name read as a drawn LINE —
the thing `stroke()` does — and the call sites showed it:
`.outline(chevron()).fill(ramp)` filled an "outline", and
`.outline(shape).stroke(brush)` put two halves of one idea under one
word. `outline()` was DELETED in R3. `PaintContext::outline` keeps its
name, because there it
genuinely IS the path a decoration traces, and so does
`ksp::Conic::outline` and anything else that means a path rather than an
element's region.

### `.stroke(where, what[, name])`

```cpp
.stroke(brush)                                   // the whole boundary
.stroke(spans::corners(18), stroke(2, ink))      // reticle corners
.stroke(spans::edges(14), stroke(1, ink))        // a rule with open corners
.stroke(spans::upTo(animate(from(0.f).to(1.f), {600ms})), wire)  // draws on
.stroke(spans::fit("title", 6), glow, "gap")     // sized from keyed content
.stroke(spans::rest(), stroke(1, ink))           // …and everything else
```

`where` is a `spans::` value; `|` unions them. Repeated calls **append**,
in declaration order — the decoration law.

**Fraction 0 is the BOTTOM-LEFT corner**, and the boundary runs UP the
left edge from there — `addRRect`'s own start index, inherited. So
`spans::upTo(0.25)` on a square claims its LEFT edge, not its top one,
and `spans::at(1, 4)` is the top. Nothing stated this until R2 wrote two
tests against "top-left, clockwise" and watched them fail; every earlier
assertion in the suite happened to be symmetric under the choice. A
custom `shape()` seams wherever its own path starts.

**Ordering, precisely:** the unqualified strokes paint FIRST (they are
foregrounds and share that list), then the span passes in their own
declaration order. Within each group declaration order holds; between the
groups the unqualified ones are underneath. Interleaving the two by call
order is not expressible today — if a span pass has to sit *under* a
whole-boundary one, make the whole-boundary one a span pass too
(`spans::every(1)`) so both are in one list.

The factories:

| Factory | Claims |
| --- | --- |
| `spans::range(a, b)` | `[a, b]` of the boundary's total arc length (clamped) |
| `spans::wrap(a, b)` | the same window on a CYCLE — wraps the seam when `a > b` |
| `spans::upTo(t)` | `range(0, t)` — **the reveal** |
| `spans::corners(arm[, angleDeg])` | `arm` px either side of every tangent break |
| `spans::edges(arm[, angleDeg])` | the runs between the breaks |
| `spans::every(n[, duty])` | `n` equal slots, each claiming its leading `duty` |
| `spans::at(i, n)` | one slot of `n` |
| `spans::fit(key[, margin])` | the run a keyed element covers (derive phase) |
| `spans::rest()` | whatever the other claiming passes left |
| `spans::rest("name")` | one named pass's complement (may overlay) |

**One boundary, one mark.** Span-qualified passes CLAIM, and two claims
that overlap are a mistake with no sensible rendering, so the library
says so out loud, naming both passes and the shared run. To layer two
marks on one run, make them ONE pass with a composite brush
(`Brush{}.layer(a).layer(b)`, or a `LayeredBrush`) — that is the ruled
answer to double and triple lines everywhere, and it is why the reveal
below is a property of a pass rather than of a node.

**The unqualified `.stroke(what)` does not claim.** It overlays the whole
boundary, so stacked strokes (a halo under a keyline) stay legal and
never collide. Naming a `where` is what turns a pass into a claim.

`name` is LOCAL to the element — inspection, and the `rest("name")`
reference. It is never a query key; a second identity system is exactly
what the query side refuses.

**Reveals are span animation.** `spans::upTo(t)` takes any
`Animatable<float>` — a constant, an `animate(...)` entrance, or a bound
Output — so one spelling reveals every brush kind. The NODE-level
spelling of the same idea is `.mask(by::spans(where))` (see "The masking
family"), and the two are one machine — stated as law:

```cpp
.stroke(where, what, name)
    == .stroke(what, name).mask(parts::named(name), by::spans(where))
```

identical pixels. The one thing the pass form does that the sugar does
not is CLAIM its run and join the no-overlap ledger. (`Element::trim()`
was the older node-level spelling; R4 deleted it — the masking family is
its replacement.)

**The same slot in the other z-half — `.background(where, what[, name])`.**
Identical to `.stroke(where, what)` in every respect except where the mark
lands: it paints with the backgrounds, BENEATH the fill and therefore
beneath the content and the children.

```cpp
.background(spans::edges(14), stroke(3, shadowInk))  // under the fill
.stroke(spans::corners(18), stroke(2, ink))          // over the children
```

It is ONE claim ledger across both halves, and deliberately so: the
passes append into one list in declaration order, the no-overlap law
reads across both, and `rest()` complements both — a boundary does not
have two of itself. `rest("name")` can name a pass in either half.

**Marching ants — `spans::wrap`.** The seam-crossing window, the one
thing the deleted `trim()` needed a whole mode flag for:

```cpp
.stroke(spans::wrap(bind(&phase), bind(&phase).offset(0.25f)), ants)
```

`wrap(a, b)` reads the boundary as a cycle: when `a` wraps past `b` the
term claims `[a,1]` AND `[0,b]` — one term, two runs, stitched into ONE
contour so caps and additive brushes never double-hit at the seam.
`b - a <= 0` claims nothing and `>= 1` claims the whole boundary, read
from the RAW endpoints so a window driven past 1.0 keeps its length.
`range()` deliberately did NOT learn to wrap — `range(0.9, 0.1)` already
means something, and a reader auditing a claim conflict needs the call
site to say the term is cyclic.

**Sliding a claim — `.offset(by)`.** Shifting a window is normally
arithmetic on its two ENDPOINTS: `bind(&p)` and `bind(&p).offset(w)` are
two shaped views of ONE Output, and that is the marching-ants spelling
above. `Spans::offset(by)` is for the case that cannot reach — the ends
driven by one Output and the POSITION by another:

```cpp
.stroke(spans::wrap(&start, &end).offset(&drift), ants)
```

`by` is added to both endpoints of every Range/Wrap term before the
interval is read, takes the full `Animatable` treatment like the
endpoints, and participates in equality like them (a claim that only
slides must not prune to its first frame). It is the direct translation
of `trim(begin, end, offset)`'s third argument. Set on the whole value
rather than one term, because it describes the claim and not one interval
of it; terms that read no interval ignore it.

**`spans::corners()` supersedes four spellings.** `decorations::brackets`
/ `decorations::gappedRule` and `lines::cornerBrackets` /
`lines::cornerGaps` are one capability under four names (the audit's item
10); they all still work, and they now all run the same scan the spans
do. `cornerAngleDeg`'s surprise is unchanged and documented below.

A corner sitting ON the boundary's seam (fraction 0) resolves as TWO
adjacent intervals — the same split a seam-crossing trim window takes.
The pieces butt exactly, so nothing shows.

### `band(spine, across(px))`

```cpp
band(shapes::circle(), across(22)).inward().fill(brass)
band(around("dial"), across(14)).stroke(spans::edges(6), rule)
band(spine, across(myTaper)).centered().child(text(…))
```

A band is an ordinary element — it lays out, hosts children, fills,
clips and takes stroke passes like any other shape. What it adds is a
DERIVED shape: it owns an `(along, across)` space over its spine, `along`
a fraction of arc length and `across` px on the normal.
`bandPointAt(spine, along, across)` is that space, addressable.

**A band does NOT hit-test as its shape.** `hitTest` consults `shapeFn`,
and a band's silhouette is derived instead, so a band hits as its layout
box. Fixing that is the pinned organic-shape hit-testing pass (ROADMAP
§33, pinned pass 4), not stage-one work.

**Positive `across` is to the LEFT of travel** — in screen space (y down)
that is OUTSIDE a clockwise path, which is SkPath's own direction for
rects and circles, so `.outward()` exits the shape.

**THIS IS THE ONE CONVENTION** — the whole library, no exceptions, stated
once in DESIGN.md. `lines::` used to be the minority that meant the other
side (`offsetAlong`, `Rail::offset`, `Line::offset`, and the kit's
`shapers::Offset` through them). R3 flipped all of them, renaming each so
the compiler found every call site and negating every argument so no
picture moved (ROADMAP §33 ruling 5). A `Profile` now means the same side
wherever it is read.

The spine is a `Shape`, like `shape()`'s value: a comparable generator
(any `shapes::` value) prunes on its own; a raw callable is the escape
hatch — `memo()` such a node (or hold one Shape value stable) to prune
it while its size and inputs are unchanged.

Formation is explicit — `.centered()` (default), `.outward()`,
`.inward()`. There is no defensible default beyond "both", and on a
closed clockwise path (SkPath's own rect and circle direction) outward
is outside.

The spine is guide DATA, never an element. **Paths are data; only
elements render** — a path participates as an element's shape, as
borrowed geometry (`around(key)`, resolved in the derive phase), or as
pure guide data in no tree (band spines, `TextPath`, `AlongPath`).

### The profile seam

`across()` takes a px constant or a `Profile`: any comparable value with

```cpp
float across(float along) const;   // px LEFT of travel, along ∈ [0,1]
float max() const;                 // REQUIRED
bool operator==(const P &) const;  // REQUIRED (comparable-values law)
```

`max()` is required, and that is the seam's whole point: a varying width
whose reach is unknown can only be clipped silently (the trap the deleted
`Ribbon::widthFn`/`widthMax` pair left open, ROADMAP §25). The library
grows the paint cull by it, so nothing a profile draws is truncated behind
your back.

Core ships the two profiles everything else is measured against —
`strand::self()` (across ≡ 0, the boundary itself) and
`strand::offset(px)` (a parallel; parallels are rails and never cross).

**THE PX KEY — one optional line, and the seam's only mode.** `along` is a
FRACTION of the spine by default. A scheme that declares

```cpp
static constexpr bool alongIsPx = true;   // optional
```

is handed arc-length PX from the spine's start instead, measured on the
contour actually being painted (anchored reveals — upTo or a range
starting at 0; a moving-begin window or wrap re-anchors the px origin
to the revealed piece). Use it whenever the law must not move
under a reveal: a decoration under `mask(by::spans(...))` / a span pass
receives the
REVEALED contour, so a fraction is a fraction of what has been drawn *so
far* and a law keyed to it walks along the mark as it draws — identical in
a still frame, wrong in motion.

The conversion cannot live in your value, which is why it is on the seam:
it needs the length of the contour being sampled, and under a reveal that
is not the length you authored against. Consumers that have measured their
spine (`profileOffset`, a band's rails) call `Profile::acrossAt(along,
lengthPx)`; `Profile::keyedInPx()` answers which key a value uses, and the
key is part of the scheme's TYPE, so two laws that differ only in it can
never compare equal.

The KIT ships the stock shapers, one per way of bending a mark:
`kit::brush::shapers::wave` (smooth, and also readable as a profile),
`zigzag` (the same oscillation with corners), `square` (battlements, the
meander key), `rounded` (soften every corner of the MARK — not
`shapes::rounded`, which rounds a silhouette generator's result),
`jitter` (the rough.js line), `offset` (the rail). Between them they
absorbed every `ops::` struct, which is what let R3 delete that family
and leave `.shaped(value)` as the only way into a brush pipeline.

**Both `offset`s mean the same side.** `strand::offset(px)` and
`kit::brush::shapers::offset(px)` are LEFT of travel, the band's frame,
outside a clockwise path — as are `lines::offsetAcross`,
`lines::Rail::across`, `lines::Line::across`, `Profile::across` and
`TextPath::offset`. They disagreed until R3's sign port.
The oscillating family lives in the kit, per the tier rule. The profile
is SHARED vocabulary: a band's taper, a weave strand's path and the
future ribbon width are one value.

### The brush taxonomy — four kinds, two composites

A brush is what PAINTS. That is the whole vocabulary:

| | Spelled | Was (DELETED in R3) |
| --- | --- | --- |
| **kind** | `brush::solid(w, fill)` / `brush::Solid{…}` | `PathFormat`, `util::stroke` (both still ship) |
| **kind** | `brush::Pattern` — the mark built from CELLS | `brushes::PatternBrush` |
| **kind** | `brush::Scatter` — cells strewn NEAR the mark | `brushes::ScatterBrush` |
| **kind** | `brush::Art` — an element stretched ALONG it | `brushes::ArtBrush` |
| **kind** | `brush::Ribbon` — a variable-width FILLED band | `brushes::Ribbon` |
| **composite** | `brush::layers({a, b, …})` — fixed order, bottom-up | — |
| **composite** | `brush::weave({strands…}, rule)` — per-crossing order | — |

**`namespace brushes` is GONE (R3).** Everything authors spelled in it
lives in `brush::` under ONE name: the kinds above (the `*Brush` suffixes
went with the namespace — a type suffixed with its own scope was the
two-names-for-one-identity defect §22 names), plus
`brush::taper`/`calligraphic`/`ribbon` (Ribbon presets),
`brush::artAlong`, `brush::Placement`/`StampMod`/`StampModFn`,
`brush::CornerArt`/`CornerAlign`, and `brush::Restyled`/`restyle`. The
four `LayeredBrush` presets (`filament`/`circuit`/`rope`/`pulse`) were
PRESETS by the tier rule and did not belong in core under a taught
namespace: they are `kit::brush::presets::` and nothing else.

**`ops::` is now ONE DOOR, and that is deliberate.** The comparable
structs (`Wave`/`Rounded`/`Sketchy`/`Square`/`Offset`), `Brush::op()` and
the `vector<GeometryOp>` per-layer suffix were all deleted: every one of
them has a `kit::brush::shapers::` twin, and `Brush::layer(dec, {shaper…})`
reaches them. What has NO replacement is the raw lambda — a `Shaper` is
comparable by design, so a closure can never be one — so `ops::PathOp`,
`ops::chain` and `ops::debug` survive, reachable through
`brush::restyle(op, decoration)` and nowhere else, documented as a
mechanism and priced as one (it never prunes).

**`brush::ribbon(profile, fill)` is the taught Ribbon constructor.** A
`Ribbon`'s width rides the shared `Profile` seam (`across(along)` + a
REQUIRED `max()` + equality), which does three things the deleted
`widthFn`/`widthMax` pair could not: `bleed()` asks the profile instead of
trusting a second field nobody set, the value COMPARES so the node prunes
(a `widthFn` ribbon's `operator==` ended `&& !widthFn`, so it was unequal
to itself forever), and the band is built by `bandRegion()` — so its rails
go through `profileOffset`, which picks up `lines::offsetAcross`'s
real-vertex corner repair when the law is CONSTANT and takes the sampled
walk when it varies.

`widthStart`/`widthEnd` (linear taper) and `nibAngleDeg` (the calligraphic
nib) are untouched and still take the sampled walk; a default-constructed
`width` means "absent", and they apply exactly as before.

**The port moved pixels, by construction and on purpose.** The two lanes
were never the same drawing, so the corpus's 8 sites were migrated with a
designer reading before/after plates rather than under a byte-identity
gate — see ROADMAP §33's widthFn→Profile note for the plate ledger and for
the px-vs-fraction bridge decision.
`solid` replaces `PathFormat` because "path format" names the
implementation — and `pen` was rejected because it implies calligraphy,
which is a *profile*, not a kind.

Composites take ANY brush, including other composites, so a strand
painted by `layers` and a whole braid used as one strand of a bigger weave
need no new vocabulary.

**`layers` == `weave` with coincident self-strands.** Not an analogy — one
machine. Coincident strands produce no crossings, so the rule never fires
and list order applies everywhere, which is exactly "fixed order,
bottom-up". Both words are kept because they name two author intents (the
`alternate` == `sequence({Over, Under})` precedent). This is why double and
triple lines are `layers` plus offset shapers and **never** element
duplication.

### Strands — where a composite's marks run

```cpp
Strand{strand::self(),        ink}      // on the boundary
Strand{strand::offset(4),     ink}      // parallel to it — never crosses
Strand{kit::profile::wave(6, 40), ink}  // oscillating — THE braid primitive
Strand{strand::from("dial"),  ink}      // a keyed element's resolved path
Strand{strand::path(myPath),  ink}      // authored geometry
```

One strand is one PAIR of `{path, brush}`. Two parallel lists matched by
index was the first shape tried and it reproduced §10d's defect exactly
(add a strand, silently shift every brush).

Two families. **Relative** strands are displacements of the stroked
boundary in its `(along, across)` frame — *the same frame a band owns*, so
positive `across` is LEFT of travel — and any `Profile` is one, including
a custom value handed straight to `.path`. **Absolute** strands bring their
own geometry: `strand::from(key)` borrows through the derive phase (same
flat edge-store walk, same cycle guard as `connector` and `flowAround`),
and `strand::path(p)` is authored (SkPath is comparable, so it prunes).

**With only absolute strands the boundary is an unpainted host** — nothing
runs on it. That is a real and useful shape of composite, not a mistake.

**Crossings are found on strand PATHS, not on painted marks.** A wavy
*mark* — `Brush{}.shaped(kit::brush::shapers::wave(...))` — does not weave,
because the strand's path is still straight and the deviation happens inside
the brush. A wavy *path* — `Strand{kit::profile::wave(...), ink}` — does.
That is the difference between shaping a mark and moving a strand, and it is
why the braid primitive is a profile rather than a shaper.

### Crossings — who passes over whom

**Crossings are DISCOVERED, never authored.** `discoverCrossings()` finds
them by path intersection and numbers them along the boundary. Two things
are deliberately *not* crossings: coincident strands (which is what
`layers` is) and endpoint touches — a rectangle's own corner is two edges
meeting, and counting it would put a knot at every corner of every frame.
The discriminator is transversality: does the other strand pass *through*,
or only touch?

The rule ladder, one comparable value — climb only as far as needed:

```cpp
crossing::alternate()                     // == sequence({Over, Under})
crossing::sequence({Over, Over, Under})   // any repeating pattern
crossing::pairs({{0,1},{1,2},{2,0}})      // dominance; CYCLES are the point
MyRule{}                                  // your value: Order decide(const Crossing&)
```

`Order` reads against `Crossing::a`, which is always the lower strand
index, so `Over` means "strand `a` passes over strand `b`". The default is
list order. A user rule is a comparable value with the seam's one named
member — never a bare lambda, because a rule is read live and an
incomparable one never prunes.

**Pins compose onto the base rule** via `.except(index, order)`. There is
ONE `.crossing` field; pins are not stacked entries.

```cpp
brush::weave(kit::strands::braid(3, 8, 44, ink),
             crossing::alternate().except(7, Order::Under))
```

**Pins are POSITIONAL.** The index is a position in the discovered order,
so a stable *rule* survives a geometry change and a pin does not — move a
strand and pin 7 lands on a different knot. Use rules while a composition
is still moving; use pins only once it is settled and you are correcting
one knot by eye.

**What the repair does, honestly.** For every crossing the rule decides
against list order, the over-strand is repainted through the region where
the two marks overlap — the intersection of the two paths stroked to their
own reach, which is correct at any angle (a disc is not: marks meeting at
12° overlap in a long lens, and a disc sized for the perpendicular case
leaves the under-strand showing straight across the over-strand).

That region is bounded by **the knot's own territory**: each crossing is
repaired only within half the arc distance to its nearest neighbouring
crossing on the tighter of the two strands — measured *around the cycle* on
a closed strand, where the fractions 0.02 and 0.98 are neighbours rather
than opposites. The bound is not a margin. Without it the neighbouring
overlap regions touch, pathops merges them into one, and the first
crossing's patch owns the whole run — an ordinary braid then reads as a
single strand laid on top of the others.

So with **opaque** strand brushes the repair is exact *where a crossing has
room*. Adjacent shallow crossings each own only half the distance between
them, so the under-strand can show between two close knots. Widen the
strands' spacing, or the angle between them, if that shows.

With **translucent** strands it double-covers: the over-strand's alpha
composites twice inside the patch, so the crossing reads darker than the
strand does elsewhere. That is the patch MODEL, not the patch size, and it
is one of the two named hard cases ROADMAP §33 pins for the element-level
crossover pass (the other being several crossings over one region).
**Weaves want opaque inks until that pass lands.**

Only the rule VALUES are shared with that pinned pass (§33, pinned pass 1).
Its API is undecided; this vocabulary is not.

### `.shaped(value)` — the one geometry-deviation seam

```cpp
Brush{}.shaped(kit::brush::shapers::wave(5, 24))
       .shaped(kit::brush::shapers::jitter(8, 2, 21))
       .layer(brush::solid(3, ink));
```

Any comparable value with `SkPath shape(const SkPath &) const`, plus an
optional `bleed()`. SkPath in, SkPath out — dash and width are path
operations, and every deviation the corpus wanted was expressible that way.

There are deliberately **no sugar methods** over this seam. Stock shapers
are kit values, peers of anything you write. `Brush::op()` and the
`ops::` structs named the mechanism and were DELETED in R3; `.shaped()`
is the only way into a pipeline, and `Brush::layer(dec, {shaper…})` the
only way into a per-layer suffix.

The one thing a shaper cannot be is a raw lambda — a `Shaper` is
comparable by design. So `ops::PathOp` survives as ONE door, reached
through `brush::restyle(op, decoration)`. Reach for it only for a one-off
closure you accept will never prune.

The two mechanisms, named: a **shaper** bends the ONE continuous mark
(wave, zigzag, jitter — no tile exists); a **pattern** builds the mark out
of CELLS (and the cell is an element, so anything paints it).

### The kit tier is a separate library

`kit::` is `SigilComposeKit`, its own CMake target whose only include path
is compose's PUBLIC headers. The tier boundary is therefore structural,
not conventional: a kit header cannot reach `ComposeInternal.h` even by
accident. `kit/BoundaryProbe.cpp` is the negative control — built on
demand, expected to fail.

```cpp
kit::brush::shapers::wave / zigzag / square    // the ONE geometry seam:
kit::brush::shapers::rounded / jitter / offset //   one per way of bending
kit::profile::wave(amp, wavelength, phase)    // core ships self/offset only
kit::strands::braid(n, amp, wavelength, ink)  // n waves at phase k/n
kit::spans::brackets(arm)                     // a composition of core terms
kit::shapes::ring(innerRatio)                 // "annulus" rejected as jargon
kit::brush::presets::filament / circuit / rope / pulse   // see below
```

PRESETS are a different TIER, and they are scoped apart to say so.
`kit::brush::presets::` holds the four `LayeredBrush` compositions that
left core in R2 (`filament`, `circuit`, `rope`, `pulse`) — peers of the
shapers in mechanics (free functions over the public API) and not peers
in kind: a shaper is vocabulary, a preset is a finished drawing. §33's
end state for presets is an EXTERNAL loadable kit; no such mechanism is
built, and these four had to leave `brushes::` because that namespace
R3 deleted, so the kit is the waypoint. `cased`, `railway`,
`GlossContour` and their relatives are still out;
`sketch/sketches/stroke_atlas.cpp` stays the in-repo
specimen page. Standing check: a preset whose name is craft jargon over a
plain composition gets demoted — the `cased` treatment.

## The masking family

**Appearance-gating is a relation between a SELECTION and a GATE**, and the
two questions are independent:

| axis | question | vocabulary |
| --- | --- | --- |
| `parts::` | **which** of this node's paint outputs does the mask reach? | `all` · `marks` · `surface` · `content` · `children` · `named(label)` |
| `by::` | **how** does that paint arrive — by what rule is it cut? | `spans` · `edge` · `shape` / `outside` · `alpha` / `alphaOut` · `luma` / `lumaOut` |

```cpp
.mask(by::spans(spans::upTo(animate(from(0.f).to(1.f), {600ms}))))  // draws on
.mask(by::edge(90.f, bind(&sweep)))                       // a directional wipe
.mask(by::shape(Region::path(seal)))                      // keep the silhouette
.mask(by::outside(Region::rect(hole)))                    // …and the clipOut
.mask(by::alpha(Material::linear(a, b, fade)))            // soft-edged coverage
.mask(by::luma(Material::image(plate)))                   // …by BRIGHTNESS
.mask(by::lumaOut(Material::image(plate)))                // …and the inverse

.mask(parts::named("hazard"), by::edge(0.f, &armTime))    // ONE mark
.mask(parts::marks(), by::spans(spans::upTo(&sweep)))     // every mark, no fill
```

The one-argument form is the taught default and means `parts::all()`. It is
what 24 of the 25 corpus sites write.

### Why it is two words and not seven

Before this, the library had seven mechanisms that each answered both
questions at once, in one pre-multiplied token — and no two of them gated
the same set or were the same value kind:

| the old spelling | gated WHAT | gate VALUE |
| --- | --- | --- |
| `wipe(angle, t)` | everything, children included | a half-plane |
| `trim(s, e, off, mode)` | fill + every decoration; NOT children | an arc window |
| `stroke(Spans, …)` | exactly one pass | an arc window |
| `clip()` | fill + content + children; NOT decorations | the node's own shape |

`clip()` and `trim()` were exact complements on the decorations/children
axis and nobody chose that. Three of the four things an author would call a
mask — a region other than the node's own, a coverage source, a per-mark cut
— did not exist at all: a study reached for `clipOut()` and
`shapes::subtract` **by name**, found neither, and dropped below the Compose
seam to a raw `SkPathOp`; a shipped header prescribes `Material` + `kDstIn`
as an *idiom* because it is not a feature.

### The fold table

| was | is |
| --- | --- |
| `wipe(a, t)` | `mask(by::edge(a, t))` |
| `trim(s, e)` | `mask(by::spans(spans::range(s, e)))` |
| `trim(0, t)` | `mask(by::spans(spans::upTo(t)))` |
| `trim(s, e, off)` | `mask(by::spans(spans::range(s, e).offset(off)))` |
| `trim(s, e, off, Wrap)` | `mask(by::spans(spans::wrap(s, e).offset(off)))` |
| `clip()` | `mask(parts::surface() \| parts::content() \| parts::children(), by::shape(Region::own()))` — kept as sugar, and as the cheap `clipRRect` path |
| `stroke(where, what, name)` | `stroke(what, name).mask(parts::named(name), by::spans(where))` — kept as sugar, and it is the form that CLAIMS |
| a hand-rolled `kSrcIn` overlay | `mask(by::alpha(material))` |
| a raw `SkPathOp` set difference | `mask(by::shape(a))` + `mask(by::outside(b))` |

`trim()` and `wipe()` are **deleted**. Nothing is condemned-but-compiling.

### The two laws

**1. A gate is a SHOW set.** `by::edge(90, 0.3)` shows 30%; `spans::upTo(t)`
shows `[0, t]`. The complement is a **term**, never a mode flag —
`by::outside(r)` is the word for the outside of a region, and
`spans::rest()` was already the precedent. Photoshop's "mask" is
white-shows-black-hides and therefore ambiguous in isolation; a reader
auditing a picture reads which way round it is off the call site, the same
argument that made `wrap` a term rather than a flag on `range`.

**2. Stacked masks INTERSECT where their selections overlap.** Both must
pass. Nesting already means this everywhere else — `clip()` inside `clip()`
intersects, a span claim under a whole-node cut intersects — and **union is
spelled inside one gate value** (`Spans::operator|`), never across masks.
Two masks are two conditions; stacking them can only ever show less.

```cpp
.stroke(spans::corners(18), stroke(2, ink), "brk")
.mask(parts::marks(), by::spans(spans::upTo(&sweep)))
// the pass paints corners ∩ upTo(sweep): brackets that LIGHT UP as the
// sweep reaches them. One line, no second node, no re-authoring.
```

**Each mask carries its own animation**, indexed per mask, so three masks on
one node may run at three different rates and the intersection is exact per
frame. That is a design requirement, not a side effect: if they shared a
slot the second would retarget the first.

**The claim ledger reads the UNMASKED boundary.** A span pass's claim is a
statement about *where a mark goes*; a gate is a statement about *how much
of it exists yet*. So the no-overlap diagnostic resolves claims against the
uncut boundary — an overlap is a description-level mistake, and never a
mistake that blinks in and out between 0.3 and 0.7 of a transition.

### What each gate reaches, and what it does not

A **spans** gate cuts the BOUNDARY, so it addresses the paint that traces
one: the surface and the marks. Content and children do not trace a
boundary, so an arc-length window over them is not a picture and does
nothing — `mask(by::spans(...))` with the default `parts::all()` is
therefore exactly the reveal `trim()` drew. **edge**, **shape** and the two
**coverage** sources cut the PLANE and reach everything, `wipe()`'s reach.

A selection that matches no mark selects nothing, silently — the same law as
`spans::rest("unknown")` and `spans::fit("unknown")`.

Some cells of the product are not pictures (`parts::children()` × a spans
gate). The family accepts that and documents the sensible ones rather than
adding a compatibility table, because a two-factor API is honest about a
complexity that exists whether or not it is named.

### The two coverage sources, and the four mattes (2026-07-29)

A **coverage** gate takes its show set from a `Material` rather than from
geometry, and there are two readings of a material and two sides of each —
After Effects' four track-matte variants, and the same four pictures:

| | keep what it covers | keep the rest |
| --- | --- | --- |
| the material's **alpha** | `by::alpha(m)` | `by::alphaOut(m)` |
| the material's **luma** | `by::luma(m)` | `by::lumaOut(m)` |

```cpp
// A title card wiped in by a paper texture: the grain's own brightness is
// the dissolve, so the reveal has the edge quality of the plate and not of
// a gradient. `bind` animates the threshold; the matte itself is static.
row().child(text(u8"MERIDIAN", display96))
     .mask(by::luma(Material::blend({
         {Material::image(grain), SkBlendMode::kSrcOver},
         {Material::linearUnit({0, 0}, {1, 0},
                               {{0.f, {1, 1, 1, 1}}, {1.f, {0, 0, 0, 1}}}),
          SkBlendMode::kMultiply}})));
```

**The luma law, stated once.** `Y' = 0.299 R' + 0.587 G' + 0.114 B'` —
**Rec. 601 coefficients, on the ENCODED values, taken on the PREMULTIPLIED
colour.** Each of those three words is a ruling, and DESIGN.md's colour rule
is the argument:

- **encoded**, because compose has no linear stage: its surfaces carry no
  `SkColorSpace`, so a shader's channels are the display-encoded numbers the
  author wrote. Linearising here would invent a transfer function no other
  part of the pipeline applies.
- **Rec. 601**, because those are the coefficients *defined* on gamma-encoded
  R'G'B'. Rec. 709's 0.2126/0.7152/0.0722 are LUMINANCE coefficients, defined
  on linear light; using them on encoded values is the classic mistake, and
  it is 22/255 wrong on red and 33/255 on green.
- **premultiplied**, because a transparent matte must read as black and hide,
  the way AE's does. A half-transparent white and an opaque 50% grey are the
  same matte.

Pinned by `S7cTheLumaGateIsRec601OnEncodedPremultipliedValues`, whose five
plates rule out each wrong answer separately — and deliberately not with
greys, which pin nothing about the coefficients, nor with primaries alone,
which pin nothing about the transfer function because 0 and 1 are the sRGB
curve's fixed points.

**Why four terms and not a flag.** Law 1 below says the complement is a
term, never a mode flag; `by::outside(r)` is that word for a region.
A coverage source has no English spatial complement ("outside a gradient" is
not a picture), so the inversion takes the morpheme the neighbourhood
already uses for exactly this — Skia's `clipOut`, the very call `by::outside`
was written to replace. The alternative considered and rejected was
`.invert()` on the gate value: it is the mode flag law 1 forbids, it would
give `by::outside(r)` a second spelling, and it would be the only mutating
verb in a vocabulary of factories.

### Cost, and the cache class

`spans` rides a path effect, `edge` and `shape` ride a canvas clip — all
three are the cheap paths those mechanisms already used. **The coverage
gates cost a `saveLayer` per masked group** and are the expensive members.
Within them nothing else costs anything: the complement is `kDstOut` instead
of `kDstIn` (`dst·(1-a)` IS `1 - coverage`, for any source), and `luma` adds
one dot product in C++ when the material resolves to a colour, or one SkSL
pass over the coverage layer when it resolves to a shader.

A mask whose selection is EVERYTHING is hoisted to wrap the whole node once,
rather than per paint group. That is both the fast path and the exact one: a
nested pair of antialiased clips would compound its own edge coverage.

**A masked node keeps the §17 scalar memo.** A gate's animated numbers are a
bounded, per-node, resolvable-to-floats list, so they ride `ContentScalars`
and a held keyframe on a masked node repaints nothing. This is the whole
reason the family's selection lives in an ARGUMENT rather than in the slot
call or the mark value: per-pass gate scalars land in the open `spanAnims`
vector, which is excluded from that memo by a written decision and which
also disqualifies `Cache::Group`. (Per-PASS span endpoints are still
excluded — closing that is separate work.)

An `alpha` gate on a LIVE material (`uTime`, a bound uniform) declares
volatility and refuses both memos, exactly as a live material fill does.

### `Region` is a value, and that is load-bearing

```cpp
Region::own()          // the node's own silhouette — clip()'s region
Region::rect(r)  Region::oval(r)  Region::path(p)
```

The obvious signature for a shape gate takes an `OutlineFn` — an
incomparable `std::function`, which never participates in reconciler
equality and therefore **never prunes**. That is not hypothetical: it is the
highest measured-impact item on the roadmap (43.4 of 43.5 ms on one
un-prunable callable). A `Region` is closed and comparable — `SkPath` has
structural equality, so `Region::path()` is a general escape hatch that
still prunes — and that is why the shape member could ship WITH the family
instead of after it.

## The Brush engine (lines as expressive as fills)

A `Brush` is ONE comparable value: an ordered **geometry pipeline** over
the node's outline feeding ordered **paint layers** — Illustrator's brush
model (Calligraphic / Scatter / Pattern / Art) closed under composition,
grounded in REFERENCES.md §9 (leaflet/mapbox/QGIS/tldraw conventions):

```cpp
element.stroke(Brush{}
    .shaped(kit::brush::shapers::Rounded{6})  // shapers, in order
    .shaped(kit::brush::shapers::Wave{.amplitude = 3, .wavelength = 30})
    .layer(lines::cased(3, ink, 5))           // any Decoration is a layer
    .layer(brush::Scatter{.art = spark(), .spacing = 40},
           {kit::brush::shapers::Offset{.px = 6}}));  // per-layer suffix
```

`layer()`, not `leg()` (ROADMAP §33 ruling 14, R3): a Brush's stacked
marks are the same idea as `brush::layers(...)`, the fixed-order
composite — the way a strand is the unit of a weave. `leg` named a
mechanism nothing else in the grammar used.

- **The pipeline takes SHAPERS** (`.shaped(value)`, any comparable value
  with `SkPath shape(const SkPath &) const`; stock ones under
  `kit::brush::shapers::`), and so does the **per-layer suffix** — one
  seam, one word, since R3 closed the gap that kept `ops::` public.
- **Layers** are ordinary Decorations: `lines::Line` (parallel casings,
  terminal/mid caps with the tip-at-endpoint convention, railway ties,
  dash that stays phase-registered across rails), `LayeredBrush` stacks
  (`kit::brush::presets::` filament/circuit/rope/pulse),
  `brush::Scatter` (an ELEMENT
  instanced along the path, seeded jitter + a `StampModFn` programmatic
  twist), `brush::Pattern` (Illustrator tile semantics:
  integer-fit side tiles, corner/start/end tiles), `brush::Ribbon`
  (taper / calligraphic nib), or any `PathFormat`.
- **The whole Brush compares** when its parts do — a styled connector
  prunes and caches as one value; animated legs declare volatility
  through; bleeds aggregate (pipeline reach + leg reach).

**Corners are their own problem, and `brush::Pattern` now handles them
properly.** A corner tile reserves `cornerLength` of arc on each
adjacent run (so side tiles butt against the elbow instead of sliding
underneath it — set it whenever the corner art is bigger than the side
art), and it lands on the **vertex** rather than up to half a detection
step past it.

**Corner art carries its own alignment, and there is no default.**
`.corner` takes a `brush::CornerArt{art, align}` — a REQUIRED
constructor argument, the §27 break the roadmap called for and §33
ruling 8 authorised. `Bisector` is for an ORNAMENT (art symmetric about
its own bisector: one drawing serves all four corners of a rectangle);
`Outgoing` is for anything with an entry and an exit — an elbow of pipe,
a flow tick, a directional marker turning a corner. It is not a
preference, it is a statement about what the art LOOKS like, which is
why a default that only warns was not good enough: two studies shipped
visibly wrong through review under one.
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

### The rest of the brush surface, and its three traps

```cpp
kit::brush::presets::filament(glow, core, scale)  // Ori's 4-layer glow
kit::brush::presets::circuit(color, tier)         // FUI trace tiers 0/1/2
kit::brush::presets::rope(state, zoom)            // PoE's 3-state rope
kit::brush::presets::pulse(halo, core, scale)     // the travelling packet
brush::Scatter{.art, .spacing, .jitter*, .mod}          // Illustrator scatter
brush::Pattern{.side, .start, .end, .advance, .cornerLength,
                      .corner = brush::CornerArt{art, align}}  // see below
brush::artAlong(art, height, stationPx)     // Art: warps, not stamps
brush::ribbon(profile, fill)                // the seam form: comparable,
                                            // bounded, proper corners
brush::Ribbon{.widthStart, .widthEnd, .nibAngleDeg, .width (Profile)}
brush::restyle(op, inner, extraBleed)   // OP FIRST, then the decoration
                                        // (the ONE mechanism door)
kit::brush::shapers::Wave / Zigzag / Rounded / Jitter / Square / Offset
```

**1. A stamped brush caches its baked art in the VALUE.** `brush::Pattern`,
`brush::Scatter` and `brush::Art` each hold their `snapshot()` in a
`shared_ptr` member. Copy the brush and the copy shares it; **construct**
one and it gets an empty cache — so a brush built inside a per-frame
describe re-bakes every tile every frame, and each bake is a full
reconcile + layout + record pass. One study measured eighteen per frame.
Build the brush once and keep it, or keep the art Elements
pointer-stable and copy. `brush::Art` is the most expensive of the three.

**2. A `Ribbon`'s width `Profile` — declare `alongIsPx` if the host can be
MASKED.** A decoration under a span gate is handed the REVEALED contour, so a
FRACTION is a fraction of what has been drawn *so far* and a width law
keyed to it slides as the reveal grows. The two are identical in a still
frame and diverge only in motion, which is the worst way for a bug to be
visible. (The reach half of this trap is gone: `max()` is required, so a
166 px band can no longer declare 10 px and be silently clipped.)

**2b. A profile that returns a non-finite width used to delete the WHOLE
band.** One NaN vertex makes a built path non-finite and Skia draws none
of it — so the band did not pinch, it vanished, and nothing said why.
Found the hard way: `sqrt(sin(3.14159265f * along))` is NaN at
`along == 1`, because the float π rounds UP and the sine goes to
-8.7e-08. Every construction samples the law at exactly 1, so an entire
study's link bloom was invisible for its whole life (`astral_tome`,
fixed 2026-07-26). GUARDED since 2026-08-04: `profileOffset` resolves a
non-finite sample to a LOCAL pinch to the spine, so the rest of the band
draws. Still clamp inside the law — the pinch is damage control, not the
width you meant.

**3. `shapers::Jitter` is ONE pass of `SkDiscretePathEffect`.** It jitters
vertices; it does not bow the segments between them, so it is not the
Rough.js construction on its own — Rough.js draws TWO passes, full and
half deviation at different seeds. Compose two `restyle()`s (or two
`Jitter` layers) to match; no single call reproduces it. Note `restyle`
takes the OP FIRST and the decoration second, and the WRAPPER is
incomparable whatever op you hand it (it has no `operator==`) — memo the
host node or keep it pointer-stable, or it never prunes.

**Shapers are VALUES** (comparable structs with
`SkPath shape(const SkPath &) const` and an optional `bleed()`),
type-erased by `Shaper` exactly as `Decoration` type-erases paint
schemes, because Skia seals `SkPathEffect` subclassing. R3 deleted the
`ops::` structs that used to double them (`Wave`/`Rounded`/`Sketchy`/
`Square`/`Offset`, spelled with `apply()` rather than `shape()`) after
their kit twins were complete — the bodies moved to the twins unchanged.
`ops::PathOp`, with `chain()` and `debug()`, is what remains: the raw
incomparable lambda, reachable only through `brush::restyle`, kept
because nothing else can carry a closure.

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

- **`brush::artAlong(art, height, stationPx)`** (`brush::Art`) — the
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
pool->commit();                        // bulk-mutated? publish the edit
                                       // (`touch()` DELETED in R3)

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
`commit()`, `render()`; `Mode::Live` is the `Cache::None` particle path
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
  call sites self-documenting (`RowData{.name = "…", .score = 12}`).
- **Ranges**: `children()` accepts any range of `Element`, so
  `column.children(rows | std::views::transform(scoreRow))` replaces
  the loop.
- **`std::chrono` durations** for time-valued API (revising earlier
  sketches): `.transition({.duration = 400ms, .ease = ch::easeOutQuint})` —
  no naked doubles-of-seconds.
- **UDLs** for dimensions: `width(50_pct)`, `padding(24_px)`.
- `std::span`/`std::string_view` at boundaries; no exceptions in the
  hot path. (`Animatable` is deliberately NOT a `std::variant` — a
  compact class boxing the fat `Transitioned` payload out-of-line; see
  Values.)

## Non-goals (unchanged)

No input/focus/accessibility, no VDOM/scheduler (you call `render()`),
no markup language in the core, no ownership of surfaces or threads —
the Composer is a guest in your canvas, which is the entire point.
