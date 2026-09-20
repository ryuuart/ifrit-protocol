#pragma once

/** @file
 * @ingroup compose-core
 *
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
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/Animation.h>
#include <sigilmotion/schedule/Schedule.h>
#include <sigilmotion/values/Animated.h>
#include <sigilweave/layout/Block.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/layout/StyleSheet.h>
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
}  // namespace sigil::weave

namespace sigil::material::pattern {
class Tile;
}

/** DATA-DRIVEN DRAWING: a scene described as a tree of values, diffed
 *  against the last description and painted through Skia.
 *
 *  A description is a tree of `Element`s. Each is a cheap VALUE built
 *  fresh — a factory such as `box()`, `row()`, `column()`, `text()`,
 *  `image()` or `custom()` starts one, chaining setters shape it, and
 *  `children({…})` says what is under it. Nothing in the tree owns a GPU
 *  or layout resource, so a description may be built, copied and thrown
 *  away freely.
 *
 *  A `Composer` is the retained side and the only long-lived object.
 *  `Composer::render()` hands it a description, which it reconciles
 *  against the one before — matching nodes by `Element::key()`, laying
 *  out through Yoga, resolving the cascade, running the derive pass, and
 *  caching the subtrees that did not move — and `Composer::draw()` paints
 *  the result onto an `SkCanvas`. `Composer::bounds()` and
 *  `Composer::hitTest()` answer questions about what it laid out.
 *
 *  The vocabulary is CSS's wherever CSS has a word for it: flex layout,
 *  the box model, `align-items`, `z-index`, custom properties and
 *  inheritance, transforms including the 3D ones, blend modes, backdrop
 *  filters. What inherits down the tree is the font, the block, the ink,
 *  the style sheet and the custom properties; everything else a node
 *  says stays on that node.
 *
 *  The features beside the kernel: `brush/` for marks along a boundary
 *  and layer styles over a surface, `typography/` for what a text leaf
 *  says beyond its words, `kit/` for stock components built out of these
 *  verbs, and `draw/`, `texture/`, `video/` and `web/` for the leaves
 *  that bring in a pen, an offscreen scene, a video stream and a
 *  rendered page.
 *
 *  Reach for it when a picture is a function of state that changes.
 *  A one-off drawing with no state behind it is cheaper with an
 *  immediate-mode pen (SigilDraw); a lit scene in space is a set
 *  (SigilWorld). */
namespace sigil::compose {

namespace detail {
struct ElementNode;
struct Instance;
}  // namespace detail

class Composer;
class Pattern;
class VarTable;
// The typography vocabulary the text verbs take, defined under
// <sigilcompose/typography/>: a call site that dresses its type includes
// the header that spells the value it passes. Which glyphs a verb
// addresses is SigilWeave's `Selector`, declared above.
struct Track;
struct Annotation;
struct TextPath;
// <sigilcompose/core/Derive.h>: the positioning value, declared beside the
// rest of the derive family because it is resolved by the same pass.
struct Tether;

// One run of a children({…}) block; defined at the foot of this header,
// where the block verb that takes it can be read beside it.
struct Children;

/** ONE NODE OF A SCENE DESCRIPTION: what to draw, how to lay it out, and
 *  the children under it. An Element is a VALUE built fresh every frame
 *  and thrown away — it holds no GPU or layout state, and the retained
 *  tree behind it is the composer's business. The chaining setters
 *  return `*this`, so a node reads as one expression.
 *
 *  A node is STARTED by a factory — `box()`, `row()`, `column()`,
 *  `text()`, `image()`, `custom()` and the rest in Factories.h — and
 *  SHAPED by the verbs below, which are grouped here in the order a node
 *  is usually written: where it sits, what shape it is, what gates its
 *  paint, what it hands down to its children, what it paints, where it
 *  stands in depth, what it derives, what it contains, how its type is
 *  treated, who it is, and what is under it.
 *
 *  A VERB WHOSE VALUE THIS NODE CANNOT USE IS SILENTLY IGNORED rather
 *  than an error: the text verbs do nothing on a box, `region()` does
 *  nothing off an image leaf, and a `cells()` claim is read only by a
 *  grid-shaped scheme. That is what lets one kit component say
 *  everything it might mean and let each node take its share.
 *
 *  THE NODE'S IDENTITY FOR CACHING IS `key()`. The reconciler matches a
 *  child across describes by it, and `Composer::bounds` and `hitTest`
 *  answer for it; a keyless node is matched by its position among its
 *  siblings. Names given to marks and passes are LOCAL to the node and
 *  are not keys. */
class Element {
 public:
  Element();  ///< An empty box: no size, no fill, no children.

