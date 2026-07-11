#pragma once
#include "CanvasSceneBackend.h"

/**
 * The original 2D immediate-mode offscreen-canvas backend: draws the
 * resolved scene geometry (circles, edges, boxes, point labels) directly via
 * QCanvasPainter, bracketed by beginCanvasPainting()/endCanvasPainting().
 * Always available, regardless of which other backends this build was
 * compiled with.
 */
class QCanvasPainterSceneBackend final : public CanvasSceneBackend {
public:
  QCanvasImage drawScene(SpellCircleRenderer &renderer, QCanvasPainter *painter,
                         QCanvasOffscreenCanvas &canvas, QSize pixelSize) override;
};
