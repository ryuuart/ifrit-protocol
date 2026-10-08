#pragma once

/** @file
 * @ingroup world-node
 * The verb families the node kinds share, stated once: where a node
 * stands and whether it is there, what it is called and which
 * collections it joins, and — for a body — what it is made of. A kind
 * inherits the families that mean something for it and adds its own.
 */

#include <sigilcore/cache/Policy.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilmaterial/core/Backface.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Tween.h>
#include <sigilworld/advanced/Node.h>

#include <algorithm>
#include <concepts>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::world {

/** WHERE A NODE STANDS, and whether it is there. Placement and
 *  visibility are the two things that flow down the tree: a child stands
 *  where its parent's placement puts it, and nothing under a hidden node
 *  is in the scene. */
template <class Derived>
class PlacementVerbs {
 public:
  /** The node's position — `translateX/Y/Z` in one call. */
  Derived& at(glm::vec3 position) {
    return translateX(position.x).translateY(position.y).translateZ(position.z);
  }
  /** The position along x, in scene units; 0 by default. */
  Derived& translateX(motion::Animatable<float> value) {
    lanes().translateX = std::move(value);
    return self();
  }
  /** The position along y, in scene units; 0 by default. */
  Derived& translateY(motion::Animatable<float> value) {
    lanes().translateY = std::move(value);
    return self();
  }
  /** The position along z, in scene units; 0 by default. */
  Derived& translateZ(motion::Animatable<float> value) {
    lanes().translateZ = std::move(value);
    return self();
  }
  /** The turn about x, in degrees; 0 by default. */
  Derived& rotateX(motion::Animatable<float> degrees) {
    lanes().rotateX = std::move(degrees);
    return self();
  }
  /** The turn about y, in degrees; 0 by default. */
  Derived& rotateY(motion::Animatable<float> degrees) {
    lanes().rotateY = std::move(degrees);
    return self();
  }
  /** The turn about z, in degrees; 0 by default. */
  Derived& rotateZ(motion::Animatable<float> degrees) {
    lanes().rotateZ = std::move(degrees);
    return self();
  }
  /** A turn about a direction the three axis lanes cannot spell. It
   *  applies after them. */
  Derived& rotate(glm::vec3 axis, motion::Animatable<float> degrees) {
    lanes().axis = axis;
    lanes().axisDegrees = std::move(degrees);
    return self();
  }
  /** Uniform scale — writes all three scale lanes, so binding it binds
   *  all three to one output. */
  Derived& scale(motion::Animatable<float> factor) {
    lanes().scaleX = factor;
    lanes().scaleY = factor;
    lanes().scaleZ = std::move(factor);
    return self();
  }
  /** The scale along x, as a factor; 1 by default. */
  Derived& scaleX(motion::Animatable<float> factor) {
    lanes().scaleX = std::move(factor);
    return self();
  }
  /** The scale along y, as a factor; 1 by default. */
  Derived& scaleY(motion::Animatable<float> factor) {
    lanes().scaleY = std::move(factor);
    return self();
  }
  /** The scale along z, as a factor; 1 by default. */
  Derived& scaleZ(motion::Animatable<float> factor) {
    lanes().scaleZ = std::move(factor);
    return self();
  }
  /** The point the rotations and scales turn about, in the node's own
   *  coordinates. */
  Derived& transformOrigin(glm::vec3 origin) {
    lanes().originX = origin.x;
    lanes().originY = origin.y;
    lanes().originZ = origin.z;
    return self();
  }
  /** THE ESCAPE: a placement computed outside this vocabulary. A node
   *  carrying one ignores every lane above. */
  Derived& transform(const glm::mat4& matrix) {
    lanes().matrix = matrix;
    return self();
  }
  /** Ride a curve: the node stands at @p distance along @p spline,
   *  turned onto the curve's own moving frame. The distance is in the
   *  spline's units and it is a lane like any other, so a binding tows
   *  the node along.
   *  @trap It replaces the translation lanes and the axis turn; the
   *  rotation lanes, the scales and the origin still apply. */
  Derived& along(geometry::mesh::curve::Spline3 spline,
                 motion::Animatable<float> distance) {
    self().edit().along = Along{std::move(spline), std::move(distance)};
    return self();
  }
  /** Whether the node and everything under it is in the scene: a hidden
   *  body is not drawn, a hidden light lights nothing and a hidden
   *  camera is not looked through. Shown by default. */
  Derived& visible(bool shown) {
    self().edit().visible = shown;
    return self();
  }

