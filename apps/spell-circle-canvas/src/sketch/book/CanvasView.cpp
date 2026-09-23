/** @file
 * The pane's view of the canvas, the frame drawn through it, the scale
 * held while a zoom moves, and the pointer's gate.
 */

#include "CanvasView.h"

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/gpu/graphite/Surface.h>

#include <algorithm>
#include <cmath>

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

SkPoint canvasPointAt(const sketch::Placement& placement, SkPoint panePoint) {
  if (!(placement.scale > 0.0f)) return {0.0f, 0.0f};
  return {(panePoint.x() - placement.x) / placement.scale,
          (panePoint.y() - placement.y) / placement.scale};
}

namespace {

/** The layer's own size is rounded up to this many pixels, so the steps
 *  of one gesture, each needing a slightly different extent, reuse one
 *  surface rather than asking for another at each. */
constexpr int kLayerGrain = 256;

int grainAbove(int extent) {
  return (extent + kLayerGrain - 1) / kLayerGrain * kLayerGrain;
}

/** The layer's pixels as an image to draw from. A device surface is
 *  shown as it stands, since its own snapshot is always a copy there;
 *  a raster one is snapshotted, which copies nothing unless it is drawn
 *  into while the image is still held. */
sk_sp<SkImage> shownFrom(const sk_sp<SkSurface>& surface) {
  if (sk_sp<SkImage> shared = SkSurfaces::AsImage(surface)) return shared;
  return surface->makeImageSnapshot();
}

}  // namespace

void drawPane(SkCanvas& target, SkSize pane, SkSize canvas,
              const CanvasView& view,
              const std::function<void(SkCanvas&)>& paint, float drawnScale,
              PaneLayer* layer) {
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

  // THE PART OF THE CANVAS ON THE PANE, at the scale the frame is drawn
  // at, in whole pixels of a layer: what a held frame is drawn into.
  bool held = layer && drawnScale > 0.0f && placement.scale > 0.0f &&
              drawnScale != placement.scale;
  SkIRect layerPixels = SkIRect::MakeEmpty();
  if (held) {
    SkRect visible = SkRect::MakeXYWH(
        -placement.x / placement.scale, -placement.y / placement.scale,
        pane.width() / placement.scale, pane.height() / placement.scale);
    held = visible.intersect(canvasBounds);
    if (held) {
      layerPixels = SkRect::MakeLTRB(visible.left() * drawnScale,
                                     visible.top() * drawnScale,
                                     visible.right() * drawnScale,
                                     visible.bottom() * drawnScale)
                        .roundOut();
      held = !layerPixels.isEmpty() &&
             (float)layerPixels.width() <= pane.width() * 2.0f + 2.0f &&
             (float)layerPixels.height() <= pane.height() * 2.0f + 2.0f;
    }
  }
  if (held && (!layer->surface ||
               layer->surface->width() < layerPixels.width() ||
               layer->surface->height() < layerPixels.height())) {
    const SkImageInfo info = SkImageInfo::MakeN32Premul(
        grainAbove(std::max(layerPixels.width(),
                            layer->surface ? layer->surface->width() : 0)),
        grainAbove(std::max(layerPixels.height(),
                            layer->surface ? layer->surface->height() : 0)));
    // Made through the target, so it lives where the pane is drawn.
    layer->surface = target.makeSurface(info);
    held = layer->surface != nullptr;
  }

  // PAINTED EVEN WHEN NONE OF IT IS ON THE PANE. The call is the frame's
  // tick, and a sketch panned out of sight keeps its own time; the clip
  // is what spares it the drawing.
  const int depth = target.save();
  intoCanvas();
  if (held) {
    SkCanvas& into = *layer->surface->getCanvas();
    const SkRect used = SkRect::Make(SkIRect::MakeSize(layerPixels.size()));
    into.save();
    // Only the part this frame uses is cleared and drawn: the rest of the
    // layer is never shown.
    into.clipRect(used);
    into.clear(SK_ColorTRANSPARENT);
    into.translate(-(float)layerPixels.left(), -(float)layerPixels.top());
    into.scale(drawnScale, drawnScale);
    into.clipRect(canvasBounds);
    paint(into);
    into.restore();
    // One layer pixel is a canvas unit over the drawn scale, and the
    // layer's first pixel stands at the visible part's corner.
    const SkRect standsAt = SkRect::MakeLTRB(
        (float)layerPixels.left() / drawnScale,
        (float)layerPixels.top() / drawnScale,
        (float)layerPixels.right() / drawnScale,
        (float)layerPixels.bottom() / drawnScale);
    if (const sk_sp<SkImage> shown = shownFrom(layer->surface))
      target.drawImageRect(shown, used, standsAt,
                           SkSamplingOptions(SkFilterMode::kLinear), nullptr,
                           SkCanvas::kFast_SrcRectConstraint);
  } else {
    paint(target);
  }
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

float ZoomHold::drawnScale(float viewScale, Clock::time_point now) {
  if (viewScale != m_seen) {
    m_seen = viewScale;
    m_movedAt = now;
  }
  const bool settled = now - m_movedAt >= kSettle;
  const float ratio = m_drawn > 0.0f ? viewScale / m_drawn : 0.0f;
  if (settled || !(ratio >= 0.5f && ratio <= 2.0f)) m_drawn = viewScale;
  return m_drawn;
}

bool PointerGate::admit(bool pressed, bool onCanvas) {
  const bool pressBegins = pressed && !m_buttonDown;
  m_buttonDown = pressed;
  if (pressBegins) m_pressOwned = onCanvas;
  if (m_pressOwned) {
    // Followed wherever it is dragged, and its release is heard too.
    if (!pressed) m_pressOwned = false;
    return true;
  }
  // No press of the sketch's is down: a pointer over the canvas with no
  // button is heard, and a press that is not the sketch's is not.
  return !pressed && onCanvas;
}
