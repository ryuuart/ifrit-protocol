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
  /** SLIDE THE NODE ALONG X, in pixels, positive to the right — a lane
   *  that animates and binds. Zero when unstated. */
  Derived& translateX(motion::Animatable<float> v);
  /** SLIDE THE NODE ALONG Y, in pixels, positive downward — a lane that
   *  animates and binds. Zero when unstated. */
  Derived& translateY(motion::Animatable<float> v);
  /** SLIDE THE NODE ALONG X, LOOPING: the lane's pixels folded over
   *  @p period, a length read where the node lands — a `percent` of the
   *  node's own width, `em`, a viewport unit or a custom property — so a
   *  lane that keeps running loops, and the one who drives it never needs
   *  to know how long the loop is. A value within [-period, 0] is used as
   *  it stands; one outside it is folded into (-period, 0]. A period that
   *  resolves to zero or less folds nothing. */
  Derived& translateX(motion::Animatable<float> v, Dimension period);
  /** SLIDE THE NODE ALONG Y, LOOPING: the same, folded over a period read
   *  against the node's own height. */
  Derived& translateY(motion::Animatable<float> v, Dimension period);
  /** A number of pixels along X, the lane's own spelling. */
  Derived& translateX(float pixels);
  /** A number of pixels along Y, the lane's own spelling. */
  Derived& translateY(float pixels);
  /** SLIDE THE NODE ALONG X BY A LENGTH — CSS `translate`: a `percent` of
   *  the node's OWN width, so `translateX(-50_percent)` centres a node on
   *  its left edge whatever its size; `em`, `rem`, a viewport unit or a
   *  custom property against the font, the canvas and the properties in
   *  force.
   *  @trap A length is a statement, not a lane: it neither animates nor
   *  eases under `transition()`. */
  Derived& translateX(Dimension length);
  /** SLIDE THE NODE ALONG Y BY A LENGTH — a `percent` of the node's own
   *  height. */
  Derived& translateY(Dimension length);
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
   *  further right further down. A node naming both lanes takes CSS's
   *  `skewX(x) skewY(y)`, the y shear inside the x one, so it widens
   *  along x by the product of the two tangents as a browser draws it. */
  Derived& skewY(motion::Animatable<float> degrees);
  /** THE PIVOT every rotation, scale and skew turns about — CSS
   *  `transform-origin`. A percentage is of the node's own box,
   *  `percent(0)` its left or top edge and `percent(100)` its right or bottom;
   *  any other length is node-local pixels; @p z is the pivot's
   *  distance in front of the plane, which a percentage cannot say. The
   *  CENTRE, in the plane, when unstated.
   *  @trap A rule states the flat pivot alone; a @p z written in one is
   *  left out and said once. */
  Derived& transformOrigin(Dimension x, Dimension y, Dimension z = 0.0f);
  /** A BARE NUMBER IS REFUSED, as CSS refuses a length without a unit
   *  here: it reads as a fraction of the box as readily as a pixel
   *  count. Write `percent(50)` or `Dimension(12)`. */
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
