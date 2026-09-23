#pragma once

/** @file
 * @ingroup compose-core
 *
 * The 2D transform lanes and the stacking index.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Shape.h>
#include <sigilmotion/values/Animatable.h>

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
  /** THE PIVOT every rotation, scale and skew turns about — CSS
   *  `transform-origin`. A percentage is of the node's own box,
   *  `pct(0)` its left or top edge and `pct(100)` its right or bottom;
   *  any other length is node-local pixels; @p z is the pivot's
   *  distance in front of the plane, which a percentage cannot say. The
   *  CENTRE, in the plane, when unstated.
   *  @trap A rule states the flat pivot alone; a @p z written in one is
   *  left out and said once. */
  Derived& transformOrigin(Dimension x, Dimension y, Dimension z = 0.0f);
  /** A BARE NUMBER IS REFUSED, as CSS refuses a length without a unit
   *  here: it reads as a fraction of the box as readily as a pixel
   *  count. Write `pct(50)` or `Dimension(12)`. */
  Derived& transformOrigin(float, float, float = 0.0f) = delete;
  /** PAINT ORDER AMONG SIBLINGS — CSS `z-index`. Zero when unstated,
   *  so siblings paint in declaration order. It reorders nothing
   *  outside this node's own parent, and it changes no layout. */
  Derived& zIndex(int z);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
