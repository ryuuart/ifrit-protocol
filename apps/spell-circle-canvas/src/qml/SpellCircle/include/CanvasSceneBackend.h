#pragma once
#include <QtCanvasPainter/QCanvasPainterItemRenderer>

class SpellCircleRenderer;

/**
 * Draws SpellCircleRenderer's current scene into an offscreen canvas and
 * registers the result with `painter` (via QCanvasPainter::addImage()),
 * returning the resulting image.
 *
 * Two implementations exist — QCanvasPainterSceneBackend (the original 2D
 * immediate-mode path) and SkiaSceneBackend (Skia's GPU Graphite canvas,
 * only compiled in when SPELLCIRCLE_ENABLE_SKIA_CANVAS is ON) — so
 * SpellCircleRenderer selects between them through a single uniform call
 * instead of special-casing one of the two.
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
