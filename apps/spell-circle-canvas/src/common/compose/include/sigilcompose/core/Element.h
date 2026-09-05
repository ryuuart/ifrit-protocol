#pragma once

/** @file
 * SigilCompose Element — the value description of one node: what to draw,
 * how to lay it out, and the children under it, as chaining setters. The
 * factories that start one are in Factories.h; the one-shot verbs that
 * take a tree without a live composer are in Measure.h and Tiles.h.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPath.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Mask.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Shape.h>
#include <sigilcompose/core/Stroke.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/Animation.h>
#include <sigilmotion/schedule/Schedule.h>
#include <sigilmotion/values/Animated.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/style/Style.h>

#include <any>
#include <chrono>
#include <functional>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class SkCanvas;

namespace sigil::image {
class ImageAsset;
}

namespace sigil::weave {
class FontContext;
// Which glyphs a text verb addresses, and the granularity it addresses
// them by — the paragraph engine's, in <sigilweave/query/Selector.h> and
// <sigilweave/paragraph/Unit.h>.
class Selector;
class RichText;
class Story;
enum class Unit : uint8_t;
}

namespace sigil::material::pattern {
class Tile;
}

namespace sigil::compose {

namespace detail {
struct ElementNode;
struct Instance;
}  // namespace detail

class Composer;
class Pattern;
// The typography vocabulary the text verbs take, defined under
// <sigilcompose/typography/>: a call site that dresses its type includes
// the header that spells the value it passes. Which glyphs a verb
// addresses is SigilWeave's `Selector`, declared above.
struct Track;
struct Annotation;
struct TextPath;

// ---------------------------------------------------------------------------
// Element — a cheap value description

/** One node of a scene description: what to draw, how to lay it out,
 *  and the children under it. An Element is a VALUE built fresh every
 *  frame and thrown away — it holds no GPU or layout state, and the
 *  retained tree behind it is the composer's business. The chaining
 *  setters return `*this`, so a node reads as one expression. */
class Element {
 public:
  Element();  // empty box

  // ---- layout ----
  Element& row();
  Element& column();
  /** Flex-wrap: children flow onto new lines/columns when they
   *  overflow the main axis. */
  Element& wrapLines(bool on = true);
  Element& gap(float px);
  Element& padding(float all);
  Element& padding(float horizontal, float vertical);
  Element& padding(float left, float top, float right, float bottom);
  Element& margin(float all);
  Element& margin(float horizontal, float vertical);
  Element& margin(float left, float top, float right, float bottom);
  /** The flex BASIS, not a guarantee. `shrink` defaults to 1, faithful to
   *  Yoga and CSS, so a `width(150)` child of a row that overflows is
   *  150 px wide only until the row runs out of room — then it gives some
   *  back, and the result is silently narrower content rather than an
   *  error. Pair with `.shrink(0)` when `width(150)` means "this IS 150".
   *  The same holds for `height()` in a column. */
  Element& width(Dim d);
  Element& height(Dim d);
  Element& minWidth(Dim d);
  Element& maxWidth(Dim d);
  Element& minHeight(Dim d);
  Element& maxHeight(Dim d);
  Element& aspect(float ratio);
  Element& grow(float factor = 1.0f);
  Element& shrink(float factor);
  Element& basis(Dim d);
  Element& alignItems(Align a);
  Element& alignSelf(Align a);
  Element& justify(Justify j);
  Element& absolute();
  Element& inset(float all);
  Element& inset(float left, float top, float right, float bottom);
  /** Dim-valued insets: px, pct(), or autoDim() per side — autoDim()
   *  leaves that side unpinned (the CSS `auto`), so width/height (or the
   *  opposite inset) size the node instead of stretching it. */
  Element& inset(Dim left, Dim top, Dim right, Dim bottom);
  /** Pin ONE edge of an absolute node (implies absolute()): the
   *  corner-badge idiom — `.top(12).right(12)` pins a date block to the
   *  top-right without stretching it across the box. Unpinned sides stay
   *  auto. */
  Element& left(Dim d);
  Element& top(Dim d);
  Element& right(Dim d);
  Element& bottom(Dim d);
  /** Center this absolute node ON a parent-space point — the dominant
   *  placement in node-graph scenes (sockets on orbit positions, badges
   *  on markers). Resolved after measurement, so intrinsic-size nodes
   *  center correctly; implies absolute(). */
  Element& centerAt(SkPoint p);
  /** WHICH CELLS this child claims of the `layout()` scheme above it, and
   *  how many it covers — read by grid-shaped schemes (`Table`,
   *  `layouts::ModularGrid`) and by nothing else.
   *
   *  Said HERE, on the child, rather than in a list the scheme carries
   *  beside it: a parallel list has nothing to check itself against, and
   *  an inserted or reordered child silently shifts every entry after it
   *  onto the wrong cell. */
  Element& cells(int column, int row, int columns = 1, int rows = 1);
  /** Where this child sits INSIDE the cell box its span makes.
   *  `Align::Stretch` sizes it to the box instead of placing it in one. */
  Element& cellAlign(Align across, Align down);
  /** Place an absolute node on a parent-space RECT — the peer of
   *  centerAt(), for when you already know the box.
   *
   *  Exactly `left(r.fLeft).top(r.fTop).width(r.width()).height(r.height())`
   *  — it calls those four setters, so it writes the same four layout
   *  fields, prunes identically, and cannot drift from the longhand. Right
   *  and bottom stay unpinned.
   *
   *  **A primitive for placing content whose coordinates you already
   *  have**, typically because they were measured off a reference. When a
   *  position is a *relationship* instead — "inside its parent", "next to
   *  that one", "as wide as the column" — flex and inset() express it and
   *  this does not.
   *
   *      g.child(box().rect(panelBox).fill(…));
   *      g.child(text(u8"…", st).at({panelBox.fLeft + 16, panelBox.fTop}));
   *
   *  Does not cover right()/bottom() pinning, percentage insets, or
   *  autoDim() sides — those are different intents and keep the longhand.
   *  `geometry::path::centred()` (kit/Frame.h) builds the rect for the
   * centre-and-size case. */
  Element& rect(const SkRect& r);
  /** Pin an absolute node's top-left to a parent-space POINT, leaving the
   *  node to size itself from its content — `left(p.fX).top(p.fY)`. The
   *  half of the placement longhand that carries no box; same
   *  qualification as rect() above. */
  Element& at(SkPoint topLeft);

  // ---- shape (defines PaintContext::outline and clipping) ----
  Element& corners(Corners c);
  /** THE NODE'S SHAPE: a path generator over its laid-out size, in local
   *  coordinates. Overrides corners() — the fill surface, clip(), every
   *  stroke pass and every outline-following decoration (PathFormat,
   *  ContourWalk) trace it. Spiky dialogs, scalloped frames, any
   *  non-rectangular chrome.
   *
   *  A shape is a REGION; a stroke is a mark on its boundary. Filling this
   *  is `fill()`, drawing its edge is `stroke()`.
   *
   *  Takes a `Shape`. Every `shapes::` generator is a comparable value, so
   *  a shaped node prunes exactly like an unshaped one. A raw callable is
   *  accepted as the escape hatch, but it never compares equal, so the
   *  node re-patches and re-records on every describe — memo() such a
   *  node, or hold the Shape value stable, to get pruning back. */
  Element& shape(Shape path);
  /** BAND FORMATION: which side of the spine the band occupies.
   *  `.centered()` is the default and straddles it; `.outward()` and
   *  `.inward()` take one side (the offset-path lineage). No effect on a
   *  node that is not a band(). */
  Element& centered();
  Element& outward();
  Element& inward();
  /** Clip fill, content, and children to the node's shape. Decorations
   *  are NOT clipped — they dress the outline (outer strokes, shadows,
   *  glows keep their reach); hit-testing still bounds the subtree.
   *
   *  SUGAR, and exactly equivalent, so the two spellings are one machine:
   *
   *      .clip()  ==  .mask(parts::surface() | parts::content() |
   *                         parts::children(), by::shape(Region::own()))
   *
   *  Kept as its own word because it is also the cheap path: a rounded box
   *  clips with `clipRRect`, where the general shape gate has to build a
   *  path and clip against that. */
  Element& clip(bool on = true);

