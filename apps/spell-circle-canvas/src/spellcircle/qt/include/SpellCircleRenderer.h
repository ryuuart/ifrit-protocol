#pragma once
#include <sigilio/publish/Publisher.h>

#include <QColor>
#include <QFont>
#include <QtCanvasPainter/QCanvasPainterItemRenderer>
#include <cstdint>
#include <limits>
#include <memory>

#include "ReceiverDefaults.h"
#include "SceneGeometry.h"
#include "SpellCircleModel.h"

/**
 * Render-thread renderer for SpellCircle. Resolves scene entities queried
 * from the model's document into absolute, native-scaled positions (via the
 * shared spellcircle::resolveScene()), draws the resulting geometry and
 * labels onto an offscreen canvas when the scene changes, and (when the active
 * graphics backend supports publication) publishes that texture
 * for inter-application sharing.
 */
class SpellCircleRenderer : public QCanvasPainterItemRenderer {
 public:
  SpellCircleRenderer();
  ~SpellCircleRenderer() override;

  /** Copies GUI-thread model and configuration state into the renderer. */
  void synchronize(QCanvasPainterItem* item) override;
  /** Creates the Graphite context and external publishing resources. */
  void initializeResources(QCanvasPainter* painter) override;
  /** Resolves dirty geometry and records the current offscreen image. */
  void prePaint(QCanvasPainter* painter) override;
  /** Blits the cached display image into the visible item. */
  void paint(QCanvasPainter* painter) override;

 protected:
  /** Publishes the native offscreen texture after Qt records its frame. */
  void render(QRhiCommandBuffer* commandBuffer) override;

 private:
  struct RenderState;

  /** Queries the model's document (if the generation changed) and resolves
   *  every entity's canvas position/scale into m_resolved via the shared
   *  spellcircle::resolveScene(). */
  void resolveGeometry(SpellCircleModel* model);
  /** Records the current scene into the native-size offscreen canvas. */
  QCanvasImage drawScene(QCanvasPainter* painter);

  // Scene geometry in absolute, native-scaled canvas coordinates — the
  // same Qt-free structures the native macOS app draws from.
  spellcircle::ResolvedScene m_resolved;
  // Registered image for blitting the latest native offscreen scene into the
  // visible item without re-recording geometry during zoom or pan.
  QCanvasImage m_displayImage;
  // Native-size canvas, published to other apps. Recreated
  // whenever m_canvasWidth/m_canvasHeight no longer match the dimensions
  // it was allocated at.
  QCanvasOffscreenCanvas m_canvas;
  int m_allocatedCanvasWidth = 0;
  int m_allocatedCanvasHeight = 0;
  uint64_t m_knownModelGeneration = std::numeric_limits<uint64_t>::max();
  int m_knownConfigGeneration = -1;
  bool m_geometryDirty = true;
  // Null when the active QRhi backend has no publisher implementation.
  std::unique_ptr<sigil::io::publish::Publisher> m_publisher;

  // Graphite context, scene drawer and frame timing, owned by this render
  // thread. Null when the active QRhi backend has no Graphite context.
  std::unique_ptr<RenderState> m_renderState;

  // Copied from GraphicsConfig in synchronize() — defaults match the
  // GraphicsConfig defaults for an unconfigured SpellCircle item.
  QColor m_accentColor = QColor::fromRgba(spellcircle::ReceiverDefaults::color);
  qreal m_strokeWidth = spellcircle::ReceiverDefaults::strokeWidth;
  qreal m_scale = spellcircle::ReceiverDefaults::scale;
  qreal m_labelOffset = spellcircle::ReceiverDefaults::labelOffset;
  int m_canvasWidth = spellcircle::ReceiverDefaults::canvasWidth;
  int m_canvasHeight = spellcircle::ReceiverDefaults::canvasHeight;
  QFont m_font;
  qreal m_boxWidth = spellcircle::ReceiverDefaults::boxWidth;
  qreal m_boxHeight = spellcircle::ReceiverDefaults::boxHeight;
  qreal m_boxPadding = spellcircle::ReceiverDefaults::boxPadding;
  qreal m_boxDistance = spellcircle::ReceiverDefaults::boxDistance;
  qreal m_pointDistance = spellcircle::ReceiverDefaults::pointDistance;
};
