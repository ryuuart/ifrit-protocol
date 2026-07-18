#pragma once
#include "CanvasSceneBackend.h"
#include "QTextPathPainter.h"
#include "SceneGeometry.h"
#include "SpellCircleModel.h"
#include <QColor>
#include <QFont>
#include <QtCanvasPainter/QCanvasPainterItemRenderer>
#include <memory>

class TexturePublisher;

/**
 * Render-thread renderer for SpellCircle. Resolves scene entities queried
 * from the model's document into absolute, native-scaled positions (via the
 * shared spellcircle::resolveScene()), draws the resulting geometry and
 * labels onto an offscreen canvas each frame, and (when the active graphics
 * backend has a TexturePublisher — Syphon on Metal) publishes that texture
 * for inter-application sharing.
 */
class SpellCircleRenderer : public QCanvasPainterItemRenderer {
public:
  SpellCircleRenderer();
  ~SpellCircleRenderer() override;

  /** Copies GUI-thread model and configuration state into the renderer. */
  void synchronize(QCanvasPainterItem *item) override;
  /** Creates the offscreen backend and external publishing resources. */
  void initializeResources(QCanvasPainter *painter) override;
  /** Resolves dirty geometry and records the current offscreen image. */
  void prePaint(QCanvasPainter *painter) override;
  /** Blits the cached display image into the visible item. */
  void paint(QCanvasPainter *painter) override;

protected:
  /** Publishes the native offscreen texture after Qt records its frame. */
  void render(QRhiCommandBuffer *commandBuffer) override;

private:
  // Grants access to the resolved geometry (m_resolved), style fields, and
  // m_curvedTextPainter below, plus the inherited beginCanvasPainting()/
  // endCanvasPainting() recording brackets — QCanvasPainterSceneBackend
  // draws directly from these rather than going through a method on
  // SpellCircleRenderer. SkiaSceneBackendImpl (defined at global scope in
  // SkiaSceneBackend.cpp, not in an anonymous namespace, so this
  // forward-declaring friend actually names it) needs the same resolved
  // geometry and style fields to hand the shared spellcircle::SceneRenderer
  // an equivalent scene, but never uses m_curvedTextPainter or the
  // QCanvasPainter recording brackets — those are QCanvasPainter-specific.
  friend class QCanvasPainterSceneBackend;
  friend class SkiaSceneBackendImpl;

  /** Queries the model's document (if the generation changed) and resolves
   *  every entity's canvas position/scale into m_resolved via the shared
   *  spellcircle::resolveScene(). */
  void resolveGeometry(SpellCircleModel *model);

  // Scene geometry in absolute, native-scaled canvas coordinates — the
  // same Qt-free structures the native macOS app draws from.
  spellcircle::ResolvedScene m_resolved;
  QTextPathPainter m_curvedTextPainter;
  // Registered image for blitting the latest native offscreen scene into the
  // visible item without re-recording geometry during zoom or pan.
  QCanvasImage m_displayImage;
  // Syphon canvas — native size, published to other apps. Recreated
  // whenever m_canvasWidth/m_canvasHeight no longer match the dimensions
  // it was allocated at.
  QCanvasOffscreenCanvas m_canvas;
  int m_allocatedCanvasWidth = 0;
  int m_allocatedCanvasHeight = 0;
  int m_knownModelGeneration = -1;
  int m_knownConfigGeneration = -1;
  bool m_geometryDirty = true;
  // Null when the active QRhi backend has no publisher implementation (see
  // createTexturePublisher()); render() skips publishing in that case.
  std::unique_ptr<TexturePublisher> m_publisher;

  // The active offscreen-canvas backend, chosen once in
  // initializeResources() and used for every draw in prePaint(). Defaults to
  // QCanvasPainterSceneBackend; createSkiaSceneBackend() replaces it with a
  // Skia Graphite backend when this build has SPELLCIRCLE_ENABLE_SKIA_CANVAS
  // compiled in (see include/SkiaSceneBackend.h) — a build-time choice, so
  // prePaint() itself never branches on which one it has.
  std::unique_ptr<CanvasSceneBackend> m_sceneBackend;

  // Copied from GraphicsConfig in synchronize() — defaults match the
  // GraphicsConfig defaults so an unconfigured SpellCircle item (no
  // config bound) renders identically to before this property existed.
  QColor m_accentColor{"#ff0000"};
  qreal m_strokeWidth = 4.0;
  qreal m_scale = 1.0;
  qreal m_labelOffset = 0.0;
  int m_canvasWidth = 4000;
  int m_canvasHeight = 4000;
  QFont m_font;
  qreal m_boxWidth = 360.0;
  qreal m_boxHeight = 140.0;
  qreal m_boxPadding = 16.0;
  qreal m_boxDistance = 40.0;
  qreal m_pointDistance = 40.0;
};
