#include "PaintPass.h"

#include <include/core/SkPath.h>

#include <algorithm>
#include <cmath>

#include "PaintInternal.h"

namespace sigil::compose {

using namespace detail;

SkIRect PaintPass::deviceClip() const {
  const SkIRect clip = canvas.getDeviceClipBounds();
  if (impl.recordingDepth == 0) return clip;
  return impl.recordingReplay.mapRect(SkRect::Make(clip)).roundOut();
}

SkIRect PaintPass::deviceRect() {
  // The paint bounds, joined with the shape the node declares: a device
  // bake is a SURFACE, and a surface allocated to less than the ink cuts
  // it. The recording bounds leave the declared shape out because they
  // size layers, whose bounds are part of the picture.
  SkRect bounds = localBounds();
  bounds.join(impl.declaredShapeBounds(inst));
  const SkRect f = totalM.mapRect(bounds);
  SkIRect r =
      SkIRect::MakeLTRB((int)std::floor(f.left()), (int)std::floor(f.top()),
                        (int)std::ceil(f.right()), (int)std::ceil(f.bottom()));
  const int m = Composer::Impl::bakeMargin(std::max(r.width(), r.height()));
  r.outset(m, m);
  return r;
}

void PaintPass::clipBakeLayer(SkCanvas* lc, const SkIRect& bake) const {
  lc->clipIRect(deviceClip().makeOffset(-bake.left(), -bake.top()));
}

void PaintPass::deviceBlit(const sk_sp<SkImage>& image, const SkIRect& at,
                           const SkPaint* paint) {
  canvas.save();
  canvas.resetMatrix();
  if (impl.recordingDepth > 0) canvas.concat(impl.recordingReplayInverse);
  canvas.drawImage(image, (float)at.left(), (float)at.top(),
                   SkSamplingOptions(), paint);
  canvas.restore();
  if (impl.recordingDepth > 0) ++impl.recordingDeviceBakes;
}

sk_sp<SkImageFilter> PaintPass::resolveLayerFilter() {
  const PaintContext effectCtx{{rect.width(), rect.height()},
                               SkPath(),
                               impl.elapsed(),
                               impl.hostScale,
                               impl.ticker.active(),
                               &impl.fonts,
                               nullptr,
                               &inst.stampCache,
                               impl.curToRoot,
                               impl.rootLayoutSize};
  const material::skia::PaintFrame effectFrame = frameOf(effectCtx);
  return layerEffectOf(node)->resolvedImageFilter(&effectFrame);
}

void PaintPass::note(Composer::Promotion p) {
  if (profile.row != SIZE_MAX) {
    impl.profileRows[profile.row].promotion = p;
    impl.profileRows[profile.row].refusals = refusals;
  }
}

}  // namespace sigil::compose
