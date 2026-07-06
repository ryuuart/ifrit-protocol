#include "SpellCircleRenderer.h"
#include "SpellCircle.h"
#include "SyphonBridge.h"
#include <QCanvasPath>
#include <QColor>
#include <QHash>
#include <QPainterPath>
#include <cmath>
#include <entt/entt.hpp>
#include <spdlog/spdlog.h>

namespace {
// Identifies a Point's embedded circle by its raw (unscaled) values so that
// points sharing the same anchor circle (very common — e.g. a dozen points
// ringing the same big circle) resolve against one cached QPainterPath
// instead of rebuilding + re-flattening an identical ellipse per point.
struct CircleKey {
  float x = 0.0f;
  float y = 0.0f;
  uint32_t radius = 0;

  bool operator==(const CircleKey &other) const {
    return x == other.x && y == other.y && radius == other.radius;
  }
};

size_t qHash(const CircleKey &key, size_t seed = 0) {
  return qHashMulti(seed, key.x, key.y, key.radius);
}

/** Builds a native-scaled ellipse path for `circle`, with caching enabled so
 *  repeated pointAtPercent() calls (one per point anchored to it) reuse
 *  QPainterPath's internal flattened-length cache instead of recomputing it
 *  each time. */
QPainterPath circleEllipsePath(const CircleComponent &circle, float sx,
                               float sy, float s) {
  const float cx = circle.x * sx;
  const float cy = circle.y * sy;
  const float r = static_cast<float>(circle.radius) * s;

  QPainterPath path;
  path.setCachingEnabled(true);
  path.addEllipse(cx - r, cy - r, r * 2.0, r * 2.0);
  return path;
}

/**
 * Resolves a point placed at fractional `position` around `circlePath`'s
 * perimeter (clockwise from 12 o'clock) by querying the path rather than
 * doing the trigonometry by hand.
 */
QPointF pointAtPosition(const QPainterPath &circlePath, float position) {
  // QPainterPath's percent parameter starts at 3 o'clock and runs clockwise;
  // `position` is measured clockwise from 12 o'clock, a quarter-turn ahead.
  float percent = std::fmod(position + 0.75f, 1.0f);
  if (percent < 0.0f)
    percent += 1.0f;
  return circlePath.pointAtPercent(percent);
}

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

  bool needsResolve = false;

  const int modelGen = sc->model() ? sc->model()->generation() : 0;
  if (modelGen != m_knownModelGeneration) {
    m_knownModelGeneration = modelGen;
    m_geometryDirty = true;
    needsResolve = true;
  }

  const int configGen = sc->config() ? sc->config()->generation() : 0;
  if (configGen != m_knownConfigGeneration) {
    m_knownConfigGeneration = configGen;
    m_geometryDirty = true;
    needsResolve = true;
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
      m_boxDistance = sc->config()->box()->distance();
    }
  }

  // Resolved positions depend on both the scene (model) and the native
  // canvas size (config), so either changing requires re-resolving.
  if (needsResolve)
    resolveGeometry(sc->model());
}

