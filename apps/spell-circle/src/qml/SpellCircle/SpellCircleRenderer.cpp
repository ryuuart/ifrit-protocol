#include "SpellCircleRenderer.h"
#include "SpellCircle.h"
#include <spdlog/spdlog.h>

void SpellCircleRenderer::synchronize(QCanvasPainterItem *item) {
  auto *sc = static_cast<SpellCircle *>(item);
  if (sc->model()) {
    m_circles = sc->model()->circles();
  } else
    m_circles.clear();
}

void SpellCircleRenderer::initializeResources(QCanvasPainter *p) {}

void SpellCircleRenderer::prePaint(QCanvasPainter *p) {}

void SpellCircleRenderer::paint(QCanvasPainter *p) {
  for (const auto &circle : m_circles) {
    const float r = static_cast<float>(circle.radius);
    QRectF rect(circle.x - r, circle.y - r, r * 2, r * 2);
    p->beginPath();
    p->roundRect(rect, r);
    p->setFillStyle("#DBEB00");
    p->fill();
  }
}
