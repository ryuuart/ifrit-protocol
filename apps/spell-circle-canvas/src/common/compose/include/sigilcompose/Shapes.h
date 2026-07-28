#pragma once

/** @file
 * SigilCompose shape kit — the free-form answer to "everything is a
 * box". An extension over the kernel's shape() seam: every generator
 * here is a COMPARABLE VALUE (a `ShapeScheme`: params + `path(SkSize)`
 * + equality), so any element can *be* a star, blob, polygon, or
 * squircle — fill, clip, and every outline-following decoration
 * (PathFormat, ContourWalk) trace the shape, hitTest() honors it, and
 * the node PRUNES like an unshapen one (ROADMAP §3: the incomparable
 * callable era cost 43.4 of 43.5 ms on the node that measured it).
 *
 * Every generator is also CALLABLE over a size, so it still converts to
 * an `OutlineFn` anywhere a raw path-over-size function is wanted — a
 * band spine, a TextPath baseline, your own wrapper. Going the other
 * way, a raw callable handed to `shape()` is the escape hatch that
 * never prunes; give your own generators `path(SkSize)` + `operator==`
 * to make them values.
 *
 * Generators compose through wrappers: `rounded(star(5), 8)` is a
 * five-point star with consistently rounded points — corners() for
 * silhouettes that have no box corners — and the wrapper is comparable
 * whenever what it wraps is.
 *
 * `edges()` runs the other way: it extracts the sub-contours of a
 * resolved outline that face a given box edge, so per-edge treatments
 * (stress item 9) are composition — `onEdges(Edge::Top, PathFormat…)`
 * — not new primitive types.
 */

#include "sigilcompose/Compose.h"

#include <algorithm>

#include <include/core/SkContourMeasure.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkCornerPathEffect.h>
#include <include/core/SkPathUtils.h>
#include <include/pathops/SkPathOps.h>
#include <include/utils/SkParsePath.h>

#include <cmath>
#include <cstdint>