  /** @name Layout
   *  Where the node sits and how big it is: the flex direction, the box
   *  model, the flex factors, the absolute placement longhand, and the
   *  cell a grid-shaped scheme puts it in. Lengths are `Dimension`s, so
   *  a bare number is pixels.
   *  @{ */
  /** Lay the children out along the HORIZONTAL axis, so the main axis is
   *  x — CSS `flex-direction: row`. */
  Element& row();
  /** Lay the children out down the VERTICAL axis, so the main axis is y
   *  — CSS `flex-direction: column`, and what a node does when it says
   *  neither. */
  Element& column();
  /** Flex-wrap: children flow onto new lines/columns when they
   *  overflow the main axis. */
  Element& wrapLines(bool on = true);
  /** THE AIR BETWEEN THE CHILDREN, along both axes. Zero when unstated.
   *
   *  The gap, the padding and the margin take a `Dimension`: a bare
   *  number is pixels, a percent is of the parent, and `1_em`, `0.5_lh`
   *  and `1_rem` (SigilWeave's length literals) measure against the font
   *  in force — the node's own size and line height, or the root's — so
   *  the air around type follows the type. */
  Element& gap(Dimension length);
  /** The air INSIDE the node's box, between its edge and its content —
   *  the same `Dimension` forms the gap takes, and zero on every side
   *  when unstated. One value is all four sides, two are horizontal then
   *  vertical, four are left, top, right, bottom. */
  Element& padding(Dimension all);
  /** The air inside it, one length across and one down. */
  Element& padding(Dimension horizontal, Dimension vertical);
  /** The air inside it, a length per side, clockwise from the left. */
  Element& padding(Dimension left, Dimension top, Dimension right,
                   Dimension bottom);
  /** The air OUTSIDE the node's box, between its edge and its siblings —
   *  the same `Dimension` forms and the same one/two/four spellings as
   *  the padding, and zero on every side when unstated. */
  Element& margin(Dimension all);
  /** The air outside it, one length across and one down. */
  Element& margin(Dimension horizontal, Dimension vertical);
  /** The air outside it, a length per side, clockwise from the left. */
  Element& margin(Dimension left, Dimension top, Dimension right,
                  Dimension bottom);
  /** The flex BASIS, not a guarantee. `shrink` defaults to 1, faithful to
   *  Yoga and CSS, so a `width(150)` child of a row that overflows is
   *  150 px wide only until the row runs out of room — then it gives some
   *  back, and the result is silently narrower content rather than an
   *  error. Pair with `.shrink(0)` when `width(150)` means "this IS 150".
   *  The same holds for `height()` in a column. */
  Element& width(Dimension d);
  /** The node's height, in the same `Dimension` forms the width takes,
   *  and the same flex basis rather than a guarantee: in a column it is
   *  what the node asks for and gives back when the column overflows.
   *  Unstated, the node is as tall as its content. */
  Element& height(Dimension d);
  /** A FLOOR under the node's width that the flex factors may not take
   *  it below, in the same `Dimension` forms. Unstated, there is none. */
  Element& minWidth(Dimension d);
  /** A CEILING over the node's width that `grow()` may not take it
   *  above, in the same `Dimension` forms. Unstated, there is none. */
  Element& maxWidth(Dimension d);
  /** A FLOOR under the node's height, in the same `Dimension` forms.
   *  Unstated, there is none. */
  Element& minHeight(Dimension d);
  /** A CEILING over the node's height, in the same `Dimension` forms.
   *  Unstated, there is none. */
  Element& maxHeight(Dimension d);
  /** WIDTH OVER HEIGHT, held while the other axis is free — a `16f/9`
   *  video box given only a width is sized down from it. Unstated, the
   *  two axes are independent. */
  Element& aspect(float ratio);
  /** THIS NODE'S SHARE OF THE ROOM LEFT OVER along the parent's main
   *  axis, as a weight against its siblings' (CSS `flex-grow`). Zero when
   *  unstated, so a node takes none of it and stays at its basis; the
   *  bare call is a weight of one. */
  Element& grow(float factor = 1.0f);
  /** THIS NODE'S SHARE OF THE OVERFLOW when the parent's main axis runs
   *  short, as a weight against its siblings' (CSS `flex-shrink`). ONE
   *  when unstated, faithful to Yoga and CSS, which is why a stated
   *  width is a basis; `shrink(0)` is what makes a size exact. */
  Element& shrink(float factor);
  /** THE SIZE THE FLEX FACTORS START FROM along the parent's main axis
   *  (CSS `flex-basis`), in the same `Dimension` forms. Unstated, the
   *  node's own width or height on that axis is the basis. */
  Element& basis(Dimension d);
  /** WHERE THIS NODE'S CHILDREN SIT ACROSS its main axis — CSS
   *  `align-items`. `Align::Stretch` when unstated, so a child with no
   *  cross-axis size fills. A child that says `alignSelf()` overrides
   *  it for itself. */
  Element& alignItems(Align a);
  /** WHERE THIS NODE SITS ACROSS its parent's main axis, overriding that
   *  parent's `alignItems()` for this child alone — CSS `align-self`.
   *  `Align::Auto` when unstated, which is to take the parent's. */
  Element& alignSelf(Align a);
  /** HOW THIS NODE'S CHILDREN ARE DISTRIBUTED ALONG its main axis, and
   *  what becomes of the room left over — CSS `justify-content`.
   *  `Justify::Start` when unstated. */
  Element& justify(Justify j);
  /** TAKE THIS NODE OUT OF THE FLOW — CSS `position: absolute`. It no
   *  longer sizes or displaces its siblings, and it is placed by the
   *  insets, the pins, `rect()`, `at()`, `centerAt()` or `tether()`
   *  instead; with none of those it stands at its parent's origin at its
   *  own size. Every verb below that needs it implies it. */
  Element& absolute();
  /** THIS NODE FILLS THE BOX IT STANDS IN — `absolute()` and `inset(0)`,
   *  which is one sentence and was written as two. CSS's own word: the
   *  node is taken out of the flow and stretched to its parent's box, so
   *  a drawing, an overlay, a scrim, a rail and a hit surface each say
   *  what they are rather than how they are pinned.
   *
   *  A node that must fill only part of the box states that part with
   *  `inset()` instead; a node that must stand in the flow states its
   *  size and says nothing here — or says this first and its size
   *  after, which puts it back in the flow at that size: a covering
   *  node is a canvas filling its box by nature, and a box of its own
   *  is the one other thing it can be. A pin or an inset stated after
   *  this is a placement, and stands. */
  Element& cover();
  /** HOW FAR IN FROM EACH EDGE of the parent's box an absolute node's own
   *  edges stand, in pixels (implies absolute()) — CSS's four inset
   *  properties. One value is all four sides; four are left, top, right,
   *  bottom. `inset(0)` stretches the node across the whole box, which
   *  is what `cover()` says in one word. */
  Element& inset(float all);
  Element& inset(float left, float top, float right, float bottom);
  /** Dimension-valued insets: px, pct(), or autoDimension() per side —
   * autoDimension() leaves that side unpinned (the CSS `auto`), so width/height
   * (or the opposite inset) size the node instead of stretching it. */
  Element& inset(Dimension left, Dimension top, Dimension right,
                 Dimension bottom);
  /** Pin ONE edge of an absolute node (implies absolute()): the
   *  corner-badge idiom — `.top(12).right(12)` pins a date block to the
   *  top-right without stretching it across the box. Unpinned sides stay
   *  auto. */
  Element& left(Dimension d);
  /** Pin the node's TOP edge @p d below the parent's, in the same
   *  `Dimension` forms the other pins take (implies absolute()). The
   *  other three sides stay auto unless they are pinned too. */
  Element& top(Dimension d);
  /** Pin the node's RIGHT edge @p d inside the parent's, in the same
   *  `Dimension` forms (implies absolute()). Pinning left and right both
   *  stretches the node between them. */
  Element& right(Dimension d);
  /** Pin the node's BOTTOM edge @p d above the parent's, in the same
   *  `Dimension` forms (implies absolute()). Pinning top and bottom both
   *  stretches the node between them. */
  Element& bottom(Dimension d);
  /** HANG THIS NODE OFF A KEYED ONE, at a stated pair of points, with a
   *  list of places to try when the first will not fit (implies
   *  absolute()).
   *
   *      tooltip().tether({.key = "port",
   *                        .on = {0.5f, 0.0f}, .at = {0.5f, 1.0f},
   *                        .offset = {0, -6},
   *                        .fallbacks = {{.key = "port",
   *                                       .on = {0.5f, 1.0f},
   *                                       .at = {0.5f, 0.0f},
   *                                       .offset = {0, 6}}}})
   *
   *  Resolved after layout, against the geometry the anchor resolved to,
   *  and re-resolved whenever it moves. The value states the rule; see
   *  `Tether` for what fits means and what an unknown key does. */
  Element& tether(Tether t);
  /** Center this absolute node ON a parent-space point — the dominant
   *  placement in node-graph scenes (sockets on orbit positions, badges
   *  on markers). Resolved after measurement, so intrinsic-size nodes
   *  center correctly; implies absolute(). */
  Element& centerAt(SkPoint p);
  /** WHICH CELLS this child claims of the `layout()` scheme above it, and
   *  how many it covers — read by grid-shaped schemes (`layouts::Table`,
   *  `layouts::Grid`) and by nothing else.
   *
   *  Said HERE, on the child, rather than in a list the scheme carries
   *  beside it: a parallel list has nothing to check itself against, and
   *  an inserted or reordered child silently shifts every entry after it
   *  onto the wrong cell. */
  Element& cells(int column, int row, int columns = 1, int rows = 1);
  /** The same claim as one value — the shape a scheme reads it back as,
   *  so a caller computing a span passes what it computed. */
  Element& cells(CellSpan span);
  /** WHICH NAMED REGION of the `layout()` scheme above it this child
   *  claims — the same statement as `cells()` with the numbers left to the
   *  scheme's own picture of itself (`layouts::Grid::areas`).
   *
   *      layout(layouts::Grid{.areas = {"head head", "nav  main"}})
   *          .children({masthead().area("head")})
   *          .children({sidebar().area("nav")})
   *
   *  A name survives what four integers do not: insert a row into the
   *  picture and every child stays in the region it named, where every
   *  numbered child after the insertion would have moved one cell up. A
   *  name the picture does not carry is silent, and the child flows into
   *  the next free cell like any child that claimed nothing. */
  Element& area(std::string_view name);
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
   *      g.children({box().rect(panelBox).fill(…)});
   *      g.children({text(u8"…", st).at({panelBox.fLeft + 16,
   * panelBox.fTop})});
   *
   *  Does not cover right()/bottom() pinning, percentage insets, or
   *  autoDimension() sides — those are different intents and keep the longhand.
   *  `geometry::path::centred()` (kit/Frame.h) builds the rect for the
   * centre-and-size case. */
  Element& rect(const SkRect& r);
  /** Pin an absolute node's top-left to a parent-space POINT, leaving the
   *  node to size itself from its content — `left(p.fX).top(p.fY)`. The
   *  half of the placement longhand that carries no box; same
   *  qualification as rect() above. */
  Element& at(SkPoint topLeft);
  /** @} */

