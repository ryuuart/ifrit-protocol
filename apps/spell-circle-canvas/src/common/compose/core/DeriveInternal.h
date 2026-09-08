#pragma once

/** @file
 * What every borrow in the derive phase shares: the shape a laid-out
 * instance occupies in its own space, whether that shape is a silhouette
 * it declared, the hit tolerance a routed element is tested against, and
 * the cycle guard that refuses a descendant's geometry.
 *
 * This is where reference cycles are rejected — nothing upstream checks
 * for them. Every borrow resolves a key to an instance and then refuses
 * the answer if that instance is this node or one of its descendants: a
 * descendant's box is computed FROM this node's, so borrowing it would
 * feed the shape its own output and the loop would never settle. The
 * refusal is silent by design (draw nothing rather than diverge), which is
 * the same answer an unknown key gets.
 */

#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

/** Routed elements hit near their PATH, not their layout box (an inset(0)
 *  rail must not eclipse the scene): expand the route by a ±6px tolerance
 *  once at derive time; Query.cpp tests containment against it. */
inline SkPath expandForHit(const SkPath& route) {
  SkPaint p;
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(12.0f);
  p.setStrokeCap(SkPaint::kRound_Cap);
  return skpathutils::FillPathWithPaint(route, p);
}

/** The shape a laid-out instance actually occupies, in its OWN space —
 *  the same answer the painter builds, so a borrowed spine and the
 *  element it was borrowed from can never disagree. */
inline SkPath resolvedShapeOf(Instance& inst) {
  const ElementNode& node = *inst.description;
  const SkRect rect = inst.owner->instanceRect(inst);
  const SkSize size{rect.width(), rect.height()};
  if (node.deriveData && !inst.connectorPath.isEmpty())
    return inst.connectorPath;
  if (node.shapeFn) return node.shapeFn(size);
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(size.width(), size.height()));
  return b.detach();
}

/** Whether that shape is a SILHOUETTE the node declared, as opposed to the
 *  rectangle `resolvedShapeOf` falls back to. Corner radii deliberately do
 *  not count: they round the fill, not the outline the borrow family reads,
 *  and `resolvedShapeOf` ignores them everywhere else too. */
inline bool hasResolvedSilhouette(const Instance& inst) {
  const ElementNode& node = *inst.description;
  return (bool)node.shapeFn ||
         (node.deriveData && !inst.connectorPath.isEmpty());
}

/** The cycle guard every borrow in this file applies: the target must not be
 *  this node or anything under it. Borrowing a DESCENDANT's geometry is the
 *  cycle — the child's box is derived from this node's, so the shape would
 *  feed itself and the layout loop would never settle. If you add another
 *  borrow, it needs this check too, or an author can hang the layout pass
 *  with a key. */
inline bool borrowIsCyclic(const Instance& inst, Instance* target) {
  for (Instance* p = target; p; p = p->parent)
    if (p == &inst) return true;
  return false;
}

}  // namespace sigil::compose
