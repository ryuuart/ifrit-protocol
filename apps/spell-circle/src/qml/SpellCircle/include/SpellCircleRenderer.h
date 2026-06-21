#pragma once
#include "FeedModel.h"
#include "TextPathPainter.h"
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
    QList<CircleGeometry> m_circles;
    TextPathPainter m_textPathPainter;
    QCanvasOffscreenCanvas m_canvas;
    QCanvasImage m_canvasImage;
    std::unique_ptr<SyphonBridge> m_syphon;
};
