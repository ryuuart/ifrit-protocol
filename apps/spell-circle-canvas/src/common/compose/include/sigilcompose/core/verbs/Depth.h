#pragma once

/** @file
 * @ingroup compose-core
 *
 * The CSS 3D model over the 2D tree: the lanes that turn a node's
 * plane and move it in depth, the view it declares for its children,
 * and the two modes.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilmaterial/core/Backface.h>
#include <sigilmotion/values/Animatable.h>

#include <concepts>

namespace sigil::compose {

/** A NODE IS A PLANE. These lanes turn it and move it in depth, and
 *  the node projects its plane onto the one its parent paints on — one
 *  4x4 per node, flattened at paint, so tree order stays draw order.
 *  Paint-only like the 2D lanes. The frame is CSS's: x right, y down,
 *  +z TOWARD the viewer, and the three rotations compose as CSS's
 *  `rotateX() rotateY() rotateZ()` list, X outermost.
 *
 *  What none of this is: a scene. Two planes never intersect and
 *  nothing is lit. */
template <class Derived>
class DepthVerbs {
 public:
  /** Turn the plane about its horizontal axis, in degrees: positive
   *  tips the bottom edge toward the viewer. */
  Derived& rotateX(motion::Animatable<float> degrees);
  /** Turn the plane about its vertical axis, in degrees: positive tips
   *  the left edge toward the viewer — the card-flip lane. */
  Derived& rotateY(motion::Animatable<float> degrees);
  /** The rotation `rotate()` already is, under its 3D name — the SAME
   *  lane, so `rotate(30).rotateZ(45)` is one setting made twice. */
  Derived& rotateZ(motion::Animatable<float> degrees);
  /** Move the plane along the viewing axis, in px: positive is toward
   *  the viewer. Invisible without a `perspective()` above it, since an
   *  orthographic projection drops z. */
  Derived& translateZ(motion::Animatable<float> px);
  /** Scale along the viewing axis, about the transform origin. Nothing
   *  in the node's own plane moves; what it scales is the depth of the
   *  children it hosts in a shared space. */
  Derived& scaleZ(motion::Animatable<float> factor);
  /** THE VIEW, declared on an ancestor: this node's CHILDREN are seen
   *  from a viewer @p distancePx in front of the plane, as CSS's
   *  `perspective` property does. 0 is no perspective. Bindable, so a
   *  dolly is a bound distance. */
  Derived& perspective(motion::Animatable<float> distancePx);
  /** Where the viewer stands over the plane, as fractions of this
   *  node's box — the vanishing point of the view `perspective()`
   *  declares. The centre by default. */
  Derived& perspectiveOrigin(float fx, float fy);
  /** The pivot the lanes turn about, with a depth: `fx, fy` are the
   *  fractions `transformOrigin()` takes and `zPx` is a distance in
   *  front of the plane, positive toward the viewer. */
  Derived& transformOrigin3d(float fx, float fy, float zPx);
  /** THE SHARED SPACE: this node's children keep the depth their own
   *  lanes give them and are painted back to front by the depth of
   *  each child's centre. A cube is six children of one such node. A
   *  node that composites as a group cannot host a space. */
  Derived& preserve3d(bool on = true);
  /** Whether the back of this node's plane is drawn when a depth lane
   *  has turned it away. Visible by default. The side is decided by
   *  the node's whole projection, never by a 2D mirror, so
   *  `scaleX(-1)` stays visible. */
  Derived& backface(material::Backface facing);
  /** THE INTEGER-LITERAL SPELLING of these lanes — `rotateY(180)`,
   *  `perspective(900)` — which exists because a plain `int` does not
   *  convert into the animatable variant on its own. Constrained on
   *  `std::integral` so a float call can never land here and
   *  recurse. */
  template <std::integral T>
  Derived& rotateX(T deg) {
    return rotateX(motion::Animatable<float>((float)deg));
  }
  template <std::integral T>
  Derived& rotateY(T deg) {
    return rotateY(motion::Animatable<float>((float)deg));
  }
  template <std::integral T>
  Derived& rotateZ(T deg) {
    return rotateZ(motion::Animatable<float>((float)deg));
  }
  template <std::integral T>
  Derived& translateZ(T px) {
    return translateZ(motion::Animatable<float>((float)px));
  }
  template <std::integral T>
  Derived& scaleZ(T f) {
    return scaleZ(motion::Animatable<float>((float)f));
  }
  template <std::integral T>
  Derived& perspective(T px) {
    return perspective(motion::Animatable<float>((float)px));
  }

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
