#include "SpellCircleRenderer.h"
#include "QCanvasPainterSceneBackend.h"
#include "SkiaSceneBackend.h"
#include "SpellCircle.h"
#include "TexturePublisher.h"
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
  float centerX = 0.0f;
  float centerY = 0.0f;
  uint32_t radius = 0;

  bool operator==(const CircleKey &other) const {
    return centerX == other.centerX && centerY == other.centerY &&
           radius == other.radius;
  }
};

size_t qHash(const CircleKey &key, size_t seed = 0) {
  return qHashMulti(seed, key.centerX, key.centerY, key.radius);
}

/** Builds a native-scaled ellipse path for `circle`, with caching enabled so
 *  repeated pointAtPercent() calls (one per point anchored to it) reuse
 *  QPainterPath's internal flattened-length cache instead of recomputing it
 *  each time. */
QPainterPath circleEllipsePath(const CircleComponent &circle,
                               float horizontalScale, float verticalScale,
                               float radiusScale) {
  const float centerX = circle.centerX * horizontalScale;
  const float centerY = circle.centerY * verticalScale;
  const float radius = static_cast<float>(circle.radius) * radiusScale;

  QPainterPath path;
  path.setCachingEnabled(true);
  path.addEllipse(centerX - radius, centerY - radius, radius * 2.0,
                  radius * 2.0);
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

SpellCircleRenderer::SpellCircleRenderer() {
  m_font.setBold(true);
  m_font.setPointSize(36);
  // addEllipse starts at 3 o'clock and goes CW, so 0.75 lands at 12 o'clock.
  m_curvedTextPainter.setPathOffset(0.75f);
}

SpellCircleRenderer::~SpellCircleRenderer() = default;

void SpellCircleRenderer::synchronize(QCanvasPainterItem *item) {
  auto *spellCircleItem = static_cast<SpellCircle *>(item);

  bool needsResolve = false;

  const int modelGeneration =
      spellCircleItem->model() ? spellCircleItem->model()->generation() : 0;
  if (modelGeneration != m_knownModelGeneration) {
    m_knownModelGeneration = modelGeneration;
    m_geometryDirty = true;
    needsResolve = true;
  }

  const int configGeneration =
      spellCircleItem->config() ? spellCircleItem->config()->generation() : 0;
  if (configGeneration != m_knownConfigGeneration) {
    m_knownConfigGeneration = configGeneration;
    m_geometryDirty = true;
    needsResolve = true;
    if (spellCircleItem->config()) {
      GraphicsConfig *config = spellCircleItem->config();
      m_accentColor = config->color();
      m_strokeWidth = config->strokeWidth();
      m_scale = config->scale();
      m_labelOffset = config->labelOffset();
      m_pointDistance = config->pointDistance();
      m_canvasWidth = config->canvas()->width();
      m_canvasHeight = config->canvas()->height();
      m_font = config->font();
      m_boxWidth = config->box()->width();
      m_boxHeight = config->box()->height();
      m_boxPadding = config->box()->padding();
      m_boxDistance = config->box()->distance();
    }
  }

  // Resolved positions depend on both the scene (model) and the native
  // canvas size (config), so either changing requires re-resolving.
  if (needsResolve)
    resolveGeometry(spellCircleItem->model());
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
  // native. Radius/point math assumes a uniform horizontal scale.
  const float horizontalScale =
      model->sceneWidth() > 0.0f
          ? static_cast<float>(m_canvasWidth) / model->sceneWidth()
          : 1.0f;
  const float verticalScale =
      model->sceneHeight() > 0.0f
          ? static_cast<float>(m_canvasHeight) / model->sceneHeight()
          : 1.0f;
  const float radiusScale = horizontalScale;

  // Many points typically anchor to the same circle (e.g. a ring of points
  // around one big circle), so cache each circle's ellipse path by its raw
  // values rather than rebuilding it per point.
  QHash<CircleKey, QPainterPath> circlePaths;
  auto pathForCircle =
      [&](const CircleComponent &circle) -> const QPainterPath & {
    const CircleKey key{circle.centerX, circle.centerY, circle.radius};
    auto pathEntry = circlePaths.find(key);
    if (pathEntry == circlePaths.end())
      pathEntry = circlePaths.insert(
          key, circleEllipsePath(circle, horizontalScale, verticalScale,
                                 radiusScale));
    return pathEntry.value();
  };

  auto pointPosition = [&](entt::entity pointEntity) -> QPointF {
    const auto *point = registry.valid(pointEntity)
                            ? registry.try_get<PointComponent>(pointEntity)
                            : nullptr;
    if (!point)
      return {};
    if (isAnchorOnlyCircle(point->circle))
      return QPointF(point->circle.centerX * horizontalScale,
                     point->circle.centerY * verticalScale);
    return pointAtPosition(pathForCircle(point->circle), point->position);
  };

  const auto circles = registry.view<CircleComponent>();
  for (const entt::entity circleEntity : circles) {
    const CircleComponent &circle = circles.get<CircleComponent>(circleEntity);
    m_circles.append(ResolvedCircle{
        .name = circle.name,
        .center = QPointF(circle.centerX * horizontalScale,
                          circle.centerY * verticalScale),
        .radius = static_cast<float>(circle.radius) * radiusScale,
        .textStart = circle.textStart,
        .active = circle.active,
    });
  }

  const auto edges = registry.view<EdgeComponent>();
  for (const entt::entity edgeEntity : edges) {
    const EdgeComponent &edge = edges.get<EdgeComponent>(edgeEntity);
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

  const auto boxes = registry.view<BoxComponent>();
  for (const entt::entity boxEntity : boxes) {
    const BoxComponent &box = boxes.get<BoxComponent>(boxEntity);
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
  const auto points = registry.view<PointComponent>();
  for (const entt::entity pointEntity : points) {
    const PointComponent &point = points.get<PointComponent>(pointEntity);
    if (point.value.isEmpty())
      continue;
    const QPointF anchor =
        isAnchorOnlyCircle(point.circle)
            ? QPointF(point.circle.centerX * horizontalScale,
                      point.circle.centerY * verticalScale)
            : pointAtPosition(pathForCircle(point.circle), point.position);

    m_pointLabels.append(ResolvedBox{
        .value = point.value,
        .anchor = anchor,
        .direction = directionFrom(anchor),
        .active = 0.0f,
    });
  }
}

void SpellCircleRenderer::initializeResources(QCanvasPainter *painter) {
  static_cast<void>(painter);
  // Both factories inspect the active QRhi backend themselves and return
  // null when they have no implementation for it, so this method stays
  // backend-agnostic: publishing is optional, drawing falls back to the
  // always-available QCanvasPainter backend.
  m_publisher = createTexturePublisher(rhi(), "SpellCircle");
  m_sceneBackend = createSkiaSceneBackend(rhi());
  if (!m_sceneBackend)
    m_sceneBackend = std::make_unique<QCanvasPainterSceneBackend>();
}

void SpellCircleRenderer::prePaint(QCanvasPainter *painter) {
  if (!m_geometryDirty)
    return;
  m_geometryDirty = false;

  // ── Syphon canvas ─────────────────────────────────────────────────────────
  // Full native resolution for external publishing; updated at the same rate
  // as the display cache (once per scene update, not on every zoom/pan).
  // Reassigning m_canvas drops the old QCanvasOffscreenCanvas's reference to
  // its GPU texture (releasing it once unreferenced) and allocates a fresh
  // one at the new dimensions whenever canvasWidth/Height has changed.
  if (m_canvas.isNull() || m_allocatedCanvasWidth != m_canvasWidth ||
      m_allocatedCanvasHeight != m_canvasHeight) {
    m_canvas = painter->createCanvas(QSize(m_canvasWidth, m_canvasHeight));
    m_allocatedCanvasWidth = m_canvasWidth;
    m_allocatedCanvasHeight = m_canvasHeight;
  }
  m_displayImage = m_sceneBackend->drawScene(
      *this, painter, m_canvas,
      QSize(m_allocatedCanvasWidth, m_allocatedCanvasHeight));
}

void SpellCircleRenderer::paint(QCanvasPainter *painter) {
  if (!m_displayImage.isNull())
    painter->drawImage(m_displayImage, 0, 0, width(), height());
}

void SpellCircleRenderer::render(QRhiCommandBuffer *commandBuffer) {
  QCanvasPainterItemRenderer::render(commandBuffer);
  if (m_publisher && !m_canvas.isNull() && m_canvas.texture())
    m_publisher->publishFrame(m_canvas.texture(), commandBuffer,
                              m_allocatedCanvasWidth, m_allocatedCanvasHeight);
}
