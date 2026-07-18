#include "SceneGeometry.h"

#include <cmath>
#include <numbers>

namespace spellcircle {

namespace {

/**
 * Resolves a point placed at fractional `position` around a circle's
 * perimeter, measured clockwise from 12 o'clock (matching the historical
 * QPainterPath::pointAtPercent behavior on an ellipse subpath, which starts
 * at 3 o'clock and runs clockwise — a quarter-turn ahead).
 */
Vec2 pointAtPosition(Vec2 center, float radius, float position) {
  float percent = std::fmod(position + 0.75f, 1.0f);
  if (percent < 0.0f)
    percent += 1.0f;
  const float angle = percent * 2.0f * std::numbers::pi_v<float>;
  // Screen coordinates are y-down, so increasing angle sweeps clockwise.
  return Vec2{center.x + radius * std::cos(angle),
              center.y + radius * std::sin(angle)};
}

// A radius-0 circle is never drawn (see the skip in SceneRenderer::draw())
// and is used purely as an invisible anchor: `position` has no meaningful
// angle around a zero-radius perimeter, so a Point/Box/Edge anchored to one
// resolves straight to the circle's own coordinates rather than walking a
// degenerate perimeter. This is what lets boxes/points be placed at an
// arbitrary (x, y) instead of only along a visible circle's edge.
bool isAnchorOnlyCircle(const CircleComponent &circle) {
  return circle.radius == 0;
}

} // namespace

void ResolvedScene::clear() {
  circles.clear();
  edges.clear();
  boxes.clear();
  pointLabels.clear();
}

ResolvedScene resolveScene(const SceneDocument &document, float canvasWidth,
                           float canvasHeight) {
  ResolvedScene resolved;
  const entt::registry &registry = document.registry();

  // Radius/point math assumes a uniform horizontal scale.
  const float horizontalScale = document.sceneWidth() > 0.0f
                                    ? canvasWidth / document.sceneWidth()
                                    : 1.0f;
  const float verticalScale = document.sceneHeight() > 0.0f
                                  ? canvasHeight / document.sceneHeight()
                                  : 1.0f;
  const float radiusScale = horizontalScale;

  auto anchorPosition = [&](const PointComponent &point) -> Vec2 {
    const Vec2 center{point.circle.centerX * horizontalScale,
                      point.circle.centerY * verticalScale};
    if (isAnchorOnlyCircle(point.circle))
      return center;
    return pointAtPosition(
        center, static_cast<float>(point.circle.radius) * radiusScale,
        point.position);
  };

  auto pointPosition = [&](entt::entity pointEntity) -> Vec2 {
    const auto *point = registry.valid(pointEntity)
                            ? registry.try_get<PointComponent>(pointEntity)
                            : nullptr;
    return point ? anchorPosition(*point) : Vec2{};
  };

  const auto circles = registry.view<CircleComponent>();
  for (const entt::entity circleEntity : circles) {
    const CircleComponent &circle = circles.get<CircleComponent>(circleEntity);
    resolved.circles.push_back(ResolvedCircle{
        .name = circle.name,
        .center = Vec2{circle.centerX * horizontalScale,
                       circle.centerY * verticalScale},
        .radius = static_cast<float>(circle.radius) * radiusScale,
        .textStart = circle.textStart,
        .active = circle.active,
    });
  }

  const auto edges = registry.view<EdgeComponent>();
  for (const entt::entity edgeEntity : edges) {
    const EdgeComponent &edge = edges.get<EdgeComponent>(edgeEntity);
    resolved.edges.push_back(ResolvedEdge{
        .first = pointPosition(edge.first),
        .second = pointPosition(edge.second),
    });
  }

  // Boxes (and point labels, below) face the canvas center: each is anchored
  // along the ray from the center through its assigned point, so the
  // direction of that ray is resolved once here rather than recomputed every
  // frame at draw time.
  const Vec2 canvasCenter{canvasWidth / 2.0f, canvasHeight / 2.0f};
  auto directionFrom = [&](Vec2 anchor) -> Vec2 {
    const Vec2 direction{anchor.x - canvasCenter.x, anchor.y - canvasCenter.y};
    const float length = std::hypot(direction.x, direction.y);
    return length > 1e-6f ? Vec2{direction.x / length, direction.y / length}
                          : Vec2{0.0f, -1.0f};
  };

  const auto boxes = registry.view<BoxComponent>();
  for (const entt::entity boxEntity : boxes) {
    const BoxComponent &box = boxes.get<BoxComponent>(boxEntity);
    const Vec2 anchor = pointPosition(box.point);

    resolved.boxes.push_back(ResolvedBox{
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
    if (point.value.empty())
      continue;
    const Vec2 anchor = anchorPosition(point);

    resolved.pointLabels.push_back(ResolvedBox{
        .value = point.value,
        .anchor = anchor,
        .direction = directionFrom(anchor),
        .active = 0.0f,
    });
  }

  return resolved;
}

} // namespace spellcircle
