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
#include <utility>

#include "PaintInternal.h"
#include "PaintPass.h"

namespace sigil::compose {

using namespace detail;

//
// The shape of the problem this exists for: MANY SMALL ROTATED PIECES
// FORMING ONE STATIC ASSEMBLY, each piece carrying a bound entrance that
// runs for a while and then holds. Nothing in that description is
// cacheable by the volatility rule, because the bindings never
// disconnect, and everything in it is cacheable for every frame the
// entrance is not running.
//
// WHY THE BAKE IS THE EASY HALF. This is the same construction the exact
// device bake and whole-subtree promotion already use: paintContent into a
// transparent layer whose canvas carries the node's exact matrix offset by
// an INTEGER device translation, then blit with the matrix reset. The
// children's rotations, their bevels and their mutual compositing all
// happen INSIDE that bake at full precision, which is what makes it
// pixel-safe where per-piece Cache::Texture is not: baking each piece
// separately isolates it into its own layer, so every shared edge and
// abutment resolves against transparent black instead of against its
// neighbour.
//
// WHY THE INVALIDATION IS THE HARD HALF, AND THE WHOLE FEATURE. A group
// may hold a bake only while it is provably not changing, and "not
// changing" cannot be read off the volatility verdict — that verdict says
// Volatile forever, correctly. So the group compares VALUES, the same way
// the per-node scalar memo does, generalised to a whole subtree's bound
// transforms and opacities. Every frame: gather them, compare with last
// frame's, and on any difference at all DROP THE BAKE and paint live. A
// bake taken while the entrance is running would freeze the entrance, and
// would look completely correct in any still frame.
//
// The refusals are in computeVolatile (`groupRootOK`), because they are
// about what the memo can SEE, not about this frame.

namespace {

/** The group's local bake, taken where the device bake is refused. True
 *  when the node was blitted and its paint is over. */
bool paintGroupLocalBake(PaintPass& pass) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  SkCanvas& canvas = pass.canvas;
  const SkRect bake = pass.localBounds();
  if (bake.isEmpty()) return false;
  const PaintPass::BakeRung rung = pass.localBakeRung(pass.totalM);
  const float scale = std::max(0.1f, rung.scale * pass.node.bakeScale);
  const int width = std::max(1, (int)std::ceil(bake.width() * scale));
  const int height = std::max(1, (int)std::ceil(bake.height() * scale));
  const int64_t area = (int64_t)width * height;
  const size_t bytes = (size_t)area * 4;
  const bool held = inst.textureImage && !inst.textureDeviceSpace &&
                    inst.textureScale == scale && inst.textureBakeRect == bake;
  if (!held) {
    // The same ceiling and the same promoted-pixel budget the device bake
    // is held to.
    if (area > int64_t{16} * 1024 * 1024 ||
        std::max(impl.promotedBytesLast, impl.promotedBytes) + bytes >
            Composer::Impl::kPromotedBudget)
      return false;
    sk_sp<SkSurface> layer =
        canvas.makeSurface(SkImageInfo::MakeN32Premul(width, height));
    if (!layer)
      layer = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
    if (!layer) return false;
    layer->getCanvas()->scale(scale, scale);
    layer->getCanvas()->translate(-bake.left(), -bake.top());
    pass.profDraw("group bake", [&] {
      const BakeLayerScope bakeLayer(&impl);
      impl.paintContent(inst, *layer->getCanvas(), scale);
    });
    inst.textureImage = layer->makeImageSnapshot();
    inst.textureScale = scale;
    inst.textureDeviceSpace = false;
    inst.textureEffectDeferred = false;
    inst.textureBakeRect = bake;
    inst.paintDirty = false;
    inst.picture.reset();
    impl.stats.picturesRecorded++;
    if (!impl.coverageTrace) impl.stats.texturesBaked++;
  }
  impl.promotedBytes += bytes;
  if (pass.profile.row != SIZE_MAX) {
    impl.profileRows[pass.profile.row].cacheState = Composer::CacheState::Group;
    impl.profileRows[pass.profile.row].promotion = Composer::Promotion::AskedFor;
  }
  // A recording holding this blit is remade when the host's scale leaves
  // the rung the bake was taken for.
  impl.narrowScaleWindow(rung.lowest, rung.highest);
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
  pass.close();
  return true;
}

}  // namespace

bool paintGroupBake(PaintPass& pass) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  SkCanvas& canvas = pass.canvas;
  const SkMatrix& totalM = pass.totalM;

  if (!impl.liveOnly && inst.groupRootOK) {
    // Gather, compare, and become last frame — in that order. The swap is
    // what makes a settled group allocate nothing: `groupScratch` comes back
    // holding the vector that was `groupPrev`, at the right capacity.
    impl.groupScratch.clear();
    collectGroupScalars(inst, /*root=*/true, impl.groupScratch);
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
      if (impl.recordingDepth == 0) {
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
    const size_t bytes = (size_t)std::max<int64_t>(area, 0) * 4;
    const bool affordable =
        inst.textureImage ||
        std::max(impl.promotedBytesLast, impl.promotedBytes) + bytes <=
            Composer::Impl::kPromotedBudget;
    if (settled && !inst.paintDirty && pass.deviceBakeable && !moving &&
        rectStable &&
        !totalM.hasPerspective() && device.width() > 0 && device.height() > 0 &&
        area <= int64_t{16} * 1024 * 1024 && affordable) {
      const SkRect want = SkRect::Make(device);
      if (!inst.textureImage || !inst.textureDeviceSpace ||
          inst.textureEffectDeferred || inst.textureBakeRect != want ||
          inst.textureBakeClip != pass.deviceClip() ||
          inst.textureBakeMatrix != pass.totalM) {
        // No leaf blend and no leaf opacity: bakes isolate, and the node's
        // own blend/opacity are applied by the saveLayer wrapping the blit
        // — which is why leafDirectBlend excludes Cache::Group.
        sk_sp<SkImage> baked = pass.takeDeviceBake(device, [&](SkCanvas& lc) {
          const BakeLayerScope bakeLayer(&impl);
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
        pass.close();
        return true;
      }
    }
    // THE LOCAL BAKE: the subtree rasterized in the node's own space at the
    // rung the matrix asks for, and blitted through the canvas as it stands.
    // What it holds does not depend on where it lands, so one bake serves
    // every position the motion carries it through, and it is retaken only
    // when the group's content ticks, its paint bounds change or the scale
    // leaves its rung.
    if (settled && !inst.paintDirty && paintGroupLocalBake(pass)) return true;
    // Falls through: `cacheHolds` is false for a volatile group root, so the
    // picture tier cannot take it either and the node paints LIVE.
    // That is the intended outcome on a ticking frame — the same paint the
    // scene did before this feature existed.
  }

  return false;
}

}  // namespace sigil::compose
