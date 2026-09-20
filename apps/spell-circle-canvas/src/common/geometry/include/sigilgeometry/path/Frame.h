#pragma once

/** @file
 * @ingroup geometry-path
 *
 * A figure's own coordinate system, as a value: the polar `PolarFrame`,
 * the unit-map `Grid`, and the centred rect both are read through.
 *
 * A `PolarFrame` converts `(angle, radius)` — numbers measured off a
 * reference drawing — into a point, an SkRect or an arc-length fraction,
 * in that drawing's angle convention. A `Grid` carries an artefact's own
 * units onto the canvas: a scale, an origin, a y axis and a snap.
 */

#include <include/core/SkMatrix.h>
#include <include/core/SkPathTypes.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>

#include <cmath>
#include <vector>

#include "sigilgeometry/path/Numeric.h"

namespace sigil::geometry::path {

/** THE RECT OF SIZE @p w × @p h CENTRED ON @p c — the `x - w * 0.5f`
 *  arithmetic as a VALUE you can then inset, union, or hand to whatever
 *  draws the figure. Reach for it when you know the box and want the
 *  rect for something else too; a node that sizes ITSELF is centred
 *  after layout by `centerAt()` instead. */
inline SkRect centred(SkPoint c, float w, float h) {
  return SkRect::MakeXYWH(c.fX - w * 0.5f, c.fY - h * 0.5f, w, h);
}
/** The same box from a size value. */
inline SkRect centred(SkPoint c, SkSize s) {
  return centred(c, s.width(), s.height());
}

/** Where a frame's 0° points. */
enum class Zero {
  East,   ///< 3 o'clock — Skia's own convention, and `shapes::arc`'s.
  North,  ///< 12 o'clock — the engraver's and the statistical plate's.
};

/** Which way a frame's angles increase, **as seen on screen**. Screen y is
 *  down, so `CW` is the direction that *looks* clockwise. */
enum class Sense { CW, CCW };

/** A FIGURE'S OWN POLAR COORDINATE SYSTEM: a centre, a radius that
 *  `r = 1` lands on, and the convention flags — where zero points and
 *  which way the angles run, which is what makes this a value rather
 *  than a `polar()` helper every call site adds a sign flip and a -90
 *  to. An aggregate, meant for designated initialisation, since a
 *  positional constructor could not gain a field later without breaking
 *  every call site. Trivially copyable; it holds no node state. */
struct PolarFrame {
  SkPoint centre{0, 0};
  /** The px radius that `normalizedRadius = 1` maps to. Authoring the rest of a
   *  figure in normalised radius is what lets the whole plate be rescaled
   *  by editing this one number. */
  float radius = 1.0f;
  Zero zero = Zero::North;
  Sense sense = Sense::CW;
  /** An extra origin offset in SCREEN degrees (+ = clockwise on screen),
   *  applied after `zero`. Normally 0. Two things want it: a scanned
   *  source that is not square to its own axes, and an index ring offset
   *  by half a division, which otherwise becomes a stray constant added at
   *  every call site. */
  float originDeg = 0.0f;

  bool operator==(const PolarFrame&) const = default;

  /** @name Angles
   *  This frame's degrees converted to and from the screen angle Skia
   *  and the shape generators take, and back from a fraction of a turn.
   *  @{ */

  /** This frame's @p deg as a SCREEN angle: degrees from +x, increasing
   *  in the direction that looks clockwise, which is exactly what Skia's
   *  `addArc`, `shapes::arc()` and `shapes::sector()` take — so a sector
   *  spelled through this reads in the plate's units and draws in
   *  Skia's. */
  constexpr float skiaDeg(float deg) const {
    const float base = zero == Zero::North ? -90.0f : 0.0f;
    return base + originDeg + (sense == Sense::CW ? deg : -deg);
  }
  /** A SWEEP in this frame's units as a Skia sweep — sign only, no origin.
   *  Kept separate from skiaDeg() because adding the origin twice is the
   *  classic bug when an arc is spelled as two absolute angles. */
  constexpr float skiaSweep(float sweepDeg) const {
    return sense == Sense::CW ? sweepDeg : -sweepDeg;
  }
  /** This frame's @p deg AS A SCREEN ANGLE in radians about +x, ready
   *  for `std::cos`/`sin` against a screen-space point. Named apart from
   *  the free `radians()`, which is the unit conversion alone: this one
   *  carries the frame's zero and sense as well. */
  float screenRadians(float deg) const { return radians(skiaDeg(deg)); }

