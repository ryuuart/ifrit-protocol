#pragma once

/** @file
 * @ingroup compose-core
 *
 * The box model, as verbs: the air inside and outside a node, the size
 * it asks for, the floors and ceilings around that size, what the size
 * measures, and whether the node has a box at all.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Layout.h>

namespace sigil::compose {

/** THE BOX MODEL. Every length is a `Dimension`: a bare number is
 *  pixels, a percent is of the parent, and `1_em`, `0.5_lh` and `1_rem`
 *  measure against the font in force — the node's own size and line
 *  height, or the root's — so the air around type follows the type. */
template <class Derived>
class BoxVerbs {
 public:
  /** The air BETWEEN the children, along both axes. Zero when
   *  unstated. */
  Derived& gap(Dimension length);
  /** The air INSIDE the node's box, between its edge and its content,
   *  the same on all four sides. Zero when unstated. */
  Derived& padding(Dimension all);
  /** The air inside it, one length down and one across — CSS's two-value
   *  shorthand, so the VERTICAL one comes first. */
  Derived& padding(Dimension vertical, Dimension horizontal);
  /** The air inside it, top, then both sides, then bottom. */
  Derived& padding(Dimension top, Dimension horizontal, Dimension bottom);
  /** The air inside it, a length per side, clockwise from the TOP — CSS's
   *  order. An `Edges` says the same thing with the sides named. */
  Derived& padding(Dimension top, Dimension right, Dimension bottom,
                   Dimension left);
  /** The air inside it, per side, each side saying which it is. A side
   *  left unnamed is zero. */
  Derived& padding(Edges edges);
  /** The air inside it against its TOP edge alone, leaving the other
   *  three as they stand. */
  Derived& paddingTop(Dimension length);
  /** The air inside it against its RIGHT edge alone. */
  Derived& paddingRight(Dimension length);
  /** The air inside it against its BOTTOM edge alone. */
  Derived& paddingBottom(Dimension length);
  /** The air inside it against its LEFT edge alone. */
  Derived& paddingLeft(Dimension length);
  /** The air OUTSIDE the node's box, between its edge and its siblings,
   *  the same on all four sides. Zero when unstated. */
  Derived& margin(Dimension all);
  /** The air outside it, one length down and one across, the vertical one
   *  first. */
  Derived& margin(Dimension vertical, Dimension horizontal);
  /** The air outside it, top, then both sides, then bottom. */
  Derived& margin(Dimension top, Dimension horizontal, Dimension bottom);
  /** The air outside it, a length per side, clockwise from the TOP. */
  Derived& margin(Dimension top, Dimension right, Dimension bottom,
                  Dimension left);
  /** The air outside it, per side, each side saying which it is. A side
   *  left unnamed is zero. */
  Derived& margin(Edges edges);
  /** The air outside it above its TOP edge alone, leaving the other
   *  three as they stand. */
  Derived& marginTop(Dimension length);
  /** The air outside it beyond its RIGHT edge alone. */
  Derived& marginRight(Dimension length);
  /** The air outside it below its BOTTOM edge alone. */
  Derived& marginBottom(Dimension length);
  /** The air outside it beyond its LEFT edge alone. */
  Derived& marginLeft(Dimension length);
  /** The node's width, as a flex BASIS and not a guarantee: `flexShrink`
   *  defaults to 1, faithful to Yoga and CSS, so a stated width gives
   *  room back when the line it is on overflows. `flexShrink(0)` is what
   *  makes a size exact. Unstated, the node is as wide as its
   *  content. */
  Derived& width(Dimension d);
  /** The node's height, the same basis rather than a guarantee, and the
   *  same forms. Unstated, the node is as tall as its content. */
  Derived& height(Dimension d);
  /** A FLOOR under the node's width that the flex factors may not take
   *  it below. None when unstated. */
  Derived& minWidth(Dimension d);
  /** A CEILING over the node's width that `flexGrow()` may not take it
   *  above. None when unstated. */
  Derived& maxWidth(Dimension d);
  /** A FLOOR under the node's height. None when unstated. */
  Derived& minHeight(Dimension d);
  /** A CEILING over the node's height. None when unstated. */
  Derived& maxHeight(Dimension d);
  /** WIDTH OVER HEIGHT, held while the other axis is free — CSS
   *  `aspect-ratio`. A `16f/9` box given only a width is sized down
   *  from it. Unstated, the two axes are independent. */
  Derived& aspectRatio(float ratio);
  /** WHAT `width()` AND `height()` MEASURE — CSS `box-sizing`.
   *  `BoxSizing::BorderBox` when unstated: the stated size holds the
   *  padding. Under `BoxSizing::ContentBox` it is the content's, and the
   *  padding is added outside it. */
  Derived& boxSizing(BoxSizing sizing);
  /** WHETHER THE NODE HAS A BOX AT ALL — CSS `display`. `Display::Flex`
   *  when unstated. `Display::None` removes the node and everything
   *  under it from the layout, the picture and the hit test while the
   *  description keeps it. `Display::Contents` removes only the node's
   *  own box: its children are laid out as its parent's, and a size, a
   *  fill or a clip said to the node itself has nothing to apply to. */
  Derived& display(Display display);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
