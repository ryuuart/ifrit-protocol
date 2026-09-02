/** @file
 * The depth lanes at the kernel: the 4x4 a node composes in the plane its
 * parent paints on, whether a node hosts a shared space for its children,
 * and the order the planes in a space are drawn in. The arithmetic the
 * pieces are built from is Transforms.h's; what is decided here is how a
 * node's block, its parent's view and its place in a space come together.
 */

#include <algorithm>
#include <vector>

#include "ComposeRuntime.h"
#include "PaintInternal.h"

namespace sigil::compose {

using namespace detail;

SkM44 Composer::Impl::depthMatrixOf(Instance& inst, const NodeTransform& tf,
                                    const SkRect& rect) {
  const ElementNode& node = *inst.desc;
  const DepthData* depth = node.depthData ? &*node.depthData : nullptr;
  SkM44 m = tf.matrix44({rect.left(), rect.top()}, node.paint, depth,
                        rect.width(), rect.height());
  // THE PARENT'S VIEW, folded here and nowhere else. CSS's `perspective`
  // applies to the children of the node that declares it, about the point
  // of that node's box the perspective origin names; a child whose plane
  // has not turned and has no depth is untouched by it, since a point at
  // z = 0 divides by 1.
  if (Instance* parent = inst.parent) {
    const ElementNode& pn = *parent->desc;
    if (pn.depthData) {
      const float distance = parent->resolveFloat(Instance::kPerspective,
                                                  pn.depthData->perspective);
      if (distance > 0) {
        const SkRect frame = instanceRect(*parent);
        m = perspectiveMatrix(
                distance, {frame.width() * pn.depthData->perspectiveOriginX,
                           frame.height() * pn.depthData->perspectiveOriginY}) *
            m;
      }
    }
  }
  return m;
}

bool Composer::Impl::hostsSpace(Instance& inst) {
  const ElementNode& node = *inst.desc;
  if (!node.depthData || !node.depthData->preserve3d) return false;
  // THE GROUPING PROPERTIES. Each of these composites the node as ONE
  // layer — a clip, a layer effect, a coverage layer, an opacity or blend
  // layer, a bake — and a layer is a plane: the children are rasterised
  // into it and cannot keep a depth of their own past it. So a node that
  // carries any of them hosts no space, and its children project onto its
  // plane one by one exactly as they would under a flat parent. The rule
  // is CSS's, stated on Element::preserve3d.
  if (node.clipContent || node.hasMasks() || layerEffectOf(node) ||
      backdropEffectOf(node) ||
      node.paint.blendMode != SkBlendMode::kSrcOver ||
      node.boundary == Boundary::Coverage ||
      node.cacheMode == Cache::Texture || node.cacheMode == Cache::Group)
    return false;
  // Opacity is read as this frame resolves it, so a fade crossing 1 opens
  // or closes the space on the frame it does — which is the CSS behaviour
  // too, and the only reading the painter, the hit test and the bounds
  // walk can all agree on.
  return inst.resolveFloat(Instance::kOpacity, node.paint.opacity) >= 1.0f;
}

void Composer::Impl::depthOrder(Instance& host, const SkM44& space,
                                std::vector<size_t>& out) {
  out.assign(host.paintOrder.begin(), host.paintOrder.end());
  if (out.size() < 2) return;
  // The depth of each child's centre in the space. The centre is the
  // plane's own (w/2, h/2, 0) carried through the child's full matrix,
  // and its z is compared BEFORE the perspective divide: for a point in
  // front of the viewer, z/w is monotonic in z, so the order is the
  // projected order without a division that could put a point at the
  // viewer's own depth anywhere.
  std::vector<float> depth(host.children.size(), 0.0f);
  for (size_t index : out) {
    Instance& child = *host.children[index];
    const SkRect rect = instanceRect(child);
    const SkM44 m(space, depthMatrixOf(child, transformOf(child), rect));
    depth[index] = m.map(rect.width() * 0.5f, rect.height() * 0.5f, 0, 1).z;
  }
  // Stable, so two planes at one depth keep the order zIndex and
  // declaration gave them — a flat stack inside a space is still a stack.
  std::stable_sort(out.begin(), out.end(), [&depth](size_t a, size_t b) {
    return depth[a] < depth[b];
  });
}

}  // namespace sigil::compose
