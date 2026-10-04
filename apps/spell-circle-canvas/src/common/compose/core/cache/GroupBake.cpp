/** @file
 * Cache::Group — the whole subtree, held by a VALUE memo: a bake taken
 * while every bound transform and opacity under the node is provably
 * holding still, and dropped the frame any of them ticks.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

#include "paint/PaintInternal.h"
#include "paint/PaintPass.h"

namespace sigil::compose {

using namespace detail;

// A group can settle while its bindings remain declared volatile. Its
// memo compares all supported bound transforms and opacities each frame;
// changing inputs drop the bake and resume live paint. groupRootOK excludes
// inputs that this memo cannot observe.
//
// Baking the whole assembly preserves compositing between its children.
// The device path uses the live canvas's matrix, origin and clip; a local
// bake is used when the group must follow motion or a declared density.

namespace {

struct LocalGroupBake {
  SkRect bounds;
  PaintPass::BakeRung rung;
  float scale;
  SkImageInfo info;
  size_t bytes;
  /** Whether the bake the node holds is this plan's: local, at this scale
   *  and this rect, and taken at the host's scale wherever its content
   *  read one. */
  bool held;
};

std::optional<LocalGroupBake> localGroupBakePlan(PaintPass& pass) {
  const SkRect bounds = pass.localBounds();
  const PaintPass::BakeRung rung = pass.localBakeRung(pass.totalM);
  const float scale = std::max(0.1f, rung.scale * pass.node.bakeScale);
  const SkISize dimensions = PaintPass::localBakeSize(bounds, scale);
  if (dimensions.isEmpty()) return std::nullopt;
  const SkImageInfo info = pass.impl.bakeInfo(dimensions);
  const size_t bytes = (size_t)dimensions.width() *
                       (size_t)dimensions.height() *
                       (size_t)info.bytesPerPixel();
  const Instance& inst = pass.inst;
  const bool held = inst.textureImage && !inst.textureDeviceSpace &&
                    inst.textureScale == scale &&
                    (inst.textureContentScale == 0.0f ||
                     inst.textureContentScale == pass.impl.hostScale) &&
                    inst.textureBakeRect == bounds;
  if (!held &&
      std::max(pass.impl.promotedBytesLast, pass.impl.promotedBytes) + bytes >
          Composer::Impl::kPromotedBudget)
    return std::nullopt;
  return LocalGroupBake{bounds, rung, scale, info, bytes, held};
}

/** The group's local bake, taken where the device bake is refused. True
 *  when the node was blitted and its paint is over. */
bool paintGroupLocalBake(PaintPass& pass, const LocalGroupBake& plan) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  SkCanvas& canvas = pass.canvas;
  const SkRect& bake = plan.bounds;
  const float scale = plan.scale;
  if (!plan.held) {
    inst.textureImage.reset();
    sk_sp<SkSurface> layer = canvas.makeSurface(plan.info);
    if (!layer) layer = SkSurfaces::Raster(plan.info);
    if (!layer) return false;
    layer->getCanvas()->scale(scale, scale);
    layer->getCanvas()->translate(-bake.left(), -bake.top());
    const bool outerRead = std::exchange(impl.contentScaleRead, false);
    pass.profDraw("group bake", [&] {
      const BakeLayerScope bakeLayer(&impl, *layer->getCanvas());
      impl.paintContent(inst, *layer->getCanvas(), scale);
    });
    const float readScale = impl.contentScaleRead ? impl.hostScale : 0.0f;
    impl.contentScaleRead = impl.contentScaleRead || outerRead;
    inst.textureImage = layer->makeImageSnapshot();
    if (!inst.textureImage) return false;
    inst.textureScale = scale;
    inst.textureContentScale = readScale;
    inst.textureDeviceSpace = false;
    inst.textureEffectDeferred = false;
    inst.textureBakeRect = bake;
    inst.paintDirty = false;
    inst.picture.reset();
    impl.stats.picturesRecorded++;
    if (!impl.coverageTrace) impl.stats.texturesBaked++;
  }
  impl.promotedBytes += plan.bytes;
  if (pass.profile.row != SIZE_MAX) {
    impl.profileRows[pass.profile.row].cacheState = Composer::CacheState::Group;
    impl.profileRows[pass.profile.row].promotion =
        Composer::Promotion::AskedFor;
  }
  // A recording holding this blit is remade when the host's scale leaves
  // the rung the bake was taken for.
  impl.recording.narrowScaleWindow(plan.rung.lowest, plan.rung.highest);
  // …and to the one scale its content read, where a paint in it did.
  if (inst.textureContentScale != 0.0f)
    impl.recording.narrowScaleWindow(inst.textureContentScale,
                                     inst.textureContentScale);
  // Blitted through the rect the bake actually covers: the texel counts
  // were rounded up, and stretching them over the unrounded rect would
  // resample the whole group by up to a texel.
  const SkRect covered = SkRect::MakeXYWH(
      bake.left(), bake.top(), (float)inst.textureImage->width() / scale,
      (float)inst.textureImage->height() / scale);
  pass.profDraw("group blit", [&] {
    canvas.drawImageRect(inst.textureImage, covered,
                         SkSamplingOptions(SkFilterMode::kLinear), nullptr);
  });
  return true;
}

}  // namespace

