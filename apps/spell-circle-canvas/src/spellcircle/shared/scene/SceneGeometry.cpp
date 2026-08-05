#include "SceneGeometry.h"

#include <cmath>
#include <numbers>

namespace spellcircle {

float ringFractionFromTwelve(float fraction) {
  // The drawn contour starts at 3 o'clock (SkPath circles begin there and
  // wind clockwise in screen space), which sits three quarters of a turn
  // clockwise from 12 o'clock — hence the +0.75.
  float percent = std::fmod(fraction + 0.75f, 1.0f);
  if (percent < 0.0f) percent += 1.0f;
  return percent;
}

namespace {

/**
 * Resolves the canvas coordinates of a point placed at fractional `position`
 * around a circle's perimeter.
 *
 * `position` is a fraction of one full turn, measured CLOCKWISE FROM 12
 * O'CLOCK: 0 is straight up, 0.25 is 3 o'clock, 0.5 is straight down. Values
 * outside [0, 1) wrap, negatives included. ringFractionFromTwelve converts
 * that authoring convention into the trigonometric one used below, where
 * angle 0 points along +x, at 3 o'clock.
 */
Vec2 pointAtPosition(Vec2 center, float radius, float position) {
  const float percent = ringFractionFromTwelve(position);
  const float angle = percent * 2.0f * std::numbers::pi_v<float>;
  // Screen coordinates are y-down, so increasing angle sweeps clockwise.
  return Vec2{center.x + radius * std::cos(angle),
              center.y + radius * std::sin(angle)};
}

// A radius-0 circle is an invisible anchor: it is never drawn (SceneRenderer
// skips it — no stroke, fill, or ring label), and a Point, Box, or Edge
// attached to one resolves straight to the circle's own coordinates instead of
// walking a degenerate perimeter, since `position` has no meaningful angle
// around a zero-radius circle. This is what lets a scene place a point or box
// at an arbitrary (x, y) rather than only along a visible circle's edge.
bool isAnchorOnlyCircle(const CircleComponent& circle) {
  return circle.radius == 0;
}

}  // namespace

void ResolvedScene::clear() {
  circles.clear();
  edges.clear();
  boxes.clear();
  pointLabels.clear();
}

ResolvedScene resolveScene(const SceneDocument& document, float canvasWidth,
                           float canvasHeight) {
  ResolvedScene resolved;
  const entt::registry& registry = document.registry();

  // Centers scale per axis, but a circle has only one radius, so radii — and
  // therefore every point resolved on a perimeter — use the horizontal factor
  // alone. Circles stay round on a canvas whose aspect ratio differs from the
  // author's; only their centers stretch. A scene dimension of 0 means the
  // sender gave no author-space size, so that axis is left at 1:1.
  const float horizontalScale =
      document.sceneWidth() > 0.0f ? canvasWidth / document.sceneWidth() : 1.0f;
  const float verticalScale = document.sceneHeight() > 0.0f
                                  ? canvasHeight / document.sceneHeight()
                                  : 1.0f;
  const float radiusScale = horizontalScale;

  auto anchorPosition = [&](const PointComponent& point) -> Vec2 {
    const Vec2 center{point.circle.centerX * horizontalScale,
                      point.circle.centerY * verticalScale};
    if (isAnchorOnlyCircle(point.circle)) return center;
    return pointAtPosition(
        center, static_cast<float>(point.circle.radius) * radiusScale,
        point.position);
  };

  auto pointPosition = [&](entt::entity pointEntity) -> Vec2 {
    const auto* point = registry.valid(pointEntity)
                            ? registry.try_get<PointComponent>(pointEntity)
                            : nullptr;
    return point ? anchorPosition(*point) : Vec2{};
  };

  const auto circles = registry.view<CircleComponent>();
  for (const entt::entity circleEntity : circles) {
    const CircleComponent& circle = circles.get<CircleComponent>(circleEntity);
    resolved.circles.push_back(ResolvedCircle{
        .name = circle.name,
        .center = Vec2{circle.centerX * horizontalScale,
                       circle.centerY * verticalScale},
        .radius = static_cast<float>(circle.radius) * radiusScale,
        // The wire measures from 12 o'clock, matching Point.position; the
        // renderer's ring geometry measures along the drawn contour from
        // 3 o'clock. Converting here keeps both conventions out of every
        // consumer.
        .textStart = ringFractionFromTwelve(circle.textStart),
        .active = circle.active,
    });
  }

  const auto edges = registry.view<EdgeComponent>();
  for (const entt::entity edgeEntity : edges) {
    const EdgeComponent& edge = edges.get<EdgeComponent>(edgeEntity);
    resolved.edges.push_back(ResolvedEdge{
        .first = pointPosition(edge.first),
        .second = pointPosition(edge.second),
    });
  }

  // Boxes (and point labels, below) sit on the ray from the canvas center out
  // through their anchor point: the renderer pushes each one outward along the
  // unit vector resolved here, by a distance from the style. Resolving it here
  // is what keeps the canvas center out of the drawing code. An anchor exactly
  // on the center has no direction, so it gets straight up — (0, -1), y being
  // down.
  const Vec2 canvasCenter{canvasWidth / 2.0f, canvasHeight / 2.0f};
  auto directionFrom = [&](Vec2 anchor) -> Vec2 {
    const Vec2 direction{anchor.x - canvasCenter.x, anchor.y - canvasCenter.y};
    const float length = std::hypot(direction.x, direction.y);
    return length > 1e-6f ? Vec2{direction.x / length, direction.y / length}
                          : Vec2{0.0f, -1.0f};
  };

  const auto boxes = registry.view<BoxComponent>();
  for (const entt::entity boxEntity : boxes) {
    const BoxComponent& box = boxes.get<BoxComponent>(boxEntity);
    const Vec2 anchor = pointPosition(box.point);

    resolved.boxes.push_back(ResolvedBox{
        .value = box.value,
        .anchor = anchor,
        .direction = directionFrom(anchor),
        .active = box.active,
    });
  }

  // Every Point carrying a non-empty value gets its own label, whether or not
  // that same point is also an Edge endpoint or a Box's anchor — a sender can
  // use one to read out a point's live position as it travels around a circle.
  // These reuse ResolvedBox for its anchor/direction/value, but the renderer
  // draws text only, so the fill intensity is fixed at 0 rather than carried
  // from the wire.
  const auto points = registry.view<PointComponent>();
  for (const entt::entity pointEntity : points) {
    const PointComponent& point = points.get<PointComponent>(pointEntity);
    if (point.value.empty()) continue;
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

}  // namespace spellcircle
