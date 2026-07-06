#pragma once
#include "SpellCircleModel.h"
#include "TextPathPainter.h"
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
  virtual ~SpellCircleRenderer();

  void synchronize(QCanvasPainterItem *item) override;
  void initializeResources(QCanvasPainter *p) override;
  void prePaint(QCanvasPainter *p) override;
  void paint(QCanvasPainter *p) override;

protected:
  void render(QRhiCommandBuffer *cb) override;

private:
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

  void drawScene(QCanvasPainter *p);

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
  TextPathPainter m_textPathPainter;
  // Display cache — rasterised at kDisplaySize for cheap blitting in paint().
  QCanvasOffscreenCanvas m_displayCanvas;
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
