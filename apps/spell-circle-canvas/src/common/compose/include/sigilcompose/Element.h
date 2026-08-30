#pragma once

/** @file
 * SigilCompose Element — the value description of one node, the factories
 * that start one (box, stack, positioned, text, image, custom, layout,
 * slot, memo), and the one-shot verbs that take a tree without a live
 * composer: snapshot, the `tiles::` slicing of its picture, measure,
 * metrics, measureRun and runPens.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPath.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/style/Style.h>

#include <any>
#include <chrono>
#include <functional>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "sigilcompose/Layout.h"
#include "sigilcompose/Mask.h"
#include "sigilcompose/Motion.h"
#include "sigilcompose/Paint.h"
#include "sigilcompose/Shape.h"
#include "sigilcompose/Stroke.h"
#include "sigilcompose/Text.h"

class SkCanvas;

namespace sigil::image {
class ImageAsset;
}

namespace sigil::weave {
class FontContext;
}

namespace sigil::compose {

namespace detail {
struct ElementNode;
struct Instance;
}  // namespace detail

class Composer;
class Material;

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
   *  `kit::centred()` (kit/Frame.h) builds the rect for the centre-and-size
   *  case. */
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
  Element& fill(Animatable<Fill> f);
  /** Fill with a Material (gradient ramp, blend stack, sprite, SkSL) — the
   *  richer authoring value. A static Material collapses to a Fill, so it
   *  caches and prunes on the same path. See <sigilcompose/Material.h>. */
  Element& fill(Material m);
  /** Solid-color sugar: fill({r,g,b,a}) without the Fill:: ceremony. */
  Element& fill(SkColor4f color) {
    return fill(Animatable<Fill>{Fill::color(color)});
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
  Element& effect(Effect e);
  /** Filters what is already painted beneath this node's bounds before
   *  the node paints (CSS backdrop-filter). Incompatible with
   *  Cache::Texture (the backdrop depends on the live destination);
   *  such nodes fall back to picture caching. */
  Element& backdrop(Effect e);
  Element& opacity(Animatable<float> o);
  Element& blend(SkBlendMode mode);
  Element& translateX(Animatable<float> v);
  Element& translateY(Animatable<float> v);
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
  Element& rotate(Animatable<float> degrees);
  Element& scale(Animatable<float> factor);
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
  Element& scaleX(Animatable<float> factor);
  Element& scaleY(Animatable<float> factor);
  /** Shear, in degrees, about the transform origin. Paint-only like
   *  rotate/scale: animating a skew never relayouts, and content pictures
   *  replay under the new transform.
   *
   *  skewX slants verticals, skewY slants horizontals. The sense is
   *  screen-space, y down: a POSITIVE skewX shifts points further down the
   *  node further right, so the shape's top leans LEFT — the italic
   *  forward lean is a NEGATIVE skewX. */
  Element& skewX(Animatable<float> degrees);
  Element& skewY(Animatable<float> degrees);
  // Integer-literal sugar (rotate(-8) etc. — int doesn't convert into the
  // Animatable variant on its own, and the resulting error is unreadable).
  // std::integral-constrained so FLOAT calls can never land here (a plain
  // int overload would capture them via the standard float→int conversion
  // and recurse); Animatable is constructed explicitly for the same reason.
  template <std::integral T>
  Element& opacity(T v) {
    return opacity(Animatable<float>((float)v));
  }
  template <std::integral T>
  Element& translateX(T v) {
    return translateX(Animatable<float>((float)v));
  }
  template <std::integral T>
  Element& translateY(T v) {
    return translateY(Animatable<float>((float)v));
  }
  template <std::integral T>
  Element& rotate(T deg) {
    return rotate(Animatable<float>((float)deg));
  }
  template <std::integral T>
  Element& scale(T f) {
    return scale(Animatable<float>((float)f));
  }
  template <std::integral T>
  Element& scaleX(T f) {
    return scaleX(Animatable<float>((float)f));
  }
  template <std::integral T>
  Element& scaleY(T f) {
    return scaleY(Animatable<float>((float)f));
  }
  template <std::integral T>
  Element& skewX(T deg) {
    return skewX(Animatable<float>((float)deg));
  }
  template <std::integral T>
  Element& skewY(T deg) {
    return skewY(Animatable<float>((float)deg));
  }
  Element& transformOrigin(float fx, float fy);
  /** Pixel-valued transform origin (node-local px) — for pivots that
   *  aren't a fraction of THIS node's box, e.g. zooming a window that
   *  lives inside a full-canvas overlay around its own center. */
  Element& transformOriginPx(SkPoint p);
  Element& zIndex(int z);

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
   *  Being a track, it also draws through the batched glyph path, which
   *  paints glyphs and not a span's underline or strikethrough. */
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
   *          .mark(sel::word(3), box().left(0).top(pct(100))
   *                                   .width(pct(100)).height(2)
   *                                   .fill(Fill::color(ink)))
   *
   *  With no dims at all the mark simply IS the rect. A mark carrying no
   *  key is given one from its declaration order, so it prunes; a mark that
   *  carries one keeps it, and that key is what `Composer::bounds` and
   *  `hitTest` answer for.
   *
   *  A MARK IS NOT A `rich().slot()`. A slot reserves space INSIDE the flow
   *  — the line breaks around it, it moves the line's height, and the type
   *  after it starts further along. A mark reserves nothing: the text is
   *  laid out as though the mark were not there and the mark is placed on
   *  the result, so it may overlap the letters, straddle several, or hang
   *  outside the node's box entirely. Reserve a box for content that is
   *  part of the sentence; mark the type that is already there.
   *
   *  A SELECTOR RESOLVING SEVERAL UNITS GIVES ONE RECT, the union of every
   *  glyph it addressed — `sel::each(unit::Word)` therefore anchors a mark
   *  to the whole paragraph, which is a rect and rarely the intent. One
   *  mark is one element with one identity and one box; to mark each of
   *  several units, write one mark per unit. A selector resolving NOTHING —
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
  Element& mark(Selector where, Element what);

  /** Text leaves only: how lines sit inside the node's width (SigilWeave
   *  TextAlignment — kStart/kCenter/kEnd/kJustify). Meaningful when the
   *  node is WIDER than its text (explicit width, grow, stack stretch);
   *  intrinsic-width text has nothing to align within. */
  Element& textAlign(sigil::weave::TextAlignment a);

  /** Text leaves only: lay this passage out in VERTICAL-RL CJK columns
   *  (`sigil::weave::WritingMode::kVerticalRL`) instead of horizontal
   *  lines. Characters run top to bottom, columns advance RIGHT TO LEFT
   *  from the node's right edge, and the node's width is the measure the
   *  columns wrap within.
   *
   *  Per character the mode is UTR#50's: ideographs stand upright and take
   *  their `vert` forms, Latin lies on its side. A run that wants
   *  otherwise says so in its own style — `TextStyle::shaping.verticalForm`
   *  is `kUpright`, `kRotated` or `kTateChuYoko` — on a `rich()` run or
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
  // `rich()` spans, and the `shared_ptr<Paragraph>` overload, because all
  // three are one materialized paragraph by the time a restyle runs.
  //
  // The two are ordered by WHAT THEY ARE ALLOWED TO DISTURB. `spanPaint`
  // repaints and nothing else. `spanStyle` may change anything, and
  // re-shapes to do it — except a change of advance-invariant axes alone,
  // which it carries to the glyphs at draw time with the pen positions
  // standing.
  //
  // Both run on the PARAGRAPH and resolve their selection as TEXT RANGES,
  // not glyphs: `sel::text` and
  // `sel::regex` go through weave's query layer, `sel::word`, `sel::words`,
  // `sel::sentence` and `sel::range` through the paragraph's own structure,
  // and `sel::line` through the layout. `Selector::take` and
  // `Selector::drop` slice GLYPHS inside a unit, which a text range cannot
  // express — an `sel::each` selector restyles its whole units here, and
  // the slice is ignored with a warning.
  //
  // A `sel::line` restyle addresses THE LAYOUT OF THE TEXT BEFORE THE
  // RESTYLE, and costs a second layout pass. It does not chase its own
  // result: a `spanStyle` on a line that moves the line breaks leaves the
  // selection where the first breaking put it.

  /** Text leaves only: repaint the range this selector finds — a colour, a
   *  shader, an underline, an added glow pass. PAINT ONLY, so it NEVER
   *  re-shapes and never relayouts: the glyphs are exactly the glyphs the
   *  unrestyled text shaped, drawn differently. */
  Element& spanPaint(Selector where, sigil::weave::PaintStyle paint);
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
   *  batched glyph draw, which paints glyphs and not a span style's
   *  underline or strikethrough. An axis the face moves advances on, an
   *  axis the restyle drops, or any other difference is a reshape, and a
   *  later reshaping restyle over the same text keeps the earlier one a
   *  reshape too, so the later one is the one that stands. */
  Element& spanStyle(Selector where, sigil::weave::TextStyle style);

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
  Element& textFill(Material m);

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
  Element& transition(Transition t);  // node default for plain constants
  /** Container stagger: child i's subtree enters with an EXTRA
   *  order·each delay on all its animate() mount transitions, compounding
   *  through nested staggered containers. `from` picks the origin — Start
   *  (declaration order), End (last child first, a bottom-up cascade
   *  without reordering paint), Center (ripple outward). One call, no
   *  per-child delay arithmetic:
   *  `column().staggerChildren(33ms, Stagger::From::End).children(rows)`. */
  Element& staggerChildren(std::chrono::milliseconds each,
                           Stagger::From from = Stagger::From::Start);

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

// ---- factories -----------------------------------------------------------

Element box();
/** Overlap container: children share the box, painted in (zIndex,
 *  declaration order). EVERY child is absolute — the container sets it
 *  after the child's own layout props, so a child cannot rejoin the flex
 *  flow from inside a stack (it keeps its insets, which is what absolute
 *  is for: `.top(12).right(12)` pins a corner). Mixed flow wants a box
 *  with a stack inside it. */
Element stack();
/** A container whose children carry their OWN rects and skip Yoga
 *  entirely — no flex nodes anywhere below it. Generated geometry
 *  (tilings, lattices, node graphs, fields drawn as real elements) never
 *  wants layout, and this is how to say so.
 *
 *  The child spelling is the ordinary placement longhand:
 *  `.left(x).top(y).width(w).height(h)` — px, or pct() against the
 *  parent's rect; an open width/height with an opposing `.right()`/
 *  `.bottom()` pins the far edge instead; a text leaf with an open
 *  extent measures against its resolved (or the parent's) width.
 *  Rects nest: a child's children position inside ITS rect, the whole
 *  subtree Yoga-free. Everything else about the children is ordinary —
 *  decorations, strokes, masks, transitions, stagger, zIndex, hitTest,
 *  bounds() — because instances still exist; only their layout engine is
 *  gone. A large generated field therefore costs one Yoga node rather
 *  than one per element.
 *
 *  The container ITSELF is an ordinary box in its parent's flow: size it
 *  with dims or insets, because it does NOT auto-size from its children.
 *  NOT SUPPORTED INSIDE, and ignored silently when written: flex props,
 *  centerAt, layout() schemes, flowAround text. Those need the flex
 *  world. */
Element positioned();
Element text(std::u8string utf8, sigil::weave::TextStyle style);
/** Mixed-style text as a COMPARABLE VALUE — see RichText. A re-described
 *  identical value prunes, which is the whole difference between this and
 *  the pointer overload below. */
Element text(RichText spans);
/** Full-control text: a prebuilt Paragraph (spans, mixed styles) plus
 *  ParagraphLayoutOptions (justification, hyphenation, Knuth–Plass,
 *  overflow…). The paragraph is shared by reference: reuse one
 *  shared_ptr across renders to keep shaping caches warm; a fresh
 *  pointer means "content changed" and re-shapes. */
Element text(std::shared_ptr<sigil::weave::Paragraph> paragraph,
             sigil::weave::ParagraphLayoutOptions options = {});
Element image(std::shared_ptr<const sigil::image::ImageAsset> asset);
/** A box whose content is one paint program (≡ box().background(p)).
 *
 *  TWO COSTS AN AUTHOR MUST KNOW. First, it is cached like any static
 *  subtree, so a program that reads the clock — or changes for any other
 *  reason without a re-describe — MUST declare `.cache(Cache::None)`, or
 *  its first frame is recorded and replayed frozen. Second, the program is
 *  an incomparable callable, so the structural prune cannot prove a
 *  custom() node unchanged and it re-records on every render(). Wrap it in
 *  memo(), keep its Element value stable across renders, or use the keyed
 *  overload below. Value decorations (PathFormat, Slice, Shadow) prune
 *  automatically — prefer them for static chrome.
 *
 *  IT SIZES LIKE AN EMPTY BOX, and the failure is silent. Being literally
 *  `box().background(p)`, a custom() leaf has no intrinsic size: dropped
 *  into an `absolute().inset(0)` parent it stretches on the cross axis and
 *  measures ZERO on the main one, so the program runs against a
 *  zero-height context and draws nothing at all. Give it dims, or make it
 *  `absolute().inset(0)` itself — which is exactly what
 *  `instancing::instances()` returns, for exactly this reason. */
Element custom(PaintProgram program);
/** The PRUNABLE spelling: the key is the program's IDENTITY, on the same
 *  author contract as `shapes::parametric(key, …)`. One key must always
 *  name one drawing at one parameterisation — fold anything that varies
 *  into the key, or two different pictures compare equal and the stale one
 *  replays. Two describes with equal keys compare EQUAL and the node
 *  prunes; the unkeyed form above re-records every render(). */
Element custom(std::string_view key, PaintProgram program);

/** A container whose children are placed by @p scheme instead of
 *  flexbox (nests freely inside flex and vice versa). The container
 *  itself is sized by its own dims/flex; children are measured by
 *  Yoga/SigilWeave, then positioned and sized by scheme.place() in a
 *  bounded second layout pass. */
template <LayoutScheme L>
Element layout(L scheme);

namespace detail {
Element makeLayout(
    std::function<std::vector<SkRect>(const LayoutInput&)> place);
}  // namespace detail

template <LayoutScheme L>
Element layout(L scheme) {
  return detail::makeLayout(
      [s = std::move(scheme)](const LayoutInput& in) { return s.place(in); });
}

/** A named mount point whose content is supplied independently via
 *  `Composer::renderSlot()`. The surrounding tree is not re-described when
 *  the slot updates, so its caches stay valid — this is how two data
 *  domains that change at different rates share one tree.
 *
 *  THE NAME IS STORED AS THE ELEMENT'S `key`, the same field `.key()`
 *  writes: `slot("hud").key("panel")` is a slot named "panel", and
 *  `renderSlot("hud")` then finds nothing and does nothing. Name the slot
 *  here and only here; `.key()` warns once if called on one anyway. */
Element slot(std::string_view name);

namespace detail {
/** A copy of a TEXT element carrying no tracks, no marks and no children,
 *  set in one ink — the rest pose an fx() track's per-glyph deviation is
 *  measured against. Everything that could make the two copies disagree
 *  about where a letter belongs is left alone: same content, same style,
 *  same width, same layout; only the tracks and the ink differ. The key
 *  takes `-rest` after it (a keyless original leaves the copy keyless).
 *  Anything but text warns once and comes back unchanged. Building the
 *  copy means reading the description, which is why this is the kernel's
 *  and not an instrument's. */
Element textAtRest(Element moving, SkColor4f colour);

Element makeMemo(std::any props,
                 std::function<bool(const std::any&, const std::any&)> equal,
                 std::function<Element(const std::any&)> invoke);
}  // namespace detail

/** Deferred description: `fn` runs only when `props` changed (by
 *  operator==) since the last render on this position/key — AND the
 *  ambient `env::` bindings are unchanged, because a memo is a pure
 *  function of (props, environment) and would otherwise serve the theme
 *  it first described under forever. The captured stack is re-established
 *  around the deferred call, so `env::inherited<T>()` inside `fn` reads
 *  what was bound where the memo was WRITTEN, not where it runs. */
template <ComponentProps P, ComponentFn<P> F>
Element memo(P props, F fn) {
  return detail::makeMemo(
      std::any(std::move(props)),
      [](const std::any& a, const std::any& b) {
        return std::any_cast<const P&>(a) == std::any_cast<const P&>(b);
      },
      [fn = std::move(fn)](const std::any& p) -> Element {
        return fn(std::any_cast<const P&>(p));
      });
}

/** One-shot element render: reconciles, lays out, and records the
 *  paint into a picture. With an empty @p maxSize the tree takes its
 *  intrinsic (content) size; a non-empty one bounds it (root max
 *  dims). Bindings are sampled at their current values; transitions
 *  don't run — there is no live timeline. This is the bake primitive
 *  behind ContourWalk element stamps, and generally "an element tree
 *  as a brush".
 *
 *  THE INTRINSIC SIZE COMES FROM THE ROOT'S CHILDREN, not from the root's
 *  own dims, and this catches people out: `snapshot(box().width(32).
 *  fill(…))` bakes at CONTENT size and quietly ignores the 32. Wrap the
 *  sized tree in a plain `box().child(...)` and the dims are honoured,
 *  because they now belong to a child. */
sk_sp<SkPicture> snapshot(Element root, sigil::weave::FontContext& fonts,
                          SkSize maxSize = SkSize::MakeEmpty());

/** Slicing ONE baked picture into a run of tiles.
 *
 *  A strip far longer than any texture — a marquee, a scrolling ribbon, a
 *  hanging scroll — is authored as a single element tree and baked with
 *  `snapshot()`, which has no size limit because a picture is vector. The
 *  consumer then wants it as N tile-sized rasters. That slice is a clip
 *  and a translate and nothing else: **there is no windowed bake, and
 *  there is no need for one.** Replaying the whole picture per tile,
 *  behind `sliceable()` below, is as cheap as extracting each tile's ops
 *  in advance would be.
 *
 *  What DOES go wrong is the transform, and that is what these two verbs
 *  exist to own.
 *
 *  **Author the strip in the tiles' own orientation.** If the tiles are
 *  tall, the tree is a `column()`; if they are wide, a `row()`. The
 *  temptation is to author across and transpose on the way out, and a
 *  transpose has determinant -1 — it composes with whatever mirroring the
 *  consumer's own sampling already applies, and the mirror bookkeeping
 *  stops being local to either side. `Flow` therefore offers only the two
 *  non-transposing slices, on purpose.
 *
 *  **`Facing` is a statement about the CONSUMER, not the picture.** A
 *  texture sampled onto a surface whose u runs backwards — a ribbon wall
 *  mirrors its own u — shows glyphs reversed unless the tile was baked
 *  reversed to match. `Facing::Mirrored` pre-flips ACROSS the strip, on
 *  the axis perpendicular to `flow`, so that such a consumer reads it the
 *  right way round. Get this wrong and the art is legible in an offline
 *  PNG of the tile and mirrored on the surface, so it will not show up
 *  until the texture is in place. */
namespace tiles {

/** Which way the run of tiles marches through the picture. */
enum class Flow {
  Down,   ///< a column strip: tile k is the k-th slice down
  Across  ///< a row strip: tile k is the k-th slice rightward
};

/** Whether the tile is pre-flipped for a consumer that samples mirrored. */
enum class Facing {
  Forward,  ///< the tile reads like the picture
  Mirrored  ///< flipped across the strip, for mirrored sampling
};

/** The canvas transform that brings tile @p index of a @p tile -sized run
 *  into view. Concat it, then draw the picture:
 *
 *  ```
 *  SkAutoCanvasRestore restore(canvas, true);
 *  canvas->clear(SK_ColorTRANSPARENT);
 *  canvas->concat(tiles::window(size, k, Flow::Down, Facing::Mirrored));
 *  canvas->drawPicture(strip);
 *  ```
 *
 *  The surface's own bounds are the clip, so nothing else is needed —
 *  neighbouring tiles share their boundary texels and the seams vanish. */
SkMatrix window(SkISize tile, int index, Flow flow = Flow::Down,
                Facing facing = Facing::Forward);

/** The same picture, re-recorded behind a bounding-box hierarchy, so each
 *  `window()` replay visits only the ops that meet its tile instead of all
 *  of them.
 *
 *  Worth it past a handful of tiles and not before: building the
 *  hierarchy costs a pass over the picture, which a two-tile run does not
 *  earn back. Slicing WITHOUT it is quadratic, because every tile walks
 *  every tile's ops, so the saving grows with the tile count while the
 *  build cost does not.
 *
 *  It exists as a verb because the obvious one-liner has a trap:
 *  `drawPicture()` into a recorder stores a NESTED reference the
 *  bounding-box hierarchy cannot see into, leaving the tree empty and the
 *  replay cost unchanged. This flattens with `playback()` instead. */
sk_sp<SkPicture> sliceable(const sk_sp<SkPicture>& art);

}  // namespace tiles

/** A face's vertical metrics at a given size, without laying anything out.
 *
 *  A compose text node's top is the LINE BOX top, while type is usually
 *  positioned against its CAP TOP — so aligning text to a coordinate taken
 *  from a design or a reference image needs the slack between the two, and
 *  `measure()` returns only an `SkSize`. `capSlack()` below is that
 *  number.
 *
 *  `capHeight` and `xHeight` are what the face itself reports; both fall
 *  back to a fraction of the ascent when a face reports zero, which some
 *  do. All values are positive distances in px, with `ascent` measured
 *  above the baseline. */
struct TextMetrics {
  float ascent = 0;      ///< baseline to the top of the em box (positive)
  float descent = 0;     ///< baseline to the bottom (positive)
  float leading = 0;     ///< the face's recommended extra line gap
  float capHeight = 0;   ///< baseline to the top of a flat capital
  float xHeight = 0;     ///< baseline to the top of a lowercase x
  float lineHeight = 0;  ///< ascent + descent + leading
  /** How far the line box's top sits above the cap top — the number that
   *  turns "place this at the reference's y" into a coordinate. */
  float capSlack() const { return ascent - capHeight; }
};

TextMetrics metrics(const sigil::weave::TextStyle& style,
                    sigil::weave::FontContext& fonts);

/** Shape ONE RUN without building an Element: per-glyph advances in px, in
 *  visual order, through the same shaping path a text() leaf takes, so
 *  kerning and ligatures are real. The result's length is the GLYPH count,
 *  which is neither the byte nor the code-point count.
 *
 *  Pen positions are the running prefix sums, so hand-placing N glyphs
 *  costs one layout here rather than N text() leaves and N `measure()`
 *  calls. A space between two words is a gap the flow leaves rather than a
 *  glyph, so it rides the advance of the glyph before it and the sums stay
 *  true across a whole sentence; the sums therefore add up to the run's
 *  laid-out extent, not to the ink alone. The pen starts at the FIRST
 *  GLYPH, so leading whitespace is no part of the run.
 *
 *  Single style, no wrapping: the run is laid on one unbounded line. A
 *  '\n' starts a new line and resets the positions after it, so pass a
 *  RUN and not a paragraph.
 *
 *  This is the STATIC answer, for a run that is not in the tree. For a
 *  MOUNTED, animated run — one a `text()` leaf is drawing and an `fx()`
 *  track is cascading — `Composer::beatsOf` is the answer instead: it
 *  reports the rect the layout actually placed each unit in, which follows
 *  a wrap, a mixed-style run and a path baseline that no single-style
 *  measurement can see. */
std::vector<float> measureRun(std::u8string_view utf8,
                              const sigil::weave::TextStyle& style,
                              sigil::weave::FontContext& fonts);

/** WHERE THE LETTERS SIT: `measureRun`'s advances already summed. Entry i
 *  is glyph i's pen x, measured from the first glyph's pen, and there is
 *  ONE PAST-THE-END ENTRY, so `runPens(...).back()` is the run's whole
 *  laid-out width and `pens[i + 1] - pens[i]` is glyph i's advance. `n`
 *  glyphs give `n + 1` entries, and an empty run gives the single entry 0.
 *
 *  THE ONE RULE TO KNOW, which is `measureRun`'s and is stated here because
 *  this is the form that gets read: A SPACE RIDES THE PREVIOUS ADVANCE.
 *  An inter-word space is a gap the flow leaves between positioned runs
 *  rather than a glyph, so it is no entry of its own; whatever the layout
 *  left between one glyph's pen end and the next one's origin is folded
 *  into the advance of the glyph BEFORE it. That is exactly what makes
 *  these sums reproduce the pen positions the layout used, across a whole
 *  sentence and not only inside one word. Two steps are deliberately not
 *  folded: a '\n' restarts the pen, and a BACKWARDS step between two words
 *  is bidi reordering, which visual-order prefix sums cannot express (a
 *  backwards step INSIDE a word is ordinary kerning and does count). The
 *  pen starts at the first glyph, so leading whitespace is no part of the
 *  run and entry 0 is always 0.
 *
 *  Same shaping path, same single style, same unbounded line as
 *  `measureRun` — and the same division of labour: this is the STATIC
 *  answer for an unmounted run, `Composer::beatsOf` is the answer for a
 *  mounted, cascading one. */
std::vector<float> runPens(std::u8string_view utf8,
                           const sigil::weave::TextStyle& style,
                           sigil::weave::FontContext& fonts);

/** One-shot intrinsic measurement: what size would this element take?
 *  Runs the same reconcile+layout as snapshot() and returns the root's
 *  resolved size without painting. The sizing primitive behind
 *  content-fit chrome (marquees, tooltips, badges): measure the content,
 *  then describe the real tree with the answer. Same sampling rules as
 *  snapshot() — bindings at current values, no transitions. */
SkSize measure(Element root, sigil::weave::FontContext& fonts,
               SkSize maxSize = SkSize::MakeEmpty());

}  // namespace sigil::compose