  // ---- integer-literal sugar --------------------------------------------
  // `rotateY(-8)` — an int does not convert into the Animatable variant on
  // its own and the resulting error is unreadable. Constrained to
  // std::integral so a float call can never land here.
  /// @cond
  template <std::integral T>
  Derived& translateX(T v) {
    return translateX(motion::Animatable<float>((float)v));
  }
  template <std::integral T>
  Derived& translateY(T v) {
    return translateY(motion::Animatable<float>((float)v));
  }
  template <std::integral T>
  Derived& translateZ(T v) {
    return translateZ(motion::Animatable<float>((float)v));
  }
  template <std::integral T>
  Derived& rotateX(T degrees) {
    return rotateX(motion::Animatable<float>((float)degrees));
  }
  template <std::integral T>
  Derived& rotateY(T degrees) {
    return rotateY(motion::Animatable<float>((float)degrees));
  }
  template <std::integral T>
  Derived& rotateZ(T degrees) {
    return rotateZ(motion::Animatable<float>((float)degrees));
  }
  template <std::integral T>
  Derived& scale(T factor) {
    return scale(motion::Animatable<float>((float)factor));
  }
  template <std::integral T>
  Derived& scaleX(T factor) {
    return scaleX(motion::Animatable<float>((float)factor));
  }
  template <std::integral T>
  Derived& scaleY(T factor) {
    return scaleY(motion::Animatable<float>((float)factor));
  }
  template <std::integral T>
  Derived& scaleZ(T factor) {
    return scaleZ(motion::Animatable<float>((float)factor));
  }
  /// @endcond

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  Transform& lanes() { return self().edit().transform; }
};

/** WHAT A NODE IS CALLED and what it answers to: its key, the
 *  collections it joins, and what it asks of the cache and of a value
 *  that changes. */
template <class Derived>
class IdentityVerbs {
 public:
  /** The author-owned identity: what the reconciler matches a child by
   *  across describes, what a scene addresses the node by, and what a
   *  frame names a camera by. */
  Derived& key(std::string_view name) {
    self().edit().key.assign(name);
    return self();
  }
  /** Joins the collection called @p name — what a pass's `only`, a
   *  light's `lightLink` and a `Collection` term name. Repeated calls
   *  join more. */
  Derived& collection(std::string name) {
    self().edit().collections.push_back(std::move(name));
    return self();
  }
  /** What the author asked of this node's cache. */
  Derived& cache(core::Cache policy) {
    self().edit().cachePolicy = policy;
    return self();
  }
  /** The node's default transition, for the plain constants on it: a
   *  tween read for its timing, whose `from`, `to`, keyframes and repeat
   *  are not read. A duration alone is the common case:
   *  `.transition(320ms)`. */
  Derived& transition(const motion::Tween<float>& tween) {
    self().edit().nodeTransition = tween;
    return self();
  }
  Derived& transition(motion::Duration duration) {
    return transition(motion::Tween<float>{.duration = duration});
  }

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
};

/** @p material as the program a body is shaded with. A body is lit, so a
 *  material that states no response takes the stock one before it is
 *  lowered, rather than lowering to its base unlit. */
[[nodiscard]] material::Material worn(const material::Material& material);

/** The sources @p material reads of the unit a page paints it into — its
 *  relief, its brushing along the unit — by name; a body has no unit, so
 *  it is drawn without them and says which. */
[[nodiscard]] std::vector<std::string> unreadSources(
    const material::Material& material);

/** WHAT A BODY IS MADE OF: the surface every face is shaded by, and
 *  whether its reverse-wound faces are drawn. */
template <class Derived>
class SurfaceVerbs {
 public:
  /** The surface: a material's base is the base colour and its
   *  `surface({…})` the lit response, lowered once here into the surface
   *  program. A material that states no response is lit at the stock
   *  one, so `fill(colour)` lights; `.surface({.unlit = true})` is its
   *  own light; a bare program is drawn as the program it is. Effects
   *  read a 2D coverage a body has none of, so they are dropped and said
   *  once. */
  Derived& fill(material::Material surface) {
    NodeDescription& node = self().edit();
    node.material = worn(surface);
    node.unreadSources = unreadSources(surface);
    node.slots.clear();
    return self();
  }
  /** …and the per-face form: one material per slot, in slot order, each
   *  lowered as the one-material form is. */
  Derived& fill(std::span<const material::Material> slots) {
    NodeDescription& node = self().edit();
    node.slots.clear();
    node.slots.reserve(slots.size());
    node.unreadSources.clear();
    for (const material::Material& slot : slots) {
      node.slots.push_back(worn(slot));
      for (std::string& source : unreadSources(slot))
        if (std::find(node.unreadSources.begin(), node.unreadSources.end(),
                      source) == node.unreadSources.end())
          node.unreadSources.push_back(std::move(source));
    }
    node.material.reset();
    return self();
  }
  /** Whether reverse-wound faces are culled or drawn. A flat panel that
   *  must survive an orbit uses `backface(material::Backface::Visible)`;
   *  a closed solid keeps the default `Hidden`. */
  Derived& backface(material::Backface facing) {
    self().edit().backface = facing;
    return self();
  }

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
};

}  // namespace sigil::world