  // ---- mask (the appearance-gating family) ----
  /** THE FAMILY VERB, taught form: gate everything this node paints.
   *
   *      .mask(by::spans(spans::upTo(animate(from(0.f).to(1.f), {600ms}))))
   *      .mask(by::edge(90.f, bind(&sweep)))
   *      .mask(by::shape(Region::path(seal)))
   *      .mask(by::alpha(Material::linear({0,0}, {0,h}, fadeStops)))
   *
   *  Sugar for `mask(parts::all(), with)`, and the form to reach for
   *  first. A gate addresses only the paint it CAN address: an arc-length
   *  window means something to the surface and the marks and nothing to
   *  the children, so `parts::all()` with `by::spans()` gates the boundary
   *  tracers and leaves the children alone.
   *
   *  Paint-only and bindable, like the transforms: animating a mask never
   *  relayouts, and hit-testing keeps the UNMASKED shape — a mask is a
   *  paint-phase reveal, not a layout change. */
  Element& mask(Gate with);
  /** …and the granular form: gate SOME of what this node paints.
   *
   *      panel.overlay(hazardStripes, "hazard")
   *           .foreground(bevelKeyline)
   *           .mask(parts::named("hazard"), by::edge(0.f, &armTime));
   *
   *  Repeated calls APPEND, as every decoration slot does, and masks whose
   *  selections OVERLAP INTERSECT on the overlap — both gates must pass.
   *  Each mask carries its own animation slots, so masks at three
   *  different rates on one node is a picture, not a race: the
   *  intersection is recomputed exactly, per frame.
   *
   *  Union is spelled INSIDE a gate value (combining spans with `|`), never
   *  across masks — two masks are two conditions, and stacking them can
   *  only ever show less.
   *
   *  The one thing this cannot express that `stroke(where, what)` can: a
   *  span pass CLAIMS its run and joins the overlap check, and a mask does
   *  not. That check is deliberately read against the UNMASKED boundary,
   *  so an overlapping claim is a description-level mistake reported once,
   *  never one that blinks in and out partway through a transition. */
  Element& mask(Parts what, Gate with);

