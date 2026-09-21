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
#include <sigilcompose/core/StyleSheet.h>
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
#include <sigilcompose/core/verbs/Structure.h>
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
                public BandVerbs<Element>,
                public StructureVerbs<Element> {
 public:
  Element();  ///< An empty box: no size, no fill, no children.

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
