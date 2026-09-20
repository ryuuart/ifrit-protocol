#pragma once

/** @file
 * @ingroup geometry-kit
 *
 * The closed silhouettes: an SVG path, the polygon, star, circle,
 * annulus, squircle, blob, arc, sector, parallelogram and arrow, every
 * one a comparable value with a `path(SkSize)`.
 */

#include <include/core/SkPathBuilder.h>
#include <sigilcore/callable/Callable.h>

#include <cstdint>
#include <functional>

#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"

/** THE SHAPE VOCABULARY: closed silhouettes, curve families, corner
 *  treatments, tick and chord divisions and hatch fills, each a
 *  comparable value that answers a path FOR A BOX rather than a path,
 *  so one generator serves at any size and a node whose shape did not
 *  change is a node nothing has to redraw. A shape is ONE value and a
 *  lowercase factory spelling that value's fields as arguments; the
 *  value carries the documentation and the factory carries none. */
namespace sigil::geometry::shapes {

/** A silhouette generator: local-coordinate path over the node's laid-out
 *  size, which it may name or leave unnamed — an outline that is the same
 *  path whatever the box is takes `[] { return p; }`. The ESCAPE-HATCH
 *  spelling of what a node's shape accepts — a raw callable never prunes,
 *  where the generator values below do. It exists because a hand-rolled
 *  curve has to start somewhere; promote it to a value once it settles. */
using OutlineFunction = sigil::core::Callable<SkPath(SkSize)>;

/** An outline from an SVG path-d string (SkParsePath) — trace a reference
 *  silhouette in any vector tool, paste the @p d, done. The path's bounds
 *  map onto the node's box (stretch by default; @p preserveAspect fits and
 *  centers instead). Parsed ONCE at call time; the parsed SkPath is a
 *  comparable value, so an svg() shape prunes like any generator. */
struct Svg {
  SkPath parsed;
  bool preserveAspect = false;
  bool operator==(const Svg&) const = default;
  SkPath path(SkSize size) const {
    const SkRect b = parsed.getBounds();
    if (b.isEmpty() || size.isEmpty()) return parsed;
    SkMatrix m;
    if (preserveAspect) {
      m = SkMatrix::RectToRect(b, SkRect::MakeWH(size.width(), size.height()),
                               SkMatrix::kCenter_ScaleToFit);
    } else {
      m = SkMatrix::RectToRect(b, SkRect::MakeWH(size.width(), size.height()));
    }
    return parsed.makeTransform(m);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

Svg svg(const char* d, bool preserveAspect = false);

// ---------------------------------------------------------------------------
// Generators

/** Regular @p sides -gon inscribed in the box, first vertex up unless
 *  rotated; @p rotationDeg spins the whole figure clockwise. */
struct Polygon {
  int sides = 3;
  float rotationDeg = 0.0f;
  bool operator==(const Polygon&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Polygon polygon(int sides, float rotationDeg = 0.0f) {
  return Polygon{sides, rotationDeg};
}

/** A @p points -pointed star inscribed in the box (first point up); inner
 *  vertices sit at @p innerRatio of the outer radius.
 *
 *  @p waist bows each arm edge INWARD along its own bisector, in units of
 *  the outer radius. 0 is the straight-chord star, which engraved and cut
 *  stars almost never are: they narrow fast off the hub and then run out
 *  as needles. Roughly 0.10–0.25 reads as engraved; a negative value
 *  bulges the arms instead, which is the compass-rose look. */
struct Star {
  int points = 5;
  float innerRatio = 0.5f;
  float waist = 0.0f;
  bool operator==(const Star&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Star star(int points, float innerRatio = 0.5f, float waist = 0.0f) {
  return Star{points, innerRatio, waist};
}

/** THE CIRCLE INSCRIBED IN THE BOX — an ellipse on a box that is not
 *  square, unless `uniform` asks for the largest true circle that fits
 *  — with a chosen WINDING and start point, as exact conics. `inset`
 *  pulls it concentrically inside the box in px, for a ring that must
 *  stand clear of the edge.
 *  @trap The winding decides which way glyphs on this baseline face,
 *  and `startIndex` is where an arc-length fraction is measured from:
 *  both move every label placed by fraction. */
struct Circle {
  SkPathDirection direction = SkPathDirection::kCW;
  unsigned startIndex = 1;
  float inset = 0.0f;
  bool uniform = false;
  bool operator==(const Circle&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Circle circle() { return Circle{}; }
inline Circle circle(float inset) { return Circle{.inset = inset}; }
inline Circle circle(SkPathDirection direction, unsigned startIndex = 1,
                     float inset = 0.0f) {
  return Circle{direction, startIndex, inset};
}

/** A RING: the inscribed circle with a concentric hole at @p innerRatio
 *  of the radius, even-odd so it fills as an annulus. @p thickness is
 *  the same ring said the other way about, its own width in PIXELS,
 *  which is what a ring keeps when its box does not. @p dot puts a
 *  concentric disc of that pixel radius at the centre, as ONE outline
 *  with the ring so the pair fills and animates together.
 *  @silent @p innerRatio is read when @p thickness is nonzero: the
 *  thickness decides the hole instead. */
struct Annulus {
  float innerRatio = 0.6f;
  float thickness = 0.0f;
  float dot = 0.0f;
  bool operator==(const Annulus&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Annulus annulus(float innerRatio = 0.6f) { return Annulus{innerRatio}; }

inline Annulus ring(float thickness, float dot = 0.0f) {
  return Annulus{.thickness = thickness, .dot = dot};
}

/** Superellipse |x|^e + |y|^e = 1 — the squircle. @p exponent 2 is an
 *  ellipse; 4–5 is the familiar app-icon softness; large values
 *  approach the rect. */
struct Squircle {
  float exponent = 4.0f;
  bool operator==(const Squircle&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Squircle squircle(float exponent = 4.0f) { return Squircle{exponent}; }

namespace detail {
/** The one polyline sampler behind every parametric curve: evaluates
 *  @p f over t ∈ [t0, t1] in the UNIT frame (±1 spans the box) and
 *  scales onto the node's half-extents. The sampling itself is the
 *  geometry library's; only the unit-box scaling is this library's
 *  convention. */
template <typename F>
inline SkPath samplePolyline(const F& f, float t0, float t1, int samples,
                             bool close, SkSize s) {
  const float cx = s.width() * 0.5f, cy = s.height() * 0.5f;
  const path::Polyline unit = path::sample(
      [&](float t) { return path::fromSk(f(t)); }, t0, t1, samples, close);
  SkPathBuilder b;
  bool first = true;
  for (const glm::vec2& u : unit.points) {
    const SkPoint p{cx + cx * u.x, cy + cy * u.y};
    if (first)
      b.moveTo(p);
    else
      b.lineTo(p);
    first = false;
  }
  if (close) b.close();
  return b.detach();
}

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
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Blob blob(uint32_t seed, float amplitude = 0.18f, int lobes = 8) {
  return Blob{seed, amplitude, lobes};
}

/** A circular arc inscribed in the box, STARTING at @p startDeg (Skia
 *  canvas convention: 0° = +x, clockwise) and sweeping @p sweepDeg. The
 *  path begins at the arc's own start, so an arc-length reveal such as
 *  `spans::upTo(sweep/360)` needs no wrap arithmetic. Stroke it: an open
 *  arc has no fillable area. */
struct Arc {
  float startDeg = 0.0f;
  float sweepDeg = 359.9f;
  bool operator==(const Arc&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Arc arc(float startDeg, float sweepDeg = 359.9f) {
  return Arc{startDeg, sweepDeg};
}

/** A CLOSED, fillable circular sector inscribed in the box — the arc plus
 *  its two radii, or with @p innerRatio > 0 the annular segment between
 *  two radii (a donut slice). `arc()` above is deliberately open and
 *  cannot be filled; this is the one to reach for when the wedge itself
 *  is the mark: pie and polar-area charts (Nightingale's coxcomb),
 *  cooldown sweeps, radial menus, gauge fills, compass roses.
 *
 *  Angles follow Skia's canvas convention: 0° = +x, sweeping clockwise. */
struct Sector {
  float startDeg = 0.0f;
  float sweepDeg = 90.0f;
  float innerRatio = 0.0f;
  bool operator==(const Sector&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Sector sector(float startDeg, float sweepDeg, float innerRatio = 0.0f) {
  return Sector{startDeg, sweepDeg, innerRatio};
}

/** A parallelogram leaning by @p skewDeg: the top edge shifts by
 *  h·tan(skew) relative to the bottom, staying inside the box. */
struct Parallelogram {
  float skewDeg = 0.0f;
  bool operator==(const Parallelogram&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
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
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
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
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Chevron chevron(float spread = 0.20f, float drop = 0.34f,
                       float thickness = 0.16f, float bars = 0.0f) {
  return Chevron{spread, drop, thickness, bars};
}

}  // namespace sigil::geometry::shapes
