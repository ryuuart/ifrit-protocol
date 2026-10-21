/** @file
 * cache: a backdrop filter's pass, and the pixels it left, held for the
 * draws that find nothing painted beneath the node changed.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkRRect.h>
#include <include/core/SkSamplingOptions.h>
#include <src/core/SkCanvasPriv.h>
#include <src/core/SkDevice.h>
#include <src/core/SkSpecialImage.h>

#include "paint/PaintInternal.h"
#include "paint/PaintPass.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** The device a draw on @p canvas lands on, where what the pass leaves can
 *  be read back and written again exactly: its pixels stand on the
 *  canvas's own grid, and its clip has no partly covered pixel, so a copy
 *  of the pixels written under it is the pixels. Null where either fails —
 *  a recording, a canvas that keeps no pixels, a layer placed off the
 *  grid, a clip cut with anti-aliasing. */
SkDevice* deviceHoldingPixels(SkCanvas& canvas) {
  SkDevice* device = SkCanvasPriv::TopDevice(&canvas);
  if (!device || !device->isPixelAlignedToGlobal() ||
      device->isClipAntiAliased())
    return nullptr;
  return device;
}

/** The filter run over what lies beneath, inside the node's outline: a
 *  CLOSED pass, so the node's own decorations and overflowing children
 *  then paint unclipped above it (CSS clips the filter region to the
 *  element, not the element's overflow). Answers the device rect the
 *  pass's layer covered. */
SkIRect filterBeneath(SkCanvas& canvas, SkDevice* device,
                      const SkImageFilter& filter, const SkPath* shape,
                      const SkRRect& corners) {
  canvas.save();
  if (shape)
    canvas.clipPath(*shape, true);
  else
    canvas.clipRRect(corners, true);
  const SkIRect covered = device ? device->devClipBounds() : SkIRect{};
  SkCanvas::SaveLayerRec rec(nullptr, nullptr, &filter, 0);
  canvas.saveLayer(rec);
  canvas.restore();  // composite the filtered backdrop through the clip
  canvas.restore();  // release the clip — content is NOT bounded by it
  return covered;
}

}  // namespace

void paintBackdrop(PaintPass& pass, const SkImageFilter& filter,
                   const SkPath* shape, const SkRRect& corners) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  SkCanvas& canvas = pass.canvas;
  const ElementNode& node = inst.painted();
  HeldBakes::HeldBackdrop& held = inst.heldBackdrop;
  // WHAT A HELD BACKDROP RESTS ON. The pass reads every pixel beneath the
  // node, and the composer knows what it painted there and nothing about
  // the ground the host laid first — so only a host that says its ground
  // holds gets one. A filter whose own parameters move is filtered every
  // draw, and so is a pass inside a recording or a bake, which reads a
  // canvas that is not the one the pixels would be copied back onto.
  const material::Filter* effect = backdropEffectOf(node);
  const bool holdable = impl.groundHeld && !impl.paintingUnseen &&
                        impl.recording.depth == 0 && impl.bakeDepth == 0 &&
                        !impl.liveOnly && !impl.coverageTrace &&
                        !impl.surfaceMap && !(effect && effect->isRunning());
  SkDevice* device = holdable ? deviceHoldingPixels(canvas) : nullptr;
  if (!device) {
    held = {};
    filterBeneath(canvas, nullptr, filter, shape, corners);
    impl.stats.backdropsFiltered++;
    return;
  }
  const SkMatrix matrix = canvas.getTotalMatrix();
  const SkIRect clip = device->devClipBounds();
  const bool outlineHolds =
      shape ? held.shape == *shape : held.corners == corners;
  // THE PIXELS ARE THE PASS'S OWN RESULT, NOT THE LAYER: the filtered
  // layer composited over what lay beneath, through the outline's
  // anti-aliased edge, over the whole rect the layer covered. Beneath has
  // not changed, so the copy writes back the very pixels the pass would
  // leave — inside the outline the filtered composite, outside it the
  // backdrop itself — and the clip in force has no partial pixel to blend
  // them by.
  if (held.pixels && held.beneath == impl.paintedBeneath &&
      held.matrix == matrix && held.clip == clip &&
      held.description == &node && held.box == pass.rect && outlineHolds) {
    SkPaint copy;
    copy.setBlendMode(SkBlendMode::kSrc);
    canvas.save();
    canvas.resetMatrix();
    canvas.drawImageRect(held.pixels, SkRect::Make(held.source),
                         SkRect::Make(held.rect), SkSamplingOptions(), &copy,
                         SkCanvas::kStrict_SrcRectConstraint);
    canvas.restore();
    return;
  }
  const SkIRect covered = filterBeneath(canvas, device, filter, shape, corners);
  impl.stats.backdropsFiltered++;
  held = {};
  if (covered.isEmpty()) return;
  const sk_sp<SkSpecialImage> special =
      device->snapSpecial(covered, /*forceCopy=*/true);
  // A special image may stand inside a larger backing image, as a device
  // texture does: the image it names is then the backing, and the pixels
  // are its subset of it.
  sk_sp<SkImage> pixels = special ? special->asImage() : nullptr;
  if (!pixels || special->subset().size() != covered.size()) return;
  if (pixels->dimensions() == covered.size())
    held.source = SkIRect::MakeSize(covered.size());
  else if (pixels->dimensions() == special->backingStoreDimensions())
    held.source = special->subset();
  else
    return;
  const SkIPoint origin = device->getOrigin();
  held.pixels = std::move(pixels);
  held.rect = covered.makeOffset(origin.x(), origin.y());
  held.beneath = impl.paintedBeneath;
  held.matrix = matrix;
  held.clip = clip;
  held.description = &node;
  held.box = pass.rect;
  if (shape)
    held.shape = *shape;
  else
    held.corners = corners;
}

}  // namespace sigil::compose
