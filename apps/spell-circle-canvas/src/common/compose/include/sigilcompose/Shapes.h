#pragma once

/** @file
 * SigilCompose shape kit — the free-form answer to "everything is a
 * box". An extension over the kernel's outline() seam: every generator
 * here returns an OutlineFn (a path over the laid-out size), so any
 * element can *be* a star, blob, polygon, or squircle — fill, clip,
 * and every outline-following decoration (PathFormat, ContourWalk)
 * trace the shape, and hitTest() honors it.
 *
 * Generators compose through wrappers: `rounded(star(5), 8)` is a
 * five-point star with consistently rounded points — corners() for
 * silhouettes that have no box corners.
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

/** An outline from an SVG path-d string (SkParsePath) — trace a reference
 *  silhouette in any vector tool, paste the `d`, done. The path's bounds
 *  map onto the node's box (stretch by default; `preserveAspect` fits and
 *  centers instead). Parsed ONCE at call time; the generator is an
 *  incomparable callable like every outline() — memo the node or keep it
 *  pointer-stable to prune. */
inline std::function<SkPath(SkSize)> svg(const char *d,
                                         bool preserveAspect = false) {
  SkPath parsed;
  if (auto result = SkParsePath::FromSVGString(d))
    parsed = std::move(*result);
  return [parsed, preserveAspect](SkSize size) {
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
  };
}

/** A silhouette generator: local-coordinate path over the node's
 *  laid-out size. What Element::outline() accepts. */
using OutlineFn = std::function<SkPath(SkSize)>;

// ---------------------------------------------------------------------------
// Generators

/** Regular N-gon inscribed in the box (first vertex up unless rotated;
 *  @p rotationDeg spins the whole figure). */
inline OutlineFn polygon(int sides, float rotationDeg = 0.0f) {
  return [sides, rotationDeg](SkSize s) {
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
  };
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
inline OutlineFn star(int points, float innerRatio = 0.5f,
                      float waist = 0.0f) {
  return [points, innerRatio, waist](SkSize s) {
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
  };
}

/** The circle (ellipse, on a non-square box) inscribed in the box.
 *
 *  Trivial, and missing anyway — the KSP study hand-wrote it, so did
 *  ScenesSkillTree, and `onPath`'s own docs reach for one. `util::disc()`
 *  is the ELEMENT form (a pre-sized box centred on a point); this is the
 *  OutlineFn, which is what onPath, trim and the decorations take. */
inline OutlineFn circle() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.addOval(SkRect::MakeWH(s.width(), s.height()));
    return b.detach();
  };
}

/** The same circle with a chosen WINDING and start point.
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
 *  and this overload is a strict superset rather than a near-miss. (I
 *  defaulted it to 0 first and the two produced different paths, which a
 *  test caught immediately and would have been a nasty thing to
 *  discover from a drifting label.)
 *
 *  Exact conics either way — this is `addOval`, not a sampled
 *  polyline. */
inline OutlineFn circle(SkPathDirection direction, unsigned startIndex = 1) {
  return [direction, startIndex](SkSize s) {
    SkPathBuilder b;
    b.addOval(SkRect::MakeWH(s.width(), s.height()), direction, startIndex);
    return b.detach();
  };
}

/** A ring: the inscribed circle with a concentric hole at @p innerRatio
 *  of the radius. Even-odd, so it fills as an annulus. */
inline OutlineFn annulus(float innerRatio = 0.6f) {
  return [innerRatio](SkSize s) {
    const float r = std::clamp(innerRatio, 0.0f, 0.999f);
    const SkRect outer = SkRect::MakeWH(s.width(), s.height());
    SkRect inner = outer;
    inner.inset(outer.width() * 0.5f * (1 - r), outer.height() * 0.5f * (1 - r));
    SkPathBuilder b;
    b.setFillType(SkPathFillType::kEvenOdd);
    b.addOval(outer);
    b.addOval(inner);
    return b.detach();
  };
}

/** Superellipse |x|^e + |y|^e = 1 — the squircle. @p exponent 2 is an
 *  ellipse; 4–5 is the familiar app-icon softness; large values
 *  approach the rect. */
inline OutlineFn squircle(float exponent = 4.0f) {
  return [exponent](SkSize s) {
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
  };
}

namespace detail {
/** Deterministic per-index noise in [-1, 1] (splitmix-style). */
inline float hashNoise(uint32_t seed, uint32_t i) {
  uint64_t z = (uint64_t(seed) << 32 | (i * 0x9e3779b9u)) + 0x9e3779b97f4a7c15ull;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  z ^= z >> 31;
  return (float)(z & 0xffffff) / (float)0x7fffff - 1.0f;
}
} // namespace detail

