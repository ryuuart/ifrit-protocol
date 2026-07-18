#pragma once
#include <QtCanvasPainter/QCanvasPainterItemRenderer>

class SpellCircleRenderer;

/**
 * Draws SpellCircleRenderer's current scene into an offscreen canvas and
 * registers the result with `painter` (via QCanvasPainter::addImage()),
 * returning the resulting image.
 *
 * SkiaSceneBackend (Skia's GPU Graphite canvas, compiled in when
 * SPELLCIRCLE_ENABLE_SKIA_CANVAS is ON) is the one implementation; the
 * interface stays so SpellCircleRenderer never depends on Skia types and a
 * stub build simply yields no backend.
 */
class CanvasSceneBackend {
public:
  virtual ~CanvasSceneBackend() = default;

  /**
   * Draws into `canvas` (already sized to `pixelSize`) and returns the
   * registered QCanvasImage for display/publishing.
   *
   * `renderer` gives QCanvasPainter-based backends access to the resolved
   * scene geometry and the beginCanvasPainting()/endCanvasPainting()
   * recording brackets; a backend that wraps the canvas's GPU texture
   * directly with a different API (e.g. Skia) may ignore it.
   */
  virtual QCanvasImage drawScene(SpellCircleRenderer &renderer,
                                 QCanvasPainter *painter,
                                 QCanvasOffscreenCanvas &canvas,
                                 QSize pixelSize) = 0;
};
