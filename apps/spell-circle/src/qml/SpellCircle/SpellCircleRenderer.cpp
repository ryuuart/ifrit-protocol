#include "SpellCircleRenderer.h"
#include "SpellCircle.h"
#include "SyphonBridge.h"
#include <QCanvasPath>
#include <QColor>
#include <QPainterPath>
#include <spdlog/spdlog.h>

SpellCircleRenderer::SpellCircleRenderer()
    : m_syphon(std::make_unique<SyphonBridge>("SpellCircle")) {
  QFont font;
  font.setPointSize(12);
  m_textPathPainter.setFont(font);
  // addEllipse starts at 3 o'clock and goes CW, so 0.75 lands at 12 o'clock.
  m_textPathPainter.setPathOffset(0.75f);
}

SpellCircleRenderer::~SpellCircleRenderer() { m_syphon->stop(); }

void SpellCircleRenderer::synchronize(QCanvasPainterItem *item) {
  auto *sc = static_cast<SpellCircle *>(item);
  if (sc->model())
    m_circles = sc->model()->circles();
  else
    m_circles.clear();
}

void SpellCircleRenderer::initializeResources(QCanvasPainter *p) {
  m_syphon->start(rhi());
}

void SpellCircleRenderer::prePaint(QCanvasPainter *p) {
  if (m_canvas.isNull())
    m_canvas = p->createCanvas(QSize(width(), height()));
  beginCanvasPainting(m_canvas);

  for (const auto &circle : m_circles) {
    const float r = static_cast<float>(circle.radius);

    QCanvasPath circlePath;
    circlePath.circle(circle.x, circle.y, r);

    p->beginPath();
    p->addPath(circlePath);
    p->setStrokeStyle(QColor("red"));
    p->setLineWidth(4.0);
    p->stroke();

    QPainterPath textPath;
    textPath.addEllipse(circle.x - r, circle.y - r, r * 2.0f, r * 2.0f);

    m_textPathPainter.setPathOffset(0.5);
    m_textPathPainter.setPerpendicularOffset(36);
    m_textPathPainter.setFontSize(72);
    m_textPathPainter.setFontWeight(QFont::Bold);
    m_textPathPainter.setColor("red");
    m_textPathPainter.paint(p, textPath, circle.name);
  }

  endCanvasPainting();
  m_canvasImage = p->addImage(m_canvas);
}

void SpellCircleRenderer::paint(QCanvasPainter *p) {
  p->drawImage(m_canvasImage, 0, 0, width(), height());
}

void SpellCircleRenderer::render(QRhiCommandBuffer *cb) {
  QCanvasPainterItemRenderer::render(cb);
  if (!m_canvas.isNull() && m_canvas.texture())
    m_syphon->publishFrame(m_canvas.texture(), cb, static_cast<int>(width()),
                           static_cast<int>(height()));
}
