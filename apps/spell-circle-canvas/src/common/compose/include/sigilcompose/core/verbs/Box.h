#pragma once

/** @file
 * @ingroup compose-core
 *
 * The box model, as verbs: the air inside and outside a node, the size
 * it asks for, and the floors and ceilings around that size.
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
  /** The air inside it, one length across and one down. */
  Derived& padding(Dimension horizontal, Dimension vertical);
  /** The air inside it, a length per side, clockwise from the left. */
  Derived& padding(Dimension left, Dimension top, Dimension right,
                   Dimension bottom);
  /** The air OUTSIDE the node's box, between its edge and its siblings,
   *  the same on all four sides. Zero when unstated. */
  Derived& margin(Dimension all);
  /** The air outside it, one length across and one down. */
  Derived& margin(Dimension horizontal, Dimension vertical);
  /** The air outside it, a length per side, clockwise from the left. */
  Derived& margin(Dimension left, Dimension top, Dimension right,
                  Dimension bottom);
  /** The node's width, as a flex BASIS and not a guarantee: `shrink`
   *  defaults to 1, faithful to Yoga and CSS, so a stated width gives
   *  room back when the line it is on overflows. `shrink(0)` is what
   *  makes a size exact. Unstated, the node is as wide as its
   *  content. */
  Derived& width(Dimension d);
  /** The node's height, the same basis rather than a guarantee, and the
   *  same forms. Unstated, the node is as tall as its content. */
  Derived& height(Dimension d);
  /** A FLOOR under the node's width that the flex factors may not take
   *  it below. None when unstated. */
  Derived& minWidth(Dimension d);
  /** A CEILING over the node's width that `grow()` may not take it
   *  above. None when unstated. */
  Derived& maxWidth(Dimension d);
  /** A FLOOR under the node's height. None when unstated. */
  Derived& minHeight(Dimension d);
  /** A CEILING over the node's height. None when unstated. */
  Derived& maxHeight(Dimension d);
  /** WIDTH OVER HEIGHT, held while the other axis is free — a `16f/9`
   *  box given only a width is sized down from it. Unstated, the two
   *  axes are independent. */
  Derived& aspect(float ratio);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