  // ---- paint ----
  /** A colour, a shader, a transition between colours, or a LIVE binding.
   *
   *  The binding form is `fill(&output)` where the Output holds a `Fill`,
   *  and it is the answer to "this widget's colour IS its value" — a
   *  level meter whose hue is the level, a temperature readout, a health
   *  bar that reddens. Write the Fill Output from the same steppable that
   *  computes the number:
   *
   *      ch::Output<float> level; ch::Output<Fill> bar;
   *      ticker.add([&](double){ level = v; bar = Fill::color(ramp(v)); … });
   *      box().scaleX(bind(&level)).fill(&bar)
   *
   *  What does NOT exist is deriving one from the other at the binding
   *  site: `fill(bind(&level).map(ramp))` does not compile, because the
   *  shaping chain maps floats to floats. Compute the Fill in the
   *  steppable, as above. */
  Element& fill(motion::Animatable<Fill> f);
  /** Fill with a Material (gradient ramp, blend stack, sprite, SkSL) — the
   *  richer authoring value. A static Material collapses to a Fill, so it
   *  caches and prunes on the same path. See <sigilcompose/Material.h>. */
  Element& fill(material::skia::Paint m);
  /** NEITHER A TILE NOR A PATTERN IS A FILL, and the reason is where they
   *  have to be STORED. A Pattern's bake is its identity: it renders its
   *  tile once, on the shared state that Pattern holds, so a Pattern minted
   *  inside a describe is a fresh state with no bake in it and re-renders
   *  the tile on every render. Hold the Pattern where assets are held — a
   *  sketch member, a model field — and fill with what it bakes:
   *
   *      Pattern m_grain = pattern::stripes(6, 6, kInk);  // once
   *      box().fill(m_grain.material());                  // every describe
   *
   *  Deleted rather than absent so the error names the rule instead of
   *  naming an overload set. */
  Element& fill(material::pattern::Tile tile) = delete;
  Element& fill(const Pattern& pattern) = delete;
  /** Solid-color sugar: fill({r,g,b,a}) without the Fill:: ceremony. */
  Element& fill(SkColor4f color) {
    return fill(motion::Animatable<Fill>{Fill::color(color)});
  }
  /** How an image() leaf samples its source. Defaults to linear, which is
   *  right for photographs and wrong for every pixel grid: art, tilemaps,
   *  fonts baked as sprites, simulation buffers.
   *
   *      image(tileset).sampling(SkSamplingOptions(SkFilterMode::kNearest))
   *
   *  `Material::image()` takes the same options for a sprite fill. No
   *  effect on non-image leaves, silently. */
  Element& sampling(SkSamplingOptions options);
  // ---- decoration layers ----
  // Backgrounds paint below content/children (in declaration order),
  // foregrounds above; fill() is the transitionable first background,
  // custom() a box with one background program.
  // Repeated calls APPEND (the Photoshop stacked-strokes model — two
  // stroke() calls are two rings).
  // Decorations dress the OUTLINE: clip() does not clip them (it bounds
  // fill/content/children only), so outer strokes and shadows survive on
  // clipped nodes.
  /** Takes this node OUT of hit testing — CSS `pointer-events: none`.
   *
   *  READ THIS BEFORE KEYING A CONTAINER. `hitTest` returns any keyed node
   *  whose box contains the point, whether or not that node paints
   *  anything. So a keyed, full-bleed layout SHELL with no fill swallows
   *  every hit in the frame, and every query comes back naming it. There
   *  is no visual symptom and no diagnostic — the shell is invisible and
   *  the answers are simply wrong. This is the opt-out.
   *
   *  Children are still tested: this excludes the node's own box, not its
   *  subtree. */
  Element& hitTestable(bool enabled);
  /** A decoration painted OVER the fill and UNDER the content and
   *  children.
   *
   *  THE STACKING ORDER IS A CONTRACT, not a hint, and picking the wrong
   *  slot is the commonest way to draw nothing visible. `background()`
   *  sits beneath the FILL, so an opaque fill covers it completely — a
   *  bevel put there renders as a flat slab. `foreground()` paints above
   *  the children, so a texture put there greys out the node's own label.
   *  This middle slot is what hazard stripes over a surface but under the
   *  digit, scanlines over a panel but under its readout, and bevelled
   *  chrome all want. The alternative is a sibling stack, which costs a
   *  node and loses the shared outline.
   *
   *  `name` is optional and LOCAL: it labels this mark so
   *  `mask(parts::named(name), by::…)` can address it and nothing else.
   *  Same names, same law as `stroke(Spans, what, name)` — inspection and
   *  intra-element reference, never a query key. */
  Element& overlay(Decoration d, std::string name = {});
  /** A decoration painted BENEATH the fill (the CSS box-shadow
   *  ordering) — shadows, ground textures, anything the surface sits on
   *  top of. If you want it over the surface but under the children, that
   *  is `overlay()` above. `name` labels the mark for `parts::named()`. */
  Element& background(Decoration d, std::string name = {});
  /** THE BACKGROUND SLOT, span-qualified — `.stroke(where, what)`'s twin
   *  in the other z-half.
   *
   *      .background(spans::edges(14), stroke(3, shadowInk))  // under the fill
   *      .stroke(spans::corners(18), stroke(2, ink))          // over the kids
   *
   *  Identical in every respect to `stroke(Spans, ...)` except WHERE the
   *  mark lands: it paints with the backgrounds, beneath the fill and
   *  therefore beneath the content and the children. Everything else is
   *  shared, deliberately — the passes append into ONE list in declaration
   *  order, one claim record covers both z-halves, the no-overlap rule
   *  reads across both, and `rest()` complements both. A boundary does not
   *  have two of itself, so a background pass and a stroke pass claiming
   *  the same run is the same conflict as two stroke passes doing it, and
   *  `rest("name")` can name a pass in either half. */
  Element& background(Spans where, Decoration what, std::string name = {});
  /** A decoration painted OVER the children. `name` labels the mark for
   *  `parts::named()`. */
  Element& foreground(Decoration d, std::string name = {});
  /** fill's peer: dress the node's whole BOUNDARY with a brush — a
   *  PathFormat, a layered brush stack, any decoration that strokes.
   *
   *  This form does not CLAIM: it overlays the whole boundary, so repeated
   *  calls stack (two strokes are two rings) and never collide. Naming a
   *  `where` (below) is what turns a pass into a claim on part of the
   *  boundary; naming a `name` (here) is what lets a mask address this
   *  mark alone. */
  Element& stroke(Decoration brush, std::string name = {});
  /** THE STROKE SLOT: `where` on the boundary, painted by `what`.
   *
   *      .stroke(spans::corners(18), stroke(2, ink))          // reticle
   *      .stroke(spans::edges(14), stroke(1, ink))            // open corners
   *      .stroke(spans::upTo(animate(from(0.f).to(1.f), {600ms})), wire)
   *
   *  Repeated calls APPEND, in declaration order.
   *
   *  ORDERING, precisely, because CALL ORDER DOES NOT DECIDE IT: the
   *  unqualified strokes paint FIRST — they are foregrounds and share that
   *  list — then the span passes in their own declaration order. Within
   *  each group declaration order holds; between the groups the
   *  unqualified ones are always underneath. Interleaving the two by call
   *  order is not expressible, and writing them interleaved does not make
   *  it so. If a span pass must sit UNDER a whole-boundary one, make the
   *  whole-boundary one a span pass too (`spans::every(1)`) so both are in
   *  the same list.
   *
   *  Span-qualified passes CLAIM the runs they resolve to, and two claims
   *  that overlap are reported out loud, naming both passes and the
   *  overlapping run: one boundary, one mark. Layering two marks on one
   *  run is a composite BRUSH rather than two passes —
   *  `Brush{}.layer(a).layer(b)`, or a LayeredBrush.
   *
   *  Two exceptions, both deliberate: bare `spans::rest()` claims whatever
   *  the other passes left over, so a rule and its bracket corners are two
   *  calls and no arithmetic; and `spans::rest("name")` is the complement
   *  of ONE named pass and may overlay the others.
   *
   *  `name` is LOCAL to this element — for inspection, for the
   *  `rest("name")` reference, and for `mask(parts::named(name), …)`. It
   *  is not a query key; `Composer::bounds` and `hitTest` see only
   *  `key()`.
   *
   *  EXACTLY EQUIVALENT to the mask spelling, so the two are one machine:
   *
   *      .stroke(where, what, name)
   *          ==  .stroke(what, name).mask(parts::named(name),
   *                                       by::spans(where))
   *
   *  Identical pixels, and the same value under the same intersection
   *  rule — a further `mask(parts::marks(), by::spans(upTo(t)))` cuts this
   *  pass to `where ∩ upTo(t)`, which is how reticle brackets light up as
   *  a sweep reaches them. The ONE thing the pass form does that the mask
   *  spelling does not: it CLAIMS its run and joins the overlap check. */
  Element& stroke(Spans where, Decoration what, std::string name = {});
  /** WHAT THIS NODE'S DECORATIONS DRESS — its own shape (the default), the
   *  OUTLINE OF ITS GLYPHS on a text leaf, or the silhouette of WHAT IT
   *  DREW.
   *
   *      text(u8"CHROME",
   * heavy).boundary(Boundary::Glyphs).style(styles::chrome())
   *      image(cutOut).boundary(Boundary::Coverage).style(styles::chrome())
   *
   *  A decoration was never about a box: it is drawn across an outline, and
   *  which outline it gets is this. So every layer style already written —
   *  bevel, inner shadow, outer glow, gloss, the aqua and chrome presets —
   *  works on letters, or around a cut-out, the moment that is the outline,
   *  with no new preset and no second code path.
   *
   *  The glyph outline is the placement's own: it follows a wrapped line, a
   *  mixed-style run's size, a path run's curve and a vertical column's
   *  axis, because it is read off the placed glyphs rather than measured
   *  again. On a node that is not text it means the node's shape, which is
   *  what every node means by default.
   *
   *  The coverage outline is read off the node's rendered layer instead of
   *  off any description of it, which is why it is the answer for a
   *  cut-out, a clip or a mask — and why it is a staircase at the raster's
   *  resolution, and costs a raster and a trace whenever the node's layer
   *  is invalidated. Boundary states the whole bargain. */
  Element& boundary(Boundary source);

