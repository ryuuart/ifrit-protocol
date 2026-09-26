#pragma once

/** @file
 * @ingroup geometry-kit
 *
 * The closed silhouettes: the stock values over `radial` and `ellipse`
 * (polygon, star, circle, annulus, ring, squircle, arc, sector), an SVG
 * outline fitted to the box, and the one-offs that are no setting of
 * either — blob, parallelogram, arrow and chevron — every one a
 * comparable value with an `outline(size)`.
 */

#include <sigilcore/callable/Callable.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <glm/vec2.hpp>
#include <string_view>

#include "sigilgeometry/kit/Radial.h"
#include "sigilgeometry/path/Outline.h"

/** THE SHAPE VOCABULARY: closed silhouettes, curve families, corner
 *  treatments, tick and chord divisions and hatch fills, each a
 *  comparable value that answers a path FOR A BOX rather than a path,
 *  so one generator serves at any size and a node whose shape did not
 *  change is a node nothing has to redraw. A shape is ONE value and a
 *  lowercase factory spelling that value's fields as arguments; the
 *  value carries the documentation and the factory carries none. */
namespace sigil::geometry::shapes {

/** A silhouette generator: a local-coordinate outline over the node's
 *  laid-out size, which it may name or leave unnamed — an outline that is
 *  the same whatever the box is takes `[] { return outline; }`. The ESCAPE-HATCH
 *  spelling of what a node's shape accepts — a raw callable never prunes,
 *  where the generator values below do. It exists because a hand-rolled
 *  curve has to start somewhere; promote it to a value once it settles. */
using OutlineFunction = sigil::core::Callable<path::Outline(glm::vec2)>;

/** An outline from SVG path data — trace a reference silhouette in any
 *  vector tool, paste the @p data, done — fitted to the box: stretched,
 *  or with @p preserveAspect fitted and centred. Parsed ONCE at call
 *  time, so an svg() shape prunes like any generator. */
inline Fitted svg(std::string_view data, bool preserveAspect = false) {
  return fitted(path::Outline::svg(data), {.preserveAspect = preserveAspect});
}

// ---------------------------------------------------------------------------
// Stock values over `radial` and `ellipse`

/** Regular @p sides -gon inscribed in the box, first vertex up;
 *  @p rotationDegrees spins the whole figure clockwise. */
inline Radial polygon(int sides, float rotationDegrees = 0.0f) {
  return radial(std::max(sides, 3), {.fromDegrees = rotationDegrees});
}

/** A @p points -pointed star inscribed in the box (first point up); inner
 *  vertices sit at @p innerRatio of the outer radius.
 *
 *  @p waist bows each arm edge INWARD along its own bisector, in units of
 *  the outer radius. 0 is the straight-chord star, which engraved and cut
 *  stars almost never are: they narrow fast off the hub and then run out
 *  as needles. Roughly 0.10–0.25 reads as engraved; a negative value
 *  bulges the arms instead, which is the compass-rose look. */
inline Radial star(int points, float innerRatio = 0.5f, float waist = 0.0f) {
  return radial(std::max(points, 2) * 2,
                {.radii = {1.0f, innerRatio}, .waist = waist});
}

/** THE CIRCLE INSCRIBED IN THE BOX — an ellipse on a box that is not
 *  square, unless `uniform` asks for the largest true circle that fits. */
inline Ellipse circle() { return ellipse(); }
/** The circle pulled @p inset px concentrically inside the box, for a
 *  ring that must stand clear of the edge. */
inline Ellipse circle(float inset) { return ellipse({.inset = inset}); }
/** The circle drawn with a chosen WINDING and start point.
 *  @trap The winding decides which way glyphs on this baseline face, and
 *  @p start is where an arc-length fraction is measured from: both move
 *  every label placed by fraction. */
inline Ellipse circle(path::Winding winding, unsigned start = 1,
                      float inset = 0.0f) {
  return ellipse({.inset = inset, .winding = winding, .start = start});
}

/** A RING: the inscribed circle with a concentric hole at @p innerRatio
 *  of the radius, even-odd so it fills as an annulus. */
inline Ellipse annulus(float innerRatio = 0.6f) {
  return ellipse({.inner = innerRatio});
}

/** The same ring said the other way about: its own width in PIXELS,
 *  which is what a ring keeps when its box does not, with a concentric
 *  disc of @p dot px radius at the centre as ONE outline with it. */
inline Ellipse ring(float thickness, float dot = 0.0f) {
  return ellipse({.thickness = thickness, .dot = dot});
}

/** Superellipse |x|^e + |y|^e = 1 — the squircle. @p exponent 2 is an
 *  ellipse; 4–5 is the familiar app-icon softness; large values approach
 *  the rect. */
inline Ellipse squircle(float exponent = 4.0f) {
  return ellipse({.exponent = exponent});
}

/** A circular arc inscribed in the box, starting at @p startDegrees
 *  (0° = +x, clockwise) and sweeping @p sweepDegrees. The outline begins
 *  at the arc's own start, so an arc-length reveal needs no wrap
 *  arithmetic. Stroke it: an open arc has no fillable area. */
inline Ellipse arc(float startDegrees, float sweepDegrees = 359.9f) {
  return ellipse({.fromDegrees = startDegrees,
                  .sweepDegrees = sweepDegrees,
                  .close = Close::Open});
}

/** A CLOSED, fillable circular sector inscribed in the box — the arc plus
 *  its two radii, or with @p innerRatio > 0 the annular segment between
 *  two radii (a donut slice): pie and polar-area charts, cooldown sweeps,
 *  radial menus, gauge fills. Angles as `arc()`'s. */
inline Ellipse sector(float startDegrees, float sweepDegrees,
                      float innerRatio = 0.0f) {
  return ellipse({.fromDegrees = startDegrees,
                  .sweepDegrees = sweepDegrees,
                  .close = Close::Pie,
                  .inner = innerRatio});
}

// ---------------------------------------------------------------------------
// The one-offs

namespace detail {
/** The one polyline sampler behind every parametric curve: evaluates
 *  @p f over t ∈ [t0, t1] in the UNIT frame (±1 spans the box) and
 *  scales onto the box's half-extents. The sampling itself is the path
 *  tier's; only the unit-box scaling is this kit's convention. */
path::Outline samplePolyline(const std::function<glm::vec2(float)>& f,
                             float t0, float t1, int samples, bool close,
                             glm::vec2 size);
}  // namespace detail

/** Organic closed blob: @p lobes control points on the inscribed
 *  ellipse, each pushed in/out by up to @p amplitude (fraction of the
 *  radius) of seeded deterministic noise, joined by a smooth
 *  Catmull-Rom loop. Same seed → same blob, every frame, every run —
 *  chaos you can cache. */
struct Blob {
  uint32_t seed = 0;
  float amplitude = 0.18f;
  int lobes = 8;
  bool operator==(const Blob&) const = default;
  path::Outline outline(glm::vec2 size) const;
};

inline Blob blob(uint32_t seed, float amplitude = 0.18f, int lobes = 8) {
  return Blob{seed, amplitude, lobes};
}

/** A parallelogram leaning by @p skewDeg: the top edge shifts by
 *  h·tan(skew) relative to the bottom, staying inside the box. */
struct Parallelogram {
  float skewDeg = 0.0f;
  bool operator==(const Parallelogram&) const = default;
  path::Outline outline(glm::vec2 size) const;
};

inline Parallelogram parallelogram(float skewDeg) {
  return Parallelogram{skewDeg};
}

/** An arrow along +x, inscribed in the box: a shaft of @p shaftFrac of the
 *  height and a head of @p headFrac of the width.
 *
 *  @p headSpan is how far ACROSS the head reaches, as a fraction of the
 *  height: 1 is the barb that fills the box, and less than that is the
 *  paddle — a handle whose head is a stated size while its shaft runs
 *  whatever length the box is. A fan of arms of different lengths off one
 *  hub wants exactly that, since a head that is a fraction of the box
 *  grows with the arm and a fan of arms then has heads of five sizes. */
struct Arrow {
  float shaftFrac = 0.34f;
  float headFrac = 0.42f;
  float headSpan = 1.0f;
  bool operator==(const Arrow&) const = default;
  path::Outline outline(glm::vec2 size) const;
};

inline Arrow arrow(float shaftFrac = 0.34f, float headFrac = 0.42f,
                   float headSpan = 1.0f) {
  return Arrow{shaftFrac, headFrac, headSpan};
}

/** A CHEVRON: a wide flat V pointing down the box, drawn as an outline
 *  of its own thickness, with an optional pair of outrigger bars level
 *  with its shoulders. A V and not an arrowhead: @p spread takes the
 *  shoulders out as a fraction of the width, @p drop takes the point
 *  down as a fraction of the height, @p thickness is the mark's own
 *  width as a fraction of the height, and @p bars is how far the
 *  outriggers run in from each edge, zero leaving the V alone. */
struct Chevron {
  float spread = 0.20f;
  float drop = 0.34f;
  float thickness = 0.16f;
  float bars = 0.0f;
  bool operator==(const Chevron&) const = default;
  path::Outline outline(glm::vec2 size) const;
};

inline Chevron chevron(float spread = 0.20f, float drop = 0.34f,
                       float thickness = 0.16f, float bars = 0.0f) {
  return Chevron{spread, drop, thickness, bars};
}

}  // namespace sigil::geometry::shapes
