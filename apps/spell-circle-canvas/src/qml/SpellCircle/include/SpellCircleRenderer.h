#pragma once
#include "CanvasSceneBackend.h"
#include "QTextPathPainter.h"
#include "SpellCircleModel.h"
#include <QColor>
#include <QFont>
#include <QPointF>
#include <QtCanvasPainter/QCanvasPainterItemRenderer>
#include <memory>

class SyphonBridge;

/**
 * Render-thread renderer for SpellCircle. Resolves scene entities queried
 * from the model's entt::registry into absolute, native-scaled positions
 * (via QPainterPath), draws the resulting geometry and labels onto an
 * offscreen canvas each frame, and publishes the Metal texture to Syphon for
 * inter-application sharing.
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
  // Grants access to the resolved geometry (m_circles/m_edges/m_boxes/
  // m_pointLabels), style fields, and m_curvedTextPainter below, plus the
  // inherited beginCanvasPainting()/endCanvasPainting() recording brackets —
  // QCanvasPainterSceneBackend draws directly from these rather than going
  // through a method on SpellCircleRenderer. SkiaSceneBackendImpl (defined at
  // global scope in SkiaSceneBackend.mm, not in an anonymous namespace, so
  // this forward-declaring friend actually names it) needs the same resolved
  // geometry and style fields to draw the equivalent scene via Skia, but
  // never uses m_curvedTextPainter or the QCanvasPainter recording brackets —
  // those are QCanvasPainter-specific.
  friend class QCanvasPainterSceneBackend;
  friend class SkiaSceneBackendImpl;

  /** A circle resolved to absolute, native-scaled canvas coordinates. */
  struct ResolvedCircle {
    QString name;
    QPointF center;
    float radius = 0.0f;
    float textStart = 0.0f;
    float active = 0.0f; // background fill alpha/intensity [0, 1]
  };

  /** A straight connector between two resolved point positions. */
  struct ResolvedEdge {
    QPointF first;
    QPointF second;
  };

  /** A labelled box anchored at a resolved point position, offset outward
   *  along the ray from the canvas center through that point. */
  struct ResolvedBox {
    QString value;
    QPointF anchor;
    // Unit vector from the canvas center through `anchor`, used both to push
    // the box outward by the configured distance and to pick which of its
    // edges (the one facing the center) sits at that offset.
    QPointF direction;
    float active = 0.0f; // background fill alpha/intensity [0, 1]
  };

  /** Queries the model's registry (if the generation changed) and resolves
   *  every entity's canvas position/scale into m_circles/m_edges/m_boxes.
   *  This is the only place scaling and point-on-circle math happens. */
  void resolveGeometry(SpellCircleModel *model);

  QList<ResolvedCircle> m_circles;
  QList<ResolvedEdge> m_edges;
  QList<ResolvedBox> m_boxes;
  // Point.value labels: positioned the same way as m_boxes (anchor pushed
  // outward along the ray from the canvas center) and sourced from every
  // PointComponent with a non-empty value rather than from BoxComponent — a
  // Point already anchoring a Box or Edge can still carry its own label —
  // but drawn as plain text with no surrounding rect/stroke/fill, since a
  // point isn't a box. `active` is unused here (Point has no fill concept).
  QList<ResolvedBox> m_pointLabels;
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
  std::unique_ptr<SyphonBridge> m_syphon;

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