  /** Apply a whole LayerStyle (preset or hand-built): its `under` layers
   *  append as backgrounds, `over` as foregrounds — one call dresses the
   *  node in aqua gel / y2k chrome / any bundled treatment. Composable
   *  with fill() and further background()/foreground() calls. */
  Element& style(LayerStyle s);
  /** Append a misprint echo (see Echo): the node's fill shape and text
   *  re-stamped offset+flat-colored beneath the real pass. Not applied to
   *  text carrying `fx()` tracks (a moving letter draws its own batched
   *  buckets) or to image/custom content. */
  Element& echo(SkVector offset, SkColor4f color);
  /** Post-processes this node's rendered layer (forces a stacking
   *  context). Baked once under Cache::Texture. */
  Element& effect(material::skia::Effect e);
  /** Filters what is already painted beneath this node's bounds before
   *  the node paints (CSS backdrop-filter). Incompatible with
   *  Cache::Texture (the backdrop depends on the live destination);
   *  such nodes fall back to picture caching. */
  Element& backdrop(material::skia::Effect e);
  Element& opacity(motion::Animatable<float> o);
  Element& blend(SkBlendMode mode);
  Element& translateX(motion::Animatable<float> v);
  Element& translateY(motion::Animatable<float> v);
  /** Ride a CURVE instead of two lanes — the motion path (see MotionPath
   *  for the six rules). Paint-only like the lanes it outranks; the
   *  node's transform origin is the point that lands on the curve, and
   *  the curve is resolved against the PARENT's box.
   *
   *      .travel({.path = shapes::circle(),
   *               .t = bind(&phase).target(0, 1),
   *               .lookAhead = 0.02f})   // auto-orient along the tangent
   */
  Element& travel(MotionPath along);
  Element& rotate(motion::Animatable<float> degrees);
  Element& scale(motion::Animatable<float> factor);
  /** Per-axis scale about the transform origin, multiplied INTO scale().
   *  Paint-only like scale(): animating one never relayouts, and the
   *  content picture replays under the new transform.
   *
   *  Bars, wipes, meters, cooldown sweeps, drain rings and "slide this
   *  piece into its slot" are the most common animated primitive a UI
   *  has, and not one of them is uniform. Without these the idiom was a
   *  full-width fill inside a clip translated by -(1 - fraction) * width,
   *  which only survives while the fill happens to be a gradient along
   *  the OTHER axis. Set transformOrigin() to pin the growing edge —
   *  `transformOrigin(0, 0.5f).scaleX(&fraction)` grows a bar rightward
   *  from its left edge. */
  Element& scaleX(motion::Animatable<float> factor);
  Element& scaleY(motion::Animatable<float> factor);
  /** Shear, in degrees, about the transform origin. Paint-only like
   *  rotate/scale: animating a skew never relayouts, and content pictures
   *  replay under the new transform.
   *
   *  skewX slants verticals, skewY slants horizontals. The sense is
   *  screen-space, y down: a POSITIVE skewX shifts points further down the
   *  node further right, so the shape's top leans LEFT — the italic
   *  forward lean is a NEGATIVE skewX. */
  Element& skewX(motion::Animatable<float> degrees);
  Element& skewY(motion::Animatable<float> degrees);
  // Integer-literal sugar (rotate(-8) etc. — int doesn't convert into the
  // Animatable variant on its own, and the resulting error is unreadable).
  // std::integral-constrained so FLOAT calls can never land here (a plain
  // int overload would capture them via the standard float→int conversion
  // and recurse); the Animatable is constructed explicitly for the same
  // reason.
  template <std::integral T>
  Element& opacity(T v) {
    return opacity(motion::Animatable<float>((float)v));
  }
  template <std::integral T>
  Element& translateX(T v) {
    return translateX(motion::Animatable<float>((float)v));
  }
  template <std::integral T>
  Element& translateY(T v) {
    return translateY(motion::Animatable<float>((float)v));
  }
  template <std::integral T>
  Element& rotate(T deg) {
    return rotate(motion::Animatable<float>((float)deg));
  }
  template <std::integral T>
  Element& scale(T f) {
    return scale(motion::Animatable<float>((float)f));
  }
  template <std::integral T>
  Element& scaleX(T f) {
    return scaleX(motion::Animatable<float>((float)f));
  }
  template <std::integral T>
  Element& scaleY(T f) {
    return scaleY(motion::Animatable<float>((float)f));
  }
  template <std::integral T>
  Element& skewX(T deg) {
    return skewX(motion::Animatable<float>((float)deg));
  }
  template <std::integral T>
  Element& skewY(T deg) {
    return skewY(motion::Animatable<float>((float)deg));
  }
  Element& transformOrigin(float fx, float fy);
  /** Pixel-valued transform origin (node-local px) — for pivots that
   *  aren't a fraction of THIS node's box, e.g. zooming a window that
   *  lives inside a full-canvas overlay around its own center. */
  Element& transformOriginPx(SkPoint p);
  Element& zIndex(int z);

  // ---- depth: the CSS 3D model over the 2D tree ----
  //
  // A node is a PLANE. These lanes turn it and move it in depth, and the
  // node projects its plane onto the one its parent paints on — one 4x4
  // per node, flattened at paint, so tree order stays draw order and
  // everything the node holds (its fill, its text, its children, its
  // caches) lives in the plane exactly as it did before. Paint-only like
  // the 2D lanes: animating one never relayouts, and a settled node's
  // recording is taken in its own plane and replayed through the
  // projection. The frame is CSS's: x right, y down, and +z TOWARD the
  // viewer, so a positive `translateZ` under a `perspective` comes closer
  // and grows.
  //
  // The three rotations compose as CSS's `rotateX() rotateY() rotateZ()`
  // list — X outermost — and then scale and skew, about the transform
  // origin, exactly where the 2D `rotate → scale → skew` stack stands.
  // What none of this is: a scene. Two planes never intersect, nothing is
  // lit, and a depth is not a position in a world — a set (SigilWorld) is
  // where that lives.

  /** Turn the plane about its horizontal axis, in degrees: positive tips
   *  the bottom edge toward the viewer. */
  Element& rotateX(motion::Animatable<float> degrees);
  /** Turn the plane about its vertical axis, in degrees: positive tips the
   *  left edge toward the viewer — the card-flip lane. */
  Element& rotateY(motion::Animatable<float> degrees);
  /** The rotation `rotate()` already is, under its 3D name — the SAME lane,
   *  so `rotate(30).rotateZ(45)` is one setting made twice, not two turns. */
  Element& rotateZ(motion::Animatable<float> degrees);
  /** Move the plane along the viewing axis, in px: positive is toward the
   *  viewer. Invisible without a `perspective` above it — an orthographic
   *  projection drops z — and inside a shared space it is what puts a face
   *  in front of another. */
  Element& translateZ(motion::Animatable<float> px);
  /** Scale along the viewing axis, about the transform origin. Nothing in
   *  the node's own plane moves (its z is zero); what it scales is the
   *  depth of the children it hosts in a shared space. */
  Element& scaleZ(motion::Animatable<float> factor);
  /** THE VIEW, declared on an ancestor: this node's children are seen from
   *  a viewer `distancePx` in front of the plane, so a child turned or
   *  moved in depth converges toward the perspective origin as it recedes.
   *  Applies to the children, never to this node itself, as CSS's
   *  `perspective` property does; 0 is no perspective — an orthographic
   *  projection where a turned plane only narrows. A shared space carries
   *  the view of the ancestor that declared it down to every plane in the
   *  space. Bindable, so a dolly is a bound distance. */
  Element& perspective(motion::Animatable<float> distancePx);
  /** Where the viewer stands over the plane, as fractions of this node's
   *  box — the vanishing point of the view `perspective()` declares. The
   *  centre by default. */
  Element& perspectiveOrigin(float fx, float fy);
  /** The pivot the lanes turn about, with a depth: `fx, fy` are the
   *  fractions `transformOrigin()` takes and `zPx` is a distance in front
   *  of the plane (positive toward the viewer). A card that swings on a
   *  hinge behind it turns about a negative z. */
  Element& transformOrigin3d(float fx, float fy, float zPx);
  /** THE SHARED SPACE: this node's children keep the depth their own
   *  lanes give them — their planes compose with this node's rather than
   *  flattening into it — and are painted back to front by the depth of
   *  each child's centre, whatever order they were declared in. A cube is
   *  six children of one such node. Nested `preserve3d()` compounds the
   *  space; a child that does not declare it ends the space at its own
   *  plane, and its children are flat inside it.
   *
   *  Two rules, both stated so they are not discovered: PLANES DO NOT
   *  INTERSECT — a child crossing another is drawn whole, in the order
   *  their centres sort — and a node that composites as a group cannot
   *  host a space. A `clip()`, an opacity below 1, a blend that is not
   *  source-over, an `effect()`, a `backdrop()`, a `mask()`, a coverage
   *  boundary or an explicit `cache(Cache::Texture)` / `Cache::Group`
   *  flattens the node exactly as CSS's grouping properties do: its
   *  children are then projected one by one onto its plane, in tree
   *  order, with no depth between them. The node's own paint stands at the
   *  front of its own plane and is drawn before its children. */
  Element& preserve3d(bool on = true);
  /** Whether the back of this node's plane is drawn when a depth lane has
   *  turned it away — see `Backface`. Visible by default. */
  Element& backface(Backface facing);
  template <std::integral T>
  Element& rotateX(T deg) {
    return rotateX(motion::Animatable<float>((float)deg));
  }
  template <std::integral T>
  Element& rotateY(T deg) {
    return rotateY(motion::Animatable<float>((float)deg));
  }
  template <std::integral T>
  Element& rotateZ(T deg) {
    return rotateZ(motion::Animatable<float>((float)deg));
  }
  template <std::integral T>
  Element& translateZ(T px) {
    return translateZ(motion::Animatable<float>((float)px));
  }
  template <std::integral T>
  Element& scaleZ(T f) {
    return scaleZ(motion::Animatable<float>((float)f));
  }
  template <std::integral T>
  Element& perspective(T px) {
    return perspective(motion::Animatable<float>((float)px));
  }

