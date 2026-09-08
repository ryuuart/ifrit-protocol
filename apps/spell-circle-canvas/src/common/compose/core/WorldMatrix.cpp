/** @file
 * The node→root matrix recomputed outside paint, and the released-motion
 * scan over it: an externally driven output that moves re-declares
 * volatility in the same frame, so nothing stale ever replays.
 */

#include <array>
#include <utility>
#include <vector>

#include "ComposeRuntime.h"
#include "PaintInternal.h"

namespace sigil::compose {

using namespace detail;

/** The movement scan for released instances: re-checked once per draw, so
 *  an EXTERNALLY-driven output that moves re-declares volatility (and
 *  stales every ancestor recording) in the same frame — nothing stale ever
 *  replays. Cheap by construction: released nodes are few and each check is
 *  a handful of float resolves. */
// The node→root matrix, recomputed outside paint. The op sequence per level
// — preTranslate(rect), preConcat(matrix()) for a flat node; preConcat of
// the flattened 4x4 for a plane that has turned; the space's own plane
// then preConcat for a node standing in a shared space — is EXACTLY the
// sequence paint() applies to curToRoot as it recurses, on the same
// resolved floats, so the result is bit-identical to the paint-side
// accumulation. The settle compare depends on that: an ulp of drift reads
// as motion, and the node never releases.
SkMatrix Composer::Impl::worldMatrixOf(Instance& inst) {
  std::vector<Instance*> chain;
  for (Instance* i = &inst; i; i = i->parent) chain.push_back(i);
  SkMatrix m = SkMatrix::I();
  std::optional<Space> space;  // the space the current link stands in
  for (Instance* link : std::views::reverse(chain)) {
    Instance& node = *link;
    const SkRect rect = instanceRect(node);
    const NodeTransform tf = transformOf(node);
    const SkMatrix plane = m;  // the parent's plane, before this link
    const bool hosts = hostsSpace(node);
    std::optional<SkM44> depth;
    if (space || hosts || tf.spatial()) {
      SkM44 d = depthMatrixOf(node, tf, rect);
      if (space) d = SkM44(space->accum, d);
      depth = d;
    }
    if (space) {
      m = space->rootToPlane;
      m.preConcat(depth->asM33());
    } else if (depth) {
      m.preConcat(depth->asM33());
    } else {
      m.preTranslate(rect.left(), rect.top());
      m.preConcat(tf.matrix({0, 0}, node.description->paint, rect.width(),
                            rect.height()));
    }
    if (hosts)
      space = Space{*depth, space ? space->rootToPlane : plane};
    else
      space.reset();
  }
  return m;
}

namespace {
std::array<float, 6> worldSix(const SkMatrix& w) {
  return {w.getScaleX(), w.getSkewX(),  w.getTranslateX(),
          w.getSkewY(),  w.getScaleY(), w.getTranslateY()};
}
}  // namespace

std::array<float, 6> Composer::Impl::worldScalarsOf(Instance& inst) {
  if (!inst.hasWorldSpaceMaterial)
    return {};  // all-zero, matching the paint-side guard
  return worldSix(worldMatrixOf(inst));
}

void Composer::Impl::scanReleasedScalars() {
  if (volatileDirty || releasedScalars.empty())
    return;  // a pending recompute rebuilds the list (pointers may be stale)
  for (Instance* inst : releasedScalars) {
    Instance::ContentScalars now;
    now.gates = inst->resolveGateValues();
    now.tracks = inst->resolveTrackValues();
    // The node→root matrix: a released world-space node whose externally
    // driven transform resumes must re-declare THE FRAME it resumes, before
    // any recording carrying the old anchoring replays.
    now.world = worldScalarsOf(*inst);
    // The bound fill: a released node whose output is assigned while the
    // volatility walk is idle must re-declare before its settled colour
    // replays from an ancestor's recording or its own promoted bake.
    now.fill = inst->resolveBoundFill();
    // The bound tile pan: same argument, so a parked scroll re-declares
    // before its parked phase replays.
    now.pattern = inst->resolvePatternOffset();
    // …and onPath()'s phase: a released marquee whose output is driven
    // again must re-declare before its parked frame replays.
    now.pathAt = inst->resolvePathAt();
    // The hold's rescan side: it restarts the warmup from the new reading
    // and answers whether the node must re-declare.
    if (inst->settle.moved(std::move(now))) {
      inst->markPaintDirtyUp();
      volatileDirty = true;  // re-walk this frame, before anything paints
    }
  }
}

}  // namespace sigil::compose
