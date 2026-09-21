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
#include <sigilcompose/core/Image.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Mask.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Shape.h>
#include <sigilcompose/core/Stroke.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilcompose/core/Text.h>
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
#include <sigilcompose/core/verbs/TextStyle.h>
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

// <sigilcompose/core/Derive.h>: what a node hangs off, declared beside the
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
 *  SHAPED by the verb families it inherits, one mixin per family of
 *  properties: the box, the flex factors, the placement, the shape, the
 *  mask, the cascade it declares, its paint, its decorations, the
 *  compositing lanes, the two transform stacks, the text properties and
 *  the text content, the image region and the band's formation. What is
 *  declared HERE is what no other value could state: the node's place
 *  in the cascade, its identity, what the painter may keep of it, and
 *  what is under it.
 *
 *  A VERB WHOSE VALUE THIS NODE CANNOT USE IS SILENTLY IGNORED rather
 *  than an error: the text verbs do nothing on a box, `region()` does
 *  nothing off an image leaf, and a `gridCells()` claim is read only by a
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
                public TextStyleVerbs<Element>,
                public TextContentVerbs<Element>,
                public ImageVerbs<Element>,
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

  /** HANG THIS NODE OFF A KEYED ONE, at a stated pair of points, with a
   *  list of places to try when the first will not fit. It takes the
   *  node out of the flow, and where it lands is an answer of the
   *  layout: resolved against the geometry the anchor resolved to, and
   *  re-resolved whenever that moves. A later call replaces the tether. */
  Element& tether(Tether t);

  /** @name Identity, caching, transitions
   *  Who the node is across describes, whether it answers a hit, what
   *  the painter is allowed to keep of it, and how its plain constants
   *  change.
   *  @{ */
  /** Takes this node OUT of hit testing — CSS `pointer-events: none`.
   *  Its CHILDREN are still tested: this excludes the node's own box,
   *  not its subtree. A keyed, full-bleed shell with no fill otherwise
   *  swallows every hit in the frame, silently. */
  Element& hitTestable(bool enabled);
  /** The author-owned identity: what the reconciler matches a child by
   *  across describes, what `Composer::bounds` and `hitTest` answer
   *  for, and what a connector, a rail or a `spans::fit` borrows
   *  geometry by. On a `slot()` it RENAMES the mount, and warns. */
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
  /** Texture-bake resolution multiplier, `Cache::Texture` only and
   *  clamped to 0.1–1: the bake rasterizes at @p factor times the
   *  device scale and every blit scales it back up. Almost always the
   *  wrong lever — it cheapens what happens once and taxes what happens
   *  forever. */
  Element& cacheScale(float factor);
  /** HOW THIS NODE'S PLAIN CONSTANTS CHANGE when a later describe gives
   *  them a new value: the duration, easing and delay that every
   *  animatable lane on the node — its transforms, its opacity, its
   *  mask gates, its fx progresses, its fill — is retargeted over. None
   *  when unstated, so a new constant lands on the frame it arrives. A
   *  value that already carries its own `animate(...)` keeps that one;
   *  this is the node's default for the ones that do not. */
  Element& transition(motion::Transition t);
  /** Container stagger: child i's subtree enters with an EXTRA
   *  order-times-each delay on every `animate()` mount transition under
   *  it, compounding through nested staggered containers. @p from picks
   *  the origin — declaration order, last child first (a bottom-up
   *  cascade that leaves the paint order alone), or outward from the
   *  centre. One call, and no per-child delay arithmetic. */
  Element& staggerChildren(
      std::chrono::milliseconds each,
      motion::Spread::From from = motion::Spread::From::Start);
  /** @} */

  /** @name Composition
   *  What is under the node — written last, after every verb that says
   *  what is done to the node itself.
   *  @{ */
  /** THE CHILDREN, AS ONE BLOCK: what is in the node, in order. A run
   *  of the block is an element or the list `each()` made from a range,
   *  so a block mixes the two. A later call appends. */
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