  // ---- derive phase (inputs are resolved geometry) ----
  /** Text leaves only: flow this paragraph around the keyed node, with
   *  @p margin px of standoff.
   *
   *  A target that declares a SILHOUETTE — a `shape()`, or a routed
   *  connector or rail — is subtracted by that outline, concavities and
   *  holes included, so text runs into the notch of a star and through the
   *  ring of an annulus. A target that declares none is subtracted by its
   *  BOX, which is the whole of what it occupies. The margin is the same
   *  standoff from whichever edge is being subtracted; corner radii round
   *  the fill rather than the outline and do not count as a silhouette.
   *
   *  Resolved as a bounded second layout pass, so a target that moves
   *  re-cuts the lines under it; a reference to self or a descendant is
   *  ignored (cycle guard). Call repeatedly to weave around several
   *  elements. */
  Element& flowAround(std::string_view key, float margin = 0.0f);

  // ---- content ----
  /** Image leaves only: draw this sub-rect of the asset (atlas / sprite
   *  regions, in source pixels) instead of the whole image. Strictly
   *  constrained — neighboring atlas cells never bleed in. */
  Element& region(SkRect sourceRect);

  /** APPENDS a text-fx track to a text() element (see Track): which
   *  glyphs, what deviation from rest, how the beats spread, and the
   *  master progress that drives it.
   *
   *  Call it once per track. Several tracks compose per glyph — dx/dy and
   *  rotation ADD, scale and alpha MULTIPLY — in the order they were
   *  declared, and each keeps its own transition slot, so retargeting one
   *  track's progress leaves the others running. */
  Element& fx(Track track);
  /** VariationDrive (text leaves): drive a variable-font axis from a
   *  bound Output at DRAW time — paint-only volatility, no reshape, no
   *  relayout. The paint phase probes the node's fonts once per axis:
   *  an advance-variant axis (wght on most fonts) is REFUSED with a debug
   *  warning and the text draws at its shaped coordinates — drive GRAD
   *  (the advance-invariant weight) or re-render discretely instead.
   *
   *  SUGAR over `fx()`: it appends a whole-text track whose deviation is
   *  `GlyphMod::axis`, so a driven axis composes with entrances, loops and
   *  every other track instead of being a second text path they would hide.
   *  Being a track, it also draws through the batched glyph path, so a
   *  span's band stands at its rest placement while the letters move.
   *
   *  A BARE OUTPUT and not an animatable, deliberately: a drive IS a live
   *  binding — a constant axis coordinate is `style.variations`, not this
   *  — and the effect's identity is keyed on WHICH Output feeds it, so two
   *  drives of one axis from two Outputs cannot prune onto each other. */
  Element& variationDrive(const char (&tag)[5],
                          const choreograph::Output<float>* value);

  /** A SIBLING ANCHORED TO A UNIT OF THE TEXT: a caret, a callout, a tick,
   *  a rule standing at a word's edge. @p what becomes a child of this text
   *  node whose PARENT BOX is the rect @p where resolves to, so it is
   *  written in exactly the placement longhand a `positioned()` child takes
   *  — px or pct `left`/`top`/`right`/`bottom`/`width`/`height`, measured
   *  inside that rect, and free to sit outside it:
   *
   *      text(line, style)
   *          .mark(weave::sel::word(3), box().left(0).top(pct(100))
   *                                   .width(pct(100)).height(2)
   *                                   .fill(Fill::color(ink)))
   *
   *  With no dims at all the mark simply IS the rect. A mark carrying no
   *  key is given one from its declaration order, so it prunes; a mark that
   *  carries one keeps it, and that key is what `Composer::bounds` and
   *  `hitTest` answer for.
   *
   *  A MARK IS NOT A `weave::rich().slot()`. A slot reserves space INSIDE the
   * flow — the line breaks around it, it moves the line's height, and the type
   *  after it starts further along. A mark reserves nothing: the text is
   *  laid out as though the mark were not there and the mark is placed on
   *  the result, so it may overlap the letters, straddle several, or hang
   *  outside the node's box entirely. Reserve a box for content that is
   *  part of the sentence; mark the type that is already there.
   *
   *  A SELECTOR RESOLVING SEVERAL UNITS GIVES ONE RECT, the union of every
   *  glyph it addressed — `weave::sel::each(weave::unit::Word)` therefore
   * anchors a mark to the whole paragraph, which is a rect and rarely the
   * intent. One mark is one element with one identity and one box; to mark each
   * of several units, write one mark per unit. A selector resolving NOTHING —
   *  including a name no run carries and a pattern that does not compile —
   *  places nothing and warns once, on the silent-no-op family's terms.
   *
   *  THE RECT IS THE REST RECT: where the LAYOUT put those glyphs, not
   *  where an `fx()` track has thrown them this frame. The mark therefore
   *  follows a reflow, a restyle and a resize exactly as the letters do,
   *  and stands still while a cascade deviates them. That is deliberate on
   *  both counts — a deviation is per glyph and per track and several
   *  compose, so there is no one place a moving unit "is", and a mark
   *  re-placed at paint would make the layout depend on the frame. For a
   *  mark that must RIDE the motion, read `Composer::beatsOf` and drive the
   *  mark's own transform from it.
   *
   *  IT NEEDS NO REACH. A track declares one because the glyphs it throws
   *  are painted by the text node itself; a mark is a CHILD, and the
   *  recording cull already grows by the union of its children, so a mark
   *  hanging above the line is not truncated.
   *
   *  ON A PATH RUN (`onPath`) the rect is on the CURVE: the axis-aligned
   *  bound of the advance boxes where the baseline placed them, the same
   *  placement `beatsOf` reports. The rest rect there is where the run
   *  RESTS on the curve — a run driven along its baseline (`at` bound) is
   *  a paint-time deviation like any track's, and the same rule applies:
   *  read `beatsOf` to ride it. */
  Element& mark(sigil::weave::Selector where, Element what);

  /** Text leaves only: how lines sit inside the node's width (SigilWeave
   *  TextAlignment — kStart/kCenter/kEnd/kJustify). Meaningful when the
   *  node is WIDER than its text (explicit width, grow, stack stretch);
   *  intrinsic-width text has nothing to align within. */
  Element& textAlign(sigil::weave::TextAlignment a);

