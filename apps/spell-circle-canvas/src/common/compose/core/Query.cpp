/** @file
 * Query phase (resolved-side reads): shape-aware containment and the hit test
 * that walks paint()'s matrix stack backwards — transform-aware, shape-aware,
 * paint-order aware, resolving keyless hits to the nearest keyed ancestor.
 * The public bounds()/paragraphLayout()/hitTest()/stats() surface that calls
 * into these lives in Composer.cpp.
 */

#include <include/core/SkPathBuilder.h>

#include <cmath>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

bool Composer::Impl::shapeContains(Instance& inst, SkPoint local,
                                   SkSize size) const {
  const ElementNode& node = *inst.desc;
  // Routed elements (rails, connectors) hit near their PATH, not their
  // layout box — a rail placed absolute().inset(0) must not eclipse the
  // scene. The stroke-expanded hit path is built at derive time.
  if (node.deriveData && (!node.deriveData->connectFrom.empty() ||
                          !node.deriveData->railAnchors.empty()))
    return !inst.routedHitPath.isEmpty() &&
           inst.routedHitPath.contains(local.x(), local.y());
  if (node.shapeFn)
    return resolveOutline(inst, size).contains(local.x(), local.y());
  const SkRect bounds = SkRect::MakeWH(size.width(), size.height());
  if (!bounds.contains(local.x(), local.y())) return false;
  if (node.corners.any()) {
    SkPathBuilder b;
    b.addRRect(cornersRRect(bounds, node.corners));
    return b.detach().contains(local.x(), local.y());
  }
  return true;
}

std::optional<std::string> Composer::Impl::hitInstance(
    Instance& inst, SkPoint parentPt, const std::string* inheritedKey,
    const HitSpace* space) {
  const ElementNode& node = *inst.desc;

  const float opacity = std::clamp(
      inst.resolveFloat(Instance::kOpacity, node.paint.opacity), 0.0f, 1.0f);
  if (opacity <= 0.0f) return std::nullopt;  // invisible subtrees don't hit

  // Into local space: undo the layout offset, then the paint transform (the
  // exact inverse of paint()'s matrix stack).
  const SkRect rect = instanceRect(inst);
  SkPoint local{parentPt.x() - rect.left(), parentPt.y() - rect.top()};
  // One resolver for the whole matrix, the same one paint uses — a
  // travelling node's position replaces the translate lanes and its
  // auto-orient adds to rotate, and the hit test must undo exactly what
  // paint applied.
  const NodeTransform tf = transformOf(inst);
  const bool hosts = hostsSpace(inst);
  // Does the point land on this node's plane at all? A plane that has
  // turned answers through its whole projection; a flat node always has a
  // place for it.
  bool placed = true;
  std::optional<SkM44> depth;
  if (space || hosts || tf.spatial()) {
    // A PLANE THAT HAS TURNED, or one standing in a shared space: the
    // point is taken back through the flattened 4x4 paint drew with — the
    // same producer, so a hit lands on the pixels — from the plane the
    // node was drawn on, which for a node in a space is the space's own
    // rather than its parent's. Two ways not to land: the flattening has
    // no inverse (the plane is edge-on, and drew nothing), or the
    // pre-image sits at or behind the viewer, which is no place on the
    // plane however the numbers divide.
    SkM44 m = depthMatrixOf(inst, tf, rect);
    if (space) m = SkM44(space->accum, m);
    depth = m;
    const SkPoint from = space ? space->planePt : parentPt;
    const SkMatrix flat = m.asM33();
    SkMatrix inverse;
    placed = flat.invert(&inverse);
    if (placed) {
      local = inverse.mapPoint(from);
      placed = projectPoint(flat, local).has_value();
    }
    // The back of a plane whose backface is hidden was not drawn, and
    // answers no hit either.
    if (placed && node.depthData &&
        node.depthData->backface == Backface::Hidden && facesAway(m))
      placed = false;
  } else {
    // The inverse comes from SkMatrix::invert of that same matrix producer
    // (NodeTransform::matrix), never a hand-unwound copy. Two spellings of
    // one transform drift the moment either gains a lane, and the drift
    // shows up as hits landing off the pixels — which is why the producer
    // is shared rather than mirrored here.
    //
    // Degenerate lanes are sanitized rather than refused: a zero scale axis
    // or a numerically singular skew pair makes that STEP identity, so a
    // zero-scaled node still answers hits as if unscaled instead of
    // becoming unhittable.
    NodeTransform safe = tf;
    if (tf.scl * tf.sx == 0 || tf.scl * tf.sy == 0)
      safe.scl = safe.sx = safe.sy = 1;
    const float kx = std::tan(tf.skx * 0.017453293f);
    const float ky = std::tan(tf.sky * 0.017453293f);
    if (std::abs(1.0f - kx * ky) <= 1e-6f) safe.skx = safe.sky = 0;
    SkMatrix inv;
    if (safe.matrix({0, 0}, node.paint, rect.width(), rect.height())
            .invert(&inv))
      local = inv.mapPoint(local);
    else  // unreachable once sanitized; match "never refuse": translate only
      local.offset(-tf.tx, -tf.ty);
  }

  const SkSize size{rect.width(), rect.height()};
  const bool inside = placed && shapeContains(inst, local, size);
  if (node.clipContent && !inside)
    return std::nullopt;  // clip bounds the whole subtree's hit region

  const std::shared_ptr<ElementNode>& shell =
      inst.memoShell && !inst.memoShell->key.empty() ? inst.memoShell
                                                     : inst.desc;
  const std::string* key = !shell->key.empty() ? &shell->key : inheritedKey;

  if (hosts) {
    // The children stand in the space this node hosts, and the nearest
    // plane is the one under the point — the reverse of the depth order
    // they are painted in, from the plane the space is drawn on.
    const HitSpace below{*depth, space ? space->planePt : parentPt};
    std::vector<size_t> order;
    depthOrder(inst, *depth, order);
    for (auto it = order.rbegin(); it != order.rend(); ++it)
      if (auto hit = hitInstance(*inst.children[*it], {}, key, &below))
        return hit;
  } else if (placed) {
    // Children topmost-first (reverse paint order); they may overflow the
    // parent box, so recurse regardless of `inside`. A plane the point does
    // not land on carries its flat children with it: nothing under it is
    // hittable either.
    for (auto it = inst.paintOrder.rbegin(); it != inst.paintOrder.rend();
         ++it)
      if (auto hit = hitInstance(*inst.children[*it], local, key, nullptr))
        return hit;
  }

  if (inside && key && !key->empty() && node.hitTestable) return *key;
  return std::nullopt;
}

std::vector<std::string> Composer::routesAt(std::string_view nodeKey) const {
  std::vector<std::string> keys;
  auto it = m_impl->routesByAnchor.find(nodeKey);
  if (it == m_impl->routesByAnchor.end()) return keys;
  keys.reserve(it->second.size());
  for (const detail::Instance* route : it->second) {
    // A route's addressable key may live on its memo shell (memo'd routes).
    const std::shared_ptr<detail::ElementNode>& shell =
        route->memoShell ? route->memoShell : route->desc;
    if (!shell->key.empty())
      keys.push_back(shell->key);
    else if (!route->desc->key.empty())
      keys.push_back(route->desc->key);
  }
  return keys;
}

}  // namespace sigil::compose
