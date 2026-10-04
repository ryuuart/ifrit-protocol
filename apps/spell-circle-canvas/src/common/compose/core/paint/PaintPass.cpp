#include "PaintPass.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkSurface.h>
#include <src/core/SkScopeExit.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

#include "PaintInternal.h"

namespace sigil::compose {

using namespace detail;

SkIRect PaintPass::deviceClip() const {
  const SkIRect clip = canvas.getDeviceClipBounds();
  if (impl.recording.depth == 0) return clip;
  return impl.recording.replay.mapRect(SkRect::Make(clip)).roundOut();
}

SkIRect PaintPass::bakeRect(const SkRect& local) const {
  const SkRect f = totalM.mapRect(local);
  if (!f.isFinite() || f.isEmpty()) return SkIRect::MakeEmpty();
  const double left = std::floor((double)f.left());
  const double top = std::floor((double)f.top());
  const double right = std::ceil((double)f.right());
  const double bottom = std::ceil((double)f.bottom());
  const double span = std::max(right - left, bottom - top);
  const double maxCoordinate = std::numeric_limits<int>::max();
  const int m = Composer::Impl::bakeMargin((int)std::min(span, maxCoordinate));
  // Keep the paint and clip margins on the canvas's grid; negative
  // coordinates have no scratch pixels to read. Crop before narrowing:
  // finite transformed edges and their integer spans can still overflow.
  const SkIRect clip = deviceClip();
  if (clip.isEmpty()) return SkIRect::MakeEmpty();
  const double croppedLeft = std::max({0.0, left - m, (double)clip.left() - m});
  const double croppedTop = std::max({0.0, top - m, (double)clip.top() - m});
  const double croppedRight =
      std::min({maxCoordinate, right + m, (double)clip.right() + m});
  const double croppedBottom =
      std::min({maxCoordinate, bottom + m, (double)clip.bottom() + m});
  if (croppedRight <= croppedLeft || croppedBottom <= croppedTop)
    return SkIRect::MakeEmpty();
  return SkIRect::MakeLTRB((int)croppedLeft, (int)croppedTop, (int)croppedRight,
                           (int)croppedBottom);
}

SkIRect PaintPass::deviceRect() {
  // The paint bounds, which already hold the shape the node declares and
  // the reach the effects under it filter over: a device bake is a
  // SURFACE, and a surface allocated to less than the ink cuts it.
  return bakeRect(localBounds());
}

SkCanvas& Composer::Impl::paintDestination(SkCanvas& canvas) const {
  return recording.depth > 0 && recording.canvas ? *recording.canvas : canvas;
}

SkSurface* Composer::Impl::bakeSurface(SkCanvas& canvas, SkISize need) {
  if (need.width() <= 0 || need.height() <= 0) return nullptr;
  if (bakeSurfaces.size() <= bakeDepth) bakeSurfaces.resize(bakeDepth + 1);
  sk_sp<SkSurface>& held = bakeSurfaces[bakeDepth];
  // A picture canvas records commands but cannot allocate the destination's
  // GPU surface. Allocate against the canvas those commands will reach.
  SkCanvas& destination = paintDestination(canvas);
  if (held && (held->recorder() != destination.recorder() ||
               held->recordingContext() != destination.recordingContext()))
    held.reset();
  // GROWN, never shrunk: a host that has drawn one large frame keeps the
  // surface that frame needed, and the next bake of any size is a clip
  // inside it rather than an allocation.
  if (!held || held->width() < need.width() || held->height() < need.height()) {
    const SkImageInfo info =
        bakeInfo({std::max(need.width(), held ? held->width() : 0),
                  std::max(need.height(), held ? held->height() : 0)});
    // The destination's own kind of surface where it can make one, so a
    // device bake under a GPU canvas stays on the device.
    held = destination.makeSurface(info);
    if (!held && !destination.recorder() && !destination.recordingContext())
      held = SkSurfaces::Raster(info);
  }
  return held.get();
}

sk_sp<SkImage> PaintPass::takeDeviceBake(
    const SkIRect& device, const std::function<void(SkCanvas&)>& content) {
  SkSurface* scratch =
      impl.bakeSurface(canvas, {device.right(), device.bottom()});
  if (!scratch) return nullptr;
  SkCanvas* lc = scratch->getCanvas();
  const SkAutoCanvasRestore restore(lc, true);
  lc->resetMatrix();
  // CLEARED OVER THE WHOLE RECT, because the surface is reused and the rect
  // is what the image is taken from: what the clip below leaves unpainted
  // still ends up in the image, and it has to be the transparent black a
  // fresh surface would have offered.
  lc->clipIRect(device);
  lc->clear(SK_ColorTRANSPARENT);
  lc->clipIRect(deviceClip());
  lc->setMatrix(totalM);  // the node's own matrix, on the canvas's own grid
  // A bake taken INSIDE this one gets a surface of its own: this one is
  // still holding the paint that reached it.
  ++impl.bakeDepth;
  const SkScopeExit leaveBake([&] { --impl.bakeDepth; });
  content(*lc);
  return scratch->makeImageSnapshot(device);
}

void Composer::Impl::countBlit(const sk_sp<SkImage>& image, const SkIRect& at) {
  if (!image || compositePlane.width <= 0) return;
  SkBitmap read;
  if (!read.tryAllocPixels(
          SkImageInfo::MakeN32Premul(image->width(), image->height())))
    return;
  if (!image->readPixels(nullptr, read.pixmap(), 0, 0)) return;
  const SkIRect on =
      SkIRect::MakeWH(compositePlane.width, compositePlane.height);
  for (int y = std::max(at.top(), on.top());
       y < std::min(at.bottom(), on.bottom()); ++y) {
    const SkColor* row = (const SkColor*)read.getAddr32(0, y - at.top());
    uint8_t* out =
        compositePlane.counts.data() + (size_t)y * (size_t)compositePlane.width;
    for (int x = std::max(at.left(), on.left());
         x < std::min(at.right(), on.right()); ++x) {
      // A BLIT THAT CONTRIBUTES NOTHING ROUNDS NOTHING: transparent black
      // leaves the destination as it stood, to the bit.
      if (SkColorGetA(row[x - at.left()]) == 0) continue;
      if (out[x] < 255) ++out[x];
    }
  }
}

void PaintPass::deviceBlit(const sk_sp<SkImage>& image, const SkIRect& at,
                           const SkPaint* paint) {
  canvas.save();
  canvas.resetMatrix();
  if (impl.recording.depth > 0) canvas.concat(impl.recording.replayInverse);
  canvas.drawImage(image, (float)at.left(), (float)at.top(),
                   SkSamplingOptions(), paint);
  canvas.restore();
  if (impl.recording.depth > 0) ++impl.recording.deviceBakes;
  if (impl.countComposites && !impl.paintingUnseen) impl.countBlit(image, at);
}

PaintPass::BakeRung PaintPass::localBakeRung(const SkMatrix& total) {
  BakeRung rung{.scale = impl.bakeDensity,
                .lowest = 0.0f,
                .highest = std::numeric_limits<float>::infinity()};
  if (impl.bakeDensity > 0) return rung;
  // maxScaleOf, NOT the matrix diagonal: a quarter-turned node's diagonal
  // is (0, 0) and would clamp to the floor, baking at a quarter resolution
  // to be upscaled by the blit. The paint bounds locate the Jacobian
  // samples when the matrix carries a host perspective. An underestimate
  // here means a stale, blurry bake rather than a wasted one.
  static constexpr float kBakeSteps[] = {0.25f, 0.5f, 0.75f, 1.0f,
                                         1.5f,  2.0f, 3.0f,  4.0f};
  const float measured = maxScaleOf(total, localBounds());
  const float raw = std::clamp(measured, 0.25f, 4.0f);
  rung.scale = kBakeSteps[std::size(kBakeSteps) - 1];
  float below = 0.0f;
  for (float step : kBakeSteps)
    if (step >= raw) {
      rung.scale = step;
      break;
    } else {
      below = step;
    }
  // The rung spans (below, scale]; carried onto the host's scale by the
  // ratio between the two, which is the node's own transform and holds
  // while the host's scale is the only thing moving. The clamps are the
  // ends of the ladder and reach past it: a node already rasterizing at
  // the floor or the ceiling answers the same rung however much further
  // the host goes that way.
  if (measured > 0.0f) {
    const float perHost = impl.hostScale / measured;
    rung.lowest = raw <= kBakeSteps[0] ? 0.0f : below * perHost;
    rung.highest = raw >= kBakeSteps[std::size(kBakeSteps) - 1]
                       ? std::numeric_limits<float>::infinity()
                       : rung.scale * perHost;
  }
  return rung;
}

SkISize PaintPass::localBakeSize(const SkRect& bounds, float scale) {
  if (bounds.isEmpty() || !bounds.isFinite() || !std::isfinite(scale) ||
      scale <= 0.0f)
    return SkISize::MakeEmpty();
  const double width = std::ceil((double)bounds.width() * scale);
  const double height = std::ceil((double)bounds.height() * scale);
  constexpr double maxPixels = 16.0 * 1024 * 1024;
  if (width < 1.0 || height < 1.0 || width > maxPixels || height > maxPixels ||
      width * height > maxPixels)
    return SkISize::MakeEmpty();
  return SkISize::Make((int)width, (int)height);
}

sk_sp<SkImageFilter> PaintPass::resolveLayerFilter() {
  const PaintContext effectCtx{
      .size = {rect.width(), rect.height()},
      .elapsedSeconds = impl.elapsed(),
      .contentScale = impl.hostScale,
      .contentScaleRead = &impl.contentScaleRead,
      .destination = impl.outputInfo,
      .recorder = impl.paintDestination(canvas).recorder(),
      .animating = impl.engine.isRunning(),
      .fonts = &impl.fonts,
      .stamps = &inst.stampCache,
      .toRoot = geometry::path::fromSk(impl.curToRoot),
      .rootSize = geometry::path::fromSk(impl.rootLayoutSize)};
  const material::FrameData effectFrame = frameOf(effectCtx);
  return material::skia::resolvedImageFilter(*layerEffectOf(node),
                                             &effectFrame);
}
void PaintPass::note(Composer::Promotion p) {
  if (profile.row != SIZE_MAX) {
    impl.profileRows[profile.row].promotion = p;
    impl.profileRows[profile.row].refusals = refusals;
  }
}

}  // namespace sigil::compose
