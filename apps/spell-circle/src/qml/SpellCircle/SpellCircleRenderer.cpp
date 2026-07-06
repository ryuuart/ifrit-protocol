#include "SpellCircleRenderer.h"
#include "SpellCircle.h"
#include "SyphonBridge.h"
#include <QCanvasPath>
#include <QColor>
#include <QPainterPath>
#include <spdlog/spdlog.h>

namespace {
const QColor kCircleFill(255, 0, 0, 255);
const QColor kBoxActiveText("#0a0a0a");
// Display cache resolution. QCanvasPainter is a CPU software rasterizer: it
// tessellates paths on the CPU and uploads pixels to the GPU. Rendering at
// 4000² = 16 MP costs ~16× more CPU time than 1024² = 1 MP. paint() blits
// this cached image as a cheap GPU texture scale; it is only redrawn when
// scene geometry actually changes, not on zoom or pan repaints.
constexpr int kDisplaySize = 1024;
} // namespace

SpellCircleRenderer::SpellCircleRenderer()
    : m_syphon(std::make_unique<SyphonBridge>("SpellCircle")) {
  m_font.setBold(true);
  m_font.setPointSize(36);
  // addEllipse starts at 3 o'clock and goes CW, so 0.75 lands at 12 o'clock.
  m_textPathPainter.setPathOffset(0.75f);
}

SpellCircleRenderer::~SpellCircleRenderer() { m_syphon->stop(); }

void SpellCircleRenderer::synchronize(QCanvasPainterItem *item) {
  auto *sc = static_cast<SpellCircle *>(item);

  const int modelGen = sc->model() ? sc->model()->generation() : 0;
  if (modelGen != m_knownModelGeneration) {
    m_knownModelGeneration = modelGen;
    m_geometryDirty = true;
    if (sc->model()) {
      m_circles = sc->model()->circles();
      m_points = sc->model()->points();
      m_edges = sc->model()->edges();
      m_boxes = sc->model()->boxes();
    } else {
      m_circles.clear();
      m_points.clear();
      m_edges.clear();
      m_boxes.clear();
    }
  }

  const int configGen = sc->config() ? sc->config()->generation() : 0;
  if (configGen != m_knownConfigGeneration) {
    m_knownConfigGeneration = configGen;
    m_geometryDirty = true;
    if (sc->config()) {
      m_accentColor = sc->config()->color();
      m_strokeWidth = sc->config()->strokeWidth();
      m_scale = sc->config()->scale();
      m_labelOffset = sc->config()->labelOffset();
      m_canvasWidth = sc->config()->canvas()->width();
      m_canvasHeight = sc->config()->canvas()->height();
      m_font = sc->config()->font();
      m_boxWidth = sc->config()->box()->width();
      m_boxHeight = sc->config()->box()->height();
      m_boxPadding = sc->config()->box()->padding();
    }
  }
}

void SpellCircleRenderer::initializeResources(QCanvasPainter *p) {
  m_syphon->start(rhi());
}

