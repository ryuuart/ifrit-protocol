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
 *  what fills it can change without any of them changing. Today the answer
 *  is the description's own declarations, copied as they stand.
 *
 *  WRITTEN WHERE THE DESCRIPTION IS PATCHED, and nowhere else. A patch the
 *  reconciler PRUNES swaps in a description `propertiesEqual()` proved
 *  carries the same properties, so the style standing here is still the
 *  answer — the same invariant the pruned node's replayed recording rests
 *  on, read one level up. The one property that prune proves STRUCTURALLY
 *  rather than by value is a material fill, which compares by the recipe
 *  it was built from: what stands here is then the shader that recipe
 *  minted at the last patch rather than the one this describe minted, and
 *  the comparator's whole claim is that the two are interchangeable.
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
              "computeStyle() below, then bump this count. A field declared "
              "here and never filled reads as its type's default on every "
              "node in every tree, which is a wrong pixel nothing reports.");

/** The style @p node declares, as it stands: no resolution, no inheritance,
 *  no rule folded in. */
inline void computeStyle(const ElementNode& node, ComputedStyle& out) {
  out.layout = node.layout;
  out.paint = node.paint;
  out.corners = node.corners;
  out.clipContent = node.clipContent;
}

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