  /** @name Shape
   *  The node's own outline, which is what its fill covers, what
   *  `clip()` clips to, and what every stroke pass and
   *  outline-following decoration traces (`PaintContext::outline`).
   *  @{ */
  /** ROUND THE NODE'S CORNERS, per corner, in pixels — CSS
   *  `border-radius`. Square when unstated, and overridden outright by
   *  `shape()`. It is the cheap path: a rounded box clips and strokes as
   *  a round rect where a general shape has to build a path. */
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
  /** THE KEYED SPELLING: the generator plus the value it closes over, so
   *  the node settles. Sugar for `shape(keyedShape(key, fn))` — see
   *  KeyedShape for the one-key-one-drawing contract the author takes on.
   *  A path already cooked wants `shape(heldPath(p))` instead. */
  template <typename K, typename F>
    requires core::PrefixCallable<const F&, SkPath(SkSize)>
  Element& shape(K key, F fn) {
    return shape(Shape(keyedShape(std::move(key), std::move(fn))));
  }
  /** BAND FORMATION: which side of the spine the band occupies.
   *  `.centered()` is the default and straddles it; `.outward()` and
   *  `.inward()` take one side (the offset-path lineage). No effect on a
   *  node that is not a band(). */
  Element& centered();
  /** Band formation: the whole band sits on the LEFT of travel, which in
   *  screen space is outside a clockwise spine — so it exits a `shapes::`
   *  rect or circle. No effect on a node that is not a band(). */
  Element& outward();
  /** Band formation: the whole band sits on the RIGHT of travel, which
   *  in screen space is inside a clockwise spine. No effect on a node
   *  that is not a band(). */
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
  /** @} */

  /** @name Mask
   *  The appearance-gating family: what of this node's paint is shown,
   *  and where. Paint-only and bindable, so a mask never relayouts and
   *  hit-testing keeps the unmasked shape.
   *  @{ */
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
  /** @} */

