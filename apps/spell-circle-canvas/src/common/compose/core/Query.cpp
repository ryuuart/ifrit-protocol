/** @file
 * Query phase (resolved-side reads): shape-aware containment and the hit test
 * that walks paint()'s matrix stack backwards — transform-aware, shape-aware,
 * paint-order aware, resolving keyless hits to the nearest keyed ancestor.
 * The public bounds()/paragraphLayout()/hitTest()/stats() surface that calls
 * into these lives in Composer.cpp.
 */

#include <include/core/SkPathBuilder.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/path/Contour.h>
#include <sigilgeometry/path/Numeric.h>

#include <cmath>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** How far from an open contour's line a point still lands on it, in the
 *  node's own units: the reach a pointer is given along a wire or an arc,
 *  whose drawn width is the dress's business and not the shape's. */
constexpr float kOpenContourReach = 6.0f;

/** Whether @p local lands on @p outline: inside its CLOSED contours under
 *  the path's fill rule, or within `kOpenContourReach` of the line of an
 *  OPEN one. An open contour is a line, and the region the fill's implicit
 *  close would enclose — the lens under an arc, the triangle inside an
 *  elbow — is empty space beside what is drawn along it. */
bool outlineContains(const SkPath& outline, SkPoint local) {
  SkPathBuilder closedPart(outline.getFillType());
  SkPathBuilder contour;
  bool open = false;
  bool anyOpen = false;
  const auto finish = [&] {
    if (contour.isEmpty()) return;
    if (open)
      anyOpen = true;
    else
      closedPart.addPath(contour.detach());
    contour = SkPathBuilder();
  };
  SkPath::Iter iter(outline, false);
  SkPoint points[4];
  for (SkPath::Verb verb; (verb = iter.next(points)) != SkPath::kDone_Verb;) {
    switch (verb) {
      case SkPath::kMove_Verb:
        finish();
        open = !iter.isClosedContour();
        contour.moveTo(points[0]);
        break;
      case SkPath::kLine_Verb: contour.lineTo(points[1]); break;
      case SkPath::kQuad_Verb: contour.quadTo(points[1], points[2]); break;
      case SkPath::kConic_Verb:
        contour.conicTo(points[1], points[2], iter.conicWeight());
        break;
      case SkPath::kCubic_Verb:
        contour.cubicTo(points[1], points[2], points[3]);
        break;
      case SkPath::kClose_Verb: contour.close(); break;
      default: break;
    }
  }
  finish();
  if (!anyOpen) return outline.contains(local.x(), local.y());
  if (closedPart.detach().contains(local.x(), local.y())) return true;
  const glm::vec2 point = geometry::path::fromSk(local);
  for (const geometry::path::Contour& line :
       geometry::path::Contour::of(geometry::path::fromSk(outline)))
    if (!line.closed() && line.nearest(point).gap <= kOpenContourReach)
      return true;
  return false;
}

}  // namespace

bool Composer::Impl::shapeContains(Instance& inst, SkPoint local,
                                   SkSize size) const {
  const ElementNode& node = *inst.description;
  if (node.shapeFn) return outlineContains(resolveOutline(inst, size), local);
  const SkRect bounds = SkRect::MakeWH(size.width(), size.height());
  if (!bounds.contains(local.x(), local.y())) return false;
  const Corners& corners = inst.computed.corners;
  if (corners.any()) {
    SkPathBuilder b;
    b.addRRect(cornersRRect(bounds, corners));
    return b.detach().contains(local.x(), local.y());
  }
  return true;
}

std::optional<std::string> Composer::Impl::hitInstance(
    Instance& inst, SkPoint parentPt, const std::string* inheritedKey,
    const HitSpace* space) {
  const ElementNode& node = *inst.description;
  const ComputedStyle& style = inst.computed;
  if (style.layout.display == Display::None) return std::nullopt;

  const float opacity = std::clamp(
      inst.resolveFloat(Instance::kOpacity, style.paint.opacity), 0.0f, 1.0f);
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
        node.depthData->backface == material::Backface::Hidden && facesAway(m))
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
    const float kx = std::tan(geometry::path::radians(tf.skx));
    const float ky = std::tan(geometry::path::radians(tf.sky));
    if (std::abs(1.0f - kx * ky) <= 1e-6f) safe.skx = safe.sky = 0;
    SkMatrix inv;
    if (safe.matrix({0, 0}, rect.width(), rect.height()).invert(&inv))
      local = inv.mapPoint(local);
    else  // unreachable once sanitized; match "never refuse": translate only
      local.offset(-tf.tx, -tf.ty);
  }

  const SkSize size{rect.width(), rect.height()};
  const bool inside = placed && shapeContains(inst, local, size);
  if (style.clipContent && !inside)
    return std::nullopt;  // clip bounds the whole subtree's hit region

  const std::shared_ptr<ElementNode>& shell =
      inst.memoShell && !inst.memoShell->key.empty() ? inst.memoShell
                                                     : inst.description;
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
    for (auto it = inst.paintOrder.rbegin(); it != inst.paintOrder.rend(); ++it)
      if (auto hit = hitInstance(*inst.children[*it], local, key, nullptr))
        return hit;
  }

  if (inside && key && !key->empty() && node.hitTestable) return *key;
  return std::nullopt;
}

}  // namespace sigil::compose
