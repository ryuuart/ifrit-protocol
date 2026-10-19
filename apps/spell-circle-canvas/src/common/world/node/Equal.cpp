/** @file
 * The structural prune: whether two descriptions are provably the same
 * node described twice, and the field pins that keep the comparison from
 * quietly losing a field.
 */

#include <sigilcore/comparable/Fields.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilworld/advanced/Light.h>
#include <sigilworld/advanced/Node.h>

#include <optional>

namespace sigil::world {

namespace {

using motion::propertyEqual;

static_assert(core::kFieldCount<Transform> == 15,
              "Transform gained or lost a field — rule on it in "
              "transformEqual() below, then bump this count.");
bool transformEqual(const Transform& a, const Transform& b) {
  if (a.matrix.has_value() != b.matrix.has_value()) return false;
  if (a.matrix && *a.matrix != *b.matrix) return false;
  if (a.axis != b.axis) return false;
  return propertyEqual(a.translateX, b.translateX) &&
         propertyEqual(a.translateY, b.translateY) &&
         propertyEqual(a.translateZ, b.translateZ) &&
         propertyEqual(a.rotateX, b.rotateX) &&
         propertyEqual(a.rotateY, b.rotateY) &&
         propertyEqual(a.rotateZ, b.rotateZ) &&
         propertyEqual(a.scaleX, b.scaleX) &&
         propertyEqual(a.scaleY, b.scaleY) &&
         propertyEqual(a.scaleZ, b.scaleZ) &&
         propertyEqual(a.originX, b.originX) &&
         propertyEqual(a.originY, b.originY) &&
         propertyEqual(a.originZ, b.originZ) &&
         propertyEqual(a.axisDegrees, b.axisDegrees);
}

static_assert(core::kFieldCount<geometry::mesh::curve::Spline3> == 3,
              "Spline3 gained or lost a field — rule on it in "
              "splineEqual() below, then bump this count.");
bool splineEqual(const geometry::mesh::curve::Spline3& a,
                 const geometry::mesh::curve::Spline3& b) {
  return a.points == b.points && a.type == b.type && a.closed == b.closed;
}

static_assert(core::kFieldCount<Along> == 2,
              "Along gained or lost a field — rule on it in alongEqual() "
              "below, then bump this count.");
bool alongEqual(const std::optional<Along>& a, const std::optional<Along>& b) {
  if (a.has_value() != b.has_value()) return false;
  if (!a) return true;
  return splineEqual(a->spline, b->spline) &&
         propertyEqual(a->distance, b->distance);
}

static_assert(core::kFieldCount<geometry::mesh::camera::Camera> == 6,
              "Camera gained or lost a field — rule on it in "
              "cameraEqual() below, then bump this count.");
bool lensEqual(const geometry::mesh::camera::Camera& a,
               const geometry::mesh::camera::Camera& b) {
  return a.eye == b.eye && a.target == b.target && a.up == b.up &&
         a.fovYDeg == b.fovYDeg && a.zNear == b.zNear && a.zFar == b.zFar;
}

static_assert(core::kFieldCount<CameraDescription> == 3,
              "CameraDescription gained or lost a field — rule on it in "
              "cameraEqual() below, then bump this count.");
bool cameraEqual(const std::optional<CameraDescription>& a,
                 const std::optional<CameraDescription>& b) {
  if (a.has_value() != b.has_value()) return false;
  if (!a) return true;
  return lensEqual(a->lens, b->lens) &&
         propertyEqual(a->exposure, b->exposure) &&
         a->toneCurve == b->toneCurve;
}

static_assert(core::kFieldCount<LightDescription> == 15,
              "LightDescription gained or lost a field — rule on it in "
              "lightEqual() below, then bump this count.");
bool lightEqual(const std::optional<LightDescription>& a,
                const std::optional<LightDescription>& b) {
  if (a.has_value() != b.has_value()) return false;
  if (!a) return true;
  return a->kind == b->kind && a->direction == b->direction &&
         propertyEqual(a->color, b->color) &&
         propertyEqual(a->intensity, b->intensity) &&
         propertyEqual(a->groundColor, b->groundColor) &&
         a->range == b->range && a->innerAngle == b->innerAngle &&
         a->outerAngle == b->outerAngle && a->width == b->width &&
         a->height == b->height && a->radius == b->radius &&
         a->lightLink == b->lightLink && a->shadowLink == b->shadowLink &&
         a->castShadow == b->castShadow && a->shadow == b->shadow;
}

bool materialsEqual(const NodeDescription& a, const NodeDescription& b) {
  if (a.material.has_value() != b.material.has_value()) return false;
  if (a.material && !(*a.material == *b.material)) return false;
  if (a.slots.size() != b.slots.size()) return false;
  for (size_t i = 0; i < a.slots.size(); ++i)
    if (!(a.slots[i] == b.slots[i])) return false;
  return a.unreadSources == b.unreadSources;
}

}  // namespace

static_assert(
    core::kFieldCount<NodeDescription> == 22,
    "A field of NodeDescription appeared or vanished. Rule on it in "
    "propertiesEqual() below — participate, or a stated reason not to "
    "— then bump this count. A miss is silent: the node prunes, "
    "the host is never told the field moved, and the scene keeps "
    "drawing what the old value produced for as long as the node "
    "lives.");
bool propertiesEqual(const NodeDescription& a, const NodeDescription& b) {
  if (a.key != b.key) return false;
  if (!transformEqual(a.transform, b.transform)) return false;
  if (!alongEqual(a.along, b.along)) return false;
  if (a.visible != b.visible) return false;
  if (a.lookAt != b.lookAt) return false;
  if (a.forward != b.forward) return false;
  // The geometry slot's value type IS the node's kind, and the variant's
  // own equality compares both: two nodes holding different alternatives
  // are unequal, two holding the same one compare its contents. A
  // generator with no `==` compares equal only to its own copies, so a
  // node carrying one re-patches rather than pruning into a stale cook.
  if (!(a.geometry == b.geometry)) return false;
  if (a.backface != b.backface) return false;
  if (a.primitiveColor != b.primitiveColor) return false;
  if (a.facing != b.facing) return false;
  if (a.castShadow != b.castShadow) return false;
  if (a.receiveShadow != b.receiveShadow) return false;
  if (!materialsEqual(a, b)) return false;
  if (a.collections != b.collections) return false;
  if (!lightEqual(a.light, b.light)) return false;
  if (!cameraEqual(a.camera, b.camera)) return false;
  if (a.cachePolicy != b.cachePolicy) return false;
  if (a.nodeTransition.has_value() != b.nodeTransition.has_value())
    return false;
  if (a.nodeTransition &&
      !motion::tweenEqual(*a.nodeTransition, *b.nodeTransition))
    return false;
  // `memo` is compared earlier and more strictly by the reconciler, and
  // `children` are reconciled by key rather than compared.
  return true;
}

}  // namespace sigil::world