  /** @name The cascade
   *  What flows down the TREE, from a node to everything under it,
   *  wherever the code that built a child ran: the font, the block, the
   *  colour (the ink), the style sheet, the classes resolved through it,
   *  and the custom properties. Everything else a node says about itself
   *  stays on that node — CSS's own split between the properties that
   *  inherit and the ones that do not. A node that leaves one unset takes
   *  the nearest ancestor's, and the root's are the composer's
   *  `setInherited` defaults.
   *  @{ */
  /** THE FONT EVERYTHING UNDER THIS NODE IS SET IN, as a PARTIAL: the
   *  fields @p partial names override the inherited font, and every field
   *  it leaves unset inherits — `font({.size = 22})` is the inherited face
   *  and colour at another size. A text leaf reads its own; a container's
   *  reaches every text under it that does not say otherwise. Written
   *  twice on one node, the later call wins field by field, and so does a
   *  class written between. A relative size resolves against the PARENT's
   *  font: `font({.size = 1.5_em})` is half again the size inherited. */
  Element& font(sigil::weave::Type partial);
  /** THE BLOCK everything under this node is set in, as a PARTIAL: the
   *  fields it names — leading, alignment, justification, hyphenation,
   *  tab stops, the first- and last-line indents, widows and orphans,
   *  balanced ragging, the breaking strategy, the last line, the writing
   *  mode, the line-break locale, the line tables — override the block
   *  inherited and the rest inherit, exactly as `font` does for the type.
   *  A text leaf's every block is set in the block in force where the
   *  leaf lands, unless the leaf wrote a whole `weave::ParagraphStyle`
   *  for it, which inherits nothing; a block named through
   *  `paragraphs(names)` is that name's partial laid over it. This is the
   *  ONE spelling of every block field — `block({.alignment =
   *  TextAlignment::kCenter})` centres every line under the node,
   *  `block({.writingMode = WritingMode::kVerticalRL})` sets the text under
   *  it in vertical columns, and so on for every field of `weave::Block` —
   *  and it cascades the same way from wherever it is written.
   *
   *  A VERTICAL leaf measures on the other axis: its main extent is its
   *  height, its intrinsic width one column pitch per column, and for
   *  `Align::Baseline` it reports its first character's baseline. Per
   *  character the mode is UTR#50's — ideographs upright with their `vert`
   *  forms, Latin on its side — and a run that wants otherwise says so in
   *  its own style. A run on a path (`onPath`) ignores the mode: its
   *  baseline is its own geometry and has no columns to advance; setting
   *  both warns once and the path wins. */
  Element& block(sigil::weave::Block partial);
  /** THE INK: the colour text under this node is set in and every mark
   *  that names no colour is painted in — CSS's `color`, which is the
   *  font's own colour spelled alone. `Fill::currentInk()` reads it back
   *  wherever a fill must be named. A node whose ink changes under a
   *  `transition()` eases it, and everything under it follows. */
  Element& ink(SkColor4f colour);
  /** The ink read from a custom property in force here:
   *  `ink(var("accent"))`. A property nobody set, or one holding a length,
   *  leaves the inherited ink standing and says so once. */
  Element& ink(VarRef reference);
  /** THE SHEET this node and everything under it resolve their classes
   *  through: rules under names, each a type half and a block half,
   *  stated on any node and inherited down the tree as the font is, a
   *  nearer sheet's rules standing over a farther one's by name. A sheet
   *  is a value on the description, so a subtree carries its own and
   *  nothing is bound around the code that builds it. */
  Element& styleSheet(sigil::weave::StyleSheet sheet);
  /** A SEMANTIC ROLE with default typography. The rule's name selects a
   *  rule from the sheet where this node lands; that rule overrides these
   *  defaults, ordinary classes override the role, and the node's own
   *  font and block override both. Unstated fields inherit. A sheet need
   *  not carry the role: the defaults make a component useful on its own.
   *  A later call replaces the role and its defaults together. */
  Element& role(sigil::weave::Rule defaults);
  /** A semantic role with no default fields, styled by the sheet in force. */
  Element& role(std::string name);
  /** CLASSES: the partials the sheets in force register under each name
   *  in @p names — several, separated by spaces, as CSS's class attribute
   *  lists them, folded in left to right — resolved by the cascade pass
   *  where the element LANDS, and laid under the node's own `font()` and
   *  `block()`, as an inline style stands over a class. The fields a class
   *  sets then inherit down the tree. A name neither sheet in force
   *  carries warns once and sets nothing. */
  Element& styleClass(std::string_view names);
  /** A CUSTOM PROPERTY set on this node and inherited by everything under
   *  it, read back through `var(name)` written as a length, `Fill::var`
   *  written as a fill, or `ink(var(name))`. The nearest ancestor that set
   *  a name wins, as CSS's custom properties do. A material, a layer
   *  style and every other value the kernel cannot see inside take
   *  concrete values, so a property reaches exactly what resolves through
   *  the paint context: a fill, a stroke, a mark, a length, the ink. */
  Element& var(std::string_view name, SkColor4f colour);
  /** The same, holding a LENGTH rather than a colour, read back through
   *  `var(name)` wherever a `Dimension` is taken. A property is one or
   *  the other, and reading one as the other leaves the target standing
   *  and says so once. */
  Element& var(std::string_view name, Dimension length);
  /** FALLBACK CUSTOM PROPERTIES for this node and its descendants.
   *  Inherited properties override these defaults, and properties this
   *  node sets with var() override both, including explicit zero values.
   *  A later call replaces this table. */
  Element& varDefaults(VarTable defaults);
  /** @} */