  /** Text leaves only: THE FRAME THIS ONE FILLS INTO — the next link of a
   *  chain over one `weave::Story`.
   *
   *      root.child(frame(article).key("a").thread("b").width(Dim(280)))
   *          .child(frame(article).key("b").thread("c").width(Dim(280)))
   *          .child(frame(article).key("c").width(Dim(280)));
   *
   *  Each frame fills from where the one before it stopped, so the cut
   *  moves as any frame's measure moves. A frame that threads somewhere
   *  has a remainder BY DESIGN: overflow is the normal case there and
   *  draws no marker, whatever ellipsis the leaf asked for. The last frame
   *  of a chain is the one that threads nowhere, and it keeps its.
   *
   *  A frame nothing threads into is a chain's head and starts at the
   *  story's first word. A chain that closes on itself stops where it
   *  closes, as a cyclic borrow does. */
  Element& thread(std::string_view key);

  /** Text leaves only: A READING SET BESIDE THE TYPE — furigana over a
   *  compound, emphasis dots down a column, a gloss under a phrase.
   *
   *      text(passage, body)
   *          .writingMode(WritingMode::kVerticalRL)
   *          .annotate({.where = weave::sel::text(u8"漢字"),
   *                     .unit = weave::unit::Word,          // group ruby
   *                     .readings = {u8"かんじ"},
   *                     .style = furigana})
   *
   *  A reading is PART OF THE TEXT rather than a thing standing next to
   *  it: where it reserves, the band it occupies goes into the base's
   *  strut BEFORE the base is broken, so the base is laid out once with the
   *  room already there and the readings are then placed on the result.
   *  `kit::annotate` is the other half of the idea, and marginalia, word
   *  labels and callouts belong there — a sibling that reserves nothing
   *  and reads the finished text.
   *
   *  Mono, group and jukugo ruby are the `unit` choice, and a base that
   *  breaks across a line or a column splits its reading with it, in
   *  proportion to the base's advance either side. See `Annotation`. */
  Element& annotate(Annotation reading);

  /** Text leaves only: how each BLOCK of this passage is set — one entry
   *  per block, in block order, a block being the text between two hard
   *  breaks.
   *
   *      text(rich(body).add(u8"A heading\nand its body text\nand more"))
   *          .paragraphs({headingStyle, bodyStyle})
   *
   *  A block past the end of the list is set by this leaf's own alignment,
   *  justification, hyphenation and tab stops alone, so one entry styles
   *  the first block and leaves the rest plain — which is what a heading
   *  over a body wants. `sigil::weave::ParagraphStyle` carries the leading,
   *  the air before and after, the four indents, the keeps, and whichever
   *  of the four layout-wide settings the block overrides; SigilWeave's
   *  README is the canon for what each one means. */
  Element& paragraphs(std::vector<sigil::weave::ParagraphStyle> blocks);
  /** The same, by NAME, resolved through the `ParagraphStyleSet` the
   *  environment offers (`env::Provide<sigil::weave::ParagraphStyleSet>`).
   *
   *  Resolution happens where this is written, inside the author's describe
   *  scope, so the finished description holds real styles and depends on no
   *  scope that has since ended — the same discipline `weave::rich().add(text,
   *  name)` follows for character styles. A name the set does not carry
   *  resolves to the set's base entry, and with no set in scope every name
   *  resolves to a plain block. */
  Element& paragraphs(std::span<const std::string_view> names);
  /** Every block of this passage set alike. */
  Element& paragraph(sigil::weave::ParagraphStyle style);

  /** Text leaves only: WHERE THE FIRST BASELINE SITS below the top of this
   *  leaf's box — the first line's own ascent (the default), its cap
   *  height, its x-height, its whole pitch, or `offset` outright. Every
   *  later baseline follows at its own block's pitch, so this moves the
   *  whole passage rather than its first line. Two leaves of different type
   *  seated on cap height start their text at the same height, which is
   *  what a page ruled against a grid needs and an ascent cannot give. */
  Element& firstBaseline(sigil::weave::FrameOptions::FirstBaseline rule,
                         float offset = 0);
  /** Text leaves only: what becomes of the room left over down this leaf's
   *  box — nothing (the default), half above and half below, all above, or
   *  spread BETWEEN the lines as extra leading, at most
   *  `maximumInterlineSpacing` per gap. It reads the leaf's resolved
   *  height, so a leaf sized by its own content has nothing left over and
   *  nothing to spend. */
  Element& distribute(sigil::weave::FrameOptions::Distribute rule,
                      float maximumInterlineSpacing = 0);
  /** Text leaves only: how a justified line spends what it has — the word
   *  spacing it aims at and its elasticity, then letter spacing, then a
   *  horizontal scale on the glyphs, each bounded by its own two limits.
   *  Inert unless the passage justifies. */
  Element& justification(sigil::weave::JustificationOptions spec);
  /** Text leaves only: where a tab takes the pen, what the stop pins there
   *  — the start of its cell, its end, its centre, or a named character —
   *  and the leader set across the gap it opened. */
  Element& tabStops(sigil::weave::TabStopOptions stops);

  /** Text leaves only: AN INPUT OF THIS PASSAGE IS MOVING — a measure that
   *  animates, a frame that grows, content that changes from one frame to
   *  the next — so this layout is one of a run of them rather than an
   *  answer somebody asked for once.
   *
   *      text(caption, body).width(Dim(slider)).live(true, 2000.0f)
   *
   *  It buys two things. The break decisions of a block set in a uniform
   *  measure are kept and reused, keyed on the words and on the measure
   *  taken to the whole pixel below it, so a measure already crossed costs
   *  no break decision at all. And the block is broken against the measure
   *  alone rather than against the frame's supply of lines, so a frame
   *  that only changes in DEPTH changes which lines it holds and never
   *  where they break. `Composer::settling` reports what a frame actually
   *  got for it.
   *
   *  `budgetMicroseconds` is the floor under a frame the optimizing
   *  breaker cannot finish in time: a block past it is filled greedily for
   *  that frame and counted as a degrade, and everything is back the next
   *  frame the budget is met. 0 is no floor.
   *
   *  NOTHING INFERS THIS. A live layout answers the overflow tail
   *  differently from a settled one — it is broken against the measure
   *  rather than against the lines the frame has left — so a guess would
   *  change the setting of a page that never moves. A passage that moves
   *  says so. */
  Element& live(bool on = true, float budgetMicroseconds = 0);

