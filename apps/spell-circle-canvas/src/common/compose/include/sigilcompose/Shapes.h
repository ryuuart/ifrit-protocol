#pragma once

/** @file
 * SigilCompose shape kit — the free-form answer to "everything is a box".
 *
 * An extension over the kernel's shape() seam. Every generator here is a
 * COMPARABLE VALUE — parameters, a `path(SkSize)` member and an
 * `operator==` — so any element can *be* a star, blob, polygon or
 * squircle: fill, clip and every outline-following decoration (PathFormat,
 * ContourWalk) trace the shape, hitTest() honours it, and the node PRUNES
 * exactly like an unshapen one.
 *
 * That comparability is the whole point of the value form. A raw callable
 * handed to `shape()` is the escape hatch: it can never compare equal, so
 * its node is re-patched on every describe and its subtree re-records.
 * Give your own generators `path(SkSize)` plus `operator==` and they
 * become values with the same standing as these.
 *
 * Every generator is also CALLABLE over a size, so it converts to an
 * `OutlineFn` anywhere a raw path-over-size function is wanted — a band
 * spine, a TextPath baseline, your own wrapper.
 *
 * Generators compose through wrappers: `rounded(star(5), 8)` is a
 * five-point star with consistently rounded points, which is what
 * corners() cannot do for a silhouette that has no box corners. A wrapper
 * is comparable whenever what it wraps is.
 *
 * `edges()` runs the other way: it extracts the sub-contours of a resolved
 * outline that face a given box edge, so a per-edge treatment is
 * composition — `onEdges(Edge::Top, PathFormat…)` — rather than a new
 * primitive type.
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
  const geometry::Polyline unit = geometry::sample(
      [&](float t) { return geometry::fromSk(f(t)); }, t0, t1, samples, close);
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

// ---------------------------------------------------------------------------
// Parametric curves
//
// Everything above generates a closed SHAPE from parameters. These
// generate a curve DEFINED by a parameter — Lissajous, harmonograph,
// rose, epitrochoid, spirograph, orbit trace, phase portrait.
//
// They evaluate in a UNIT frame centred on the box — x and y in [-1, 1] —
// and are then scaled onto the node's half-extents, so a curve keeps its
// proportions when the box changes and an amplitude means the same thing
// everywhere.
//
// The named families are comparable values like every other generator.
// The raw `parametric(fn, …)` holds YOUR callable, which cannot compare —
// key it (`parametric("orbit-a", fn, …)`) to make it a value: the key plus
// the sampling parameters become its identity, on the author's contract
// that one key means one function.

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
  bool operator==(const KeyedParametric& o) const {
    return key == o.key && t0 == o.t0 && t1 == o.t1 && samples == o.samples &&
           close == o.close;
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
  return KeyedParametric{std::string(key), std::move(f), t0, t1,
                         samples,          close};
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
  bool operator==(const Lissajous&) const = default;
  SkPath path(SkSize s) const;
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
  bool operator==(const Harmonograph&) const = default;
  SkPath path(SkSize s) const;
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
  bool operator==(const Rose&) const = default;
  SkPath path(SkSize s) const;
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
  bool operator==(const Spiral&) const = default;
  SkPath path(SkSize s) const;
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
  bool operator==(const Trochoid&) const = default;
  SkPath path(SkSize s) const;
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
  bool operator==(const Rounded&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Rounded rounded(Shape shape, float radius) {
  return Rounded{std::move(shape), radius};
}

// ---------------------------------------------------------------------------
// Corner geometry — the shapes a frame is actually cut to
//
// `corners()` rounds, and rounding is the ONE corner treatment the kernel
// offers. The other two are the 45° CHAMFER — the cut corner that reads as
// machined metal — and the rectangular NOTCH, the bitten corner that reads
// as a stencil or a fixing lug. Both take a per-corner MASK rather than a
// single number, because a chamfer on two corners and square on the other
// two is the common case and no radius expresses it.

/** Which corners a treatment applies to. Clockwise from the top-left. */
enum class Corner : uint8_t {
  TopLeft = 1,
  TopRight = 2,
  BottomRight = 4,
  BottomLeft = 8,
  All = 15,
  /** The two on one diagonal — the asymmetric cut that reads as a tab. */
  Diagonal = 5,       // TopLeft | BottomRight
  AntiDiagonal = 10,  // TopRight | BottomLeft
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
  bool operator==(const Chamfered&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Chamfered chamfered(float cut, Corner mask = Corner::All) {
  return Chamfered{cut, mask};
}

/** The NOTCHED box: each selected corner carries a rectangular bite @p
 *  notchWidth wide and @p depth deep — the stencil corner, the fixing lug.
 *  Both are clamped to 0.45 of the shorter side. */
struct Notched {
  float notchWidth = 0.0f;
  float depth = 0.0f;
  Corner mask = Corner::All;
  bool operator==(const Notched&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Notched notched(float notchWidth, float depth,
                       Corner mask = Corner::All) {
  return Notched{notchWidth, depth, mask};
}

// ---------------------------------------------------------------------------
// Per-edge extraction

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
SkPath edges(const SkPath& outline, Edge mask, float step = 3.0f);

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

  void paint(SkCanvas& canvas, const PaintContext& ctx) const;
  bool isAnimated() const { return inner.isAnimated(); }
  /** Structural equality, so a static per-edge border prunes like any
   *  other decoration. Without it every `onEdges(...)` compares unequal
   *  and re-records its subtree on every describe, redoing the edge
   *  extraction — a contour walk with a binary search at each run boundary
   *  — for chrome that never changed. Any adaptor added beside this one
   *  needs the same operator for the same reason. */
  bool operator==(const EdgeSlice& o) const {
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
 *  "The same bevel again, six pixels in" is the whole vocabulary of nested
 *  chrome, and without this every nested frame is either a second element
 *  or a bespoke decoration struct.
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

  void paint(SkCanvas& canvas, const PaintContext& ctx) const;
  bool isAnimated() const { return inner.isAnimated(); }
  bool operator==(const Inset& o) const {
    return px == o.px && inner == o.inner;
  }
};

inline Inset inset(float px, Decoration inner) {
  return Inset{px, std::move(inner)};
}

/** An arrow along +x, inscribed in the box: a shaft of `shaftFrac` of the
 *  height and a head of `headFrac` of the width. */
struct Arrow {
  float shaftFrac = 0.34f;
  float headFrac = 0.42f;
  bool operator==(const Arrow&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Arrow arrow(float shaftFrac = 0.34f, float headFrac = 0.42f) {
  return Arrow{shaftFrac, headFrac};
}

}  // namespace sigil::compose::shapes