  /** @name Paint
   *  What the node's own surface is painted with, and how image leaves
   *  sample their source.
   *  @{ */
  /** PAINT THE NODE'S SURFACE — a colour, a shader, a transition between
   *  colours, or a LIVE binding. Unfilled when unstated, so a box paints
   *  nothing and only its decorations and children show.
   *
   *  WHAT MAY BE PASSED, across the overloads below: an `SkColor4f`, a
   *  `Fill` (a colour or a shader), a `motion::Animatable<Fill>` (one
   *  that eases or is driven), a `material::skia::Paint` (a gradient
   *  ramp, a blend stack, a sprite, SkSL), or a `SurfacePaint`, which is
   *  the one value all of those convert into and the type a component
   *  declares so its caller may pass whichever it holds. A `Pattern` and
   *  a `material::pattern::Tile` are deliberately NOT accepted — fill
   *  with what a held Pattern bakes instead.
   *
   *  @see sigil::compose::SurfacePaint
   *  @see sigil::material::skia::Paint
   *
   *  The binding form is `fill(&output)` where the Output holds a `Fill`,
   *  and it is the answer to "this widget's colour IS its value" — a
   *  level meter whose hue is the level, a temperature readout, a health
   *  bar that reddens. Write the Fill Output from the same steppable that
   *  computes the number:
   *
   *      ch::Output<float> level; ch::Output<Fill> bar;
   *      ticker.add([&]{ level = v; bar = Fill::color(ramp(v)); … });
   *      box().scaleX(bind(&level)).fill(&bar)
   *
   *  What does NOT exist is deriving one from the other at the binding
   *  site: `fill(bind(&level).map(ramp))` does not compile, because the
   *  shaping chain maps floats to floats. Compute the Fill in the
   *  steppable, as above. */
  Element& fill(motion::Animatable<Fill> f);
  /** Fill with a paint (gradient ramp, blend stack, sprite, SkSL) — the
   *  richer authoring value, `material::skia::Paint` from
   *  <sigilmaterial/skia/Paint.h>. A static paint collapses to a Fill, so
   *  it caches and prunes on the same path. */
  Element& fill(material::skia::Paint m);
  /** A surface value supplied by component properties. Exact-type deduction
   *  keeps ordinary fill and material arguments on their own overloads. */
  template <typename P>
    requires std::same_as<std::remove_cvref_t<P>, SurfacePaint>
  Element& fill(P&& paint) {
    return paint.apply(*this);
  }

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
  /** How image leaves sample their source. Linear when nothing states
   *  it, which is right for photographs and wrong for every pixel grid:
   *  art, tilemaps, fonts baked as sprites, simulation buffers.
   *
   *      image(tileset).sampling(SkSamplingOptions(SkFilterMode::kNearest))
   *
   *  Set on any node and inherited by every image leaf under it, as CSS
   *  inherits `image-rendering`: a panel of pixel art states nearest once.
   *  `Material::image()` takes the same options for a sprite fill. */
  Element& sampling(SkSamplingOptions options);
  /** @} */

  /** @name Decoration layers
   *  The marks laid under, over and around what the node paints.
   *  Backgrounds paint below content and children, in declaration order,
   *  foregrounds above; `fill()` is the transitionable first background
   *  and `custom()` a box with one background program.
   *
   *  Repeated calls APPEND — the Photoshop stacked-strokes model, where
   *  two `stroke()` calls are two rings — and every slot takes an
   *  optional local name, which is what `mask(parts::named(name), …)`
   *  addresses and is never a query key.
   *
   *  Decorations dress the OUTLINE: `clip()` does not clip them (it
   *  bounds the fill, the content and the children only), so outer
   *  strokes and shadows survive on a clipped node.
   *  @{ */
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
  /** HOW MUCH PAINT COUNTS AS INK under `Boundary::Coverage` — the
   *  tolerance the silhouette is cut at, as a fraction of full opacity.
   *
   *      image(photo).key("fig").boundary(Boundary::Coverage).threshold(0.35f)
   *      text(body, bodyStyle).flowAround("fig", 12)
   *
   *  The default is the rule an unantialiased rasteriser uses — the paint
   *  reached at least half the pixel — so the traced edge is where the
   *  drawn edge is. It is the dial a soft edge needs: lower it and a wash,
   *  a feathered cut-out or a glow becomes silhouette; raise it and only
   *  the solid core does. Read by everything that asks this node for its
   *  coverage — its own decorations, and any text flowing around it. */
  Element& threshold(float coverage);

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
  /** @} */

