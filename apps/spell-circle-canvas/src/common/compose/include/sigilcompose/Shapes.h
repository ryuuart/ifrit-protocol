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
 *  vertices sit at @p innerRatio of the outer radius. */
inline OutlineFn star(int points, float innerRatio = 0.5f) {
  return [points, innerRatio](SkSize s) {
    const int n = std::max(points, 2) * 2;
    const float cx = s.width() / 2, cy = s.height() / 2;
    SkPathBuilder b;
    for (int i = 0; i < n; ++i) {
      const float r = (i % 2 == 0) ? 1.0f : innerRatio;
      const float a = -SK_FloatPI / 2 + i * (2 * SK_FloatPI / n);
      const SkPoint p{cx + cx * r * std::cos(a), cy + cy * r * std::sin(a)};
      if (i == 0)
        b.moveTo(p);
      else
        b.lineTo(p);
    }
    b.close();
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
    const float sweep = std::clamp(sweepDeg, -360.0f, 360.0f);
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

} // namespace sigil::compose::shapes
