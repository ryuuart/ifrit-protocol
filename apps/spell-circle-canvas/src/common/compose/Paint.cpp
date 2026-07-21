// Paint phase: the volatility walk that decides what may cache, the node
// silhouette resolution, and the stacking painter with its three cache tiers
// (live paint, auto SkPicture of provably-static subtrees, and Cache::Texture
// raster bakes). See DESIGN.md "Stacking and compositing" and "Caching".

#include "ComposeRuntime.h"

#include <sigilimage/ImageAsset.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <cmath>

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Volatility & caching

bool Composer::Impl::computeVolatile(Instance &inst) {
  const ElementNode &node = *inst.desc;

  auto boundOrRunning = [&](Instance::Slot slot, const PropValue<float> &v) {
    if (std::holds_alternative<const choreograph::Output<float> *>(v))
      return true;
    return inst.anims[slot] && inst.anims[slot]->value.isConnected();
  };
  // Paint-only volatility: transforms and opacity apply OUTSIDE the node's
  // content (in paint()'s matrix/layer stack), so a node animated only here
  // still replays its content picture — a spinning ornament re-records
  // nothing. Ancestors still can't cache across it (their recording would
  // freeze the motion), hence the return value.
  bool ownPaint = false;
  ownPaint |= boundOrRunning(Instance::kOpacity, node.paint.opacity);
  ownPaint |= boundOrRunning(Instance::kTx, node.paint.translateX);
  ownPaint |= boundOrRunning(Instance::kTy, node.paint.translateY);
  ownPaint |= boundOrRunning(Instance::kRotate, node.paint.rotate);
  ownPaint |= boundOrRunning(Instance::kScale, node.paint.scale);

  // Content volatility: what actually invalidates the node's own recording
  // (bound/lerping fills, per-frame programs, animated decorations and image
  // frames).
  bool ownContent = inst.anims[Instance::kFillLerp] &&
                    inst.anims[Instance::kFillLerp]->value.isConnected();
  if (node.paint.fill &&
      std::holds_alternative<const choreograph::Output<Fill> *>(
          *node.paint.fill))
    ownContent = true;
  if (node.liveMaterial) // a bound-uniform material re-resolves every frame
    ownContent = true;
  if (node.cacheMode == Cache::None)
    ownContent = true;
  for (const Decoration &d : node.backgrounds)
    ownContent |= d.animated();
  for (const Decoration &d : node.foregrounds)
    ownContent |= d.animated();
  if (node.kind == Kind::Image && node.imageAsset && node.imageAsset->animated())
    ownContent = true;

  bool childrenVolatile = false;
  for (auto &child : inst.children)
    childrenVolatile |= computeVolatile(*child);

  // subtreeVolatile gates the node's own caches: blocked by content volatility
  // here or ANY volatility below (children paint inside the recording,
  // transforms included) — but not by own paint volatility.
  const bool blocked = ownContent || childrenVolatile;
  if (blocked != inst.subtreeVolatile) {
    inst.subtreeVolatile = blocked;
    inst.paintDirty = true; // cacheability changed → re-record/drop
  }
  if (inst.subtreeVolatile) {
    inst.picture.reset();
    inst.textureImage.reset();
  }
  return ownPaint || blocked;
}

// ---------------------------------------------------------------------------
// Silhouette

const SkPath &Composer::Impl::resolveOutline(Instance &inst, SkSize size) const {
  if (inst.outlineCacheDesc != inst.desc.get() ||
      inst.outlineCacheSize != size) {
    inst.outlineCache = inst.desc->shapeFn(size);
    inst.outlineCacheDesc = inst.desc.get();
    inst.outlineCacheSize = size;
  }
  return inst.outlineCache;
}

// ---------------------------------------------------------------------------
// The stacking painter

