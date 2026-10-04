/** @file
 * The split bake: a node whose OWN paint is static under children that are
 * not — the own half baked and blitted device-aligned, the children and the
 * foregrounds drawn live over it.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkRect.h>
#include <sigilmeasure/time/Stopwatch.h>

#include <algorithm>
#include <cstdint>
#include <utility>

#include "paint/PaintInternal.h"
#include "paint/PaintPass.h"

namespace sigil::compose {

using namespace detail;

// Keep a static own layer while volatile children paint over its blit.
// Cost and dirtiness are measured on the own phase; descendant changes
// must not trigger a replacement of pixels that never contained them.
//
// The own layer must use srcOver without reading the backdrop. Children
// can blend with or filter the backdrop because they run after the blit.
// A whole-node image filter cannot be split: it filters the union of both
// phases. Clips and masks remain valid because each phase applies them
// around the same paint groups. The intermediate image rounds to the
// destination's precision, so overlapping own paints require pixel comparisons.

bool paintSplitBake(PaintPass& pass) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  const ElementNode& node = pass.node;
  SkCanvas& canvas = pass.canvas;
  const SkRect& rect = pass.rect;
  const SkMatrix& totalM = pass.totalM;
  const SkBlendMode leafBlend = pass.leafBlend;
  const float leafOpacity = pass.leafOpacity;
  using Phase = Composer::Impl::Phase;
  using Prom = Composer::Promotion;

  const bool splitCandidate =
      !pass.optedOut && !impl.liveOnly && inst.subtreeVolatile &&
      !inst.ownContentVolatile &&  // the CHILDREN are what block this node
      !inst.children.empty() && !inst.ownReadsBackdrop &&
      !layerEffectOf(node) && leafBlend == SkBlendMode::kSrcOver &&
      leafOpacity >= 1.0f && rect.width() >= 0.5f && rect.height() >= 0.5f &&
      pass.deviceBakeable && !inst.transformLive && !pass.spaceHost &&
      // Split caching uses promotion's axis-aligned eligibility gate.
      pass.upright;
  if (!splitCandidate)
    inst.splitBake = false;
  else if (pass.eager)
    inst.splitBake = true;  // the own half, from its first frame
  if (splitCandidate) {
    // ownPaintBounds, NOT recordBounds. recordBounds unions the children
    // in, so it moves every frame a child moves — and a bake rect that
    // moves every frame is a bake remade every frame, which is the one
    // failure mode that would make this feature cost more than it saves on
    // precisely the scenes it exists for. The own paint's extent does not
    // depend on the children at all.
    const SkIRect device = pass.bakeRect(impl.ownPaintBounds(inst));
    const int64_t area = (int64_t)device.width() * device.height();
    const size_t bytes = (size_t)std::max<int64_t>(area, 0) *
                         (size_t)impl.bakeInfo({1, 1}).bytesPerPixel();
    const bool affordable =
        inst.ownImage ||
        std::max(impl.promotedBytesLast, impl.promotedBytes) + bytes <=
            Composer::Impl::kPromotedBudget;
    bool blitted = false;
    if (inst.splitBake && device.width() > 0 && device.height() > 0 &&
        area <= int64_t{16} * 1024 * 1024 && affordable) {
      // `ownPaintDirty`, NOT `paintDirty`. markPaintDirtyUp() propagates a
      // descendant's patch to every ancestor, which is right for a
      // recording (it baked the child's draw calls) and wrong here: the
      // children were never in this bake, and the whole point is that they
      // change. If that ever inverts, the feature silently does nothing and
      // still passes every pixel test.
      const SkRect want = SkRect::Make(device);
      if (!inst.ownImage || inst.ownPaintDirty || inst.ownBakeRect != want ||
          inst.ownBakeClip != pass.deviceClip() ||
          inst.ownBakeMatrix != pass.totalM) {
        inst.ownImage.reset();
        sk_sp<SkImage> baked = pass.takeDeviceBake(device, [&](SkCanvas& lc) {
          const BakeLayerScope bakeLayer(&impl, lc);
          impl.paintContent(inst, lc, impl.hostScale, leafBlend, leafOpacity,
                            Phase::OwnOnly);
        });
        if (baked) {
          inst.ownImage = std::move(baked);
          inst.ownBakeRect = want;
          inst.ownBakeClip = pass.deviceClip();
          inst.ownBakeMatrix = pass.totalM;
          inst.ownPaintDirty = false;
          if (!impl.coverageTrace) impl.stats.texturesBaked++;
          // A bake per frame costs MORE than the live draw it replaced, so
          // a node whose own paint really is being invalidated every frame
          // must not hold the promotion on the strength of a measurement
          // taken while it was still cheap. Three consecutive re-bakes and
          // it goes live and has to earn it again over the full warmup.
          if (inst.ownRebakes < 255) ++inst.ownRebakes;
          if (inst.ownRebakes > 3) {
            inst.splitBake = false;
            inst.ownHotFrames = 0;
            inst.ownRebakes = 0;
          }
        }
      } else {
        inst.ownRebakes = 0;
      }
      if (inst.ownImage && inst.splitBake) {
        impl.promotedBytes += bytes;
        if (pass.profile.row != SIZE_MAX)
          impl.profileRows[pass.profile.row].cacheState =
              Composer::CacheState::SplitOwn;
        pass.note(Prom::SplitBaked);
        pass.profDraw("split blit",
                      [&] { pass.deviceBlit(inst.ownImage, device, nullptr); });
        blitted = true;
      }
    }
    if (!blitted) {
      // The own half, live and TIMED. This is the number the split is
      // promoted on — the node's own paint, with its children excluded by
      // construction rather than by subtraction.
      const measure::Stopwatch ownWatch;
      pass.profDraw("live own", [&] {
        impl.paintContent(inst, canvas, impl.hostScale, leafBlend, leafOpacity,
                          Phase::OwnOnly);
      });
      const double ownMs = measure::Milliseconds(ownWatch.elapsed()).count();
      inst.ownPaintMs = inst.ownPaintMs * 0.6f + (float)ownMs * 0.4f;
      if (inst.ownPaintMs > kPromoteMs) {
        if (inst.ownHotFrames < 255) ++inst.ownHotFrames;
        if (inst.ownHotFrames >= kPromoteFrames) {
          inst.splitBake = true;
          inst.ownPaintDirty = true;  // force the first bake
        } else {
          pass.note(Prom::Warming);
        }
      } else if (inst.ownHotFrames > 0) {
        --inst.ownHotFrames;
      }
    }
    // The children and the foregrounds over them — always live, whatever
    // happened above. Foregrounds paint AFTER the children, so they are in
    // this half and never in the bake.
    impl.paintContent(inst, canvas, impl.hostScale, leafBlend, leafOpacity,
                      Phase::ChildrenOnly);
    if (!impl.coverageTrace) impl.stats.nodesPainted++;
    inst.paintDirty = false;
    return true;
  }

  return false;
}

}  // namespace sigil::compose
