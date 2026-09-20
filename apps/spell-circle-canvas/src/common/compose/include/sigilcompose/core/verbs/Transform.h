#pragma once

/** @file
 * @ingroup compose-core
 *
 * The 2D transform lanes and the stacking index.
 */

#include <include/core/SkPoint.h>
#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Shape.h>
#include <sigilmotion/values/Animatable.h>

#include <concepts>

namespace sigil::compose {

/** THE PAINT-PHASE LANES: translate, then rotate, then scale, then
 *  skew, about the transform origin. Animating any of them never
 *  relayouts — the content picture replays under the new transform, the
 *  node's layout box stays where it was, and hit-testing follows the
 *  transformed box. */
template <class Derived>
class TransformVerbs {
 public:
  /** SLIDE THE NODE ALONG X, in pixels, positive to the right. Zero
   *  when unstated. */
  Derived& translateX(motion::Animatable<float> v);
  /** SLIDE THE NODE ALONG Y, in pixels, positive downward. Zero when
   *  unstated. */
  Derived& translateY(motion::Animatable<float> v);
  /** Ride a CURVE instead of two lanes. The node's transform origin is
   *  the point that lands on the curve, and the curve is resolved
   *  against the PARENT's box. */
  Derived& travel(MotionPath along);
  /** TURN THE NODE IN ITS OWN PLANE, in degrees, positive clockwise in
   *  screen space, about the transform origin. Zero when unstated. */
  Derived& rotate(motion::Animatable<float> degrees);
  /** SCALE THE NODE UNIFORMLY about the transform origin, 1 being its
   *  laid-out size. */
  Derived& scale(motion::Animatable<float> factor);
  /** Scale along X alone about the transform origin, multiplied INTO
   *  `scale()`. Bars, wipes, meters and cooldown sweeps are what it is
   *  for, with `transformOrigin()` pinning the growing edge. */
  Derived& scaleX(motion::Animatable<float> factor);
  /** Scale along Y alone about the transform origin, multiplied INTO
   *  `scale()`. */
  Derived& scaleY(motion::Animatable<float> factor);
  /** Shear that slants VERTICALS, in degrees, about the transform
   *  origin. The sense is screen-space, y down: a POSITIVE angle
   *  leans the shape's top LEFT, so the italic forward lean is a
   *  negative one. */
  Derived& skewX(motion::Animatable<float> degrees);
  /** Shear that slants HORIZONTALS, in degrees, about the transform
   *  origin. Zero when unstated; a positive angle pushes points
   *  further right further down. */
  Derived& skewY(motion::Animatable<float> degrees);
  /** THE PIVOT every rotation, scale and skew turns about, as
   *  fractions of the node's own box: 0,0 its top-left, 1,1 its
   *  bottom-right. The CENTRE when unstated. */
  Derived& transformOrigin(float fx, float fy);
  /** The pivot in node-local PIXELS instead — for a pivot that is not
   *  a fraction of this node's box, such as zooming a window that
   *  lives inside a full-canvas overlay about its own centre. */
  Derived& transformOriginPx(SkPoint p);
  /** PAINT ORDER AMONG SIBLINGS — CSS `z-index`. Zero when unstated,
   *  so siblings paint in declaration order. It reorders nothing
   *  outside this node's own parent, and it changes no layout. */
  Derived& zIndex(int z);
  /** THE INTEGER-LITERAL SPELLING of these lanes — `rotate(-8)`,
   *  `scale(2)` — which exists because a plain `int` does not convert
   *  into the animatable variant on its own and the error when it does
   *  not is unreadable. Constrained on `std::integral` so a float call
   *  can never land here and recurse. */
  template <std::integral T>
  Derived& translateX(T v) {
    return translateX(motion::Animatable<float>((float)v));
  }
  template <std::integral T>
  Derived& translateY(T v) {
    return translateY(motion::Animatable<float>((float)v));
  }
  template <std::integral T>
  Derived& rotate(T deg) {
    return rotate(motion::Animatable<float>((float)deg));
  }
  template <std::integral T>
  Derived& scale(T f) {
    return scale(motion::Animatable<float>((float)f));
  }
  template <std::integral T>
  Derived& scaleX(T f) {
    return scaleX(motion::Animatable<float>((float)f));
  }
  template <std::integral T>
  Derived& scaleY(T f) {
    return scaleY(motion::Animatable<float>((float)f));
  }
  template <std::integral T>
  Derived& skewX(T deg) {
    return skewX(motion::Animatable<float>((float)deg));
  }
  template <std::integral T>
  Derived& skewY(T deg) {
    return skewY(motion::Animatable<float>((float)deg));
  }

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
