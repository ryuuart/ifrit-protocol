/** @file
 * The split bake: a node whose OWN paint is static under children that are
 * not — the own half baked and blitted device-aligned, the children and the
 * foregrounds drawn live over it.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>
#include <sigilmeasure/time/Stopwatch.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "PaintInternal.h"
#include "PaintPass.h"

namespace sigil::compose {

using namespace detail;

// Volatility is declared per NODE, and a node gets one verdict. So a
// static full-canvas ground plane carrying one small child on a bound
// output is `subtreeVolatile`, nothing about it is cached, and the whole
// plane is re-rasterized every frame purely so the child can be redrawn
// on top of it. The node reports "its content changes every frame" when
// what changes is a child's.
//
// THE PIXEL-IDENTITY ARGUMENT, which is NOT promotion's argument.
// Promotion bakes a whole subtree and blits it in place of everything the
// node contains; the claim there is "an integer device translation cannot
// change rasterisation". Here the bake replaces only PART of what the
// node paints and the children are drawn over the blit afterwards, so the
// claim needed is:
//
//   painting the own layer into a transparent device-aligned surface,
//   blitting it, then painting the children over the result must produce
//   the same pixels as painting own-then-children directly.
//
//  - The own paint is a run of srcOver draws into a transparent layer,
//    blitted srcOver at an integer device offset. srcOver is associative,
//    so the composite is the same composite. (The 8-bit double-rounding
//    of the intermediate is the one real risk and it is asserted, not
//    argued — see SplitBakeIsPixelIdenticalAcrossTheChildsMotion, whose
//    own paint deliberately OVERLAPS itself so intra-layer compositing
//    actually happens.)
//  - A child with a non-srcOver blend is FINE here, and this is the one
//    place the split is safer than promotion: the blit lands BEFORE the
//    children, so the child resolves against the same destination bytes
//    either way. Under promotion the child was inside the bake and would
//    have resolved against transparent black. Same for a child with a
//    backdrop filter — it samples a blitted copy of identical pixels.
//  - The real failure is the node's OWN paint reading the backdrop, where
//    the bake would resolve against transparent black. That is
//    `ownReadsBackdrop`, which is why the flag was split from
//    `subtreeReadsBackdrop` before any of this existed.
//  - A LAYER EFFECT is the exclusion, and it is the only one of the three
//    wrappers that is. An image filter applies to the UNION of own paint
//    and children; filtering the own half alone and drawing the children
//    over the result is a different picture.
//  - clipContent and a whole-node mask() are NOT excluded, though it
//    looks as though they should be. They wrap both halves, and the phase
//    flag skips only the CONTENT — the clip is opened and closed inside
//    EACH phase, so both halves get the identical clip in the identical
//    device geometry and the composition is unchanged. A GRANULAR mask is
//    narrower still: its scope is entered and left around one paint
//    group, inside the half that group belongs to. Excluding clips would
//    refuse the most common shape this feature exists for, since a
//    backdrop that clips its moving child to an outline is exactly why
//    that child is a separate node.
//
// And the promotion is judged on the OWN paint alone. A split candidate
// paints in two phases from the first eligible frame precisely so that
// half can be timed by itself: judging it by the node's total would
// promote a cheap ground plane because it carries an expensive child, and
// would leave an expensive plane unpromoted under a cheap one.

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
      // `upright` for the same reason promotion needs it, and it is the
      // SAME construction: an integer device offset concatenated onto the
      // node's matrix. Under rotation a shader's local coordinates come
      // back through an inverse that cannot cancel that offset exactly, and
      // the antialiased edges land about a least-significant bit apart.
      // Leaving it out would hold the split to a weaker standard than the
      // promoter beside it.
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
    const size_t bytes = (size_t)std::max<int64_t>(area, 0) * 4;
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
          inst.ownBakeClip != pass.deviceClip()) {
        sk_sp<SkImage> baked = pass.takeDeviceBake(device, [&](SkCanvas& lc) {
          const BakeLayerScope bakeLayer(&impl);
          impl.paintContent(inst, lc, impl.hostScale, leafBlend, leafOpacity,
                            Phase::OwnOnly);
        });
        if (baked) {
          inst.ownImage = std::move(baked);
          inst.ownBakeRect = want;
          inst.ownBakeClip = pass.deviceClip();
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
      const double ownMs = ownWatch.elapsedMs();
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
    pass.close();
    return true;
  }

  return false;
}

}  // namespace sigil::compose
