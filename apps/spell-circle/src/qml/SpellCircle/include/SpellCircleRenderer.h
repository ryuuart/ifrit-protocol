#pragma once
#include "SpellCircleModel.h"
#include "TextPathPainter.h"
#include <QColor>
#include <QFont>
#include <QtCanvasPainter/QCanvasPainterItemRenderer>
#include <memory>

class SyphonBridge;

/**
 * Render-thread renderer for SpellCircle. Draws circle geometry and their
 * labels onto an offscreen canvas each frame and publishes the resulting Metal
 * texture to Syphon for inter-application sharing.
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
    void drawScene(QCanvasPainter *p);

    QList<CircleGeometry> m_circles;
    QList<PointGeometry> m_points;
    QList<EdgeGeometry> m_edges;
    QList<BoxGeometry> m_boxes;
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
};