  /** @name Transform
   *  The paint-phase lanes: opacity, the blend, and the 2D transform
   *  stack — translate, then rotate, then scale, then skew, about the
   *  transform origin. Animating any of them never relayouts; the
   *  content picture replays under the new transform, and hit-testing
   *  follows the transformed box.
   *  @{ */
  /** HOW OPAQUE THE WHOLE NODE IS, 0 clear to 1 solid, multiplying
   *  everything it and its children paint. 1 when unstated. Below 1 the
   *  node composites as a group, which is what makes a subtree fade as
   *  one picture rather than layer by layer — and what ends a
   *  `preserve3d()` space at this node. */
  Element& opacity(motion::Animatable<float> o);
  /** THE NODE FADES IN WHEN IT MOUNTS, over @p how — `opacity(animate(
   *  from(0).to(1), how))` written once, because that sentence is what
   *  every card, panel, strip and pass on a plate says as it arrives and
   *  the three values in it never vary.
   *
   *  It is the mount entrance and nothing else: after the entrance the
   *  node is opaque and behaves as an unstated opacity does, so a node
   *  that also FADES on some later condition states that with `opacity`
   *  instead. A node under a staggered container takes its share of the
   *  cascade's delay here as it would on any other entrance. */
  Element& appear(motion::Transition how);
  /** HOW THE NODE'S PAINT COMBINES with what is already beneath it — any
   *  Skia blend mode. `SkBlendMode::kSrcOver` when unstated. Anything
   *  else makes the node composite as a group, so its subtree resolves
   *  into one layer before the mode is applied, and a
   *  `preserve3d()` space ends here. */
  Element& blend(SkBlendMode mode);
  /** SLIDE THE NODE ALONG X, in pixels, positive to the right. Zero when
   *  unstated. Paint-only: the node's layout box does not move, so
   *  nothing reflows and nothing else shifts to make room. */
  Element& translateX(motion::Animatable<float> v);
  /** SLIDE THE NODE ALONG Y, in pixels, positive downward. Zero when
   *  unstated, and paint-only exactly as `translateX()` is. */
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
  /** TURN THE NODE IN ITS OWN PLANE, in degrees, positive clockwise in
   *  screen space, about the transform origin. Zero when unstated, and
   *  paint-only: the layout box stays axis-aligned where it was. */
  Element& rotate(motion::Animatable<float> degrees);
  /** SCALE THE NODE UNIFORMLY about the transform origin, 1 being its
   *  laid-out size. 1 when unstated, and paint-only, so a scaled node
   *  takes exactly the room it took unscaled. */
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
  /** Scale along Y alone about the transform origin, multiplied INTO
   *  `scale()`. 1 when unstated; the vertical twin of `scaleX()`, and
   *  what a meter or a wipe that grows downward uses with
   *  `transformOrigin(0.5f, 0)`. */
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
  /** Shear that slants HORIZONTALS, in degrees, about the transform
   *  origin. Zero when unstated, and paint-only as `skewX()` is; the
   *  sense is screen-space, y down, so a positive angle pushes points
   *  further right further down. */
  Element& skewY(motion::Animatable<float> degrees);
  /** THE INTEGER-LITERAL SPELLING of this lane and of the ones after it
   *  — `rotate(-8)`, `scale(2)` — which exists because a plain `int`
   *  does not convert into the animatable variant on its own and the
   *  error when it does not is unreadable.
   *
   *  Constrained on `std::integral` so a FLOAT call can never land here:
   *  a plain `int` overload would capture one through the standard
   *  float-to-int conversion and recurse. The animatable is constructed
   *  explicitly for the same reason. */
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
  /** THE PIVOT every rotation, scale and skew turns about, as fractions
   *  of the node's own box: 0,0 its top-left, 1,1 its bottom-right. The
   *  CENTRE (0.5, 0.5) when unstated. `transformOrigin(0, 0.5f)` is what
   *  grows a bar rightward from its left edge. */
  Element& transformOrigin(float fx, float fy);
  /** Pixel-valued transform origin (node-local px) — for pivots that
   *  aren't a fraction of THIS node's box, e.g. zooming a window that
   *  lives inside a full-canvas overlay around its own center. */
  Element& transformOriginPx(SkPoint p);
  /** PAINT ORDER AMONG SIBLINGS — CSS `z-index`. Zero when unstated, so
   *  siblings paint in declaration order; a higher number paints later
   *  and therefore on top. It reorders nothing outside this node's own
   *  parent, and it changes no layout. */
  Element& zIndex(int z);
  /** @} */

  /** @name Depth — the CSS 3D model over the 2D tree
   *  A node is a PLANE. These lanes turn it and move it in depth, and
   *  the node projects its plane onto the one its parent paints on — one
   *  4x4 per node, flattened at paint, so tree order stays draw order
   *  and everything the node holds (its fill, its text, its children,
   *  its caches) lives in the plane exactly as it did before. Paint-only
   *  like the 2D lanes: animating one never relayouts, and a settled
   *  node's recording is taken in its own plane and replayed through the
   *  projection. The frame is CSS's: x right, y down, and +z TOWARD the
   *  viewer, so a positive `translateZ` under a `perspective` comes
   *  closer and grows.
   *
   *  The three rotations compose as CSS's `rotateX() rotateY()
   *  rotateZ()` list — X outermost — and then scale and skew, about the
   *  transform origin, exactly where the 2D `rotate → scale → skew`
   *  stack stands. What none of this is: a scene. Two planes never
   *  intersect, nothing is lit, and a depth is not a position in a world
   *  — a set (SigilWorld) is where that lives.
   *  @{ */

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
  /** @} */

  /** @name Derive phase
   *  What this node says about OTHER nodes' resolved geometry, answered
   *  in a bounded second pass once layout has run.
   *  @{ */
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
  /** @} */