bool paintGroupBake(PaintPass& pass) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  SkCanvas& canvas = pass.canvas;
  const SkMatrix& totalM = pass.totalM;

  if (impl.bakesPixels() && inst.groupRootOK) {
    // Gather, compare, and become last frame — in that order. The swap is
    // what makes a settled group allocate nothing: `groupScratch` comes back
    // holding the vector that was `groupPrev`, at the right capacity.
    impl.groupScratch.clear();
    collectGroupScalars(inst, /*root=*/true, impl.groupScratch);
    const bool firstObservation = !inst.groupPrevSeen;
    const bool settled =
        inst.groupPrevSeen && impl.groupScratch == inst.groupPrev;
    std::swap(inst.groupPrev, impl.groupScratch);
    inst.groupPrevSeen = true;

    // The device rect, and the two "is it holding still" questions the
    // device path asks for its own reasons — they are the same questions
    // here, and the same answer follows them. `transformLive` is the node's
    // own declared motion and `transformLiveAbove` an ancestor's: a group
    // carried by a motion of a few pixels over many frames lands on the
    // same device rect on most of them at another sub-pixel position, so
    // the rect alone would answer "still" and the bake, exact at one
    // position only, would be taken again every frame. The rect comparison
    // catches the motions no declaration can see (a resizing host, a pinch
    // zoom). A bake pinned to a rect that moves is a bake remade every
    // frame, which costs strictly more than the paint it replaces — so
    // wherever either answers "moving", or the device-bake rule refuses,
    // the group takes the LOCAL bake below and rides the matrix above it.
    // THE RECT ITSELF IS NARROWED TO THE CANVAS HERE, on top of the clip
    // every bake layer carries: a lattice of rotated pieces with any bleed
    // overruns its own canvas on all four sides, and this tier exists for
    // exactly that content, so the pixels outside are worth not allocating.
    SkIRect device = SkIRect::MakeEmpty();
    bool rectStable = false;
    if (pass.deviceBakeable) {
      device = pass.deviceRect();
      if (!device.intersect(pass.deviceClip())) device = SkIRect::MakeEmpty();
      // The rect history is this node's own only where it is painted every
      // frame; inside a recording the recording's matrix verdict stands in.
      rectStable = pass.matrixStable;
      if (impl.recording.depth == 0) {
        rectStable = !inst.deviceRectSeen || device == inst.lastDeviceRect;
        inst.lastDeviceRect = device;
        inst.deviceRectSeen = true;
      }
    }
    const bool moving = inst.transformLive || inst.transformLiveAbove;

    // THE DROP. Not "re-bake": a group whose bindings are ticking is
    // ticking for a while, and re-baking each of those frames would pay the
    // bake on top of the paint. Hold the pixels only while they are right.
    if (!settled || inst.paintDirty) inst.textureImage.reset();

    const int64_t area = (int64_t)device.width() * device.height();
    const size_t bytes = (size_t)std::max<int64_t>(area, 0) *
                         (size_t)impl.bakeInfo({1, 1}).bytesPerPixel();
    const bool affordable =
        inst.textureImage ||
        std::max(impl.promotedBytesLast, impl.promotedBytes) + bytes <=
            Composer::Impl::kPromotedBudget;
    const bool deviceEligible =
        pass.deviceBakeable && !moving && rectStable && impl.bakeDensity <= 0 &&
        !totalM.hasPerspective() && device.width() > 0 && device.height() > 0 &&
        area <= int64_t{16} * 1024 * 1024 && affordable;
    if (settled && !inst.paintDirty && deviceEligible) {
      const SkRect want = SkRect::Make(device);
      if (!inst.textureImage || !inst.textureDeviceSpace ||
          inst.textureEffectDeferred || inst.textureBakeRect != want ||
          inst.textureBakeClip != pass.deviceClip() ||
          inst.textureBakeMatrix != pass.totalM) {
        inst.textureImage.reset();
        // No leaf blend and no leaf opacity: bakes isolate, and the node's
        // own blend/opacity are applied by the saveLayer wrapping the blit
        // — which is why leafDirectBlend excludes Cache::Group.
        sk_sp<SkImage> baked = pass.takeDeviceBake(device, [&](SkCanvas& lc) {
          const BakeLayerScope bakeLayer(&impl, lc);
          impl.paintContent(inst, lc, impl.hostScale);
        });
        if (baked) {
          inst.textureImage = std::move(baked);
          inst.textureDeviceSpace = true;
          inst.textureEffectDeferred = false;
          inst.textureBakeRect = want;
          inst.textureBakeClip = pass.deviceClip();
          inst.textureBakeMatrix = pass.totalM;
          inst.textureScale = maxScaleOf(totalM, pass.localBounds());
          inst.paintDirty = false;
          // A group root never replays a recording. It can have made one on
          // its very first frame — before it had a previous frame to compare
          // with, a group with a fully static subtree falls through to the
          // picture branch once — and holding it after that is bytes nobody
          // will ever read.
          inst.picture.reset();
          impl.stats.picturesRecorded++;
          if (!impl.coverageTrace) impl.stats.texturesBaked++;
        }
      }
      if (inst.textureImage) {
        impl.promotedBytes += bytes;
        if (pass.profile.row != SIZE_MAX) {
          impl.profileRows[pass.profile.row].cacheState =
              Composer::CacheState::Group;
          impl.profileRows[pass.profile.row].promotion =
              Composer::Promotion::AskedFor;
        }
        pass.profDraw("group blit", [&] {
          pass.deviceBlit(inst.textureImage, device, nullptr);
        });
        return true;
      }
    }
    // THE LOCAL BAKE: the subtree rasterized in the node's own space at the
    // rung the matrix asks for, and blitted through the canvas as it stands.
    // What it holds does not depend on where it lands, so one bake serves
    // every position the motion carries it through, and it is retaken only
    // when the group's content ticks, its paint bounds change or the scale
    // leaves its rung.
    if (!settled && !firstObservation && !inst.paintDirty) return false;
    const std::optional<LocalGroupBake> localPlan = localGroupBakePlan(pass);
    if (settled && !inst.paintDirty) {
      if (localPlan && paintGroupLocalBake(pass, *localPlan)) return true;
      inst.textureImage.reset();
    } else if ((firstObservation || inst.paintDirty) &&
               (deviceEligible || localPlan)) {
      // A containing picture must visit again after this paint clears the
      // dirty state. Refused rasters and changing observations do not retry.
      impl.recording.groupPending = true;
    }
    // Falls through: `cacheHolds` is false for a volatile group root, so the
    // picture tier cannot take it either and the node paints LIVE.
    // That is the intended outcome on a ticking frame — the same paint the
    // scene did before this feature existed.
  }

  return false;
}

}  // namespace sigil::compose
