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
#include <include/core/SkPath.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Band.h>
#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Mask.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Shape.h>
#include <sigilcompose/core/Stroke.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilcompose/core/verbs/Box.h>
#include <sigilcompose/core/verbs/Cascade.h>
#include <sigilcompose/core/verbs/Decoration.h>
#include <sigilcompose/core/verbs/Depth.h>
#include <sigilcompose/core/verbs/Effects.h>
#include <sigilcompose/core/verbs/Flex.h>
#include <sigilcompose/core/verbs/Mask.h>
#include <sigilcompose/core/verbs/Paint.h>
#include <sigilcompose/core/verbs/Placement.h>
#include <sigilcompose/core/verbs/Shape.h>
#include <sigilcompose/core/verbs/Transform.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/schedule/Schedule.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Animated.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilmotion/values/Transition.h>
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
class Element : public BoxVerbs<Element>,
                public FlexVerbs<Element>,
                public PlacementVerbs<Element>,
                public ShapeVerbs<Element>,
                public MaskVerbs<Element>,
                public CascadeVerbs<Element>,
                public PaintVerbs<Element>,
                public DecorationVerbs<Element>,
                public EffectVerbs<Element>,
                public TransformVerbs<Element>,
                public DepthVerbs<Element>,
                public BandVerbs<Element> {
 public:
  Element();  ///< An empty box: no size, no fill, no children.


  /** @name The cascade a node NAMES
   *  The sheet this node and everything under it resolve their classes
   *  through, the semantic role that stands under those classes, and
   *  the classes themselves. What a node DECLARES to its descendants —
   *  the font, the block, the ink, the custom properties and the
   *  sampling — is the cascade mixin's.
   *  @{ */
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
   *  `weave::selectors::word`, `weave::selectors::words` and
   *  `weave::selectors::range` through the paragraph's own structure,
   *  and `weave::selectors::line` through the layout.
   *  `weave::Selector::take` and `weave::Selector::drop` slice GLYPHS
   *  inside a unit, which a text range cannot express — an
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
   *  Who the node is across describes, whether it answers a hit, what
   *  the painter is allowed to keep of it, and how its plain constants
   *  change.
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

  // The verb mixins hold no state and reach this handle through the one
  // door a declaring value grants them.
  friend struct detail::NodeAccess;

  detail::NodeHandle m_node;
};

/** ONE RUN OF A `children({…})` BLOCK: an element, or the list `each()`
 *  made, so the block mixes both. */
struct Children {
  std::vector<Element> items;
  Children(Element one) { items.push_back(std::move(one)); }
  Children(std::vector<Element> many) : items(std::move(many)) {}
};

}  // namespace sigil::compose
