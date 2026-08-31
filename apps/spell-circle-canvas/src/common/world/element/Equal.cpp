/** @file
 * The structural prune: whether two descriptions are provably the same
 * node described twice, and the field pins that keep the comparison from
 * quietly losing a field.
 */

#include <sigilcore/reconcile/Compare.h>
#include <sigilworld/element/Node.h>

#include <cstddef>
#include <optional>
#include <tuple>
#include <utility>

namespace sigil::world {

namespace detail {

// FIELD PINS. A comparator written by hand can leave a field out, and the
// failure is invisible by construction: two different values compare
// equal, the node prunes, and it keeps whatever the old value produced
// for as long as it lives. A structured binding names every direct
// non-static data member of a struct, and the count is a hard error the
// moment the struct changes — so each hand-written comparator sits beside
// a `static_assert(detail::kFieldCount<T> == N)`, and adding a field fails the
// build until someone rules on it in the comparator and bumps the count.
//
// The pin is kept here rather than reached for in SigilCore because two
// of the pinned types belong to libraries beneath this one, and a pin
// must be able to name them without a helper landing in their namespace.

inline auto fields(Transform& v) {
  auto& [tx, ty, tz, rx, ry, rz, sx, sy, sz, ox, oy, oz, axis, axisDeg,
         matrix] = v;
  return std::tie(tx, ty, tz, rx, ry, rz, sx, sy, sz, ox, oy, oz, axis, axisDeg,
                  matrix);
}
inline auto fields(Along& v) {
  auto& [spline, distance] = v;
  return std::tie(spline, distance);
}
inline auto fields(Window& v) {
  auto& [head, span] = v;
  return std::tie(head, span);
}
inline auto fields(Emission& v) {
  auto& [intensity, red, green, blue] = v;
  return std::tie(intensity, red, green, blue);
}
inline auto fields(Camera& v) {
  auto& [eye, target, up, fovYDeg, zNear, zFar] = v;
  return std::tie(eye, target, up, fovYDeg, zNear, zFar);
}
inline auto fields(Spline3& v) {
  auto& [points, type, closed] = v;
  return std::tie(points, type, closed);
}
inline auto fields(ElementNode& v) {
  auto& [key, transform, along, window, geometry, material, slots, tags, light,
         emission, camera, cachePolicy, nodeTransition, children, memo] = v;
  return std::tie(key, transform, along, window, geometry, material, slots,
                  tags, light, emission, camera, cachePolicy, nodeTransition,
                  children, memo);
}

/** How many direct non-static data members @p T has, as the pinned
 *  decomposition above sees them. */
template <class T>
inline constexpr std::size_t kFieldCount =
    std::tuple_size_v<decltype(fields(std::declval<T&>()))>;

}  // namespace detail

namespace {

using core::propEqual;

static_assert(detail::kFieldCount<Transform> == 15,
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

static_assert(detail::kFieldCount<Spline3> == 3,
              "Spline3 gained or lost a field — rule on it in "
              "splineEqual() below, then bump this count.");
bool splineEqual(const Spline3& a, const Spline3& b) {
  return a.points == b.points && a.type == b.type && a.closed == b.closed;
}

static_assert(detail::kFieldCount<Along> == 2,
              "Along gained or lost a field — rule on it in alongEqual() "
              "below, then bump this count.");
bool alongEqual(const std::optional<Along>& a, const std::optional<Along>& b) {
  if (a.has_value() != b.has_value()) return false;
  if (!a) return true;
  return splineEqual(a->spline, b->spline) &&
         propEqual(a->distance, b->distance);
}

static_assert(detail::kFieldCount<Window> == 2,
              "Window gained or lost a field — rule on it in "
              "windowEqual() below, then bump this count.");
bool windowEqual(const std::optional<Window>& a,
                 const std::optional<Window>& b) {
  if (a.has_value() != b.has_value()) return false;
  if (!a) return true;
  return propEqual(a->head, b->head) && propEqual(a->span, b->span);
}

static_assert(detail::kFieldCount<Emission> == 4,
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

static_assert(detail::kFieldCount<Camera> == 6,
              "Camera gained or lost a field — rule on it in "
              "cameraEqual() below, then bump this count.");
bool cameraEqual(const std::optional<Camera>& a,
                 const std::optional<Camera>& b) {
  if (a.has_value() != b.has_value()) return false;
  if (!a) return true;
  return a->eye == b->eye && a->target == b->target && a->up == b->up &&
         a->fovYDeg == b->fovYDeg && a->zNear == b->zNear && a->zFar == b->zFar;
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

static_assert(detail::kFieldCount<ElementNode> == 15,
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
  if (!cameraEqual(a.camera, b.camera)) return false;
  if (a.cachePolicy != b.cachePolicy) return false;
  if (a.nodeTransition.has_value() != b.nodeTransition.has_value())
    return false;
  if (a.nodeTransition &&
      !core::transitionEqual(*a.nodeTransition, *b.nodeTransition))
    return false;
  // `memo` is compared earlier and more strictly by the reconciler, and
  // `children` are reconciled by key rather than compared.
  return true;
}

}  // namespace sigil::world