/** Organic closed blob: @p lobes control points on the inscribed
 *  ellipse, each pushed in/out by up to @p amplitude (fraction of the
 *  radius) of seeded deterministic noise, joined by a smooth
 *  Catmull-Rom loop. Same seed → same blob, every frame, every run —
 *  chaos you can cache. */
inline OutlineFn blob(uint32_t seed, float amplitude = 0.18f,
                      int lobes = 8) {
  return [seed, amplitude, lobes](SkSize s) {
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
  };
}

/** A circular arc inscribed in the box, STARTING at @p startDeg (Skia
 *  canvas convention: 0° = +x, clockwise) and sweeping @p sweepDeg — the
 *  path begins at the arc's start, so `.trim(0, sweep/360)`-style reveals
 *  and orbit connectors (the PoE Orbit idiom, REFERENCES.md §5) need no
 *  wrap math. Stroke it; an unstroked open arc has no fillable area. */
inline OutlineFn arc(float startDeg, float sweepDeg = 359.9f) {
  return [startDeg, sweepDeg](SkSize s) {
    SkPathBuilder b;
    b.addArc(SkRect::MakeWH(s.width(), s.height()), startDeg,
             std::min(sweepDeg, 359.9f));
    return b.detach();
  };
}

/** A CLOSED, fillable circular sector inscribed in the box — the arc plus
 *  its two radii, or with @p innerRatio > 0 the annular segment between
 *  two radii (a donut slice). `arc()` above is deliberately open and
 *  cannot be filled; this is the one to reach for when the wedge itself
 *  is the mark: pie and polar-area charts (Nightingale's coxcomb),
 *  cooldown sweeps, radial menus, gauge fills, compass roses.
 *
 *  Angles follow Skia's canvas convention: 0° = +x, sweeping clockwise. */
inline OutlineFn sector(float startDeg, float sweepDeg,
                        float innerRatio = 0.0f) {
  return [startDeg, sweepDeg, innerRatio](SkSize s) {
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
  };
}

/** A parallelogram leaning by @p skewDeg (the ATLUS slash, REFERENCES.md
 *  §1: P3R ≈ −12°, P5R ≈ −20°): the top edge shifts by h·tan(skew) relative
 *  to the bottom, staying inside the box. */
inline OutlineFn parallelogram(float skewDeg) {
  return [skewDeg](SkSize s) {
    const float lean = std::tan(skewDeg * 0.017453293f) * s.height();
    const float l = std::max(0.0f, -lean), r = std::max(0.0f, lean);
    SkPathBuilder b;
    b.moveTo(l, 0);
    b.lineTo(s.width() - r + l, 0); // top edge (shifted)
    b.lineTo(s.width() - l, s.height());
    b.lineTo(r - l >= 0 ? r : 0, s.height());
    b.close();
    return b.detach();
  };
}

// ---------------------------------------------------------------------------
// Wrappers

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
// They are still incomparable callables, so a node carrying one still
// re-records on every render() (ROADMAP.md §3). Naming the families is
// the half that could land tonight; making them comparable VALUES is the
// half that changes Element::outline()'s signature.

/** Samples @p f over t ∈ [t0, t1] into a polyline. @p f returns UNIT
 *  coordinates (±1 spans the box); @p samples is the segment count, and
 *  @p close joins the last point back to the first.
 *
 *      .outline(shapes::parametric([](float t) {
 *        return SkPoint{std::cos(3 * t), std::sin(2 * t)};
 *      }, 0, 2 * SK_FloatPI, 720))
 */
inline OutlineFn parametric(std::function<SkPoint(float)> f, float t0,
                            float t1, int samples = 512, bool close = false) {
  return [f = std::move(f), t0, t1, samples, close](SkSize s) {
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
  };
}

/** Lissajous figure: x = sin(a·t + δ), y = sin(b·t). The ratio a:b picks
 *  the family (1:1 an ellipse, 3:2 the classic pretzel, 5:4 a tight
 *  weave) and δ its phase — the same two numbers a physical harmonograph
 *  is set to. `turns` is how many 2π the parameter runs for; the curve
 *  closes when a:b is rational and `turns` covers the period. */
inline OutlineFn lissajous(float a, float b, float deltaDeg = 0.0f,
                           float turns = 1.0f, int samples = 720) {
  const float delta = deltaDeg * SK_FloatPI / 180.0f;
  return parametric(
      [a, b, delta](float t) {
        return SkPoint{std::sin(a * t + delta), std::sin(b * t)};
      },
      0.0f, turns * 2.0f * SK_FloatPI, samples);
}