  /** Text leaves only: WHICH CHARACTERS MAY NOT STAND AT A LINE'S EDGE —
   *  kinsoku shori, as a house's own table over whatever the line-break
   *  locale already prohibits. The prohibition is settled during
   *  segmentation, so both breakers obey it and neither learns a rule.
   *  `sigil::weave::kit::kinsoku()` is the stock table and a caller's own
   *  is its peer. */
  Element& kinsoku(sigil::weave::KinsokuTable table);
  /** Text leaves only: HOW FAR A CHARACTER MAY STAND OUTSIDE THE MEASURE,
   *  as a fraction of its own advance — optical margin alignment along a
   *  line, burasagari down a column. It is the LINE EDGE and has nothing
   *  to do with the hanging indent, which is a negative
   *  `ParagraphStyle::indent.firstLine`. `sigil::weave::kit::hanging()` is
   *  the stock table. */
  Element& hanging(sigil::weave::HangingTable table);
  /** Text leaves only: HOW MUCH ROOM STANDS BETWEEN TWO ADJACENT
   *  FULL-WIDTH CHARACTERS, by the class of each, as a fraction of the em
   *  — negative closes the gap up, which is what nearly every entry of a
   *  real table does. `tsume` closes the gap after every full-width
   *  character the table gives no class of its own, on top of that. Both
   *  apply where two characters meet across a break opportunity; two
   *  characters shaped inside one word are the face's and the shaper's. */
  Element& mojikumi(sigil::weave::MojikumiTable table, float tsume = 0);
  /** Text leaves only: ROOM BESIDE EVERY LINE of this passage, over and
   *  above the leading — `before` above a line and right of a column,
   *  `after` below one and left. It is a layout input: the room is in the
   *  strut before anything is broken. `annotate` reserves its own band on
   *  top of this, so a passage that only carries readings needs none of
   *  this. */
  Element& reserve(sigil::weave::ReservedBand band);
  /** Text leaves only: THE TAILORING THE LINE-BREAK ANALYSIS RUNS UNDER —
   *  `"ja@lb=strict"` is the strict Japanese rule set a printed page is
   *  set under, `"zh@lb=loose"` the loose Chinese one. A tailored
   *  prohibition is a boundary that never opens, so nothing downstream
   *  learns a rule. It belongs to the Paragraph rather than to the layout
   *  options, so it is a field-masked override like `writingMode`: a
   *  locale nobody names leaves a passed-in paragraph's own standing. */
  Element& lineBreakLocale(std::string_view locale);

  /** Text leaves only: lay this passage out in VERTICAL-RL CJK columns
   *  (`sigil::weave::WritingMode::kVerticalRL`) instead of horizontal
   *  lines. Characters run top to bottom, columns advance RIGHT TO LEFT
   *  from the node's right edge, and the node's width is the measure the
   *  columns wrap within.
   *
   *  Per character the mode is UTR#50's: ideographs stand upright and take
   *  their `vert` forms, Latin lies on its side. A run that wants
   *  otherwise says so in its own style — `TextStyle::shaping.verticalForm`
   *  is `kUpright`, `kRotated` or `kTateChuYoko` — on a `weave::rich()` run or
   *  through `spanStyle`.
   *
   *  A vertical leaf MEASURES ON THE OTHER AXIS: its main extent is its
   *  HEIGHT, and its intrinsic width is one column pitch per column. It has
   *  no baseline — the reading axis is y — so for `Align::Baseline` it
   *  reports its first character's baseline, which lines a column's opening
   *  character up with a horizontal neighbour's first line.
   *
   *  `onPath` IGNORES IT: a path run's baseline is its own geometry and has
   *  no columns to advance. Setting both warns once and the path wins. */
  Element& writingMode(sigil::weave::WritingMode mode);

  // ---- span restyling: the type treatment, addressed by selector -------
  //
  // The same `sel::` vocabulary the fx() tracks address glyphs with, used
  // to say what a range LOOKS LIKE rather than how it moves. Each verb
  // takes an ordered list — call any of them as many times as the passage
  // needs — and a LATER DECLARATION WINS wherever two overlap, so a broad
  // rule followed by a narrow exception reads in the order it is written.
  //
  // They apply to every content form alike: plain `text(utf8, style)`,
  // `weave::rich()` spans, and the `shared_ptr<Paragraph>` overload, because
  // all three are one materialized paragraph by the time a restyle runs.
  //
  // The two are ordered by WHAT THEY ARE ALLOWED TO DISTURB. `spanPaint`
  // repaints and nothing else. `spanStyle` may change anything, and
  // re-shapes to do it — except a change of advance-invariant axes alone,
  // which it carries to the glyphs at draw time with the pen positions
  // standing.
  //
  // …which is why "later wins" holds PER DIMENSION where the two meet: the
  // PAINT of a range is `spanPaint`'s to say, so a `spanStyle` over text an
  // earlier `spanPaint` coloured applies its other dimensions and leaves
  // that colour standing. Either order therefore does the same thing, and
  // neither verb has to know what the other declared. A `spanStyle` alone
  // paints with the style it is given, as ever.
  //
  // Both run on the PARAGRAPH and resolve their selection as TEXT RANGES,
  // not glyphs: `weave::sel::text` and
  // `weave::sel::regex` go through weave's query layer, `weave::sel::word`,
  // `weave::sel::words`, `weave::sel::sentence` and `weave::sel::range` through
  // the paragraph's own structure, and `weave::sel::line` through the layout.
  // `weave::Selector::take` and `weave::Selector::drop` slice GLYPHS inside a
  // unit, which a text range cannot express — an `weave::sel::each` selector
  // restyles its whole units here, and the slice is ignored with a warning.
  //
  // A `weave::sel::line` restyle addresses THE LAYOUT OF THE TEXT BEFORE THE
  // RESTYLE, and costs a second layout pass. It does not chase its own
  // result: a `spanStyle` on a line that moves the line breaks leaves the
  // selection where the first breaking put it.

  /** Text leaves only: repaint the range this selector finds — a colour, a
   *  shader, an underline, an added glow pass. PAINT ONLY, so it NEVER
   *  re-shapes and never relayouts: the glyphs are exactly the glyphs the
   *  unrestyled text shaped, drawn differently. The paint it declares is
   *  the one the range keeps: a `spanStyle` on the same text after it
   *  restyles everything else and leaves this colour alone. */
  Element& spanPaint(sigil::weave::Selector where,
                     sigil::weave::PaintStyle paint);
  /** Text leaves only: restyle the range this selector finds with a
   *  complete TextStyle — a different face, size, weight or tracking as
   *  well as paint. Re-shapes, and only the words the range covers: the
   *  shaping cache is content-addressed, so the rest of the paragraph is
   *  reused as it stands.
   *
   *  A style that differs from the text it covers ONLY IN VARIABLE-FONT
   *  AXES the face carries advance-invariantly — a grade (GRAD) thickens a
   *  letter without moving the letter after it — does not re-shape at all:
   *  the coordinate is held on the glyphs at draw time, so the layout the
   *  paragraph already has stands to the pen position. It is then a track
   *  carrying `TextEffect::variableAxis`, and it inherits what that means:
   *  the same size-scaled snapping ladder a driven axis takes, composition
   *  with entrances and loops rather than being hidden by them, and the
   *  batched glyph draw, where a span style's band stands at its rest
   *  placement while the letters move. An axis the face moves advances on,
   *  an axis the restyle drops, or any other difference is a reshape, and
   *  a later reshaping restyle over the same text keeps the earlier one a
   *  reshape too, so the later one is the one that stands. A `spanPaint`
   *  declared EARLIER over the same text keeps its colour: this style's own
   *  paint stands only where none reached. */
  Element& spanStyle(sigil::weave::Selector where,
                     sigil::weave::TextStyle style);

  // ---- layout options, fluently ----------------------------------------
  //
  // The general knobs of `weave::ParagraphLayoutOptions`, as setters that
  // work on every content form. The rest of that struct — justification
  // elasticity, Knuth-Plass tolerance, tab stops, line-metric overrides —
  // stays behind the `shared_ptr<Paragraph>` overload, which takes the
  // whole options value.
  //
  // ON THE PARAGRAPH OVERLOAD THESE OVERRIDE FIELD BY FIELD, and only the
  // fields actually set: options passed to `text(paragraph, options)` stand
  // for everything a setter did not name. Setting none of them leaves the
  // passed options untouched.

