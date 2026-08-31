#pragma once

/** @file
 * SigilCompose shape generators — the closed silhouettes: an SVG path, the
 * polygon, star, circle, annulus, squircle, blob, arc, sector and
 * parallelogram, every one a comparable value with a `path(SkSize)`.
 */

#include <include/core/SkPathBuilder.h>

#include <cstdint>

#include "sigilcompose/Compose.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::compose::shapes {

/** A silhouette generator: local-coordinate path over the node's laid-out
 *  size. The ESCAPE-HATCH spelling of what Element::shape() accepts — a
 *  raw callable never prunes, where the generator values below do. It
 *  exists because a hand-rolled curve has to start somewhere; promote it
 *  to a value once it settles. */
using OutlineFn = std::function<SkPath(SkSize)>;

/** An outline from an SVG path-d string (SkParsePath) — trace a reference
 *  silhouette in any vector tool, paste the `d`, done. The path's bounds
 *  map onto the node's box (stretch by default; `preserveAspect` fits and
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

/** Regular N-gon inscribed in the box (first vertex up unless rotated;
 *  @p rotationDeg spins the whole figure). */
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

/** N-pointed star inscribed in the box (first point up); inner
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

/** The circle (ellipse, on a non-square box) inscribed in the box, with a
 *  chosen WINDING and start point.
 *
 *  Direction is not a detail on a text baseline — it decides which way the
 *  glyphs face. `onPath` orients to the tangent, so a clockwise ring puts
 *  glyph-up radially OUTWARD and a counter-clockwise one puts it INWARD.
 *  Both are uniform engraver's conventions, and they are opposite in sign,
 *  so a ring inscription that reads upside down wants this argument rather
 *  than a hand-written `OutlineFn`.
 *
 *  @p startIndex picks which of the oval's four extreme points the contour
 *  begins at, which is what `TextPath::at` measures from. It defaults to 1
 *  to match Skia's own `addOval(rect, dir)`, so `circle(kCW)` yields
 *  byte-for-byte the path `circle()` gives and the oriented overload is a
 *  strict superset. Changing that default would silently move every label
 *  placed by arc-length fraction.
 *
 *  @p inset pulls the circle concentrically inside the box by that many
 *  px — the spelling for a ring that must stand CLEAR of the box edge: a
 *  text baseline whose glyphs straddle the circle and need room on both
 *  sides, a band drawn inside a frame. Zero is the inscribed circle;
 *  negative pushes it outside the box, which every consumer that clips
 *  at the box will truncate.
 *
 *  Exact conics either way — this is `addOval`, not a sampled polyline. */
struct Circle {
  SkPathDirection direction = SkPathDirection::kCW;
  unsigned startIndex = 1;
  float inset = 0.0f;
  bool operator==(const Circle&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

/** `kit::disc()` is the ELEMENT form (a pre-sized box centred on a
 *  point); this is the shape value, which is what onPath, the mask gates
 *  and the decorations take. */
inline Circle circle() { return Circle{}; }
inline Circle circle(float inset) { return Circle{.inset = inset}; }
inline Circle circle(SkPathDirection direction, unsigned startIndex = 1,
                     float inset = 0.0f) {
  return Circle{direction, startIndex, inset};
}

/** A ring: the inscribed circle with a concentric hole at @p innerRatio
 *  of the radius. Even-odd, so it fills as an annulus. */
struct Annulus {
  float innerRatio = 0.6f;
  bool operator==(const Annulus&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Annulus annulus(float innerRatio = 0.6f) { return Annulus{innerRatio}; }

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
  const geometry::path::Polyline unit = geometry::path::sample(
      [&](float t) { return geometry::path::fromSk(f(t)); }, t0, t1, samples,
      close);
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

}  // namespace sigil::compose::shapes