  /** @name Content
   *  What a leaf holds and how it is set: the image region, the
   *  per-glyph fx tracks, the marks and readings beside the type, the
   *  frame chain, and how each block of a passage is styled. Every verb
   *  here does nothing on a node of the wrong kind.
   *  @{ */
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
   *  `GlyphModifier::axis`, so a driven axis composes with entrances, loops and
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
   *          .mark(weave::selectors::word(3), box().left(0).top(pct(100))
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
   *  glyph it addressed — `weave::selectors::each(weave::Unit::Word)` therefore
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

  /** Text leaves only: THE FRAME THIS ONE FILLS INTO — the next link of a
   *  chain over one `weave::Story`.
   *
   *      root.children({frame(article).key("a").thread("b").width(Dimension(280))})
   *          .children({frame(article).key("b").thread("c").width(Dimension(280))})
   *          .children({frame(article).key("c").width(Dimension(280))});
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

  /** Text leaves only: THIS FRAME OPENS A BALANCED RUN of its chain —
   *  itself and every frame after it up to the next frame that opens one,
   *  or the chain's end.
   *
   *      frame(article).key("a").thread("b").balanceChain()
   *
   *  The run is filled to the SHALLOWEST DEPTH that still holds what it
   *  was asked to hold, found by halving the depth the frames declare;
   *  every frame of the run resolves to that one depth, which is what
   *  makes three columns of one story three columns of the same length
   *  instead of two full ones and a stub.
   *
   *  `throughLine` is what the run must hold, as a story-relative line
   *  number: the default holds ALL of the story, and a number holds the
   *  story down to that line and leaves the rest to the frames after the
   *  run. That is how a run of columns stops at a spanning element — the
   *  content above it is balanced and shortened to fit, and what is left
   *  resumes below.
   *
   *  THE FRAMES MUST DECLARE A DEPTH IN PIXELS: that depth is the ceiling
   *  the halving starts from, and a run whose frames are sized by anything
   *  else is left alone. */
  Element& balanceChain(uint32_t throughLine = ~0u);

  /** Text leaves only: A READING SET BESIDE THE TYPE — furigana over a
   *  compound, emphasis dots down a column, a gloss under a phrase.
   *
   *      text(passage, body)
   *          .block({.writingMode = WritingMode::kVerticalRL})
   *          .annotate({.where = weave::selectors::text(u8"漢字"),
   *                     .unit = weave::Unit::Selection,     // group ruby
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
   *  of the four layout-wide settings the block overrides — the alignment,
   *  the justification, the hyphenation and the tab stops — each of which
   *  falls back to this leaf's own where the block leaves it unset. */
  Element& paragraphs(std::vector<sigil::weave::ParagraphStyle> blocks);
  /** The same, by NAME: one class per block, resolved through the block
   *  half of the sheet in force where the leaf LANDS, when it lays out,
   *  and laid over the block in force there — so a named block keeps the
   *  leading it inherits and changes only what its rule says. A name no
   *  sheet in force carries warns once and changes nothing about its
   *  block. */
  Element& paragraphs(std::span<const std::string_view> names);
  /** Text leaves only: THIS PASSAGE'S OPENING SET LARGE — a versal sized so
   *  its cap height spans the lines it is given, seated on the baseline it
   *  sinks to, with the lines under it wrapping the notch it cuts.
   *
   *      text(body, bodyStyle).initialLetter({.lines = 3})
   *      text(body, bodyStyle).initialLetter({.lines = 3, .sink = 1})
   *
   *  No key, no second element and no split string: the letter is part of
   *  the passage, and the two numbers it is made of — the size that makes a
   *  cap span three lines, and the baseline it lands on — are answered
   *  where the block's pitch and the face's own metrics are, which is
   *  inside the layout. `sigil::weave::InitialLetter` carries how many
   *  letters, which metric the alignment is made on, whether the following
   *  lines wrap the box or the glyph, the standoff, and the style it is set
   *  in. It applies to the FIRST block of this passage. */
  Element& initialLetter(sigil::weave::InitialLetter initial);

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

  /** Text leaves only: AN INPUT OF THIS PASSAGE IS MOVING — a measure that
   *  animates, a frame that grows, content that changes from one frame to
   *  the next — so this layout is one of a run of them rather than an
   *  answer somebody asked for once.
   *
   *      text(caption, body).width(Dimension(slider)).live(true, 6000)
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
   *  `candidates` is the floor under a frame the optimizing breaker
   *  cannot finish: how many break candidates it may weigh for one block —
   *  one candidate being one line it scores — before that block is filled
   *  greedily for that frame and counted as a degrade. Everything is back
   *  the next frame the floor is met, and 0 is no floor. It is a COUNT and
   *  not a stretch of clock, so a passage draws the same on a loaded
   *  machine as on an idle one.
   *
   *  NOTHING INFERS THIS. A live layout answers the overflow tail
   *  differently from a settled one — it is broken against the measure
   *  rather than against the lines the frame has left — so a guess would
   *  change the setting of a page that never moves. A passage that moves
   *  says so. */
  Element& live(bool on = true, int candidates = 0);

  /** Text leaves only: ROOM BESIDE EVERY LINE of this passage, over and
   *  above the leading — `before` above a line and right of a column,
   *  `after` below one and left. It is a layout input: the room is in the
   *  strut before anything is broken. `annotate` reserves its own band on
   *  top of this, so a passage that only carries readings needs none of
   *  this. */
  Element& reserve(sigil::weave::ReservedBand band);
  /** @} */

  /** @name Span restyling — the type treatment, addressed by selector
   *  The same `selectors::` vocabulary the fx() tracks address glyphs
   *  with, used to say what a range LOOKS LIKE rather than how it moves.
   *  Each verb takes an ordered list — call any of them as many times as
   *  the passage needs — and a LATER DECLARATION WINS wherever two
   *  overlap, so a broad rule followed by a narrow exception reads in
   *  the order it is written.
   *
   *  They apply to every content form alike: plain `text(utf8, style)`,
   *  `weave::rich()` spans, and the `shared_ptr<Paragraph>` overload,
   *  because all three are one materialized paragraph by the time a
   *  restyle runs.
   *
   *  The two are ordered by WHAT THEY ARE ALLOWED TO DISTURB.
   *  `spanPaint` repaints and nothing else. `spanStyle` may change
   *  anything, and re-shapes to do it — except a change of
   *  advance-invariant axes alone, which it carries to the glyphs at
   *  draw time with the pen positions standing.
   *
   *  …which is why "later wins" holds PER DIMENSION where the two meet:
   *  the PAINT of a range is `spanPaint`'s to say, so a `spanStyle` over
   *  text an earlier `spanPaint` coloured applies its other dimensions
   *  and leaves that colour standing. Either order therefore does the
   *  same thing, and neither verb has to know what the other declared. A
   *  `spanStyle` alone paints with the style it is given, as ever.
   *
   *  Both run on the PARAGRAPH and resolve their selection as TEXT
   *  RANGES, not glyphs: `weave::selectors::text` and
   *  `weave::selectors::regex` go through weave's query layer,
   *  `weave::selectors::word`, `weave::selectors::words`,
   *  `weave::selectors::sentence` and `weave::selectors::range` through
   *  the paragraph's own structure, and `weave::selectors::line` through
   *  the layout. `weave::Selector::take` and `weave::Selector::drop`
   *  slice GLYPHS inside a unit, which a text range cannot express — an
   *  `weave::selectors::each` selector restyles its whole units here,
   *  and the slice is ignored with a warning.
   *
   *  A `weave::selectors::line` restyle addresses THE LAYOUT OF THE TEXT
   *  BEFORE THE RESTYLE, and costs a second layout pass. It does not
   *  chase its own result: a `spanStyle` on a line that moves the line
   *  breaks leaves the selection where the first breaking put it.
   *  @{ */

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
  /** Text leaves only: restyle the range this selector finds with a
   *  PARTIAL — the fields it names over the style the range is set in,
   *  which is the font in force for an inheriting leaf and the leaf's own
   *  style otherwise; the rest stands. A size in ems is of that style's
   *  size. A partial that names no shaping field — a colour, a decoration,
   *  a pass — is a repaint and never re-shapes; one that names a face, a
   *  size, tracking, features or any other shaping field re-shapes the
   *  words it covers, exactly as the whole style above does. */
  Element& spanStyle(sigil::weave::Selector where, sigil::weave::Type partial);
  /** @} */

  /** @name Layout options, fluently
   *  The general knobs of `weave::ParagraphLayoutOptions`, as setters
   *  that work on every content form. The rest of that struct —
   *  justification elasticity, Knuth-Plass tolerance, tab stops,
   *  line-metric overrides — stays behind the `shared_ptr<Paragraph>`
   *  overload, which takes the whole options value.
   *
   *  ON THE PARAGRAPH OVERLOAD THESE OVERRIDE FIELD BY FIELD, and only
   *  the fields actually set: options passed to `text(paragraph,
   *  options)` stand for everything a setter did not name. Setting none
   *  of them leaves the passed options untouched.
   *  @{ */

  /** Text leaves only: the marker appended to the last line when the text
   *  overflows its geometry. Empty disables it. */
  Element& ellipsis(Utf8 marker);
  /** Text leaves only: use at most this many lines (CSS line-clamp); the
   *  rest reports as overflow and `ellipsis()`, when set, lands on the
   *  clamped line. 0 is unclamped. */
  Element& maxLines(int lines);

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
   *  wordmark can also be a staggered entrance.
   *
   *  Takes everything the node's own fill takes — a colour, a Fill, a
   *  paint, a material — because both dress the same surface. An EMPTY
   *  paint clears the override and the glyphs go back to the style's own
   *  foreground.
   *
   *  The three spellings the slot cannot store are the cascade's two
   *  references — the ink in force and a custom property — and a live
   *  fill binding, because a glyph paint is resolved without the tree
   *  and without a binding's identity. Each of them LEAVES A STANDING
   *  OVERRIDE ALONE rather than blanking it: the glyphs are already
   *  painted in the ink in force where no override reaches, so there is
   *  nothing a reference could add, and silently clearing a ramp
   *  somebody set would repaint the letters in a colour nobody named.
   *  Clear it with an empty paint. */
  Element& textFill(SurfacePaint paint);

  /** Strokes the GLYPHS, under the fill — engraved display type, an
   *  outlined label, a caption that has to survive over an image.
   *
   *  NOT `Element::stroke()`, which dresses the node's BOX outline and is
   *  a different thing entirely. This one thickens the letterforms.
   *
   *  Composes with `textFill()` — the stroke is a pass beneath whatever
   *  fills the letterforms — with the style's own underlays and overlays,
   *  which it joins rather than replaces, and with `fx()`, which carries
   *  every pass along as the glyph moves.
   *
   *  The outline is one comparable Fill on the node, so a plain fill and
   *  a static paint collapse onto it — a gradient authored in absolute
   *  coordinates keeps its shader. A live or geometry-dependent paint
   *  has no single colour to give a slot measured without a frame, and
   *  the glyphs are outlined in the ink in force; give such a paint to
   *  `textFill()`, which resolves against the frame. */
  Element& textStroke(float width, SurfacePaint paint);
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
  /** @} */

  /** @name Identity, caching, transitions
   *  Who the node is across describes, what the painter is allowed to
   *  keep of it, and how its plain constants change.
   *  @{ */
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
  /** WHAT THE PAINTER MAY KEEP OF THIS SUBTREE — record it as a picture,
   *  bake it to a texture, bake it with its children, or nothing at all.
   *  `Cache::Auto` when unstated, which records provably-static subtrees
   *  and leaves the rest alone. A per-frame paint program that reads the
   *  clock MUST state `Cache::None`, because nothing can see that it
   *  sampled the time. See `Cache` for what each mode costs and refuses.
   *
   *  @see sigil::compose::Cache */
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
  /** HOW THIS NODE'S PLAIN CONSTANTS CHANGE when a later describe gives
   *  them a new value: the duration, easing and delay that every
   *  animatable lane on the node — its transforms, its opacity, its
   *  mask gates, its fx progresses, its fill — is retargeted over. None
   *  when unstated, so a new constant lands on the frame it arrives. A
   *  value that already carries its own `animate(...)` keeps that one;
   *  this is the node's default for the ones that do not. */
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
  /** @} */

  /** @name Composition
   *  What is under the node — written last, after every verb that says
   *  what is done to the node itself.
   *  @{ */
  /** THE CHILDREN, AS ONE BLOCK: what is in the node, in order, after
   *  every verb that says what is done to it —
   *
   *      column().gap(9).children({
   *          heading(),
   *          each(rows, row),
   *          footer(),
   *      });
   *
   *  A run of the block is an element or the list `each()` made from a
   *  range, so a block mixes the two. Braces on a description mean this
   *  and nothing else. */
  Element& children(std::initializer_list<Children> runs);
  /** THE CHILDREN FROM A RANGE, appended in the range's own order — for
   *  a container whose whole content is a collection, where the braced
   *  block would hold one `each()` and nothing else. */
  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, Element>
  Element& children(R&& range) {
    for (auto&& e : range) append(std::move(e));
    return *this;
  }
  /** @} */

  /** @private reconciler access */
  const std::shared_ptr<detail::ElementNode>& node() const {
    return m_node.value;
  }
  explicit Element(std::shared_ptr<detail::ElementNode> n)
      : m_node(std::move(n)) {}

 private:
  /** One more child at the end, which both `children()` forms go through. */
  void append(Element e);
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

/** ONE RUN OF A `children({…})` BLOCK: an element, or the list `each()`
 *  made, so the block mixes both. */
struct Children {
  std::vector<Element> items;
  Children(Element one) { items.push_back(std::move(one)); }
  Children(std::vector<Element> many) : items(std::move(many)) {}
};

}  // namespace sigil::compose
