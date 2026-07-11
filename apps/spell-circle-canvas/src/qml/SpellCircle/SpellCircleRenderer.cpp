#include "SpellCircleRenderer.h"
#include "QCanvasPainterSceneBackend.h"
#include "SkiaSceneBackend.h"
#include "SpellCircle.h"
#include "SyphonBridge.h"
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

// A radius-0 circle is never drawn (see the skip in
// QCanvasPainterSceneBackend::drawScene()) and is used purely as an invisible
// anchor: `position` has no meaningful angle around a
// zero-radius perimeter, so a Point/Box/Edge anchored to one resolves
// straight to the circle's own coordinates rather than walking a degenerate
// ellipse path. This is what lets boxes/points be placed at an arbitrary
// (x, y) instead of only along a visible circle's edge.
bool isAnchorOnlyCircle(const CircleComponent &circle) {
  return circle.radius == 0;
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
      m_pointDistance = sc->config()->pointDistance();
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
  m_pointLabels.clear();
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
    if (isAnchorOnlyCircle(point->circle))
      return QPointF(point->circle.x * sx, point->circle.y * sy);
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

  // Boxes (and point labels, below) face the canvas center: each is anchored
  // along the ray from the center through its assigned point, so the
  // direction of that ray is resolved once here rather than recomputed every
  // frame in the scene backend's drawScene().
  const QPointF canvasCenter(m_canvasWidth / 2.0f, m_canvasHeight / 2.0f);
  auto directionFrom = [&](const QPointF &anchor) {
    QPointF direction = anchor - canvasCenter;
    const double length = std::hypot(direction.x(), direction.y());
    return length > 1e-6 ? direction / length : QPointF(0.0, -1.0);
  };

  for (auto [entity, box] : registry.view<BoxComponent>().each()) {
    const QPointF anchor = pointPosition(box.point);

    m_boxes.append(ResolvedBox{
        .value = box.value,
        .anchor = anchor,
        .direction = directionFrom(anchor),
        .active = box.active,
    });
  }

  // Every Point carrying a non-empty value gets its own label, regardless of
  // whether it's also an Edge endpoint or a Box's anchor — e.g. an animation
  // script tracking a point's live position as it moves around a circle.
  for (auto [entity, point] : registry.view<PointComponent>().each()) {
    if (point.value.isEmpty())
      continue;
    const QPointF anchor =
        isAnchorOnlyCircle(point.circle)
            ? QPointF(point.circle.x * sx, point.circle.y * sy)
            : pointAtPosition(pathForCircle(point.circle), point.position);

    m_pointLabels.append(ResolvedBox{
        .value = point.value,
        .anchor = anchor,
        .direction = directionFrom(anchor),
        .active = 0.0f,
    });
  }
}

void SpellCircleRenderer::initializeResources(QCanvasPainter *p) {
  m_syphon->start(rhi());
  m_sceneBackend = createSkiaSceneBackend(rhi());
  if (!m_sceneBackend)
    m_sceneBackend = std::make_unique<QCanvasPainterSceneBackend>();
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
  m_displayImage = m_sceneBackend->drawScene(
      *this, p, m_canvas, QSize(m_allocatedCanvasWidth, m_allocatedCanvasHeight));
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
