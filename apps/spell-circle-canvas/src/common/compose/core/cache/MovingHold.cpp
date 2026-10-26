/** @file
 * The pixels a live composer holds while a node's own transform or opacity
 * moves: the subtree painted once, on the device the draw is on — in the
 * node's own frame at the scale it is shown at where the node moves, in
 * place on the device grid where only its opacity does — and composited
 * under each new matrix at each new opacity.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "paint/PaintInternal.h"
#include "paint/PaintPass.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** The scale pixels are formed at for a node shown at @p shown device
 *  pixels a unit: rounded UP to an eighth of an octave, so a scale that
 *  creeps forms the pixels again only once in each eighth rather than at
 *  every draw, and a turned node is never sampled up from fewer texels
 *  than it covers. Bounded below and above, as every bake is. */
float formingScale(float shown) {
  constexpr float kSteps = 8.0f;
  const float bounded = std::clamp(shown, 0.0625f, 4.0f);
  return std::exp2(std::ceil(std::log2(bounded) * kSteps) / kSteps);
}

/** The node fading where it stands: its subtree painted once on the
 *  device grid, under the matrix and clip the draw has, exactly as a layer
 *  opened for the fade would hold it, and composited there at each draw's
 *  opacity. Formed again when the node's paint, matrix, clip or rect
 *  changes. */
bool paintHeldInPlace(PaintPass& pass) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  const SkIRect device = pass.deviceRect();
  if (device.isEmpty()) return false;
  const SkIRect clip = pass.deviceClip();
  const bool stale = !inst.movingImage || !inst.movingInPlace ||
                     inst.paintDirty || inst.movingDeviceRect != device ||
                     inst.movingMatrix != pass.totalM ||
                     inst.movingClip != clip;
  if (stale) {
    inst.movingImage.reset();
    sk_sp<SkImage> formed = pass.takeDeviceBake(device, [&](SkCanvas& layer) {
      // no leaf blend: the pixels isolate, as a layer's do
      const BakeLayerScope bakeLayer(&impl, layer);
      impl.paintContent(inst, layer, impl.hostScale);
    });
    if (!formed) return false;
    inst.movingImage = std::move(formed);
    inst.movingInPlace = true;
    inst.movingDeviceRect = device;
    inst.movingMatrix = pass.totalM;
    inst.movingClip = clip;
    // What the node recorded before it faded holds none of what changed
    // since: it is recorded afresh the draw its opacity holds still.
    inst.picture.reset();
    inst.paintDirty = false;
    impl.stats.picturesRecorded++;
    impl.stats.texturesBaked++;
  }
  if (pass.profile.row != SIZE_MAX)
    impl.profileRows[pass.profile.row].cacheState =
        Composer::CacheState::Moving;
  ++impl.stats.nodesHeldWhileMoving;
  SkPaint composite;
  composite.setAlphaf(pass.opacity);
  pass.deviceBlit(inst.movingImage, device, &composite);
  return true;
}

}  // namespace

bool paintHeldWhileMoving(PaintPass& pass) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  SkCanvas& canvas = pass.canvas;
  // A NODE THAT ONLY FADES stands where it stood, so its pixels are held on
  // the device grid and composite exactly as the layer the fade would open.
  if (pass.matrixStable && !inst.transformLive && !inst.transformLiveAbove)
    return paintHeldInPlace(pass);
  const SkRect& bounds = pass.localBounds();
  // THE LARGEST SCALE THE MOTION HAS SHOWN THE NODE AT: pixels formed for
  // a smaller one would be sampled up, so a node shown larger forms them
  // again, and one shown smaller keeps them and samples them down.
  const float shown = maximumScaleOf(pass.totalM, bounds);
  if (!(shown > 0.0f) || !std::isfinite(shown)) return false;
  const float needed = formingScale(shown);
  const bool held = inst.movingImage && !inst.movingInPlace;
  const bool stale = !held || inst.paintDirty || inst.movingBounds != bounds ||
                     inst.movingScale < needed;
  if (stale) {
    const float scale =
        std::max(needed, held && !inst.paintDirty ? inst.movingScale : 0.0f);
    inst.movingImage.reset();
    const SkISize dimensions = PaintPass::localBakeSize(bounds, scale);
    if (dimensions.isEmpty()) return false;
    // ON THE DEVICE THE DRAW IS ON, as every bake is: a surface the device
    // refuses is no hold, and the node paints as it would without one.
    sk_sp<SkSurface> layer =
        impl.localBakeSurface(canvas, impl.bakeInfo(dimensions));
    if (!layer) return false;
    SkCanvas& formed = *layer->getCanvas();
    formed.scale(scale, scale);
    formed.translate(-bounds.left(), -bounds.top());
    const bool outerRead = std::exchange(impl.contentScaleRead, false);
    {  // no leaf blend: the pixels isolate, as a bake's do
      const BakeLayerScope bakeLayer(&impl, formed);
      impl.paintContent(inst, formed, scale);
    }
    impl.contentScaleRead = outerRead;
    inst.movingImage = layer->makeImageSnapshot();
    if (!inst.movingImage) return false;
    inst.movingInPlace = false;
    inst.movingScale = scale;
    inst.movingBounds = bounds;
    // What the node recorded before it moved holds none of what changed
    // since: it is recorded afresh the draw the node holds still.
    inst.picture.reset();
    inst.paintDirty = false;
    impl.stats.picturesRecorded++;
    impl.stats.texturesBaked++;
  }
  if (pass.profile.row != SIZE_MAX)
    impl.profileRows[pass.profile.row].cacheState =
        Composer::CacheState::Moving;
  ++impl.stats.nodesHeldWhileMoving;
  // Through the rect the pixels ACTUALLY cover: their size was rounded up,
  // so stretching them across `bounds` alone would resample the node by up
  // to a texel's worth of scale. The overshoot is transparent.
  const SkRect destination =
      SkRect::MakeXYWH(bounds.left(), bounds.top(),
                       (float)inst.movingImage->width() / inst.movingScale,
                       (float)inst.movingImage->height() / inst.movingScale);
  // The pixels were formed without the node's opacity, and no layer is
  // open for it: it rides the one draw they are composited by.
  SkPaint composite;
  composite.setAlphaf(pass.opacity);
  canvas.drawImageRect(inst.movingImage, destination,
                       SkSamplingOptions(SkFilterMode::kLinear), &composite);
  return true;
}

}  // namespace sigil::compose