void SpellCircleRenderer::resolveGeometry(SpellCircleModel *model) {
  m_circles.clear();
  m_edges.clear();
  m_boxes.clear();
  if (!model)
    return;

  const entt::registry &registry = model->registry();

  // Scenes may be authored in a smaller coordinate space and scaled up to
  // the native canvas; width/height of 0 mean coordinates are already
  // native. Radius/point math assumes a uniform scale, matching sx.
  const float sx = model->sceneWidth() > 0.0f
                       ? static_cast<float>(m_canvasWidth) / model->sceneWidth()
                       : 1.0f;
  const float sy =
      model->sceneHeight() > 0.0f
          ? static_cast<float>(m_canvasHeight) / model->sceneHeight()
          : 1.0f;
  const float s = sx;

  // Many points typically anchor to the same circle (e.g. a ring of points
  // around one big circle), so cache each circle's ellipse path by its raw
  // values rather than rebuilding it per point.
  QHash<CircleKey, QPainterPath> circlePaths;
  auto pathForCircle =
      [&](const CircleComponent &circle) -> const QPainterPath & {
    const CircleKey key{circle.x, circle.y, circle.radius};
    auto it = circlePaths.find(key);
    if (it == circlePaths.end())
      it = circlePaths.insert(key, circleEllipsePath(circle, sx, sy, s));
    return it.value();
  };

  auto pointPosition = [&](entt::entity pointEntity) -> QPointF {
    const auto *point = registry.valid(pointEntity)
                            ? registry.try_get<PointComponent>(pointEntity)
                            : nullptr;
    if (!point)
      return {};
    return pointAtPosition(pathForCircle(point->circle), point->position);
  };

  for (auto [entity, circle] : registry.view<CircleComponent>().each()) {
    m_circles.append(ResolvedCircle{
        .name = circle.name,
        .center = QPointF(circle.x * sx, circle.y * sy),
        .radius = static_cast<float>(circle.radius) * s,
        .textStart = circle.textStart,
        .active = circle.active,
    });
  }

  for (auto [entity, edge] : registry.view<EdgeComponent>().each()) {
    m_edges.append(ResolvedEdge{
        .first = pointPosition(edge.first),
        .second = pointPosition(edge.second),
    });
  }

  // Boxes face the canvas center: each is anchored along the ray from the
  // center through its assigned point, so the direction of that ray is
  // resolved once here rather than recomputed every frame in drawScene().
  const QPointF canvasCenter(m_canvasWidth / 2.0f, m_canvasHeight / 2.0f);

  for (auto [entity, box] : registry.view<BoxComponent>().each()) {
    const QPointF anchor = pointPosition(box.point);
    QPointF direction = anchor - canvasCenter;
    const double length = std::hypot(direction.x(), direction.y());
    direction = length > 1e-6 ? direction / length : QPointF(0.0, -1.0);

    m_boxes.append(ResolvedBox{
        .value = box.value,
        .anchor = anchor,
        .direction = direction,
        .active = box.active,
    });
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
  const float boxDistance = static_cast<float>(m_boxDistance * m_scale);

  QFont scaledFont = m_font;
  scaledFont.setPointSizeF(m_font.pointSizeF() * m_scale);

  // ── Edges (drawn underneath everything) ──────────────────────────────────
  p->setStrokeStyle(m_accentColor);
  p->setLineWidth(strokeWidth);
  for (const auto &edge : m_edges) {
    p->beginPath();
    QCanvasPath line;
    line.moveTo(edge.first.x(), edge.first.y());
    line.lineTo(edge.second.x(), edge.second.y());
    p->addPath(line);
    p->stroke(line);
    p->closePath();
  }

  // ── Circles ──────────────────────────────────────────────────────────────
  for (const auto &circle : m_circles) {
    const float r = circle.radius;
    const float cx = static_cast<float>(circle.center.x());
    const float cy = static_cast<float>(circle.center.y());

    QCanvasPath circlePath;
    circlePath.circle(cx, cy, r);

    p->beginPath();
    p->addPath(circlePath);
    p->setStrokeStyle(m_accentColor);
    p->setLineWidth(strokeWidth);
    p->stroke(circlePath);
    if (circle.active) {
      p->setFillStyle(m_accentColor);
      p->fill(circlePath);
    }
    p->closePath();

    if (!circle.name.isEmpty()) {
      QPainterPath textPath;
      textPath.setCachingEnabled(true);
      textPath.addEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

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

    // The box's center sits on the ray from the canvas center through the
    // point, pushed outward by boxDistance beyond the point — nothing more.
    // An earlier version instead aligned whichever box edge the ray leans on
    // more (picking a horizontal or vertical edge, or interpolating between
    // them via ray/rectangle intersection near the corner), but that made the
    // box's touching edge slide around its own perimeter as the point moved,
    // producing a visible extra wobble on top of the translation whenever the
    // ray swept past a corner. Anchoring just the center avoids that: the box
    // rigidly translates along the ray with no edge-dependent correction.
    const QPointF boxCenter = box.anchor + box.direction * boxDistance;
    const float hw = bw / 2.0f;
    const float hh = bh / 2.0f;
    const float bx = static_cast<float>(boxCenter.x()) - hw;
    const float by = static_cast<float>(boxCenter.y()) - hh;

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
    if (box.active) {
      p->setGlobalCompositeOperation(
          QCanvasPainter::CompositeOperation::DestinationOut);
      p->setFillStyle(QColorConstants::Black);
      p->fillText(box.value, bx + boxPadding, by + boxPadding);
      p->setGlobalCompositeOperation(
          QCanvasPainter::CompositeOperation::SourceOver);
    } else {
      p->setFillStyle(m_accentColor);
      p->fillText(box.value, bx + boxPadding, by + boxPadding);
    }
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
  drawScene(p);
  m_displayImage =
      p->addImage(m_canvas, QCanvasPainter::ImageFlag::GenerateMipmaps);
  endCanvasPainting();
}

void SpellCircleRenderer::paint(QCanvasPainter *p) {
  if (!m_displayImage.isNull())
    p->drawImage(m_displayImage, 0, 0, width(), height());
}

void SpellCircleRenderer::render(QRhiCommandBuffer *cb) {
  QCanvasPainterItemRenderer::render(cb);
  if (!m_canvas.isNull() && m_canvas.texture())
    m_syphon->publishFrame(m_canvas.texture(), cb, m_allocatedCanvasWidth,
                           m_allocatedCanvasHeight);
}