void Composer::Impl::paintContent(Instance &inst, SkCanvas &canvas,
                                  float contentScale, SkBlendMode leafBlend,
                                  float leafOpacity) {
  const ElementNode &node = *inst.desc;
  const SkRect bounds = SkRect::MakeWH(YGNodeLayoutGetWidth(inst.yoga),
                                       YGNodeLayoutGetHeight(inst.yoga));
  const SkRRect rrect = cornersRRect(bounds, node.corners);

  // The node's shape: routed connector path, custom outline(), or the
  // corner-rounded box.
  const bool customShape = node.shapeFn && node.connectFrom.empty();
  SkPath outlinePath;
  if (!node.connectFrom.empty()) {
    outlinePath = inst.connectorPath; // derive phase routed it
  } else if (customShape) {
    outlinePath = resolveOutline(inst, {bounds.width(), bounds.height()});
  } else {
    SkPathBuilder outlineBuilder;
    outlineBuilder.addRRect(rrect);
    outlinePath = outlineBuilder.detach();
  }

  if (node.clipContent) {
    if (customShape)
      canvas.clipPath(outlinePath, true);
    else
      canvas.clipRRect(rrect, true);
  }

  // The node's own layer effect wraps everything painted here, so it is
  // captured by picture recordings and BAKED by texture snapshots.
  const bool hasEffect = node.layerEffect && node.layerEffect->imageFilter();
  if (hasEffect) {
    SkPaint effectPaint;
    effectPaint.setImageFilter(node.layerEffect->imageFilter());
    canvas.saveLayer(nullptr, &effectPaint);
  }
  const PaintContext paintCtx{{bounds.width(), bounds.height()},
                              std::move(outlinePath),
                              elapsed(),
                              contentScale,
                              ticker.active(),
                              &fonts};

  // Background decorations paint beneath the fill (the CSS box-shadow
  // ordering): shadow/pattern layers first, then the surface.
  for (const Decoration &decoration : node.backgrounds)
    decoration.paint(canvas, paintCtx);

  // Fill (background): a live material resolves per frame from its bound
  // uniforms + the PaintContext; otherwise the stored Fill (binding, lerp, or
  // plain).
  std::optional<Fill> resolvedFill;
  if (node.liveMaterial) {
    resolvedFill = node.liveMaterial->resolve(paintCtx);
  } else if (node.paint.fill) {
    Fill fill;
    if (const auto *binding =
            std::get_if<const choreograph::Output<Fill> *>(&*node.paint.fill))
      fill = (*binding)->value();
    else if (inst.anims[Instance::kFillLerp] &&
             inst.anims[Instance::kFillLerp]->started &&
             inst.anims[Instance::kFillLerp]->value.isConnected()) {
      const float t = inst.anims[Instance::kFillLerp]->value.value();
      fill = inst.fillTo;
      for (int i = 0; i < 4; ++i)
        fill.colorValue.vec()[i] =
            inst.fillFrom.colorValue.vec()[i] +
            (inst.fillTo.colorValue.vec()[i] -
             inst.fillFrom.colorValue.vec()[i]) * t;
      fill.kind = Fill::Kind::Color;
    } else {
      ResolvedProp<Fill> resolved =
          resolveProp(*node.paint.fill, node.nodeTransition);
      fill = resolved.target;
    }
    resolvedFill = fill;
  }

  if (resolvedFill && resolvedFill->kind != Fill::Kind::None) {
    const Fill &fill = *resolvedFill;
    SkPaint paint;
    paint.setAntiAlias(true);
    if (fill.kind == Fill::Kind::Color)
      paint.setColor4f(fill.colorValue, nullptr);
    else
      paint.setShader(fill.shaderValue);
    // Leaf fast path: paint() proved a layer is unnecessary and routed the
    // node's blend/opacity straight onto the fill.
    paint.setBlendMode(leafBlend);
    if (leafOpacity < 1.0f)
      paint.setAlphaf(paint.getAlphaf() * leafOpacity);
    if (customShape)
      canvas.drawPath(paintCtx.outline, paint);
    else
      canvas.drawRRect(rrect, paint);
  }

  // Content
  switch (node.kind) {
  case Kind::Text:
    if (inst.paragraph) {
      // Yoga skips the measure callback when both dimensions are fully
      // determined (absolute + all four insets); lay out on demand at the
      // resolved width so such text still paints.
      if (inst.measuredRev != inst.contentRev)
        layoutText(inst, bounds.width());
      inst.textLayout.drawBatched(&canvas, *inst.paragraph);
    }
    break;
  case Kind::Image:
    if (node.imageAsset && !node.imageAsset->frames().empty()) {
      const auto &frame = node.imageAsset->frameAt(elapsed() * 1000.0);
      if (frame.image) {
        if (node.imageRegion)
          canvas.drawImageRect(frame.image, *node.imageRegion, bounds,
                               SkSamplingOptions(SkFilterMode::kLinear), nullptr,
                               SkCanvas::kStrict_SrcRectConstraint);
        else
          canvas.drawImageRect(frame.image, bounds,
                               SkSamplingOptions(SkFilterMode::kLinear));
      }
    }
    break;
  case Kind::Custom:
    if (node.program)
      node.program(canvas, paintCtx);
    break;
  case Kind::Box:
  case Kind::Stack:
  case Kind::Slot:
    break;
  }

  // Children in stacking order (each clean static child replays its own nested
  // picture — ancestor re-records don't repaint clean subtrees).
  for (size_t index : inst.paintOrder)
    paint(*inst.children[index], canvas);

  for (const Decoration &decoration : node.foregrounds)
    decoration.paint(canvas, paintCtx);

  if (hasEffect)
    canvas.restore();
}

