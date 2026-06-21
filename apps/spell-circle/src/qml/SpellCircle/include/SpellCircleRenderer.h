#pragma once
#include "FeedModel.h"
#include <QtCanvasPainter/QCanvasPainterItemRenderer>
#include <memory>

class SyphonBridge;

/**
 * Render-thread renderer for SpellCircle. Draws circle geometry onto an
 * offscreen canvas each frame and publishes the resulting Metal texture to
 * Syphon for inter-application sharing.
 */
class SpellCircleRenderer : public QCanvasPainterItemRenderer {
public:
    SpellCircleRenderer();
    virtual ~SpellCircleRenderer();

    /** Copies the current circle list from the SpellCircle item on the render thread. */
    void synchronize(QCanvasPainterItem *item) override;

    /** Creates the offscreen canvas and starts the Syphon server (rhi() is valid here). */
    void initializeResources(QCanvasPainter *p) override;

    /** Paints all circles onto the offscreen canvas before the main paint pass. */
    void prePaint(QCanvasPainter *p) override;

    /** Composites the offscreen canvas image into the item surface. */
    void paint(QCanvasPainter *p) override;

protected:
    /**
     * Calls the base render to flush canvas commands, then publishes the
     * offscreen texture to Syphon. Overrides paint() path to gain access to cb.
     */
    void render(QRhiCommandBuffer *cb) override;

private:
    QList<CircleGeometry> m_circles;
    QCanvasOffscreenCanvas m_canvas;
    QCanvasImage m_canvasImage;
    std::unique_ptr<SyphonBridge> m_syphon;
};
