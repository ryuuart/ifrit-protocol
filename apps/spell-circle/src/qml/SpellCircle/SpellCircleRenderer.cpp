#include "SpellCircleRenderer.h"
#include "SpellCircle.h"
#include "SyphonBridge.h"
#include <spdlog/spdlog.h>

SpellCircleRenderer::SpellCircleRenderer()
    : m_syphon(std::make_unique<SyphonBridge>("SpellCircle")) {}

SpellCircleRenderer::~SpellCircleRenderer() { m_syphon->stop(); }

void SpellCircleRenderer::synchronize(QCanvasPainterItem *item) {
  auto *sc = static_cast<SpellCircle *>(item);
  if (sc->model()) {
    m_circles = sc->model()->circles();
  } else
    m_circles.clear();
}

void SpellCircleRenderer::initializeResources(QCanvasPainter *p) {
  m_syphon->start(rhi()); // rhi() is valid here because initializeResources is called from within render()
}

void SpellCircleRenderer::prePaint(QCanvasPainter *p) {
  // this is where offscreen canvases are drawn into
  if (m_canvas.isNull()) {
    m_canvas = p->createCanvas(QSize(width(), height()));
  }
  beginCanvasPainting(m_canvas);
  for (const auto &circle : m_circles) {
    const float r = static_cast<float>(circle.radius);
    QRectF rect(circle.x - r, circle.y - r, r * 2, r * 2);
    p->beginPath();
    p->roundRect(rect, r);
    p->setFillStyle("#DBEB00");
    p->fill();
  }
  endCanvasPainting();
  m_canvasImage = p->addImage(m_canvas);
}

void SpellCircleRenderer::paint(QCanvasPainter *p) {
  p->drawImage(m_canvasImage, 0, 0);
}

void SpellCircleRenderer::render(QRhiCommandBuffer *cb) {
  QCanvasPainterItemRenderer::render(cb); // base records all canvas draw commands first
  if (!m_canvas.isNull() && m_canvas.texture())
    m_syphon->publishFrame(m_canvas.texture(), cb, static_cast<int>(width()),
                           static_cast<int>(height()));
}
