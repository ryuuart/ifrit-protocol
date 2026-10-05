#pragma once

/** @file
 * @ingroup world-node
 * THE ONE NODE MODEL underneath every kind: the type-erased handle a
 * group's children are held as, and the description it carries — every
 * field of one node, whichever factory made it — with the comparison
 * that decides whether two descriptions are provably the same node
 * described twice.
 *
 * A typed node — a `Group`, a `Null`, a `Mesh`, a `Light`, a `Camera` —
 * IS one of these with the verbs its kind can use on top, so
 * reconciliation, equality and keys see one model whatever the kinds.
 * Nothing here is retained: an entity, a running motion and a cooked
 * resource all live on the other side of the seam, in the Scene.
 */

#include <sigilcore/cache/Policy.h>
#include <sigilcore/reconcile/Memo.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilmaterial/core/Backface.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Tween.h>
#include <sigilworld/advanced/Geometry.h>
#include <sigilworld/advanced/Light.h>
#include <sigilworld/node/Transform.h>

#include <any>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sigil::world {

struct NodeDescription;

/** ONE NODE, its kind erased: a copy-on-write handle on a description.
 *  Every typed node converts to one, which is what lets a group hold a
 *  mesh, a light and another group in one list. */
class Node {
 public:
  /** A fresh node describing nothing: a placement and no more. */
  Node();
  /** Wraps a description the reconciler already holds. */
  explicit Node(std::shared_ptr<NodeDescription> description);

  /** The description this value carries, for the reconciler and a host
   *  reading one. */
  [[nodiscard]] const std::shared_ptr<NodeDescription>& node() const {
    return m_node;
  }
  /** The description, made this value's own before it is written: a
   *  fluent mutation never alters another copy or a description a Scene
   *  retains. */
  NodeDescription& edit();

 private:
  std::shared_ptr<NodeDescription> m_node;
};

/** A node's ride along a curve: the spline, and how far along it the
 *  node stands. */
struct Along {
  geometry::mesh::curve::Spline3 spline;
  motion::Animatable<float> distance{0.0f};
};

/** What the tone curve is that compresses a lit sum onto what a display
 *  can hold. It runs last, after the exposure, on linear light, and its
 *  answer is what is encoded for the display. */
enum class ToneCurve : uint8_t {
  /** Reinhard's operator over luminance: each colour divided by one plus
   *  its luminance. Never reaches white, and halves a white surface lit to
   *  show its own colour — a soft, low-contrast reading. */
  Reinhard,
  /** The neutral curve: below a shoulder near 0.8 it changes nothing but
   *  a small offset of four hundredths, eased to nothing toward black, so
   *  a surface lit to show its own colour shows very nearly that colour
   *  and a page reads as painted; above it a smooth compression brings any
   *  brightness under one, desaturating a highlight only as much as its
   *  brightness needs. */
  Neutral,
  /** No curve: the exposed radiance, cut off at one by the target. */
  Linear,
};

/** WHICH WAY A BODY TURNS TO FACE THE CAMERA, whatever its node's own
 *  rotation says — the billboard a sprite, a flame or a glint is drawn on.
 *  The body keeps where it stands and its scale; its own +z is turned
 *  toward the camera. Instanced geometry turns each instance about where
 *  it stands, keeping that instance's own scale. */
enum class Facing : uint8_t {
  /** As placed: the node's rotation stands. */
  None,
  /** Square on to the camera: its x along the picture's right, its y up
   *  the picture. */
  Camera,
  /** Turned toward the camera about the world's up axis only, standing
   *  upright — a tree, a flame or a column of smoke that must not lean
   *  back as the camera rises over it. */
  CameraUpright,
};

/** WHAT A CAMERA NODE DESCRIBES: Geometry's lens, and how the picture it
 *  takes is read — the exposure a radiance is multiplied by before the
 *  tone curve, and the curve. */
struct CameraDescription {
  geometry::mesh::camera::Camera lens;
  motion::Animatable<float> exposure{1.0f};
  ToneCurve toneCurve = ToneCurve::Reinhard;
};

/** A deferred describe and its key — SigilCore's, over this library's
 *  node. */
using Memo = core::Memo<Node>;

/** ONE NODE'S DESCRIPTION, field by field. Which factory made the node
 *  decides which fields it ever writes: only a group has children, only
 *  a mesh a geometry slot and a surface, only a light an emitter, only a
 *  camera a lens, and only a light or a camera a node it looks at. A
 *  null writes none of them. */
struct NodeDescription {
  /** What children are matched on; empty means positional. */
  std::string key;
  Transform transform;
  std::optional<Along> along;
  /** Whether the node and everything under it is in the scene at all. */
  bool visible = true;
  /** The key of the node a light or a camera is aimed at, resolved each
   *  frame against where that node stands; empty for none. */
  std::string lookAt;
  Geometry geometry;
  material::Backface backface = material::Backface::Hidden;
  /** Whether a body turns to face the camera. */
  Facing facing = Facing::None;
  /** The one surface. */
  std::optional<::sigil::material::Material> material;
  /** …or the per-face slots, in slot order. A node carries one form or
   *  the other, never both. */
  std::vector<::sigil::material::Material> slots;
  /** The collections the node joined. */
  std::vector<std::string> collections;
  std::optional<LightDescription> light;
  std::optional<CameraDescription> camera;
  core::Cache cachePolicy = core::Cache::Auto;
  std::optional<motion::Tween<float>> nodeTransition;
  std::vector<Node> children;
  std::optional<Memo> memo;
};

/** THE STRUCTURAL PRUNE: are @p a and @p b provably the same node
 *  described twice? Every field of NodeDescription is ruled on, and
 *  anything that cannot be compared answers false.
 *  @trap Two fields are excluded and both are compared elsewhere: the
 *  memo, which the reconciler compares earlier and more strictly, and
 *  the children, which are reconciled by key. */
bool propertiesEqual(const NodeDescription& a, const NodeDescription& b);

namespace detail {
Node makeMemo(std::any properties,
              std::function<bool(const std::any&, const std::any&)> equal,
              std::function<Node(const std::any&)> invoke);
}  // namespace detail

/** Deferred description: @p fn runs only when @p properties changed (by
 *  `operator==`) since the last render at this position or key, AND the
 *  ambient `environment::` bindings are unchanged — a memo is a pure
 *  function of (properties, environment). The captured stack is
 *  re-established around the deferred call, so
 *  `environment::inherited<T>()` inside @p fn reads what was bound where
 *  the memo was WRITTEN. */
template <class P, class F>
  requires std::equality_comparable<P> && std::invocable<F, const P&>
Node memo(P properties, F fn) {
  return detail::makeMemo(
      std::any(std::move(properties)),
      [](const std::any& a, const std::any& b) {
        return std::any_cast<const P&>(a) == std::any_cast<const P&>(b);
      },
      [fn = std::move(fn)](const std::any& p) -> Node {
        return Node(fn(std::any_cast<const P&>(p)));
      });
}

}  // namespace sigil::world