void Composer::Impl::paint(Instance &inst, SkCanvas &canvas) {
  const ElementNode &node = *inst.desc;
  const SkRect rect = instanceRect(inst);

  const float opacity = std::clamp(
      inst.resolveFloat(Instance::kOpacity, node.paint.opacity), 0.0f, 1.0f);
  if (opacity <= 0.0f)
    return;

  canvas.save();
  canvas.translate(rect.left(), rect.top());

  const float tx = inst.resolveFloat(Instance::kTx, node.paint.translateX);
  const float ty = inst.resolveFloat(Instance::kTy, node.paint.translateY);
  const float rot = inst.resolveFloat(Instance::kRotate, node.paint.rotate);
  const float scl = inst.resolveFloat(Instance::kScale, node.paint.scale);
  if (tx != 0 || ty != 0)
    canvas.translate(tx, ty);
  if (rot != 0 || scl != 1) {
    const SkPoint origin = {rect.width() * node.paint.originX,
                            rect.height() * node.paint.originY};
    canvas.translate(origin.x(), origin.y());
    if (rot != 0)
      canvas.rotate(rot);
    if (scl != 1)
      canvas.scale(scl, scl);
    canvas.translate(-origin.x(), -origin.y());
  }

  const bool hasBackdrop =
      node.backdropEffect && node.backdropEffect->imageFilter();
  if (hasBackdrop) {
    canvas.save();
    if (node.shapeFn)
      canvas.clipPath(resolveOutline(inst, {rect.width(), rect.height()}), true);
    else
      canvas.clipRRect(cornersRRect(SkRect::MakeWH(rect.width(), rect.height()),
                                    node.corners),
                       true);
    SkCanvas::SaveLayerRec rec(nullptr, nullptr,
                               node.backdropEffect->imageFilter().get(), 0);
    canvas.saveLayer(rec);
  }

  // Fill-only leaves route blend/opacity straight onto the fill paint instead
  // of a (device-clip-sized!) saveLayer — a field of plus-blended shapes costs
  // path draws, not full-canvas layers. Excluded: live opacity (must stay
  // outside any cached recording) and texture bakes (blending must hit the
  // real destination, not the bake's transparent surface).
  const bool opacityLive =
      std::holds_alternative<const choreograph::Output<float> *>(
          node.paint.opacity) ||
      (inst.anims[Instance::kOpacity] &&
       inst.anims[Instance::kOpacity]->value.isConnected());
  const bool leafDirectBlend =
      (node.kind == Kind::Box || node.kind == Kind::Stack) &&
      inst.children.empty() && node.backgrounds.empty() &&
      node.foregrounds.empty() && !node.layerEffect && !node.backdropEffect &&
      !node.clipContent && !opacityLive && node.cacheMode != Cache::Texture;
  const bool needsLayer =
      (opacity < 1.0f || node.paint.blendMode != SkBlendMode::kSrcOver) &&
      !leafDirectBlend;
  if (needsLayer) {
    SkPaint layerPaint;
    layerPaint.setAlphaf(opacity);
    layerPaint.setBlendMode(node.paint.blendMode);
    canvas.saveLayer(nullptr, &layerPaint);
  }
  const SkBlendMode leafBlend =
      leafDirectBlend ? node.paint.blendMode : SkBlendMode::kSrcOver;
  const float leafOpacity = leafDirectBlend ? opacity : 1.0f;

  // Automatic caching at topmost provably-static subtrees: pictures by
  // default, a rasterized image under Cache::Texture (the raster-target pixel
  // win — replaying a picture re-rasterizes, blitting doesn't).
  if (!liveOnly && !inst.subtreeVolatile &&
      node.cacheMode == Cache::Texture && !node.backdropEffect) {
    // Rasterize at the canvas's current scale so zoomed hosts stay crisp — but
    // quantized UP to a coarse step, so a continuously changing scale (window
    // resize, pinch zoom) reuses one bake per step instead of re-rasterizing
    // every frame. Between steps the draw minifies slightly, which stays sharp.
    SkMatrix total = canvas.getTotalMatrix();
    const float raw = std::clamp(
        std::max(std::abs(total.getScaleX()), std::abs(total.getScaleY())),
        0.25f, 4.0f);
    static constexpr float kBakeSteps[] = {0.25f, 0.5f, 0.75f, 1.0f,
                                           1.5f, 2.0f, 3.0f, 4.0f};
    float scale = kBakeSteps[std::size(kBakeSteps) - 1];
    for (float step : kBakeSteps)
      if (step >= raw) { scale = step; break; }
    if (!inst.textureImage || inst.paintDirty || inst.textureScale != scale) {
      const int pw = std::max(1, (int)std::ceil(rect.width() * scale));
      const int ph = std::max(1, (int)std::ceil(rect.height() * scale));
      sk_sp<SkSurface> layer =
          canvas.makeSurface(SkImageInfo::MakeN32Premul(pw, ph));
      if (!layer)
        layer = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(pw, ph));
      layer->getCanvas()->scale(scale, scale);
      paintContent(inst, *layer->getCanvas(), scale); // no leaf blend:
      inst.textureImage = layer->makeImageSnapshot(); // bakes isolate
      inst.textureScale = scale;
      inst.paintDirty = false;
      stats.picturesRecorded++;
    }
    canvas.drawImageRect(inst.textureImage,
                         SkRect::MakeWH(rect.width(), rect.height()),
                         SkSamplingOptions(SkFilterMode::kLinear));
  } else if (!liveOnly && !inst.subtreeVolatile &&
             node.cacheMode != Cache::None &&
             (node.cacheMode == Cache::Picture || !inst.children.empty() ||
              node.kind == Kind::Text || node.kind == Kind::Custom ||
              !node.backgrounds.empty() || !node.foregrounds.empty() ||
              node.layerEffect)) {
    // (Childless Image leaves deliberately absent: one drawImageRect is
    // cheaper than a nested picture indirection — tile maps stay flat inside
    // their chunk's recording. Cache::Picture opts back in.)
    if (!inst.picture || inst.paintDirty) {
      SkPictureRecorder recorder;
      SkCanvas *rec =
          recorder.beginRecording(SkRect::MakeWH(rect.width(), rect.height()));
      paintContent(inst, *rec, hostScale, leafBlend, leafOpacity);
      inst.picture = recorder.finishRecordingAsPicture();
      inst.paintDirty = false;
      stats.picturesRecorded++;
    }
    canvas.drawPicture(inst.picture);
  } else {
    stats.nodesPainted++;
    paintContent(inst, canvas, hostScale, leafBlend, leafOpacity);
    inst.paintDirty = false;
  }

  if (needsLayer)
    canvas.restore();
  if (hasBackdrop) {
    canvas.restore(); // backdrop layer
    canvas.restore(); // clip
  }
  canvas.restore();
}

} // namespace sigil::compose