  /** @p deg AS THE ARC-LENGTH FRACTION of a circular baseline, in
   *  [0, 1) — the value a text path is addressed by. @p baseline is the
   *  direction of the PATH, not of this frame, and the two are
   *  independent: numbers running anticlockwise around a clockwise ring
   *  is an ordinary thing to want.
   *  @trap Only exact on a CIRCLE — an ellipse's arc length is not
   *  proportional to its angle, so an oblong box drifts. */
  float fraction(float deg,
                 SkPathDirection baseline = SkPathDirection::kCW) const {
    const float screen =
        baseline == SkPathDirection::kCW ? skiaDeg(deg) : -skiaDeg(deg);
    const float f = std::fmod(screen / 360.0f, 1.0f);
    return f < 0 ? f + 1.0f : f;
  }
  /** The inverse of fraction(): an arc-length fraction back into this
   *  frame's degrees — for labelling a ring that was already placed by
   *  fraction, or for reading a hit test back out in plate units. */
  constexpr float degOf(float frac,
                        SkPathDirection baseline = SkPathDirection::kCW) const {
    const float base = zero == Zero::North ? -90.0f : 0.0f;
    const float screen =
        baseline == SkPathDirection::kCW ? frac * 360.0f : -frac * 360.0f;
    const float rel = screen - base - originDeg;
    return sense == Sense::CW ? rel : -rel;
  }

  /** @} */

  /** @name Points
   *  An angle and a radius resolved into the frame's parent space, and
   *  the outward direction at an angle.
   *  @{ */

  /** `(angle, NORMALISED radius)` → a point in the frame's parent space.
   *  `normalizedRadius = 1` is `radius`. */
  SkPoint at(float deg, float normalizedRadius = 1.0f) const {
    return px(deg, normalizedRadius * radius);
  }
  /** `(angle, PX radius)` → a point, for a figure whose radii were
   *  measured in pixels rather than as fractions of one figure radius. */
  SkPoint px(float deg, float rPx) const {
    const float a = screenRadians(deg);
    return {centre.fX + rPx * std::cos(a), centre.fY + rPx * std::sin(a)};
  }
  /** The unit vector pointing out along @p deg — the direction a tick, a
   *  leader or a radial label runs. */
  SkVector dir(float deg) const {
    const float a = screenRadians(deg);
    return {std::cos(a), std::sin(a)};
  }

  /** @} */

  /** @name Boxes
   *  The square a silhouette generator inscribes itself in.
   *  @{ */

  /** The square box of radius `normalizedRadius` about the centre — the frame
   * every silhouette generator inscribes itself in: inset it, union it, or hand
   * it to whatever draws the figure. */
  SkRect box(float normalizedRadius = 1.0f) const {
    return centred(centre, 2 * normalizedRadius * radius,
                   2 * normalizedRadius * radius);
  }
  /** @} */

  /** @name Derived frames
   *  Another frame that inherits this one's conventions, so the four
   *  fields are never restated and a convention is never silently
   *  dropped.
   *  @{ */

  /** A concentric frame at @p k of this radius, same centre and
   *  conventions — the inner limb, the cell band, the hub. Saves the
   *  four-field restatement, which is where a convention gets silently
   *  dropped. */
  constexpr PolarFrame scaled(float k) const {
    return {centre, radius * k, zero, sense, originDeg};
  }
  /** The same frame about a different centre — a satellite figure that
   *  inherits the plate's angle convention. */
  constexpr PolarFrame about(SkPoint c) const {
    return {c, radius, zero, sense, originDeg};
  }
  /** The same frame with its zero turned by @p deg IN THIS FRAME'S SENSE.
   *  Composes: `f.turned(4.5f).turned(-4.5f) == f`. */
  constexpr PolarFrame turned(float deg) const {
    return {centre, radius, zero, sense,
            originDeg + (sense == Sense::CW ? deg : -deg)};
  }
  /** @} */
};

// ---------------------------------------------------------------------------
// Grid — the unit map.

/** AUTHOR IN THE ARTEFACT'S OWN UNITS; MULTIPLY ONCE. A value rather
 *  than a `float g(float)` because it needs three things — scale, origin
 *  and snap — and because more than one grid has to be alive at once: a
 *  plate at a 4 px geometry pitch carrying a readout on a 2.5 px text
 *  pitch is ordinary.
 *  @trap A LENGTH takes no origin and a POSITION does, which is why the
 *  four readings are named apart: adding the origin to a width is the
 *  bug the split prevents. */
struct Grid {
  /** Canvas px per artefact unit. */
  float scale = 1.0f;
  /** THE Y AXIS, AS A MULTIPLE OF `scale` — its direction and its
   *  relative size in one number. 1, the default, is the canvas's own
   *  frame: y down, square units. −1 is the MATH FRAME, y counting UP
   *  from the origin, which a plotted function, a projected sky and a
   *  surveyed elevation are drawn in. Anything else is an anisotropic
   *  map, for a chart whose two axes are different quantities. */
  float yScale = 1.0f;
  /** Where artefact (0, 0) lands on the canvas. */
  SkPoint origin{0, 0};
  /** Snap the RESULT to a multiple of this many canvas px (0 = off). This
   *  is a canvas-px pitch, not a unit count: snapping to the grid's own
   *  pitch and snapping to the device pixel are different values. */
  float snap = 0.0f;

