/** @file
 * The bake an author asked for with Cache::Texture, and the one a deferred
 * layer effect asks for: the exact device-space bake where the node is
 * holding still, the quantized local bake where it is not, the static
 * effect lifted off the content raster, and the blit over either.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <utility>

#include "PaintInternal.h"
#include "PaintPass.h"

namespace sigil::compose {

using namespace detail;

bool paintTextureBake(PaintPass& pass) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  const ElementNode& node = pass.node;
  SkCanvas& canvas = pass.canvas;
  const SkRect& rect = pass.rect;
  const SkMatrix& totalM = pass.totalM;
  const float opacity = pass.opacity;
  const bool memoStale = pass.memoStale;
  const bool deferBlendToBlit = pass.deferBlendToBlit;
  const SkBlendMode leafBlend = pass.leafBlend;
  const float leafOpacity = pass.leafOpacity;
  const bool deferLiveEffect = pass.deferLiveEffect;
  bool& deferEffect = pass.deferEffect;
  sk_sp<SkImageFilter>& deferredFilter = pass.deferredFilter;
  Instance::ContentScalars& scalarsNow = pass.scalarsNow;
  using Phase = Composer::Impl::Phase;

  // ---- the exact bake -------------------------------------------------
  // A bake held in LOCAL space and blitted through the node's transform
  // is resampled by whatever that transform is: at a quarter turn the
  // texel grid lands half a texel off the device grid and every sample
  // interpolates two texels, which softens edges and flattens gradients
  // across the axis whose device edge falls on a half pixel. Baking at
  // the correct scale is necessary for sharpness but not sufficient.
  //
  // Baking in DEVICE space, snapped OUT to whole device pixels and
  // blitted with the matrix reset, has nothing left to resample: the
  // texel grid IS the device grid, at any angle. Three conditions gate
  // it, each about not throwing away what the local bake is FOR:
  //
  //  - bakeScale must be 1. Its whole purpose is to rasterize BELOW
  //    device resolution and let the blit stretch it back.
  //  - the device-bake rule: at the root, or inside recordings
  //    all pinned to the matrix they were made under, never inside one
  //    that replays under a declared motion. Inside a pinned recording
  //    the device grid is the recording's space composed out through
  //    its replay (`totalM` here), the blit concatenates the replay's
  //    inverse so the replay lands it back at the device rect, and the
  //    recording is remade the frame that matrix differs.
  //  - the node must be HOLDING STILL, by both available measures, which
  //    are not the same measure:
  //      * `transformLive` — its own transform is declared as animating.
  //        A spinning ornament must keep the local bake and ride it,
  //        even on a frame where it happens to land on the same rect.
  //      * the device rect it lands on has not moved since last frame.
  //        A node with no animated property of its own still moves under
  //        a resizing window, a pinch zoom, a pan, or an uncached
  //        ancestor's live transform — none of which any per-node
  //        DECLARATION can see, and all of which would re-bake a
  //        device-pinned texture every frame. At the root the node is
  //        painted every frame and keeps that history itself; inside a
  //        recording it is painted only when the recording is, so the
  //        outermost recording's own matrix history answers instead.
  //    While either says "moving", the quantized local bake is correct
  //    and cheap: one bake per coarse scale step, reused across the rest.
  //    A node refused for the matrix alone inside a recording marks the
  //    recording DEFERRED, and it is retaken once the matrix holds still
  //    so the node takes the exact bake then rather than after its next
  //    content change.
  const SkRect localBounds = pass.localBounds();
  bool deviceRectStable = false;
  SkIRect deviceR = SkIRect::MakeEmpty();
  if (impl.unpinnedRecordingDepth == 0) {
    deviceR = pass.deviceRect();
    if (impl.recordingDepth == 0) {
      deviceRectStable = !inst.deviceRectSeen || deviceR == inst.lastDeviceRect;
      inst.lastDeviceRect = deviceR;
      inst.deviceRectSeen = true;
    } else {
      deviceRectStable = pass.matrixStable;
    }
  }
  const int64_t deviceArea = (int64_t)deviceR.width() * deviceR.height();
  // A DEFERRED EFFECT keeps the LOCAL bake. A device-space bake blits with
  // the matrix reset, and an image filter's parameters are read in the
  // space of the canvas that applies it: a sigma declared in the node's
  // own units would become a sigma in device units, so the effect would
  // change size with the host's scale. The local bake blits through the
  // node's own matrix, which is the matrix the effect's saveLayer stood
  // under, so the filter is applied in exactly the space it was declared
  // in.
  //  - and no declared bake density. A device-space bake IS a bake at
  //    the view's own scale, pinned to the view's own grid; a host that
  //    has said what density its rasters are taken at has said this
  //    path is not what it wants, and the local bake below is the one
  //    that honours it.
  const bool deviceEligible =
      !deferEffect && !inst.transformLive && impl.unpinnedRecordingDepth == 0 &&
      node.bakeScale >= 1.0f && !totalM.hasPerspective() &&
      impl.bakeDensity <= 0 && deviceR.width() > 0 && deviceR.height() > 0 &&
      deviceArea <= int64_t{16} * 1024 * 1024;
  if (deviceEligible && !deviceRectStable && impl.recordingDepth > 0)
    impl.recordingDeviceDeferred = true;
  if (deviceEligible && deviceRectStable) {
    const SkRect bakeRect = SkRect::Make(deviceR);
    if (!inst.textureImage || inst.paintDirty || !inst.textureDeviceSpace ||
        memoStale || inst.textureBakeRect != bakeRect ||
        inst.textureBakeClip != pass.deviceClip()) {
      sk_sp<SkImage> baked;
      pass.profDraw("bake", [&] {
        baked = pass.takeDeviceBake(deviceR, [&](SkCanvas& lc) {
          const BakeLayerScope bakeLayer(&impl);
          impl.paintContent(inst, lc,
                            impl.hostScale);  // no leaf blend: bakes isolate
        });
      });
      if (baked) {
        inst.textureImage = std::move(baked);
        inst.textureInk = {};
        inst.textureDeviceSpace = true;
        inst.textureEffectDeferred = false;
        inst.textureBakeRect = bakeRect;
        inst.textureBakeClip = pass.deviceClip();
        inst.textureScale = maxScaleOf(totalM, localBounds);
        inst.bakedLiveShader = inst.hasPendingLiveFill
                                   ? inst.pendingLiveFill.shaderValue
                                   : nullptr;
        inst.bakedScalars = scalarsNow;
        inst.paintDirty = false;
        impl.stats.picturesRecorded++;
        if (!impl.coverageTrace) impl.stats.texturesBaked++;
      }
    }
    if (inst.textureImage && inst.textureDeviceSpace) {
      if (pass.profile.row != SIZE_MAX) {
        impl.profileRows[pass.profile.row].cacheState =
            Composer::CacheState::Texture;
        impl.profileRows[pass.profile.row].promotion =
            Composer::Promotion::AskedFor;
      }
      // Identity CTM is global canvas space even inside a saveLayer (the
      // layer device carries its own origin), so an opacity/blend bake
      // still composites through the layer above.
      pass.profDraw("blit", [&] {
        if (deferBlendToBlit) {
          // The node's blend and opacity on the ONE draw it composites
          // as — cheaper and slightly MORE exact than the layer it
          // replaces: no full-canvas intermediate, one less rounding.
          SkPaint blit;
          blit.setAlphaf(opacity);
          blit.setBlendMode(node.paint.blendMode);
          pass.deviceBlit(inst.textureImage, deviceR, &blit);
        } else {
          pass.deviceBlit(inst.textureImage, deviceR, nullptr);
        }
      });
      pass.close();
      return true;
    }
  }
  // WHAT RESOLUTION A BAKE IS TAKEN AT, and there are two answers.
  //
  // A DECLARED DENSITY is a picture of the canvas: the host has said how
  // many device pixels a layout unit is worth, the bake is taken at that
  // and at nothing else, and the blit carries it through whatever the
  // view does afterwards — sharp at the density it was taken for,
  // magnified past it, exactly as an image node's pixels are. Nothing
  // about the frame's matrix reaches the decision, so a reader zooming
  // walks no ladder and waits on no re-rasterization; only a change of
  // what the picture IS re-takes it.
  //
  // NO DECLARED DENSITY is a picture of the view: rasterize at the
  // canvas's current scale so a zoomed host stays crisp — quantized UP
  // to a coarse step, so a continuously changing scale reuses one bake
  // per step instead of re-rasterizing every frame. Between steps the
  // draw minifies slightly, which stays sharp. The DEVICE matrix
  // (composed out through any recording), so a bake taken inside a
  // replayed-at-scale recording is rasterized at the scale it will be
  // shown at.
  const SkMatrix& total = totalM;
  // A SCALE MOTION THAT NAMES ITS DESTINATION IS BAKED AT THE
  // DESTINATION, ONCE — a ladder question, so a declared density skips
  // it: that bake is not at a scale the motion can move. The ladder
  // quantizes so that a scale nobody
  // declared — a window resize, a pinch zoom — reuses one bake per step
  // instead of re-rasterizing per frame. An entrance is the opposite
  // case: it is not an unknown scale drifting, it is a known scale being
  // travelled, and quantizing it bakes the node again at every rung it
  // passes. A `from(a).to(b)` on a scale lane names b, so the bake is
  // taken there and the blit MINIFIES through the entrance, which is the
  // sharp direction. A scale driven by a binding names nothing and keeps
  // the ladder.
  //
  // The substitution is on the node's OWN lanes, rebuilt against the
  // matrix its parent supplied — not a factor applied to the current
  // reading, which is zero at the start of an entrance from nothing.
  // Only the flat placement is rebuilt this way: a plane that has turned
  // or stands in a shared space is placed by a 4x4 whose own producer
  // owns that composition.
  SkMatrix destTotal = total;
  if (impl.bakeDensity <= 0 && !pass.placedByPlane && !pass.spaceHost) {
    NodeTransform destTf = *pass.tf;
    bool declared = false;
    const auto lane = [&](Instance::Slot slot,
                          const motion::Animatable<float>& v, float& out) {
      const AnimatedFloat* a = inst.anims[slot].get();
      if (v.binding() || !a || !a->started || !a->value.isConnected()) return;
      out = a->target;
      declared = true;
    };
    lane(Instance::kScale, node.paint.scale, destTf.scl);
    lane(Instance::kScaleX, node.paint.scaleX, destTf.sx);
    lane(Instance::kScaleY, node.paint.scaleY, destTf.sy);
    if (declared) {
      destTotal =
          impl.recordingDepth == 0
              ? pass.parentCanvasM
              : SkMatrix::Concat(impl.recordingReplay, pass.parentCanvasM);
      destTotal.preTranslate(rect.left(), rect.top());
      destTotal.preConcat(
          destTf.matrix({0, 0}, node.paint, rect.width(), rect.height()));
    }
  }
  // maxScaleOf, NOT the matrix diagonal: a quarter-turned node's diagonal
  // is (0, 0) and would clamp to the 0.25 floor, baking at a quarter
  // resolution to be upscaled by the blit (see maxScaleOf in
  // ComposeRuntime.h). The node's local bounds locate the Jacobian
  // samples when the CTM carries a host perspective. This ladder feeds
  // the re-bake test below, so an underestimate here means a stale,
  // blurry bake rather than a wasted one.
  static constexpr float kBakeSteps[] = {0.25f, 0.5f, 0.75f, 1.0f,
                                         1.5f,  2.0f, 3.0f,  4.0f};
  float scale = impl.bakeDensity;
  if (impl.bakeDensity <= 0) {
    const float raw =
        std::clamp(maxScaleOf(destTotal, localBounds), 0.25f, 4.0f);
    scale = kBakeSteps[std::size(kBakeSteps) - 1];
    for (float step : kBakeSteps)
      if (step >= raw) {
        scale = step;
        break;
      }
  }
  // bakeScale(): opt-in reduced raster scale — the bake evaluates fewer
  // pixels and the blit below linear-upscales through the same dst rect.
  scale = std::max(0.1f, scale * node.bakeScale);
  // THE STATIC EFFECT IS LIFTED OFF THE CONTENT HERE. The bake is taken
  // in two steps instead of one: the content rasterizes into a surface
  // with the effect left out, and the effect is then run over that image
  // into the surface the node holds. What it replaces is the layer the
  // filter opened INSIDE the content raster — allocated over the node's
  // whole band plus the filter's reach, cleared, drawn into and
  // composited back — and that layer is most of what an effect over a
  // large node costs, which is why a small sigma paid nearly what a
  // large one did.
  //
  // NOT ON THE BLIT, which is where a MOVING effect goes. A filter hung
  // on the blit's paint is evaluated per draw, and Skia answers it from
  // its own cache only while the mapping that draw stands under holds
  // still — so a node that TURNS, which is the whole population that
  // keeps a local bake rather than a device one, would pay the entire
  // filter on every frame in exchange for paying it once per bake.
  const bool deferStaticEffect = pass.staticEffectCandidate;
  if (deferStaticEffect) {
    deferredFilter = pass.resolveLayerFilter();
    deferEffect = (bool)deferredFilter;
  }
  // Bake the full PAINT bounds, not just the box — decoration bleed and
  // overflowing children truncate otherwise (same rule as the picture
  // cull).
  const SkRect bake = localBounds;
  const int pw = std::max(1, (int)std::ceil(bake.width() * scale));
  const int ph = std::max(1, (int)std::ceil(bake.height() * scale));
  // THE SAME CEILING EVERY BAKE TIER TAKES: a surface past it is a
  // hundreds-of-megabytes allocation to hold one node, and the node is
  // painted live instead. A surface the device refused is not a bake
  // either — the node paints live rather than drawing through nothing.
  const int64_t area = (int64_t)pw * ph;
  if (area <= int64_t{16} * 1024 * 1024 &&
      (!inst.textureImage || inst.paintDirty || inst.textureScale != scale ||
       inst.textureDeviceSpace || memoStale ||
       inst.textureEffectDeferred != deferLiveEffect ||
       inst.textureBakeRect != bake)) {
    sk_sp<SkSurface> layer =
        canvas.makeSurface(SkImageInfo::MakeN32Premul(pw, ph));
    if (!layer) layer = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(pw, ph));
    if (layer) {
      layer->getCanvas()->scale(scale, scale);
      layer->getCanvas()->translate(-bake.left(), -bake.top());
      pass.profDraw("bake", [&] {  // no leaf blend: bakes isolate
        const BakeLayerScope bakeLayer(&impl);
        impl.paintContent(inst, *layer->getCanvas(), scale,
                          SkBlendMode::kSrcOver, 1.0f, Phase::All, deferEffect);
      });
      // THE STATIC EFFECT, RUN OVER THE CONTENT BAKE. One image draw
      // into a second surface of the same rect, under the same matrix
      // the effect's layer stood under, so the filter reads its
      // parameters in the units they were declared in and its output is
      // cut where the layer's output was cut — by the surface's own
      // edge. The image lands texel for texel: the dst rect is the
      // texels the content surface actually holds, in local units, so
      // nothing resamples on the way through.
      if (deferStaticEffect) {
        const sk_sp<SkImage> content = layer->makeImageSnapshot();
        sk_sp<SkSurface> filtered =
            canvas.makeSurface(SkImageInfo::MakeN32Premul(pw, ph));
        if (!filtered)
          filtered = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(pw, ph));
        if (content && filtered) {
          SkCanvas* fc = filtered->getCanvas();
          fc->scale(scale, scale);
          fc->translate(-bake.left(), -bake.top());
          SkPaint fp;
          fp.setImageFilter(deferredFilter);
          pass.profDraw("bake effect", [&] {
            fc->drawImageRect(
                content,
                SkRect::MakeXYWH(bake.left(), bake.top(), (float)pw / scale,
                                 (float)ph / scale),
                SkSamplingOptions(), &fp);
          });
          layer = std::move(filtered);
        }
      }
      // The ink grid, off the surface's own pixels — before the
      // snapshot, so nothing is copied for it. A GPU surface answers no
      // pixmap and the grid stays empty, which is a whole-rect blit.
      SkPixmap baked;
      inst.textureInk =
          layer->peekPixels(&baked) ? inkGridOf(baked) : InkGrid{};
      inst.textureImage = layer->makeImageSnapshot();
      inst.textureScale = scale;
      inst.textureDeviceSpace = false;
      inst.textureEffectDeferred = deferLiveEffect;
      inst.textureBakeRect = bake;
      inst.bakedLiveShader =
          inst.hasPendingLiveFill ? inst.pendingLiveFill.shaderValue : nullptr;
      inst.bakedScalars = std::move(scalarsNow);
      inst.paintDirty = false;
      impl.stats.picturesRecorded++;
      if (!impl.coverageTrace) impl.stats.texturesBaked++;
    }
  }
  if (pass.profile.row != SIZE_MAX) {
    impl.profileRows[pass.profile.row].cacheState =
        inst.textureImage ? Composer::CacheState::Texture
                          : Composer::CacheState::Live;
    impl.profileRows[pass.profile.row].promotion =
        Composer::Promotion::AskedFor;
    impl.profileRows[pass.profile.row].effectDeferred =
        deferEffect && inst.textureImage;
  }
  if (!inst.textureImage) {
    // Nothing to blit: the surface was refused, or the bake would be
    // past the ceiling. The node paints itself — and takes back the
    // blend and opacity the blit was to have carried, since there is no
    // blit to carry them.
    if (deferBlendToBlit) {
      SkPaint layerPaint;
      layerPaint.setAlphaf(opacity);
      layerPaint.setBlendMode(node.paint.blendMode);
      const SkRect content = impl.recordBounds(inst);
      canvas.saveLayer(&content, &layerPaint);
    }
    pass.profDraw("live", [&] {
      impl.paintContent(inst, canvas, impl.hostScale, leafBlend, leafOpacity);
    });
    if (deferBlendToBlit) canvas.restore();
    pass.close();
    return true;
  }
  // Blit through the rect the bake ACTUALLY covers, not `bake`: pw/ph were
  // rounded UP, so stretching an image of ceil(w·s) texels across w local
  // units resamples the whole node by up to one texel's worth of scale.
  // The overshoot is transparent padding, so nothing new becomes visible.
  const SkRect dst =
      SkRect::MakeXYWH(bake.left(), bake.top(),
                       (float)inst.textureImage->width() / inst.textureScale,
                       (float)inst.textureImage->height() / inst.textureScale);
  pass.profDraw("blit", [&] {
    SkPaint blit;
    bool dressed = false;
    if (deferBlendToBlit) {  // same rule as the device blit above
      blit.setAlphaf(opacity);
      blit.setBlendMode(node.paint.blendMode);
      dressed = true;
    }
    // The deferred layer effect, applied to the bake rather than to the
    // content: the canvas stands at the node's own matrix here, which is
    // the matrix the effect's saveLayer stood under, so the filter reads
    // its parameters in the units they were declared in. Skia grows the
    // draw for the filter's own reach, so nothing the effect spreads
    // outside the bake rect is lost.
    //
    // THE BLEED RULE, which both deferred tiers ask of the bake: the
    // filter reads the bake's own margin — the transparent band a node's
    // paint bounds carry around its ink — and it reads NOTHING else,
    // because outside the bake there are no pixels. So a node wearing a
    // deferred effect must carry the effect's reach as that margin, which
    // is what a declared bleed is for; a reach the paint bounds do not
    // hold is spread from a cut edge.
    if (deferLiveEffect) {
      blit.setImageFilter(deferredFilter);
      dressed = true;
    }
    // AN EFFECT ON THE BLIT IS NOT ADMITTED BY THE INK. The filter
    // spreads the content OUTSIDE the pixels that carry it — that is
    // what a glow is — and the grid describes where the ink is, not
    // where the filter will put it. Blitted whole. A STATIC effect is
    // already in the pixels the grid was taken from, so it keeps its
    // grid.
    //
    // AND THE INK CLIP IS A DEVICE-SPACE CLIP, so it obeys the device
    // bake's rule rather than the picture tier's. A region names whole
    // pixels of the device and ignores the matrix — which is what makes
    // it a set of pixels rather than an outline — so one recorded into a
    // picture is applied, unchanged, in the space that picture is
    // replayed into. It is therefore computed through the replay, and a
    // recording holding one is pinned to the matrix it was made under
    // exactly as one holding a device blit is. An UNPINNED recording —
    // one under a declared motion, which replays under a matrix nobody
    // knows yet — can hold no such clip, and the bake is blitted whole
    // inside it.
    const bool inkAdmitted = !deferLiveEffect && !inst.textureInk.empty() &&
                             impl.unpinnedRecordingDepth == 0;
    drawInkedImage(canvas, inst.textureImage,
                   inkAdmitted ? inst.textureInk : InkGrid{}, dst, totalM,
                   pass.deviceClip(), SkSamplingOptions(SkFilterMode::kLinear),
                   dressed ? &blit : nullptr);
    if (inkAdmitted && impl.recordingDepth > 0) ++impl.recordingDeviceBakes;
  });
  return false;
}

}  // namespace sigil::compose
