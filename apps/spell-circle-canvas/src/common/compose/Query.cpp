// Query phase (resolved-side reads): shape-aware containment and the hit test
// that walks paint()'s matrix stack backwards — transform-aware, shape-aware,
// paint-order aware, resolving keyless hits to the nearest keyed ancestor.
// The public bounds()/paragraphLayout()/hitTest()/stats() surface that calls
// into these lives in Composer.cpp.

#include "ComposeRuntime.h"

#include <include/core/SkPathBuilder.h>

#include <cmath>

namespace sigil::compose {

using namespace detail;

bool Composer::Impl::shapeContains(Instance &inst, SkPoint local,
                                   SkSize size) const {
  const ElementNode &node = *inst.desc;
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
  if (!bounds.contains(local.x(), local.y()))
    return false;
  if (node.corners.any()) {
    SkPathBuilder b;
    b.addRRect(cornersRRect(bounds, node.corners));
    return b.detach().contains(local.x(), local.y());
  }
  return true;
}

std::optional<std::string>
Composer::Impl::hitInstance(Instance &inst, SkPoint parentPt,
                            const std::string *inheritedKey) {
  const ElementNode &node = *inst.desc;

  const float opacity = std::clamp(
      inst.resolveFloat(Instance::kOpacity, node.paint.opacity), 0.0f, 1.0f);
  if (opacity <= 0.0f)
    return std::nullopt; // invisible subtrees don't hit

  // Into local space: undo the layout offset, then the paint transform (the
  // exact inverse of paint()'s matrix stack).
  const SkRect rect = instanceRect(inst);
  SkPoint local{parentPt.x() - rect.left(), parentPt.y() - rect.top()};
  local.offset(-inst.resolveFloat(Instance::kTx, node.paint.translateX),
               -inst.resolveFloat(Instance::kTy, node.paint.translateY));
  const float rot = inst.resolveFloat(Instance::kRotate, node.paint.rotate);
  const float scl = inst.resolveFloat(Instance::kScale, node.paint.scale);
  const float skx = inst.resolveFloat(Instance::kSkewX, node.paint.skewX);
  const float sky = inst.resolveFloat(Instance::kSkewY, node.paint.skewY);
  if (rot != 0 || scl != 1 || skx != 0 || sky != 0) {
    const SkPoint origin =
        resolveOrigin(node.paint, rect.width(), rect.height());
    SkPoint v{local.x() - origin.x(), local.y() - origin.y()};
    // Inverse of paint()'s rotate→scale→skew, applied in reverse order.
    if (skx != 0 || sky != 0) {
      const float kx = std::tan(skx * 0.017453293f);
      const float ky = std::tan(sky * 0.017453293f);
      const float det = 1.0f - kx * ky;
      if (std::abs(det) > 1e-6f)
        v = {(v.x() - kx * v.y()) / det, (v.y() - ky * v.x()) / det};
    }
    if (scl != 0 && scl != 1)
      v = {v.x() / scl, v.y() / scl};
    if (rot != 0) {
      const float rad = -rot * SK_FloatPI / 180.0f;
      const float c = std::cos(rad), s = std::sin(rad);
      v = {v.x() * c - v.y() * s, v.x() * s + v.y() * c};
    }
    local = {origin.x() + v.x(), origin.y() + v.y()};
  }

  const SkSize size{rect.width(), rect.height()};
  const bool inside = shapeContains(inst, local, size);
  if (node.clipContent && !inside)
    return std::nullopt; // clip bounds the whole subtree's hit region

  const std::shared_ptr<ElementNode> &shell =
      inst.memoShell && !inst.memoShell->key.empty() ? inst.memoShell
                                                     : inst.desc;
  const std::string *key = !shell->key.empty() ? &shell->key : inheritedKey;

  // Children topmost-first (reverse paint order); they may overflow the parent
  // box, so recurse regardless of `inside`.
  for (auto it = inst.paintOrder.rbegin(); it != inst.paintOrder.rend(); ++it)
    if (auto hit = hitInstance(*inst.children[*it], local, key))
      return hit;

  if (inside && key && !key->empty())
    return *key;
  return std::nullopt;
}

} // namespace sigil::compose