namespace sigil::compose::shapes {

/** A silhouette generator: local-coordinate path over the node's
 *  laid-out size. The ESCAPE-HATCH spelling of what Element::shape()
 *  accepts — a raw callable never prunes; the generator values below
 *  do. Kept because helpers across the corpus return it, and because a
 *  hand-rolled curve has to start somewhere. */
using OutlineFn = std::function<SkPath(SkSize)>;

/** An outline from an SVG path-d string (SkParsePath) — trace a reference
 *  silhouette in any vector tool, paste the `d`, done. The path's bounds
 *  map onto the node's box (stretch by default; `preserveAspect` fits and
 *  centers instead). Parsed ONCE at call time; the parsed SkPath is a
 *  comparable value, so an svg() shape prunes like any generator. */
struct Svg {
  SkPath parsed;
  bool preserveAspect = false;
  bool operator==(const Svg &) const = default;
  SkPath path(SkSize size) const {
    const SkRect b = parsed.getBounds();
    if (b.isEmpty() || size.isEmpty())
      return parsed;
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

inline Svg svg(const char *d, bool preserveAspect = false) {
  SkPath parsed;
  if (auto result = SkParsePath::FromSVGString(d))
    parsed = std::move(*result);
  return Svg{std::move(parsed), preserveAspect};
}

// ---------------------------------------------------------------------------
// Generators

/** Regular N-gon inscribed in the box (first vertex up unless rotated;
 *  @p rotationDeg spins the whole figure). */
struct Polygon {
  int sides = 3;
  float rotationDeg = 0.0f;
  bool operator==(const Polygon &) const = default;
  SkPath path(SkSize s) const {
    const int n = std::max(sides, 3);
    const float cx = s.width() / 2, cy = s.height() / 2;
    const float base = rotationDeg * SK_FloatPI / 180 - SK_FloatPI / 2;
    SkPathBuilder b;
    for (int i = 0; i < n; ++i) {
      const float a = base + i * (2 * SK_FloatPI / n);
      const SkPoint p{cx + cx * std::cos(a), cy + cy * std::sin(a)};
      if (i == 0)
        b.moveTo(p);
      else
        b.lineTo(p);
    }
    b.close();
    return b.detach();
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Polygon polygon(int sides, float rotationDeg = 0.0f) {
  return Polygon{sides, rotationDeg};
}

/** N-pointed star inscribed in the box (first point up); inner
 *  vertices sit at @p innerRatio of the outer radius.
 *
 *  @p waist bows each arm edge INWARD along its own bisector, in units of
 *  the outer radius. 0 is the straight-chord star. Engraved and cut stars
 *  are almost never straight-chorded — Chladni's 1787 sound-figures
 *  narrow fast off the hub and then run as needles, and nine figures on
 *  that one plate wanted exactly this parameter. ~0.10–0.25 reads as
 *  engraved; negative bulges the arms instead (a compass rose). */
struct Star {
  int points = 5;
  float innerRatio = 0.5f;
  float waist = 0.0f;
  bool operator==(const Star &) const = default;
  SkPath path(SkSize s) const {
    const int n = std::max(points, 2) * 2;
    const float cx = s.width() / 2, cy = s.height() / 2;
    auto vertex = [&](int i) {
      const float r = (i % 2 == 0) ? 1.0f : innerRatio;
      const float a = -SK_FloatPI / 2 + i * (2 * SK_FloatPI / n);
      return SkPoint{cx + cx * r * std::cos(a), cy + cy * r * std::sin(a)};
    };
    SkPathBuilder b;
    b.moveTo(vertex(0));
    for (int i = 0; i < n; ++i) {
      const SkPoint from = vertex(i), to = vertex((i + 1) % n);
      if (waist == 0.0f) {
        b.lineTo(to);
        continue;
      }
      // Pull the edge's midpoint toward the centre along its own radius,
      // so both edges of an arm pinch symmetrically and the tip stays put.
      const SkPoint mid{(from.fX + to.fX) * 0.5f, (from.fY + to.fY) * 0.5f};
      const float dx = mid.fX - cx, dy = mid.fY - cy;
      b.quadTo({mid.fX - dx * waist, mid.fY - dy * waist}, to);
    }
    b.close();
    return b.detach();
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Star star(int points, float innerRatio = 0.5f, float waist = 0.0f) {
  return Star{points, innerRatio, waist};
}

/** The circle (ellipse, on a non-square box) inscribed in the box, with a
 *  chosen WINDING and start point.
 *
 *  Direction is not a detail on a text baseline — it decides which way
 *  the glyphs face. `onPath` orients to the tangent, so a clockwise ring
 *  puts glyph-up radially OUTWARD (Nightingale's 1858 plate) and a
 *  counter-clockwise one puts it INWARD (Chevreul's 1864 limb). Both are
 *  uniform engraver's conventions and they are opposite in sign, so
 *  exactly half of all ring inscriptions were writing their own
 *  `OutlineFn` because of a default nobody chose.
 *
 *  @p startIndex picks which of the oval's four extreme points the
 *  contour begins at, which is what `TextPath::at` measures from. It
 *  defaults to 1 because that is what Skia's own `addOval(rect, dir)`
 *  uses — so `circle(kCW)` is byte-for-byte the path `circle()` gives,
 *  and the oriented overload is a strict superset rather than a
 *  near-miss. (It was defaulted to 0 first and the two produced
 *  different paths, which a test caught immediately and would have been
 *  a nasty thing to discover from a drifting label.)
 *
 *  Exact conics either way — this is `addOval`, not a sampled
 *  polyline. */
struct Circle {
  SkPathDirection direction = SkPathDirection::kCW;
  unsigned startIndex = 1;
  bool operator==(const Circle &) const = default;
  SkPath path(SkSize s) const {
    SkPathBuilder b;
    b.addOval(SkRect::MakeWH(s.width(), s.height()), direction, startIndex);
    return b.detach();
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

/** `util::disc()` is the ELEMENT form (a pre-sized box centred on a
 *  point); this is the shape value, which is what onPath, the mask gates
 *  and the decorations take. */
inline Circle circle() { return Circle{}; }
inline Circle circle(SkPathDirection direction, unsigned startIndex = 1) {
  return Circle{direction, startIndex};
}

/** A ring: the inscribed circle with a concentric hole at @p innerRatio
 *  of the radius. Even-odd, so it fills as an annulus. */
struct Annulus {
  float innerRatio = 0.6f;
  bool operator==(const Annulus &) const = default;
  SkPath path(SkSize s) const {
    const float r = std::clamp(innerRatio, 0.0f, 0.999f);
    const SkRect outer = SkRect::MakeWH(s.width(), s.height());
    SkRect inner = outer;
    inner.inset(outer.width() * 0.5f * (1 - r), outer.height() * 0.5f * (1 - r));
    SkPathBuilder b;
    b.setFillType(SkPathFillType::kEvenOdd);
    b.addOval(outer);
    b.addOval(inner);
    return b.detach();
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Annulus annulus(float innerRatio = 0.6f) { return Annulus{innerRatio}; }

/** Superellipse |x|^e + |y|^e = 1 — the squircle. @p exponent 2 is an
 *  ellipse; 4–5 is the familiar app-icon softness; large values
 *  approach the rect. */
struct Squircle {
  float exponent = 4.0f;
  bool operator==(const Squircle &) const = default;
  SkPath path(SkSize s) const {
    const float e = std::max(exponent, 0.5f);
    const float cx = s.width() / 2, cy = s.height() / 2;
    constexpr int kSegments = 96;
    SkPathBuilder b;
    for (int i = 0; i < kSegments; ++i) {
      const float t = i * (2 * SK_FloatPI / kSegments);
      const float c = std::cos(t), si = std::sin(t);
      const float x = std::copysign(std::pow(std::abs(c), 2.0f / e), c);
      const float y = std::copysign(std::pow(std::abs(si), 2.0f / e), si);
      const SkPoint p{cx + cx * x, cy + cy * y};
      if (i == 0)
        b.moveTo(p);
      else
        b.lineTo(p);
    }
    b.close();
    return b.detach();
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Squircle squircle(float exponent = 4.0f) { return Squircle{exponent}; }

namespace detail {
/** Deterministic per-index noise in [-1, 1] (splitmix-style). */
inline float hashNoise(uint32_t seed, uint32_t i) {
  uint64_t z = (uint64_t(seed) << 32 | (i * 0x9e3779b9u)) + 0x9e3779b97f4a7c15ull;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  z ^= z >> 31;
  return (float)(z & 0xffffff) / (float)0x7fffff - 1.0f;
}
/** Localise the point where a property of a contour CHANGES, given a
 *  bracket that straddles it.
 *
 *  Every scanner that walks a contour at a stride has the same two
 *  halves: notice between two samples that something flipped, then
 *  narrow the bracket. This is the second half, once, and it exists
 *  because the first half's convention is a bug magnet: **the answer is
 *  `hi`, the first distance PROVEN past the transition — never the
 *  midpoint of the bracket.** Recording the midpoint puts the result up
 *  to stride/2 early, which was measured at 3 px on a corner scan with
 *  advance 24, diagnosed, fixed in one copy, and then written again from
 *  the shape of the surrounding code in a second one days later.
 *
 *  @param stillNear answers "is @p d still on the starting side?" for any
 *         distance in the bracket. A sample it cannot evaluate must
 *         answer `true`: that can only move `lo`, so it can never report
 *         a transition earlier than the truth. */
template <typename Pred>
inline float bisectTransition(float lo, float hi, Pred stillNear,
                              int iterations = 8) {
  for (int i = 0; i < iterations; ++i) {
    const float mid = (lo + hi) * 0.5f;
    if (stillNear(mid))
      lo = mid;
    else
      hi = mid;
  }
  return hi;
}

/** The one polyline sampler behind every parametric curve: evaluates
 *  @p f over t ∈ [t0, t1] in the UNIT frame (±1 spans the box) and
 *  scales onto the node's half-extents. */
template <typename F>
inline SkPath samplePolyline(const F &f, float t0, float t1, int samples,
                             bool close, SkSize s) {
  const int n = std::max(samples, 2);
  const float cx = s.width() * 0.5f, cy = s.height() * 0.5f;
  SkPathBuilder b;
  for (int i = 0; i <= n; ++i) {
    const float t = t0 + (t1 - t0) * ((float)i / (float)n);
    const SkPoint u = f(t);
    const SkPoint p{cx + cx * u.fX, cy + cy * u.fY};
    if (i == 0)
      b.moveTo(p);
    else
      b.lineTo(p);
  }
  if (close)
    b.close();
  return b.detach();
}

} // namespace detail

/** Organic closed blob: @p lobes control points on the inscribed
 *  ellipse, each pushed in/out by up to @p amplitude (fraction of the
 *  radius) of seeded deterministic noise, joined by a smooth
 *  Catmull-Rom loop. Same seed → same blob, every frame, every run —
 *  chaos you can cache. */
struct Blob {
  uint32_t seed = 0;
  float amplitude = 0.18f;
  int lobes = 8;
  bool operator==(const Blob &) const = default;
  SkPath path(SkSize s) const {
    const int n = std::max(lobes, 3);
    const float cx = s.width() / 2, cy = s.height() / 2;
    std::vector<SkPoint> pts((size_t)n);
    for (int i = 0; i < n; ++i) {
      const float a = -SK_FloatPI / 2 + i * (2 * SK_FloatPI / n);
      const float r =
          1.0f - amplitude * (0.5f + 0.5f * detail::hashNoise(seed, (uint32_t)i));
      pts[(size_t)i] = {cx + cx * r * std::cos(a), cy + cy * r * std::sin(a)};
    }
    // Catmull-Rom → cubic Béziers around the loop.
    SkPathBuilder b;
    b.moveTo(pts[0]);
    for (int i = 0; i < n; ++i) {
      const SkPoint &p0 = pts[(size_t)((i - 1 + n) % n)];
      const SkPoint &p1 = pts[(size_t)(i % n)];
      const SkPoint &p2 = pts[(size_t)((i + 1) % n)];
      const SkPoint &p3 = pts[(size_t)((i + 2) % n)];
      const SkPoint c1{p1.x() + (p2.x() - p0.x()) / 6.0f,
                       p1.y() + (p2.y() - p0.y()) / 6.0f};
      const SkPoint c2{p2.x() - (p3.x() - p1.x()) / 6.0f,
                       p2.y() - (p3.y() - p1.y()) / 6.0f};
      b.cubicTo(c1, c2, p2);
    }
    b.close();
    return b.detach();
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Blob blob(uint32_t seed, float amplitude = 0.18f, int lobes = 8) {
  return Blob{seed, amplitude, lobes};
}

/** A circular arc inscribed in the box, STARTING at @p startDeg (Skia
 *  canvas convention: 0° = +x, clockwise) and sweeping @p sweepDeg — the
 *  path begins at the arc's start, so `spans::upTo(sweep/360)`-style reveals
 *  and orbit connectors (the PoE Orbit idiom, REFERENCES.md §5) need no
 *  wrap math. Stroke it; an unstroked open arc has no fillable area. */
struct Arc {
  float startDeg = 0.0f;
  float sweepDeg = 359.9f;
  bool operator==(const Arc &) const = default;
  SkPath path(SkSize s) const {
    SkPathBuilder b;
    b.addArc(SkRect::MakeWH(s.width(), s.height()), startDeg,
             std::min(sweepDeg, 359.9f));
    return b.detach();
  }
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
  bool operator==(const Sector &) const = default;
  SkPath path(SkSize s) const {
    const float cx = s.width() * 0.5f, cy = s.height() * 0.5f;
    // arcTo swallows a full turn, so sector(start, 360, inner) — the most
    // obvious call there is, a gauge's annular TRACK — silently drew
    // nothing. Clamp inside the primitive rather than at every call site.
    const float sweep = std::clamp(sweepDeg, -359.99f, 359.99f);
    const float inner = std::clamp(innerRatio, 0.0f, 0.999f);
    const SkRect outerBox = SkRect::MakeWH(s.width(), s.height());
    SkPathBuilder b;
    if (inner <= 0.0f) {
      b.moveTo(cx, cy);
      b.arcTo(outerBox, startDeg, sweep, false);
      b.close();
      return b.detach();
    }
    const SkRect innerBox =
        SkRect::MakeXYWH(cx - cx * inner, cy - cy * inner,
                         s.width() * inner, s.height() * inner);
    b.arcTo(outerBox, startDeg, sweep, true);
    b.arcTo(innerBox, startDeg + sweep, -sweep, false);
    b.close();
    return b.detach();
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Sector sector(float startDeg, float sweepDeg, float innerRatio = 0.0f) {
  return Sector{startDeg, sweepDeg, innerRatio};
}

/** A parallelogram leaning by @p skewDeg (the ATLUS slash, REFERENCES.md
 *  §1: P3R ≈ −12°, P5R ≈ −20°): the top edge shifts by h·tan(skew) relative
 *  to the bottom, staying inside the box. */
struct Parallelogram {
  float skewDeg = 0.0f;
  bool operator==(const Parallelogram &) const = default;
  SkPath path(SkSize s) const {
    const float lean = std::tan(skewDeg * 0.017453293f) * s.height();
    const float l = std::max(0.0f, -lean), r = std::max(0.0f, lean);
    SkPathBuilder b;
    b.moveTo(l, 0);
    b.lineTo(s.width() - r + l, 0); // top edge (shifted)
    b.lineTo(s.width() - l, s.height());
    b.lineTo(r - l >= 0 ? r : 0, s.height());
    b.close();
    return b.detach();
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Parallelogram parallelogram(float skewDeg) {
  return Parallelogram{skewDeg};
}

// ---------------------------------------------------------------------------
// Parametric curves
//
// Everything above generates a closed SHAPE from parameters. A curve
// DEFINED by a parameter — Lissajous, harmonograph, rose, epitrochoid,
// spirograph, orbit trace, phase portrait — had no generator at all, so
// every study that needed one wrote the same SkPathBuilder loop inside
// its own outline() lambda: the Vertigo titles, the Nightingale rings,
// and (predictably) every diagram sketch since.
//
// These evaluate in a UNIT frame centred on the box — x and y in [-1, 1]
// — and are then scaled onto the node's half-extents, so a curve keeps
// its proportions when the box changes and `amplitude` means the same
// thing everywhere.
//
// The named families are comparable values like every other generator.
// The raw `parametric(fn, …)` holds YOUR callable, which cannot compare —
// key it (`parametric("orbit-a", fn, …)`) to make it a value: the key
// plus the sampling parameters become its identity, on the author's
// contract that one key means one function.

/** Samples @p f over t ∈ [t0, t1] into a polyline. @p f returns UNIT
 *  coordinates (±1 spans the box); @p samples is the segment count, and
 *  @p close joins the last point back to the first.
 *
 *      .shape(shapes::parametric([](float t) {
 *        return SkPoint{std::cos(3 * t), std::sin(2 * t)};
 *      }, 0, 2 * SK_FloatPI, 720))
 *
 *  UNKEYED: the callable is the whole identity and it cannot compare, so
 *  a node shaped by this re-records every render() — the escape hatch.
 *  The keyed overload below is the prunable spelling. */
struct Parametric {
  std::function<SkPoint(float)> f;
  float t0 = 0.0f;
  float t1 = 1.0f;
  int samples = 512;
  bool close = false;
  SkPath path(SkSize s) const {
    return detail::samplePolyline(f, t0, t1, samples, close, s);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Parametric parametric(std::function<SkPoint(float)> f, float t0,
                             float t1, int samples = 512, bool close = false) {
  return Parametric{std::move(f), t0, t1, samples, close};
}

/** The KEYED parametric: comparable by (key, t0, t1, samples, close), so
 *  the node prunes. The key is the FUNCTION's identity — the author's
 *  contract is that one key always names one curve; reusing a key for a
 *  different `f` silently keeps whichever recorded first. Change the key
 *  (or fold the changing number into a parameter of a named family
 *  below) when the curve changes. */
struct KeyedParametric {
  std::string key;
  std::function<SkPoint(float)> f;
  float t0 = 0.0f;
  float t1 = 1.0f;
  int samples = 512;
  bool close = false;
  bool operator==(const KeyedParametric &o) const {
    return key == o.key && t0 == o.t0 && t1 == o.t1 &&
           samples == o.samples && close == o.close;
  }
  SkPath path(SkSize s) const {
    return detail::samplePolyline(f, t0, t1, samples, close, s);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline KeyedParametric parametric(std::string_view key,
                                  std::function<SkPoint(float)> f, float t0,
                                  float t1, int samples = 512,
                                  bool close = false) {
  return KeyedParametric{std::string(key), std::move(f), t0, t1, samples,
                         close};
}

/** Lissajous figure: x = sin(a·t + δ), y = sin(b·t). The ratio a:b picks
 *  the family (1:1 an ellipse, 3:2 the classic pretzel, 5:4 a tight
 *  weave) and δ its phase — the same two numbers a physical harmonograph
 *  is set to. `turns` is how many 2π the parameter runs for; the curve
 *  closes when a:b is rational and `turns` covers the period. */
struct Lissajous {
  float a = 3.0f;
  float b = 2.0f;
  float deltaDeg = 0.0f;
  float turns = 1.0f;
  int samples = 720;
  bool operator==(const Lissajous &) const = default;
  SkPath path(SkSize s) const {
    const float delta = deltaDeg * SK_FloatPI / 180.0f;
    return detail::samplePolyline(
        [fa = a, fb = b, delta](float t) {
          return SkPoint{std::sin(fa * t + delta), std::sin(fb * t)};
        },
        0.0f, turns * 2.0f * SK_FloatPI, samples, false, s);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Lissajous lissajous(float a, float b, float deltaDeg = 0.0f,
                           float turns = 1.0f, int samples = 720) {
  return Lissajous{a, b, deltaDeg, turns, samples};
}

/** A harmonograph: a Lissajous whose amplitudes DECAY, which is what
 *  makes a real pen-and-pendulum figure spiral inward instead of
 *  retracing one closed rosette. @p damping is the exponential rate per
 *  unit t; @p precession spins the whole figure as it draws (the rotating
 *  turntable under John Whitney's pendulum). */
struct Harmonograph {
  float a = 3.0f;
  float b = 2.0f;
  float deltaDeg = 0.0f;
  float damping = 0.05f;
  float precession = 0.0f;
  float turns = 6.0f;
  int samples = 2000;
  bool operator==(const Harmonograph &) const = default;
  SkPath path(SkSize s) const {
    const float delta = deltaDeg * SK_FloatPI / 180.0f;
    return detail::samplePolyline(
        [fa = a, fb = b, delta, fdamping = damping,
         fprecession = precession](float t) {
          const float env = std::exp(-fdamping * t);
          const float x = env * std::sin(fa * t + delta);
          const float y = env * std::sin(fb * t);
          if (fprecession == 0.0f)
            return SkPoint{x, y};
          const float th = fprecession * t;
          const float c = std::cos(th), sn = std::sin(th);
          return SkPoint{x * c - y * sn, x * sn + y * c};
        },
        0.0f, turns * 2.0f * SK_FloatPI, samples, false, s);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Harmonograph harmonograph(float a, float b, float deltaDeg = 0.0f,
                                 float damping = 0.05f, float precession = 0.0f,
                                 float turns = 6.0f, int samples = 2000) {
  return Harmonograph{a, b, deltaDeg, damping, precession, turns, samples};
}

/** Rose (rhodonea) r = cos(k·θ). Integer @p k gives k petals when k is
 *  odd and 2k when even; rational k gives the multi-lobed forms. */
struct Rose {
  float k = 3.0f;
  float turns = 1.0f;
  int samples = 720;
  bool operator==(const Rose &) const = default;
  SkPath path(SkSize s) const {
    return detail::samplePolyline(
        [fk = k](float th) {
          const float r = std::cos(fk * th);
          return SkPoint{r * std::cos(th), r * std::sin(th)};
        },
        0.0f, turns * 2.0f * SK_FloatPI, samples, false, s);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Rose rose(float k, float turns = 1.0f, int samples = 720) {
  return Rose{k, turns, samples};
}

/** Spiral from the centre outward. @p logarithmic switches Archimedean
 *  (even spacing — a clock spring, a record groove) for logarithmic
 *  (constant angle — a nautilus, a galaxy arm). */
struct Spiral {
  float turns = 3.0f;
  bool logarithmic = false;
  float growth = 0.25f;
  int samples = 720;
  bool operator==(const Spiral &) const = default;
  SkPath path(SkSize s) const {
    const float total = turns * 2.0f * SK_FloatPI;
    return detail::samplePolyline(
        [flog = logarithmic, fgrowth = growth, total](float th) {
          const float r = flog
                              ? std::exp(fgrowth * th) / std::exp(fgrowth * total)
                              : th / total;
          return SkPoint{r * std::cos(th), r * std::sin(th)};
        },
        0.0f, total, samples, false, s);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Spiral spiral(float turns = 3.0f, bool logarithmic = false,
                     float growth = 0.25f, int samples = 720) {
  return Spiral{turns, logarithmic, growth, samples};
}

/** Epitrochoid / hypotrochoid — the spirograph pair. A circle of radius
 *  @p r rolls around one of radius @p R (outside for an epitrochoid,
 *  inside when @p inside), with the pen @p d from its centre. Everything
 *  is normalised so the figure fills the box. */
struct Trochoid {
  float R = 5.0f;
  float r = 3.0f;
  float d = 5.0f;
  bool inside = false;
  float turns = 1.0f;
  int samples = 1440;
  bool operator==(const Trochoid &) const = default;
  SkPath path(SkSize s) const {
    const float sign = inside ? -1.0f : 1.0f;
    const float sum = R + sign * r;
    const float extent = std::max(std::abs(sum) + std::abs(d), 1e-3f);
    return detail::samplePolyline(
        [fR = R, fr = r, fd = d, sign, sum, extent](float t) {
          const float k = sum / std::max(fr, 1e-3f);
          return SkPoint{(sum * std::cos(t) - sign * fd * std::cos(k * t)) / extent,
                         (sum * std::sin(t) - fd * std::sin(k * t)) / extent};
        },
        0.0f, turns * 2.0f * SK_FloatPI, samples, false, s);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Trochoid trochoid(float R, float r, float d, bool inside = false,
                         float turns = 1.0f, int samples = 1440) {
  return Trochoid{R, r, d, inside, turns, samples};
}

// ---------------------------------------------------------------------------
// Wrappers — generators over generators

/** Wraps any shape so every sharp corner rounds with a consistent
 *  radius — corners() for arbitrary silhouettes:
 *  `.shape(rounded(star(5), 8))`. Comparable whenever the wrapped shape
 *  is (a wrapped raw callable stays the escape hatch). */
struct Rounded {
  Shape inner;
  float radius = 0.0f;
  bool operator==(const Rounded &) const = default;
  SkPath path(SkSize s) const {
    SkPath src = inner(s);
    if (radius <= 0)
      return src;
    SkPathBuilder dst;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(radius);
        fx && fx->filterPath(&dst, src, &rec))
      return dst.detach();
    return src;
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Rounded rounded(Shape shape, float radius) {
  return Rounded{std::move(shape), radius};
}

// ---------------------------------------------------------------------------
// Corner geometry — the shapes a frame is actually cut to
//
// `corners()` rounds, and rounding is the ONE corner treatment the kernel
// offers. Modern game UI is built on the other two: the 45° CHAMFER (every
// panel in Cyberpunk 2077, Destiny, Deus Ex, XCOM; the "cut corner" that
// reads as machined metal) and the rectangular NOTCH (the bitten corner
// that reads as a stencil or a fixing lug). Both were hand-built with
// SkPathBuilder in the corpus, per panel, which is where a mask enum
// belongs instead: a chamfer on two corners and square on the other two is
// the commonest real case, and it is not expressible by a radius at all.

/** Which corners a treatment applies to. Clockwise from the top-left. */
enum class Corner : uint8_t {
  TopLeft = 1,
  TopRight = 2,
  BottomRight = 4,
  BottomLeft = 8,
  All = 15,
  /** The two on one diagonal — the asymmetric cut that reads as a tab. */
  Diagonal = 5,      // TopLeft | BottomRight
  AntiDiagonal = 10, // TopRight | BottomLeft
};
constexpr Corner operator|(Corner a, Corner b) {
  return Corner(uint8_t(a) | uint8_t(b));
}
constexpr bool has(Corner mask, Corner c) {
  return (uint8_t(mask) & uint8_t(c)) != 0;
}

/** The CHAMFERED box: each selected corner replaced by a 45° cut of @p cut
 *  px. Clamped to half the short side, so an over-large cut degenerates to
 *  a diamond rather than an inside-out path. */
struct Chamfered {
  float cut = 0.0f;
  Corner mask = Corner::All;
  bool operator==(const Chamfered &) const = default;
  SkPath path(SkSize s) const {
    const float w = s.width(), h = s.height();
    const float c = std::clamp(cut, 0.0f, std::min(w, h) * 0.5f);
    SkPathBuilder b;
    if (has(mask, Corner::TopLeft))
      b.moveTo(c, 0);
    else
      b.moveTo(0, 0);
    if (has(mask, Corner::TopRight)) {
      b.lineTo(w - c, 0);
      b.lineTo(w, c);
    } else {
      b.lineTo(w, 0);
    }
    if (has(mask, Corner::BottomRight)) {
      b.lineTo(w, h - c);
      b.lineTo(w - c, h);
    } else {
      b.lineTo(w, h);
    }
    if (has(mask, Corner::BottomLeft)) {
      b.lineTo(c, h);
      b.lineTo(0, h - c);
    } else {
      b.lineTo(0, h);
    }
    if (has(mask, Corner::TopLeft))
      b.lineTo(0, c);
    b.close();
    return b.detach();
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Chamfered chamfered(float cut, Corner mask = Corner::All) {
  return Chamfered{cut, mask};
}

/** The NOTCHED box: each selected corner carries a rectangular bite @p
 *  notchWidth wide and @p depth deep — the stencil corner, the fixing lug,
 *  the Aliens-console cut. */
struct Notched {
  float notchWidth = 0.0f;
  float depth = 0.0f;
  Corner mask = Corner::All;
  bool operator==(const Notched &) const = default;
  SkPath path(SkSize s) const {
    const float w = s.width(), h = s.height();
    const float n = std::clamp(notchWidth, 0.0f, std::min(w, h) * 0.45f);
    const float d = std::clamp(depth, 0.0f, std::min(w, h) * 0.45f);
    SkPathBuilder b;
    if (has(mask, Corner::TopLeft))
      b.moveTo(n, 0);
    else
      b.moveTo(0, 0);
    if (has(mask, Corner::TopRight)) {
      b.lineTo(w - n, 0);
      b.lineTo(w - n, d);
      b.lineTo(w, d);
    } else {
      b.lineTo(w, 0);
    }
    if (has(mask, Corner::BottomRight)) {
      b.lineTo(w, h - d);
      b.lineTo(w - n, h - d);
      b.lineTo(w - n, h);
    } else {
      b.lineTo(w, h);
    }
    if (has(mask, Corner::BottomLeft)) {
      b.lineTo(n, h);
      b.lineTo(n, h - d);
      b.lineTo(0, h - d);
    } else {
      b.lineTo(0, h);
    }
    if (has(mask, Corner::TopLeft)) {
      b.lineTo(0, d);
      b.lineTo(n, d);
    }
    b.close();
    return b.detach();
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Notched notched(float notchWidth, float depth,
                       Corner mask = Corner::All) {
  return Notched{notchWidth, depth, mask};
}

// ---------------------------------------------------------------------------
// Per-edge extraction (stress item 9)

enum class Edge : uint8_t {
  Top = 1,
  Right = 2,
  Bottom = 4,
  Left = 8,
  All = 15,
};
constexpr Edge operator|(Edge a, Edge b) {
  return Edge(uint8_t(a) | uint8_t(b));
}
constexpr bool has(Edge mask, Edge e) {
  return (uint8_t(mask) & uint8_t(e)) != 0;
}

/** Extracts the sub-contours of @p outline that face the selected box
 *  edges. Facing is classified against the outline's bounds center
 *  (diagonal split, so rounded-rect corner arcs divide naturally
 *  between their two edges). Exact geometry via SkContourMeasure
 *  segment extraction; @p step is the classification sampling length
 *  in px. */
inline SkPath edges(const SkPath &outline, Edge mask, float step = 3.0f) {
  const SkRect bounds = outline.getBounds();
  const float cx = bounds.centerX(), cy = bounds.centerY();
  const float hw = std::max(bounds.width() / 2, 1.0f);
  const float hh = std::max(bounds.height() / 2, 1.0f);
  auto classify = [&](SkPoint p) {
    const float nx = (p.x() - cx) / hw, ny = (p.y() - cy) / hh;
    if (std::abs(nx) > std::abs(ny))
      return nx > 0 ? Edge::Right : Edge::Left;
    return ny > 0 ? Edge::Bottom : Edge::Top;
  };

  SkPathBuilder out;
  SkContourMeasureIter iter(outline, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float length = contour->length();
    if (length <= 0)
      continue;
    const int samples = std::max(8, (int)std::ceil(length / step));
    float runStart = 0.0f;
    SkPoint pos;
    if (!contour->getPosTan(0, &pos, nullptr))
      continue;
    Edge runEdge = classify(pos);
    auto flushRun = [&](float endD) {
      if (has(mask, runEdge) && endD > runStart) {
        SkPathBuilder segment;
        if (contour->getSegment(runStart, endD, &segment, true))
          out.addPath(segment.detach());
      }
    };
    for (int i = 1; i <= samples; ++i) {
      const float d = length * (float)i / (float)samples;
      if (!contour->getPosTan(std::min(d, length), &pos, nullptr))
        continue;
      const Edge e = classify(pos);
      if (e != runEdge) {
        // The boundary between the previous sample and this one. Shared
        // with lines::detail::findCorners — the SCANNERS differ (that one
        // brackets a tangent break, this one a quadrant change) but the
        // refinement is the same operation, and it is the half that has
        // been got wrong twice.
        const float at = detail::bisectTransition(
            length * (float)(i - 1) / (float)samples, d, [&](float mid) {
              SkPoint mp;
              return contour->getPosTan(mid, &mp, nullptr) &&
                     classify(mp) == runEdge;
            });
        flushRun(at);
        runStart = at;
        runEdge = e;
      }
    }
    flushRun(length);
  }
  return out.detach();
}

/** Decoration adaptor: runs @p inner with the PaintContext outline
 *  replaced by the selected edges — any primitive (PathFormat,
 *  ContourWalk, custom programs) becomes a per-edge treatment. */
struct EdgeSlice {
  Edge mask = Edge::All;
  Decoration inner{PaintProgram{}};
  float step = 3.0f;

  /** Forwarded, or an inner weave's strand::from(key) would never be
   *  registered for the derive pass (BorrowingDecoration). */
  std::vector<std::string> borrows() const { return inner.borrows(); }
  float reach() const { return inner.reach(); }

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    PaintContext local = ctx;
    local.outline = edges(ctx.outline, mask, step);
    inner.paint(canvas, local);
  }
  bool isAnimated() const { return inner.isAnimated(); }
  /** Structural equality, so a static per-edge border prunes like any
   *  other decoration. It did not have one — `Inset`, the sibling
   *  adaptor twelve lines below, always did — so every
   *  `shapes::onEdges(...)` in the corpus compared unequal and
   *  re-recorded its subtree EVERY FRAME. The edge extraction itself is
   *  the expensive half (a contour walk with a binary search at each
   *  boundary), and it was being redone at 60 Hz for chrome that never
   *  changed. Nothing about the type prevented this; the operator was
   *  simply never written. */
  bool operator==(const EdgeSlice &o) const {
    return mask == o.mask && step == o.step && inner == o.inner;
  }
};

inline EdgeSlice onEdges(Edge mask, Decoration inner, float step = 3.0f) {
  return EdgeSlice{mask, std::move(inner), step};
}

/** Runs a decoration against an INSET (or outset) copy of the node's
 *  outline — EdgeSlice's sibling, and the same trick: rewrite
 *  `PaintContext::outline` and delegate.
 *
 *  "The same bevel again, six pixels in" is the entire vocabulary of
 *  nested chrome — 2Advanced's own SWF names two panel classes,
 *  FSingleBevelPanel and FDoubleBevelPanel, and the second is literally
 *  the first run twice at two insets. Without this, every nested frame is
 *  either a second element or a bespoke decoration struct.
 *
 *  Positive `px` shrinks; negative grows. Implemented as a stroke-and-fill
 *  offset of the resolved outline, so it follows any silhouette — a
 *  chamfered panel, a star, a blob — not just rectangles. */
struct Inset {
  float px = 0;
  Decoration inner{PaintProgram{}};

  /** Forwarded, or an inner weave's strand::from(key) would never be
   *  registered for the derive pass (BorrowingDecoration). */
  std::vector<std::string> borrows() const { return inner.borrows(); }
  float reach() const { return inner.reach(); }

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    PaintContext local = ctx;
    if (px != 0) {
      SkPaint offset;
      offset.setStyle(SkPaint::kStroke_Style); // the RING, not the grown shape
      offset.setStrokeWidth(std::abs(px) * 2.0f);
      offset.setStrokeJoin(SkPaint::kMiter_Join);
      // The stroke-and-fill of the outline is the RING of width 2|px|
      // straddling it. Subtracting that ring shrinks the silhouette;
      // unioning it grows the silhouette by the same amount.
      const SkPath ring = skpathutils::FillPathWithPaint(ctx.outline, offset);
      SkPath result;
      if (Op(ctx.outline, ring,
             px > 0 ? SkPathOp::kDifference_SkPathOp
                    : SkPathOp::kUnion_SkPathOp,
             &result))
        local.outline = std::move(result);
    }
    inner.paint(canvas, local);
  }
  bool isAnimated() const { return inner.isAnimated(); }
  bool operator==(const Inset &o) const {
    return px == o.px && inner == o.inner;
  }
};

inline Inset inset(float px, Decoration inner) {
  return Inset{px, std::move(inner)};
}

/** An arrow along +x, inscribed in the box: a shaft of `shaftFrac` of the
 *  height and a head of `headFrac` of the width. Every HUD, gizmo,
 *  manoeuvre node and diagram draws one, and every one of them was
 *  hand-built with SkPathBuilder. */
struct Arrow {
  float shaftFrac = 0.34f;
  float headFrac = 0.42f;
  bool operator==(const Arrow &) const = default;
  SkPath path(SkSize s) const {
    const float w = s.width(), h = s.height();
    const float half = std::clamp(shaftFrac, 0.02f, 1.0f) * h * 0.5f;
    const float head = std::clamp(headFrac, 0.05f, 1.0f) * w;
    const float cy = h * 0.5f;
    SkPathBuilder b;
    b.moveTo(0, cy - half);
    b.lineTo(w - head, cy - half);
    b.lineTo(w - head, 0);
    b.lineTo(w, cy);
    b.lineTo(w - head, h);
    b.lineTo(w - head, cy + half);
    b.lineTo(0, cy + half);
    b.close();
    return b.detach();
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Arrow arrow(float shaftFrac = 0.34f, float headFrac = 0.42f) {
  return Arrow{shaftFrac, headFrac};
}

} // namespace sigil::compose::shapes
