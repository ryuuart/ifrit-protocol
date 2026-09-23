/** @file
 * The pane's view of the canvas, and the frame drawn through it.
 */

#include "CanvasView.h"

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>

namespace sketch = sigil::sketch;

sketch::Placement placeCanvas(SkSize canvas, SkSize pane,
                              const CanvasView& view) {
  if (!(view.scale > 0.0f))
    return sketch::fitInto(canvas,
                           SkRect::MakeWH(pane.width(), pane.height()));
  sketch::Placement placement;
  placement.scale = view.scale;
  placement.x = pane.width() * 0.5f + view.offset.x() -
                canvas.width() * view.scale * 0.5f;
  placement.y = pane.height() * 0.5f + view.offset.y() -
                canvas.height() * view.scale * 0.5f;
  return placement;
}

void drawPane(SkCanvas& target, SkSize pane, SkSize canvas,
              const CanvasView& view,
              const std::function<void(SkCanvas&)>& paint) {
  target.clear(SK_ColorTRANSPARENT);
  const sketch::Placement placement = placeCanvas(canvas, pane, view);
  const SkRect paneBounds = SkRect::MakeWH(pane.width(), pane.height());
  const SkRect canvasBounds = SkRect::MakeWH(canvas.width(), canvas.height());
  const auto intoCanvas = [&] {
    // The pane first and the canvas after, in that order: the first clip
    // is in the pane's own units and is what bounds the work a magnified
    // frame asks for, the second is the canvas's edge as the sketch
    // declared it.
    target.clipRect(paneBounds);
    target.translate(placement.x, placement.y);
    target.scale(placement.scale, placement.scale);
    target.clipRect(canvasBounds);
  };
  // PAINTED EVEN WHEN NONE OF IT IS ON THE PANE. The call is the frame's
  // tick, and a sketch panned out of sight keeps its own time; the clip
  // is what spares it the drawing.
  const int depth = target.save();
  intoCanvas();
  paint(target);
  target.restoreToCount(depth);
  // Behind what was painted rather than under it before: a sketch clears
  // its canvas to its own ground, alpha and all, so a matte laid first
  // would be replaced rather than shown through.
  target.save();
  intoCanvas();
  SkPaint matte;
  matte.setColor(kPaneMatte);
  matte.setBlendMode(SkBlendMode::kDstOver);
  target.drawRect(canvasBounds, matte);
  target.restore();
}
