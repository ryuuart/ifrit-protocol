#pragma once
/** @file
 * The conic section about its FOCUS — the one curve family that is not a
 * closed figure inscribed in a box.
 *
 * Every other generated curve here is centred: an ellipse in a box, a
 * spiral about a point, a rose about its origin. A conic is measured from
 * one FOCUS, which is off centre, and that is not a detail of the
 * arithmetic — it is what the curve is for. The thing at the focus is
 * what the curve goes round, and a drawing that puts it in the middle of
 * the ellipse instead has said something false about the picture.
 *
 *     r(v) = p / (1 + e cos v)
 *
 * One value covers the whole family, because e alone decides which curve
 * it is: 0 a circle, under 1 an ellipse, 1 a parabola, over 1 the
 * hyperbola that never comes back. The angle v is measured FROM the
 * periapsis — the near point — so the same v means the same place on the
 * curve whatever the eccentricity, and a sweep from -180 to 180 is always
 * the near half.
 *
 * Angles run clockwise on screen, because y grows downward, which is the
 * convention `arrange::onEllipse` places a ring by.
 */

#include <include/core/SkPath.h>

#include <glm/vec2.hpp>

namespace sigil::geometry::path {

/** A conic section, in the caller's own units, standing on its focus. */
struct Conic {
  /** The point the curve is measured from — what it goes round. */
  glm::vec2 focus{0, 0};
  /** The semi-latus rectum: the radius a quarter turn from the periapsis,
   *  which is the conic's SIZE and the one length that means the same
   *  thing on every one of the four curves. */
  float semiLatus = 1;
  /** 0 a circle, under 1 an ellipse, 1 a parabola, over 1 a hyperbola. */
  float eccentricity = 0;
  /** Which way the near point lies, in degrees. */
  float periapsisDeg = 0;

  bool operator==(const Conic&) const = default;

  /** How far the curve stands from the focus at @p anomalyDeg. On an open
   *  conic the denominator reaches zero at the asymptote and the radius
   *  runs away; it is clamped just short of that, so a caller that asks
   *  past the asymptote gets a very distant point rather than an infinity
   *  that poisons a path. */
  float radiusAt(float anomalyDeg) const;

  /** Where the curve is at @p anomalyDeg. */
  glm::vec2 at(float anomalyDeg) const;

  /** The unit direction of travel — dP/dv normalised — which is the
   *  tangent taken in the direction of INCREASING anomaly. On a circle it
   *  is square to the radius; on everything else it is not, and the
   *  difference between the two is the whole reason both are here. */
  glm::vec2 alongAt(float anomalyDeg) const;

  /** The unit direction away from the focus. */
  glm::vec2 outwardAt(float anomalyDeg) const;

  /** The anomaly the curve runs off to, in degrees: 180 on a closed
   *  conic, which is simply the far side, and `acos(-1/e)` on an open one,
   *  which is the ASYMPTOTE — the direction the branch approaches and
   *  never reaches. Sampling up to it is what makes a hyperbola's arms
   *  disappear off the frame instead of stopping in mid-air. */
  float asymptoteDeg() const;

  /** Whether the curve comes back to where it started. */
  bool closes() const { return eccentricity < 1; }
};

/** How much of a conic to draw, and how finely.
 *
 *  `reach` is the distance from the focus past which the curve is not
 *  drawn at all. An open conic goes to infinity, and a path that carries
 *  a point a million units out is a path whose bounds, contour measure and
 *  dash phase are all decided by somewhere nobody can see; past `reach`
 *  the contour BREAKS rather than being clamped, so what comes back is the
 *  parts that are near enough to be drawn and no line between them. Zero
 *  keeps everything. */
struct ConicSpan {
  float fromDeg = -180;
  float toDeg = 180;
  int steps = 320;
  float reach = 0;
  bool operator==(const ConicSpan&) const = default;
};

/** The conic over that span, as a polyline path. Closed — one contour,
 *  returning to its start — only when the conic closes and the span goes
 *  the whole way round; every other span is open, which is what a
 *  reveal along it, a label riding it or a dash marching down it wants.
 *
 *  The path begins at `fromDeg`, so a trim from the start of the contour
 *  is a trim from there and needs no wrap arithmetic. */
SkPath conicPath(const Conic& conic, const ConicSpan& span = {});

}  // namespace sigil::geometry::path