  /** Text leaves only: greedy (the fast default) or Knuth-Plass optimal
   *  line breaking. */
  Element& lineBreak(sigil::weave::LineBreakStrategy strategy);
  /** Text leaves only: whether soft-hyphen break opportunities are taken,
   *  and what Knuth-Plass charges for taking them. */
  Element& hyphenation(sigil::weave::HyphenationOptions options);
  /** Text leaves only: the marker appended to the last line when the text
   *  overflows its geometry. Empty disables it. */
  Element& ellipsis(std::u8string_view marker);
  /** Text leaves only: use at most this many lines (CSS line-clamp); the
   *  rest reports as overflow and `ellipsis()`, when set, lands on the
   *  clamped line. 0 is unclamped. */
  Element& maxLines(int lines);
  /** Text leaves only: how a paragraph-final or hard-break-final line sits
   *  under `TextAlignment::kJustify` — its own alignment, or `justify` to
   *  stretch it to the full measure like every other line. Inert under the
   *  other alignments, which have no special last line. */
  Element& lastLine(sigil::weave::TextAlignment alignment,
                    bool justify = false);

  /** Text leaves only: paint the GLYPHS with this material, mapped to
   *  TEXT-METRIC space — the material's unit square lands with x across
   *  the widest line and y from the first line's CAP TOP (real cap height
   *  from the face's metrics) to the last line's baseline. That is what
   *  makes chrome type work at any size: author the ramp once in [0,1] and
   *  its horizon crosses the capitals whatever the font size, with no
   *  hand-positioned gradients.
   *
   *  Supersedes the style's foreground paint. A live material re-resolves
   *  per frame. COMBINES with `fx()`: a letter in flight is painted with
   *  the metric material exactly as a resting one is, so a chrome
   *  wordmark can also be a staggered entrance. */
  Element& textFill(material::skia::Paint m);

  /** Strokes the GLYPHS, under the fill — engraved display type, an
   *  outlined label, a caption that has to survive over an image.
   *
   *  NOT `Element::stroke()`, which dresses the node's BOX outline and is
   *  a different thing entirely. This one thickens the letterforms.
   *
   *  Composes with `textFill()` — the stroke is a pass beneath whatever
   *  fills the letterforms — with the style's own underlays and overlays,
   *  which it joins rather than replaces, and with `fx()`, which carries
   *  every pass along as the glyph moves. */
  Element& textStroke(float width, Fill fill);
  /** Text leaves only: lay the run out along a PATH instead of a line.
   *  See TextPath. Single-line runs; the node's own box still sizes the
   *  path, so give it the box the curve should be inscribed in
   *  (`kit::disc`-style: width(2r).height(2r).centerAt(centre)).
   *
   *  Interacts with the rest of the text surface the way you would hope:
   *  the style's underlays, overlays and decorations all still draw, and
   *  `fx()` wins if both are set (a track draws its own batched buckets
   *  along the flow, not along the curve). */
  Element& onPath(TextPath spec);
  /** Text leaves only: THIS LEAF AS IT STANDS AT REST, as a second element
   *  that can stand beside it in one tree — the same content, style,
   *  measure and layout, carrying nothing that deviates or restyles a
   *  glyph at paint time: no `fx()` tracks, no span restyles, and no
   *  children, since a text node's children are its marks and its slot
   *  mounts and both are already on screen once. A slot's reserved RUN
   *  stays — it is content, and it holds the same space in the copy's
   *  paragraph, which is what keeps the two copies' letters in the same
   *  places. The key takes `-rest` after it (a keyless original leaves the
   *  copy keyless), so both are addressable and both prune; the ink is
   *  left to the caller, which is what `textFill` is for.
   *
   *  A rest pose is what a track's per-glyph deviation is measured
   *  against, and `kit::restGhost` draws it under the moving copy.
   *  Anything but text warns once and comes back as a plain copy. */
  [[nodiscard]] Element atRest() const;

  // ---- identity, caching, transitions ----
  /** The author-owned identity: what the reconciler matches a child by
   *  across describes, and what `connector`/`rail`/`spans::fit` borrow
   *  geometry by.
   *
   *  ON A `slot()` IT RENAMES THE MOUNT. A slot's name IS its key — there
   *  is no second field — so `slot("hud").key("panel")` produces a slot
   *  called "panel", and `renderSlot("hud")` then finds nothing and does
   *  nothing. It warns once, in Release too, because the visible symptom
   *  is an empty region rather than an error. */
  Element& key(std::string_view k);
  Element& cache(Cache c);
  /** Texture-bake resolution multiplier (Cache::Texture only; 0.1–1).
   *  The bake rasterizes at `factor` times the device scale and the blit
   *  scales it back up with linear sampling.
   *
   *  ALMOST ALWAYS THE WRONG LEVER. It cheapens the BAKE, which happens
   *  once, and taxes every BLIT with an upscaling resample, which happens
   *  forever — backwards for the bake-once/blit-every-frame node
   *  Cache::Texture exists for. Reach for it only when something forces
   *  FREQUENT re-bakes (a live material stepping at its own rate, a
   *  resizing node) AND the content is soft enough to survive the
   *  resample. Sharp text and 1 px hairlines never belong under a reduced
   *  bake. */
  Element& bakeScale(float factor);
  Element& transition(
      motion::Transition t);  // node default for plain constants
  /** Container stagger: child i's subtree enters with an EXTRA
   *  order·each delay on all its animate() mount transitions, compounding
   *  through nested staggered containers. `from` picks the origin — Start
   *  (declaration order), End (last child first, a bottom-up cascade
   *  without reordering paint), Center (ripple outward). One call, no
   *  per-child delay arithmetic:
   *  `column().staggerChildren(33ms, motion::Spread::From::End)
   *  .children(rows)`. */
  Element& staggerChildren(
      std::chrono::milliseconds each,
      motion::Spread::From from = motion::Spread::From::Start);

  // ---- composition ----
  Element& child(Element e);
  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, Element>
  Element& children(R&& range) {
    for (auto&& e : range) child(std::move(e));
    return *this;
  }

  /** @private reconciler access */
  const std::shared_ptr<detail::ElementNode>& node() const {
    return m_node.value;
  }
  explicit Element(std::shared_ptr<detail::ElementNode> n)
      : m_node(std::move(n)) {}

 private:
  /** Register a decoration's declared derive borrows (see
   *  BorrowingDecoration). Every slot that takes a Decoration must call
   *  this: a borrow honoured in some slots and not others resolves to
   *  nothing in the others, and draws nothing, with no diagnostic. */
  void claimBorrows(const Decoration& d);

  /** The shared body of stroke(Spans,…) and background(Spans,…). `half` is
   *  a detail::StrokePass::Half, passed as an int so the exported header
   *  does not have to name an internal enum. */
  Element& addSpanPass(Spans where, Decoration what, std::string name,
                       int half);

  /** Bind the optional LOCAL label an unqualified mark slot took to the
   *  mark it just appended, for `parts::named()`. `slot` is a
   *  detail::MarkSlot as an int, for the same reason addSpanPass takes
   *  its half that way. */
  void labelMark(int slot, size_t index, std::string name);

  /** Copy-on-write handle: Element stays a cheap value, but fluent mutation
   *  can never alter another copy or a description retained by Composer. */
  struct NodeHandle {
    explicit NodeHandle(std::shared_ptr<detail::ElementNode> node)
        : value(std::move(node)) {}

    detail::ElementNode* operator->();
    const detail::ElementNode* operator->() const;

    std::shared_ptr<detail::ElementNode> value;
  };

  NodeHandle m_node;
};

}  // namespace sigil::compose