/** A harmonograph: a Lissajous whose amplitudes DECAY, which is what
 *  makes a real pen-and-pendulum figure spiral inward instead of
 *  retracing one closed rosette. @p damping is the exponential rate per
 *  unit t; @p precession spins the whole figure as it draws (the rotating
 *  turntable under John Whitney's pendulum). */
inline OutlineFn harmonograph(float a, float b, float deltaDeg = 0.0f,
                              float damping = 0.05f, float precession = 0.0f,
                              float turns = 6.0f, int samples = 2000) {
  const float delta = deltaDeg * SK_FloatPI / 180.0f;
  return parametric(
      [a, b, delta, damping, precession](float t) {
        const float env = std::exp(-damping * t);
        const float x = env * std::sin(a * t + delta);
        const float y = env * std::sin(b * t);
        if (precession == 0.0f)
          return SkPoint{x, y};
        const float th = precession * t;
        const float c = std::cos(th), sn = std::sin(th);
        return SkPoint{x * c - y * sn, x * sn + y * c};
      },
      0.0f, turns * 2.0f * SK_FloatPI, samples);
}

/** Rose (rhodonea) r = cos(k·θ). Integer @p k gives k petals when k is
 *  odd and 2k when even; rational k gives the multi-lobed forms. */
inline OutlineFn rose(float k, float turns = 1.0f, int samples = 720) {
  return parametric(
      [k](float th) {
        const float r = std::cos(k * th);
        return SkPoint{r * std::cos(th), r * std::sin(th)};
      },
      0.0f, turns * 2.0f * SK_FloatPI, samples);
}

/** Spiral from the centre outward. @p logarithmic switches Archimedean
 *  (even spacing — a clock spring, a record groove) for logarithmic
 *  (constant angle — a nautilus, a galaxy arm). */
inline OutlineFn spiral(float turns = 3.0f, bool logarithmic = false,
                        float growth = 0.25f, int samples = 720) {
  const float total = turns * 2.0f * SK_FloatPI;
  return parametric(
      [logarithmic, growth, total](float th) {
        const float r = logarithmic
                            ? std::exp(growth * th) / std::exp(growth * total)
                            : th / total;
        return SkPoint{r * std::cos(th), r * std::sin(th)};
      },
      0.0f, total, samples);
}

/** Epitrochoid / hypotrochoid — the spirograph pair. A circle of radius
 *  @p r rolls around one of radius @p R (outside for an epitrochoid,
 *  inside when @p inside), with the pen @p d from its centre. Everything
 *  is normalised so the figure fills the box. */
inline OutlineFn trochoid(float R, float r, float d, bool inside = false,
                          float turns = 1.0f, int samples = 1440) {
  const float sign = inside ? -1.0f : 1.0f;
  const float sum = R + sign * r;
  const float extent = std::max(std::abs(sum) + std::abs(d), 1e-3f);
  return parametric(
      [R, r, d, sign, sum, extent](float t) {
        const float k = sum / std::max(r, 1e-3f);
        return SkPoint{(sum * std::cos(t) - sign * d * std::cos(k * t)) / extent,
                       (sum * std::sin(t) - d * std::sin(k * t)) / extent};
      },
      0.0f, turns * 2.0f * SK_FloatPI, samples);
}

/** Wraps any outline generator so every sharp corner rounds with a
 *  consistent radius — corners() for arbitrary silhouettes:
 *  `.outline(rounded(star(5), 8))`. */
inline OutlineFn rounded(OutlineFn shape, float radius) {
  return [shape = std::move(shape), radius](SkSize s) {
    SkPath src = shape(s);
    if (radius <= 0)
      return src;
    SkPathBuilder dst;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(radius);
        fx && fx->filterPath(&dst, src, &rec))
      return dst.detach();
    return src;
  };
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
        // Refine the boundary between the previous sample and this one.
        float lo = length * (float)(i - 1) / (float)samples, hi = d;
        for (int r = 0; r < 8; ++r) {
          const float mid = (lo + hi) / 2;
          SkPoint mp;
          if (contour->getPosTan(mid, &mp, nullptr) &&
              classify(mp) == runEdge)
            lo = mid;
          else
            hi = mid;
        }
        flushRun(hi);
        runStart = hi;
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

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    PaintContext local = ctx;
    local.outline = edges(ctx.outline, mask, step);
    inner.paint(canvas, local);
  }
  bool animated() const { return inner.animated(); }
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
  bool animated() const { return inner.animated(); }
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
inline OutlineFn arrow(float shaftFrac = 0.34f, float headFrac = 0.42f) {
  return [shaftFrac, headFrac](SkSize s) {
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
  };
}

} // namespace sigil::compose::shapes
