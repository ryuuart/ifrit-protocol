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
#include <include/core/SkSurface.h>

#include <algorithm>
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

bool paintGroupBake(PaintPass& pass) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  SkCanvas& canvas = pass.canvas;
  const SkMatrix& totalM = pass.totalM;

  if (!impl.liveOnly && inst.groupRootOK && pass.deviceBakeable) {
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
    // device path asks for its own reasons — they are the same
    // questions here. `transformLive` is the node's own declared motion; the
    // rect comparison catches the motions no declaration can see (a resizing
    // host, a pinch zoom, an uncached ancestor's live transform). A bake
    // pinned to a rect that moves is a bake remade every frame, which costs
    // strictly more than the paint it replaces.
    // THE RECT ITSELF IS NARROWED TO THE CANVAS HERE, on top of the clip
    // every bake layer carries: a lattice of rotated pieces with any bleed
    // overruns its own canvas on all four sides, and this tier exists for
    // exactly that content, so the pixels outside are worth not allocating.
    SkIRect device = pass.deviceRect();
    if (!device.intersect(pass.deviceClip())) device = SkIRect::MakeEmpty();
    // The rect history is this node's own only where it is painted every
    // frame; inside a recording the recording's matrix verdict stands in.
    bool rectStable = pass.matrixStable;
    if (impl.recordingDepth == 0) {
      rectStable = !inst.deviceRectSeen || device == inst.lastDeviceRect;
      inst.lastDeviceRect = device;
      inst.deviceRectSeen = true;
    }

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
    if (settled && !inst.paintDirty && !inst.transformLive && rectStable &&
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
    // Falls through: `cacheHolds` is false for a volatile group root, so the
    // picture tier cannot take it either and the node paints LIVE.
    // That is the intended outcome on a ticking frame — the same paint the
    // scene did before this feature existed.
  }

  return false;
}

}  // namespace sigil::compose
