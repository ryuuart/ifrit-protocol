/** @file
 * The structural prune: whether two descriptions are provably the same
 * node described twice, and the field pins that keep the comparison from
 * quietly losing a field.
 */

#include <sigilcore/comparable/Fields.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilworld/element/Node.h>

#include <optional>

namespace sigil::world {

namespace {

using motion::propEqual;

static_assert(core::kFieldCount<Transform> == 15,
              "Transform gained or lost a field — rule on it in "
              "transformEqual() below, then bump this count.");
bool transformEqual(const Transform& a, const Transform& b) {
  if (a.matrix.has_value() != b.matrix.has_value()) return false;
  if (a.matrix && *a.matrix != *b.matrix) return false;
  if (a.axis != b.axis) return false;
  return propEqual(a.translateX, b.translateX) &&
         propEqual(a.translateY, b.translateY) &&
         propEqual(a.translateZ, b.translateZ) &&
         propEqual(a.rotateX, b.rotateX) && propEqual(a.rotateY, b.rotateY) &&
         propEqual(a.rotateZ, b.rotateZ) && propEqual(a.scaleX, b.scaleX) &&
         propEqual(a.scaleY, b.scaleY) && propEqual(a.scaleZ, b.scaleZ) &&
         propEqual(a.originX, b.originX) && propEqual(a.originY, b.originY) &&
         propEqual(a.originZ, b.originZ) &&
         propEqual(a.axisDegrees, b.axisDegrees);
}

static_assert(core::kFieldCount<Spline3> == 3,
              "Spline3 gained or lost a field — rule on it in "
              "splineEqual() below, then bump this count.");
bool splineEqual(const Spline3& a, const Spline3& b) {
  return a.points == b.points && a.type == b.type && a.closed == b.closed;
}

static_assert(core::kFieldCount<Along> == 2,
              "Along gained or lost a field — rule on it in alongEqual() "
              "below, then bump this count.");
bool alongEqual(const std::optional<Along>& a, const std::optional<Along>& b) {
  if (a.has_value() != b.has_value()) return false;
  if (!a) return true;
  return splineEqual(a->spline, b->spline) &&
         propEqual(a->distance, b->distance);
}

static_assert(core::kFieldCount<Window> == 2,
              "Window gained or lost a field — rule on it in "
              "windowEqual() below, then bump this count.");
bool windowEqual(const std::optional<Window>& a,
                 const std::optional<Window>& b) {
  if (a.has_value() != b.has_value()) return false;
  if (!a) return true;
  return propEqual(a->head, b->head) && propEqual(a->span, b->span);
}

static_assert(core::kFieldCount<Emission> == 4,
              "Emission gained or lost a field — rule on it in "
              "emissionEqual() below, then bump this count.");
bool emissionEqual(const std::optional<Emission>& a,
                   const std::optional<Emission>& b) {
  if (a.has_value() != b.has_value()) return false;
  if (!a) return true;
  const auto dialEqual = [](const std::optional<motion::Animatable<float>>& x,
                            const std::optional<motion::Animatable<float>>& y) {
    if (x.has_value() != y.has_value()) return false;
    return !x || propEqual(*x, *y);
  };
  return dialEqual(a->intensity, b->intensity) && dialEqual(a->red, b->red) &&
         dialEqual(a->green, b->green) && dialEqual(a->blue, b->blue);
}

static_assert(core::kFieldCount<Camera> == 6,
              "Camera gained or lost a field — rule on it in "
              "cameraEqual() below, then bump this count.");
bool cameraEqual(const std::optional<Camera>& a,
                 const std::optional<Camera>& b) {
  if (a.has_value() != b.has_value()) return false;
  if (!a) return true;
  return a->eye == b->eye && a->target == b->target && a->up == b->up &&
         a->fovYDeg == b->fovYDeg && a->zNear == b->zNear && a->zFar == b->zFar;
}

static_assert(core::kFieldCount<SkyDials> == 6,
              "SkyDials gained or lost a field — rule on it in "
              "skyEqual() below, then bump this count.");
bool skyEqual(const std::optional<SkyDials>& a,
              const std::optional<SkyDials>& b) {
  if (a.has_value() != b.has_value()) return false;
  if (!a) return true;
  const auto dialEqual = [](const std::optional<motion::Animatable<float>>& x,
                            const std::optional<motion::Animatable<float>>& y) {
    if (x.has_value() != y.has_value()) return false;
    return !x || propEqual(*x, *y);
  };
  return dialEqual(a->diffuse, b->diffuse) &&
         dialEqual(a->specular, b->specular) &&
         dialEqual(a->roughnessBias, b->roughnessBias) &&
         dialEqual(a->crossfade, b->crossfade) &&
         dialEqual(a->backdrop, b->backdrop) &&
         dialEqual(a->backdropBlur, b->backdropBlur);
}

bool materialsEqual(const ElementNode& a, const ElementNode& b) {
  if (a.material.has_value() != b.material.has_value()) return false;
  if (a.material && !(*a.material == *b.material)) return false;
  if (a.slots.size() != b.slots.size()) return false;
  for (size_t i = 0; i < a.slots.size(); ++i)
    if (!(a.slots[i] == b.slots[i])) return false;
  return true;
}

}  // namespace

static_assert(core::kFieldCount<ElementNode> == 18,
              "A field of ElementNode appeared or vanished. Rule on it in "
              "propsEqual() below — participate, or a stated reason not to "
              "— then bump this count. A miss is silent: the node prunes, "
              "the host is never told the field moved, and the scene keeps "
              "drawing what the old value produced for as long as the node "
              "lives.");
bool propsEqual(const ElementNode& a, const ElementNode& b) {
  if (a.key != b.key) return false;
  if (!transformEqual(a.transform, b.transform)) return false;
  if (!alongEqual(a.along, b.along)) return false;
  if (!windowEqual(a.window, b.window)) return false;
  // The geometry slot's value type IS the node's kind, and the variant's
  // own equality compares both: two nodes holding different alternatives
  // are unequal, two holding the same one compare its contents. A
  // generator with no `==` compares equal only to its own copies, so a
  // node carrying one re-patches rather than pruning into a stale cook.
  if (!(a.geometry == b.geometry)) return false;
  if (!materialsEqual(a, b)) return false;
  if (a.tags != b.tags) return false;
  if (a.light != b.light) return false;
  if (!emissionEqual(a.emission, b.emission)) return false;
  // An environment is a plain value all the way down — the panorama
  // behind it is a shared handle, so comparing two is comparing two
  // pointers and a handful of floats.
  if (a.environment != b.environment) return false;
  if (!skyEqual(a.sky, b.sky)) return false;
  if (!cameraEqual(a.camera, b.camera)) return false;
  if (a.cachePolicy != b.cachePolicy) return false;
  if (a.nodeTransition.has_value() != b.nodeTransition.has_value())
    return false;
  if (a.nodeTransition &&
      !motion::transitionEqual(*a.nodeTransition, *b.nodeTransition))
    return false;
  // A cascade that changed is a different mount order for whatever
  // arrives next, so the node must be told rather than pruned.
  if (a.childStagger.has_value() != b.childStagger.has_value()) return false;
  if (a.childStagger && !(*a.childStagger == *b.childStagger)) return false;
  // `memo` is compared earlier and more strictly by the reconciler, and
  // `children` are reconciled by key rather than compared.
  return true;
}

}  // namespace sigil::world
