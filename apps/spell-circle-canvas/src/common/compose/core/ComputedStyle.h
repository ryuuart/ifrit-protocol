#pragma once

/** @file
 * Internal to the kernel — the style COMPUTED for one mounted node: the
 * answers layout, paint, hit testing and the transition lanes read, and
 * the pair a lane reads an endpoint from.
 */

#include "ComposeInternal.h"

namespace sigil::compose::detail {

/** THE STYLE IN FORCE ON ONE MOUNTED NODE.
 *
 *  It is the ONE place the phases after the patch read a property from, so
 *  what fills it can change without any of them changing. The answer is
 *  the fold `resolveStyle()` makes: the node's own declarations over the
 *  parent's answers, with every property written as a keyword resolved
 *  against the two.
 *
 *  IT IS FILLED TWICE, BY ONE FUNCTION, and the second time is what makes
 *  an inherited answer honest. The PATCH fills it, holding the parent's
 *  style — the reconciler walks parents before children, so the parent's
 *  answer is current there. A node the reconciler PRUNES never reaches the
 *  patch: its description compared equal, so its own declarations did not
 *  move, and the style standing here is still the answer — unless it takes
 *  a value from ABOVE, which the node's own declarations cannot see
 *  changing. That is the case the CASCADE PASS answers: it walks the whole
 *  tree from the root whenever anything changed, and re-folds every node
 *  that writes a keyword, which is the only way a value reaches one from
 *  its parent. A node that writes none is never re-folded and pays
 *  nothing.
 *
 *  A material fill is the one property the prune proves STRUCTURALLY
 *  rather than by value: it compares by the recipe it was built from, so
 *  what stands here is the shader that recipe minted at the last patch
 *  rather than the one this describe minted. The two are interchangeable,
 *  and not only by the comparator's word: `fill(material::skia::Paint)`
 *  routes every animated or geometry-dependent paint to the material slot
 *  and clears `paint.fill`, so only a paint that is neither reaches the
 *  recipe branch — and equal recipes mint equal shaders.
 *
 *  It lives on the INSTANCE, one per mounted node, and not on the
 *  description, which is allocated per node per frame: a copy here costs
 *  the patch that writes it, not the describe. */
struct ComputedStyle {
  LayoutProps layout;
  PaintProps paint;
  Corners corners;
  /** overflow(Overflow::Clip): the node's content is cut to its shape. */
  bool clipContent = false;
};

static_assert(kFieldCount<ComputedStyle> == 4,
              "ComputedStyle gained or lost a field — fill it in "
              "resolveStyle() below, and give every property it holds a "
              "row in copyProperty(), then bump this count. A field "
              "declared here and never filled reads as its type's default "
              "on every node in every tree, which is a wrong pixel nothing "
              "reports; a property with no row there is one no keyword can "
              "move, silently.");

/** THE FOLD: @p node's own declarations over @p parent's answers, with
 *  every property written as a keyword resolved against the two.
 *
 *  A node that writes no keyword — which is nearly every node — folds to
 *  the declarations as they stand, so the common case costs the copy and
 *  nothing else. `inherit` takes the parent's computed value for that one
 *  property, `initial` the value the property's field carries on a node
 *  nobody wrote to, and `unset` whichever of the two the property's own
 *  behaviour asks for. @p parent is null at the root, which inherits from
 *  nothing and therefore reads every `inherit` as `initial`. */
void resolveStyle(const ComputedStyle* parent, const ElementNode& node,
                  ComputedStyle& out);

/** One property of @p from written over @p into — the one place a
 *  property's name is tied to the field it is kept in, and the whole of
 *  what a keyword does. A property the computed style does not carry is
 *  answered where it lives and is a no-op here. */
void copyProperty(Property property, const ComputedStyle& from,
                  ComputedStyle& into);

/** A MOUNTED NODE AND THE STYLE COMPUTED FOR IT — the two halves a lane
 *  reads its endpoint from.
 *
 *  Every property lane (an opacity, a transform, a fill's progress) is in
 *  the computed style; the POSITIONAL ones — a travel path's `t`, a text
 *  track's progress, a mask gate's fraction, a depth plane's turn — are
 *  blocks of the description, which the style does not carry. One value, so
 *  the slot table, the two sides of a patch and a mount all read a lane the
 *  same way. */
struct StyledNode {
  const ComputedStyle& style;
  const ElementNode& node;
};

}  // namespace sigil::compose::detail
