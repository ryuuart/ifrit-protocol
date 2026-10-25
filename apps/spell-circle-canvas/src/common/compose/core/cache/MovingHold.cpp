/** @file
 * The pixels a live composer holds while a node's own transform moves: the
 * subtree painted once in the node's own frame, on the device the draw is
 * on, at the scale it is shown at, and composited under each new matrix.
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

}  // namespace

bool paintHeldWhileMoving(PaintPass& pass) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  SkCanvas& canvas = pass.canvas;
  const SkRect& bounds = pass.localBounds();
  // THE LARGEST SCALE THE MOTION HAS SHOWN THE NODE AT: pixels formed for
  // a smaller one would be sampled up, so a node shown larger forms them
  // again, and one shown smaller keeps them and samples them down.
  const float shown = maxScaleOf(pass.totalM, bounds);
  if (!(shown > 0.0f) || !std::isfinite(shown)) return false;
  const float needed = formingScale(shown);
  const bool stale = !inst.movingImage || inst.paintDirty ||
                     inst.movingBounds != bounds || inst.movingScale < needed;
  if (stale) {
    const float scale = std::max(needed, inst.movingImage && !inst.paintDirty
                                             ? inst.movingScale
                                             : 0.0f);
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
  const SkRect destination = SkRect::MakeXYWH(
      bounds.left(), bounds.top(),
      (float)inst.movingImage->width() / inst.movingScale,
      (float)inst.movingImage->height() / inst.movingScale);
  // A leaf's opacity rides its fill rather than a layer, and the pixels
  // were formed without it: it rides the one draw they are composited by.
  SkPaint composite;
  composite.setAlphaf(pass.leafOpacity);
  canvas.drawImageRect(inst.movingImage, destination,
                       SkSamplingOptions(SkFilterMode::kLinear), &composite);
  return true;
}

}  // namespace sigil::compose