  bool operator==(const Grid&) const = default;

  /** Rounds half away from zero, like `std::round`, but CONSTEXPR — which
   *  `std::round` is not before C++23. That is why it is hand-rolled: a
   *  unit map typically feeds `constexpr` canvas constants (a canvas width
   *  declared as so many artefact units), and a helper that cannot run at
   *  compile time cannot be used for those. */
  constexpr float snapped(float v) const {
    if (!(snap > 0)) return v;
    const float q = v / snap;
    return snap * (float)(long long)(q + (q < 0 ? -0.5f : 0.5f));
  }
  /** A LENGTH along x, in artefact units → px. */
  constexpr float lengthX(float units) const { return snapped(units * scale); }
  /** A LENGTH along y, which a flipped or anisotropic frame measures
   *  differently — and which comes back SIGNED under a flip, because a
   *  length up the page IS negative in canvas px. */
  constexpr float lengthY(float units) const {
    return snapped(units * scale * yScale);
  }
  /** A POSITION on x: the length from the origin. */
  constexpr float positionX(float units) const {
    return snapped(origin.fX + units * scale);
  }
  /** A POSITION on y, down the canvas even where the axis runs up. */
  constexpr float positionY(float units) const {
    return snapped(origin.fY + units * scale * yScale);
  }
  constexpr SkPoint at(SkPoint units) const {
    return {positionX(units.fX), positionY(units.fY)};
  }
  /** SORTED, so a flipped frame answers a rect and not an inside-out one:
   *  under `yScale` < 0 the unit-space top is the canvas-space bottom, and
   *  every consumer of an SkRect reads left ≤ right and top ≤ bottom. */
  SkRect rect(float ux, float uy, float uw, float uh) const {
    return SkRect::MakeLTRB(positionX(ux), positionY(uy), positionX(ux + uw),
                            positionY(uy + uh))
        .makeSorted();
  }
  /** The artefact-unit rect as canvas px, corner-by-corner — so a snapped
   *  grid keeps both edges on the grid rather than only the near one, and
   *  sorted for the same reason the other overload is. */
  SkRect rect(const SkRect& units) const {
    return SkRect::MakeLTRB(positionX(units.fLeft), positionY(units.fTop),
                            positionX(units.fRight), positionY(units.fBottom))
        .makeSorted();
  }
  /** A polyline in artefact units → canvas px. */
  std::vector<SkPoint> map(const std::vector<SkPoint>& units) const {
    std::vector<SkPoint> out;
    out.reserve(units.size());
    for (const SkPoint& p : units) out.push_back(at(p));
    return out;
  }
  /** The affine matrix, for handing a whole SkPath through in one go. NOT
   *  snapped — a matrix cannot round per-point, and pretending otherwise
   *  is how a "snapped" plate ends up half on the grid. */
  SkMatrix matrix() const {
    return SkMatrix::Translate(origin.fX, origin.fY)
        .preScale(scale, scale * yScale);
  }
  /** A grid at @p k of this one's scale, same origin, y axis and snap —
   *  the nested unit system (a plate at 4 px/unit carrying a readout at
   *  2.5). */
  constexpr Grid scaled(float k) const {
    return {scale * k, yScale, origin, snap};
  }
};

}  // namespace sigil::geometry::path
