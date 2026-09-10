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
  // The paint bounds, which already hold the shape the node declares and
  // the reach the effects under it filter over: a device bake is a
  // SURFACE, and a surface allocated to less than the ink cuts it.
  const SkRect f = totalM.mapRect(localBounds());
  SkIRect r =
      SkIRect::MakeLTRB((int)std::floor(f.left()), (int)std::floor(f.top()),
                        (int)std::ceil(f.right()), (int)std::ceil(f.bottom()));
  const int m = Composer::Impl::bakeMargin(std::max(r.width(), r.height()));
  r.outset(m, m);
  // …AND NO FURTHER THAN THE CLIP THE BAKE CARRIES IN. A filter whose
  // output does not depend on its input — a shader run over the layer —
  // reaches everywhere, and a surface cannot be allocated to everywhere.
  // What such a filter paints is bounded by the clip the layer is given,
  // which is this one, so the allocation stops there and no ink is lost:
  // nothing outside that clip is drawn into a bake at all.
  SkIRect capped = deviceClip();
  capped.outset(m, m);
  if (!r.intersect(capped)) return SkIRect::MakeEmpty();
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
  const PaintContext effectCtx{.size = {rect.width(), rect.height()},
                               .elapsedSeconds = impl.elapsed(),
                               .contentScale = impl.hostScale,
                               .animating = impl.ticker.active(),
                               .fonts = &impl.fonts,
                               .stamps = &inst.stampCache,
                               .toRoot = impl.curToRoot,
                               .rootSize = impl.rootLayoutSize};
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
