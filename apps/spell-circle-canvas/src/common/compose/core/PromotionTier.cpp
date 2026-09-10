/** @file
 * Automatic texture promotion: whether a node the author never asked to
 * cache may be baked and blitted instead of painted, the refusals that say
 * why not, the warmup that decides it is worth it, and the device-space
 * bake it admits.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <cstdint>

#include "PaintInternal.h"
#include "PaintPass.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** Temporal promotion. A node whose only volatility is a live material may
 *  hold a bake while the material is provably holding still, and re-bakes
 *  when it ticks — which is only a win if it ticks slower than the frame
 *  rate. A bake costs about what the replay it replaces costs, so the
 *  break-even stable fraction is around a half: promote at 0.5, keep until
 *  0.3. A material bound to a continuous output sits at 0 and never gets
 *  close; one quantized to a step slower than the frame rate sits well
 *  above the promote bar. */
constexpr float kStablePromote = 0.5f;
constexpr float kStableKeep = 0.3f;

}  // namespace

void PaintPass::accrue(double cost) {
  // Nothing warms under a trace: the node is painted twice in the frame
  // it is traced, and a promotion clock that counted both would run at
  // double rate for a reason the viewer never sees.
  if (impl.coverageTrace) return;
  // EMA so one scheduling hiccup neither promotes nor un-promotes.
  inst.replayMs = inst.replayMs * 0.6f + (float)cost * 0.4f;
  if (promotable && inst.replayMs > kPromoteMs) {
    if (inst.hotFrames < 255) ++inst.hotFrames;
    if (inst.hotFrames >= kPromoteFrames) {
      inst.autoTexture = true;
      inst.paintDirty = true;  // force the first bake
    } else {
      note(Composer::Promotion::Warming);
    }
  } else if (inst.hotFrames > 0) {
    --inst.hotFrames;
  }
}

// ---------------------------------------------------------------------------
// Eligibility is deliberately narrow. Everything here is a condition under
// which a device-aligned bake is provably the same pixels as the replay;
// anything else keeps replaying. See Composer::setAutoTexturePromotion.

void decidePromotion(PaintPass& pass) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  const ElementNode& node = pass.node;
  const SkRect& rect = pass.rect;
  const SkBlendMode leafBlend = pass.leafBlend;
  const float leafOpacity = pass.leafOpacity;

  const bool temporallyStable =
      inst.liveMatOnly &&
      inst.liveStableRate >= (inst.autoTexture ? kStableKeep : kStablePromote);
  const bool contentStable = !inst.subtreeVolatile || temporallyStable;

  // Every refusal below is a condition under which a bake would produce
  // DIFFERENT PIXELS, or would not pay for itself. Naming them is not
  // decoration: a node reported as expensive live paint with no reason
  // beside it gives an author nothing to act on.
  //
  // A BIT MASK, so ALL refusals are reported, not the first. A first-match
  // chain would report only `Volatile` for a node that is both volatile and
  // clipped, and an author who fixed the volatility would then meet a
  // second refusal nobody had mentioned. `why` below is derived FROM this
  // mask rather than computed alongside it, so the summary and the full set
  // cannot disagree.
  using Prom = Composer::Promotion;
  uint16_t refusals = 0;
  const auto flag = [&](Prom p) { refusals |= (uint16_t)(1u << (unsigned)p); };
  // autoPromoteEffective, not autoPromote: the backend-aware default (off
  // on GPU unless the host asked) is applied in draw(). See
  // ComposeRuntime.h.
  const bool optedOut =
      impl.autoPromoteEffective == Composer::PromotionPolicy::Off ||
      node.cacheMode != Cache::Auto;
  // EAGER SKIPS THE STOPWATCH AND NOTHING ELSE. Every refusal below is a
  // condition under which a bake would paint different pixels, and this
  // policy changes none of them — it answers only "is this node expensive
  // enough to be worth baking" with yes, so a run that means to exercise
  // the promoter exercises every node the rules admit rather than the few
  // the machine happened to be slow on.
  const bool eager =
      impl.autoPromoteEffective == Composer::PromotionPolicy::Eager;
  if (optedOut) flag(Prom::OptedOut);
  if (!contentStable) flag(Prom::Volatile);
  if (leafBlend != SkBlendMode::kSrcOver || leafOpacity < 1.0f)
    flag(Prom::Composited);
  if (layerEffectOf(node) || node.clipContent) flag(Prom::Filtered);
  if (inst.subtreeReadsBackdrop)  // incl. this node's own backdrop()
    flag(Prom::ReadsBackdrop);
  if (rect.width() < 0.5f || rect.height() < 0.5f)
    flag(Prom::TooBig);  // degenerate, not large — same "cannot bake" bucket
  if (!pass.upright) flag(Prom::Transformed);
  if (pass.spaceHost) flag(Prom::HostsSpace);

  // The PRIMARY verdict: the first refusal in the order an author should
  // address them (their own switches first, then content, then geometry).
  static constexpr Prom kRefusalOrder[] = {
      Prom::OptedOut,      Prom::HostsSpace,  Prom::Volatile,
      Prom::Composited,    Prom::Transformed, Prom::Filtered,
      Prom::ReadsBackdrop, Prom::TooBig};
  Prom why = Prom::Cheap;
  for (Prom p : kRefusalOrder)
    if (refusals & (uint16_t)(1u << (unsigned)p)) {
      why = p;
      break;
    }

  // THE DEVICE-BAKE RULE, which the Cache::Group, Cache::Texture and split
  // tiers share: a device-space bake blits with the matrix reset at
  // an ABSOLUTE device rect, so it is exact under one matrix and wrong
  // under every other. It may be taken at the root, or inside recordings
  // that are all PINNED — made under a matrix stamped on the instance and
  // remade the frame it differs — and never inside an unpinned one, which
  // replays under a declared motion and would be remade every frame.
  // Inside a pinned recording the node is painted only when the recording
  // is, so the matrix must also be holding still by the recording's own
  // history: a bake taken under a moving matrix would pin a recording that
  // is then remade, and the bake with it, on every frame of the motion.
  // …AND NEVER INSIDE ANOTHER BAKE. The bake above already holds this
  // node: its image is what the blit lands, and a bake of a node inside it
  // is consulted only on the frames that one is remade — which is the
  // measured cost being near zero, so the cost rule would never ask for it
  // and only the eager policy ever does. What it costs is a composite: the
  // node's own coverage into its own image, that image into the layer
  // above, and the layer above onto the canvas, where the contract allows
  // ONE more composite than the live paint made.
  const bool deviceBakeable = impl.unpinnedRecordingDepth == 0 &&
                              impl.bakeDepth == 0 &&
                              (impl.recordingDepth == 0 || pass.matrixStable);
  const bool promotable =
      why == Prom::Cheap && !impl.liveOnly && deviceBakeable;
  if (!promotable)
    inst.autoTexture = false;
  else if (eager)
    inst.autoTexture = true;  // no warmup: the bake is taken below, this frame
  pass.refusals = refusals;
  pass.optedOut = optedOut;
  pass.eager = eager;
  pass.deviceBakeable = deviceBakeable;
  pass.promotable = promotable;
  pass.note(why);
}