void SpellCircleRenderer::drawScene(QCanvasPainter *p) {
  // All coordinates are in 0..canvasWidth / 0..canvasHeight scene space. The
  // caller is responsible for setting up any scale transform before calling
  // this.

  const float strokeWidth = static_cast<float>(m_strokeWidth * m_scale);
  const float boxWidth = static_cast<float>(m_boxWidth * m_scale);
  const float boxHeight = static_cast<float>(m_boxHeight * m_scale);
  const float boxPadding = static_cast<float>(m_boxPadding * m_scale);

  QFont scaledFont = m_font;
  scaledFont.setPointSizeF(m_font.pointSizeF() * m_scale);

  // ── Edges (drawn underneath everything) ──────────────────────────────────
  p->setStrokeStyle(m_accentColor);
  p->setLineWidth(strokeWidth);
  for (const auto &edge : m_edges) {
    p->beginPath();
    QCanvasPath line;
    line.moveTo(edge.x1, edge.y1);
    line.lineTo(edge.x2, edge.y2);
    p->addPath(line);
    p->stroke(line);
    p->closePath();
  }

  // ── Circles ──────────────────────────────────────────────────────────────
  for (const auto &circle : m_circles) {
    const float r = static_cast<float>(circle.radius);

    QCanvasPath circlePath;
    circlePath.circle(circle.x, circle.y, r);

    p->beginPath();
    p->addPath(circlePath);
    p->setStrokeStyle(m_accentColor);
    p->setLineWidth(strokeWidth);
    p->stroke(circlePath);
    if (circle.active) {
      p->setFillStyle(kCircleFill);
      p->fill(circlePath);
    }
    p->closePath();

    if (!circle.name.isEmpty()) {
      QPainterPath textPath;
      textPath.setCachingEnabled(true);
      textPath.addEllipse(circle.x - r, circle.y - r, r * 2.0f, r * 2.0f);

      m_textPathPainter.setPathOffset(circle.textStart);
      m_textPathPainter.setPerpendicularOffset(
          static_cast<float>(m_labelOffset * m_scale));
      m_textPathPainter.setFont(scaledFont);
      m_textPathPainter.setColor(m_accentColor);
      m_textPathPainter.paint(p, textPath, circle.name);
    }
  }

  // ── Boxes ──────────────────────────────────────────────────────────────────
  p->setFont(scaledFont);
  p->setTextAlign(QCanvasPainter::TextAlign::Left);
  p->setTextBaseline(QCanvasPainter::TextBaseline::Top);
  for (const auto &box : m_boxes) {
    const QRectF bounds = p->textBoundingBox(box.value, 0.0f, 0.0f);
    const float textW = static_cast<float>(bounds.width());
    const float bw = qMax(textW + boxPadding * 2.0f, boxWidth);
    const float bh = boxHeight;
    const float bx = box.x;
    const float by = box.y;

    QCanvasPath rect;
    QRectF boxGeom(bx, by, bw, bh);
    rect.rect(boxGeom);

    p->clearRect(boxGeom);
    p->beginPath();
    p->addPath(rect);
    if (box.active) {
      p->setFillStyle(m_accentColor);
      p->fill();
    }
    p->setStrokeStyle(m_accentColor);
    p->setLineWidth(strokeWidth);
    p->stroke();
    p->closePath();

    p->beginPath();
    p->setFillStyle(box.active ? kBoxActiveText : m_accentColor);
    p->fillText(box.value, bx + boxPadding, by + boxPadding);
    p->closePath();
  }
}

void SpellCircleRenderer::prePaint(QCanvasPainter *p) {
  if (!m_geometryDirty)
    return;
  m_geometryDirty = false;

  const float nw = static_cast<float>(m_canvasWidth);
  const float nh = static_cast<float>(m_canvasHeight);

  // ── Syphon canvas ─────────────────────────────────────────────────────────
  // Full native resolution for external publishing; updated at the same rate
  // as the display cache (once per scene update, not on every zoom/pan).
  // Reassigning m_canvas drops the old QCanvasOffscreenCanvas's reference to
  // its GPU texture (releasing it once unreferenced) and allocates a fresh
  // one at the new dimensions whenever canvasWidth/Height has changed.
  if (m_canvas.isNull() || m_allocatedCanvasWidth != m_canvasWidth ||
      m_allocatedCanvasHeight != m_canvasHeight) {
    m_canvas = p->createCanvas(QSize(m_canvasWidth, m_canvasHeight));
    m_allocatedCanvasWidth = m_canvasWidth;
    m_allocatedCanvasHeight = m_canvasHeight;
  }
  beginCanvasPainting(m_canvas);
  // p->clearRect(0.0f, 0.0f, nw, nh);
  drawScene(p);
  m_displayImage =
      p->addImage(m_canvas, QCanvasPainter::ImageFlag::GenerateMipmaps);
  endCanvasPainting();
}

void SpellCircleRenderer::paint(QCanvasPainter *p) {
  // Blit the pre-rasterised display cache. This is a GPU texture scale —
  // it costs almost nothing regardless of zoom level or frame rate.
  if (!m_displayImage.isNull())
    p->drawImage(m_displayImage, 0, 0, width(), height());
}

void SpellCircleRenderer::render(QRhiCommandBuffer *cb) {
  QCanvasPainterItemRenderer::render(cb);
  if (!m_canvas.isNull() && m_canvas.texture())
    m_syphon->publishFrame(m_canvas.texture(), cb, m_allocatedCanvasWidth,
                           m_allocatedCanvasHeight);
}