bool paintPromotedBake(PaintPass& pass) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  SkCanvas& canvas = pass.canvas;
  const SkMatrix& totalM = pass.totalM;
  const SkBlendMode leafBlend = pass.leafBlend;
  const float leafOpacity = pass.leafOpacity;
  const bool memoStale = pass.memoStale;
  const Instance::ContentScalars& scalarsNow = pass.scalarsNow;
  using Prom = Composer::Promotion;

  if (!pass.promotable || !inst.autoTexture) return false;
  // Bake in DEVICE space, snapped OUT to whole device pixels, then blit
  // with the matrix reset. An integer device translation cannot change
  // rasterisation for an AXIS-ALIGNED matrix — which is what `upright`
  // guards, and why that guard is about exactness rather than about
  // resampling.
  const SkIRect device = pass.deviceRect();
  const int64_t area = (int64_t)device.width() * device.height();
  const size_t bytes = (size_t)std::max<int64_t>(area, 0) * 4;
  // The bake this frame would ADD to what the previous frame already held.
  // A node keeping a bake it already has is never refused for budget —
  // dropping it would only make the next frame re-bake it.
  // max(), not a sum: impl.promotedBytesLast is the previous frame's FULL
  // total and impl.promotedBytes is this frame so far, and the two overlap.
  const bool affordable =
      inst.textureImage ||
      std::max(impl.promotedBytesLast, impl.promotedBytes) + bytes <=
          Composer::Impl::kPromotedBudget;
  if (device.width() > 0 && device.height() > 0 &&
      area <= int64_t{16} * 1024 * 1024 && affordable) {
    // Re-bake when the recording is stale, when the device rect moved or
    // resized (which is how a transform-SCALE change arrives here), when
    // the clip the bake was cut to moved, or — the temporal case — when
    // the live material has actually ticked and the baked shader is no
    // longer the one this frame resolves to.
    if (!inst.textureImage || inst.paintDirty || memoStale ||
        inst.textureEffectDeferred ||
        inst.textureBakeRect != SkRect::Make(device) ||
        inst.textureBakeClip != pass.deviceClip()) {
      sk_sp<SkImage> baked;
      pass.profDraw("promote bake", [&] {
        baked = pass.takeDeviceBake(device, [&](SkCanvas& lc) {
          const BakeLayerScope bakeLayer(&impl);
          impl.paintContent(inst, lc, impl.hostScale, leafBlend, leafOpacity);
        });
      });
      if (baked) {
        inst.textureImage = std::move(baked);
        inst.textureInk = {};
        inst.textureDeviceSpace = true;
        inst.textureEffectDeferred = false;
        inst.textureBakeRect = SkRect::Make(device);
        inst.textureBakeClip = pass.deviceClip();
        inst.bakedLiveShader = inst.hasPendingLiveFill
                                   ? inst.pendingLiveFill.shaderValue
                                   : nullptr;
        inst.bakedScalars = scalarsNow;
        inst.paintDirty = false;
        // AND THE RECORDING GOES. A bake is taken from the description as
        // it stands, and the node stops recording from the frame it is
        // taken — so a recording made before it holds the content of some
        // earlier frame with nothing left to say so: the scalars that
        // separated them are this bake's now, and a settled node is not
        // dirty. The bake is refused again the moment the matrix under it
        // moves (a plate photographed at another scale, a host resized),
        // and a recording kept here is what would replay then. Re-recording
        // costs one paint; replaying that one is a picture of another
        // frame.
        inst.picture.reset();
        impl.stats.picturesRecorded++;
        if (!impl.coverageTrace) impl.stats.texturesBaked++;
      }
    }
    if (inst.textureImage) {
      impl.promotedBytes += bytes;
      if (pass.profile.row != SIZE_MAX)
        impl.profileRows[pass.profile.row].cacheState =
            Composer::CacheState::Promoted;
      pass.note(Prom::Promoted);
      pass.deviceBlit(inst.textureImage, device, nullptr);
      pass.close();
      return true;
    }
  }
  inst.autoTexture = false;  // could not bake — fall through to the picture
  pass.note(Prom::TooBig);
  return false;
}

}  // namespace sigil::compose
