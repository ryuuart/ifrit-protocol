/** @file
 * The silhouette catalog's bodies: every generator's `path(SkSize)`, the
 * corner wrappers and the per-edge extraction.
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkCornerPathEffect.h>
#include <include/pathops/SkPathOps.h>
#include <include/utils/SkParsePath.h>
#include <sigilcompose/shape/Shapes.h>

#include <algorithm>
#include <cmath>

#include "sigilgeometry/path/Noise.h"
#include "sigilgeometry/path/Numeric.h"

namespace sigil::compose::shapes {

Svg svg(const char* d, bool preserveAspect) {
  SkPath parsed;
  if (auto result = SkParsePath::FromSVGString(d)) parsed = std::move(*result);
  return Svg{std::move(parsed), preserveAspect};
}

SkPath Polygon::path(SkSize s) const {
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

SkPath Star::path(SkSize s) const {
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

SkPath Circle::path(SkSize s) const {
  SkRect r = SkRect::MakeWH(s.width(), s.height());
  r.inset(inset, inset);
  SkPathBuilder b;
  b.addOval(r, direction, startIndex);
  return b.detach();
}

SkPath Annulus::path(SkSize s) const {
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

SkPath Squircle::path(SkSize s) const {
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

SkPath Blob::path(SkSize s) const {
  const int n = std::max(lobes, 3);
  const float cx = s.width() / 2, cy = s.height() / 2;
  std::vector<SkPoint> pts((size_t)n);
  for (int i = 0; i < n; ++i) {
    const float a = -SK_FloatPI / 2 + i * (2 * SK_FloatPI / n);
    const float r =
        1.0f - amplitude * (0.5f + 0.5f * geometry::path::noise::hash(
                                              seed, (uint32_t)i));
    pts[(size_t)i] = {cx + cx * r * std::cos(a), cy + cy * r * std::sin(a)};
  }
  // Catmull-Rom → cubic Béziers around the loop.
  SkPathBuilder b;
  b.moveTo(pts[0]);
  for (int i = 0; i < n; ++i) {
    const SkPoint& p0 = pts[(size_t)((i - 1 + n) % n)];
    const SkPoint& p1 = pts[(size_t)(i % n)];
    const SkPoint& p2 = pts[(size_t)((i + 1) % n)];
    const SkPoint& p3 = pts[(size_t)((i + 2) % n)];
    const SkPoint c1{p1.x() + (p2.x() - p0.x()) / 6.0f,
                     p1.y() + (p2.y() - p0.y()) / 6.0f};
    const SkPoint c2{p2.x() - (p3.x() - p1.x()) / 6.0f,
                     p2.y() - (p3.y() - p1.y()) / 6.0f};
    b.cubicTo(c1, c2, p2);
  }
  b.close();
  return b.detach();
}

SkPath Arc::path(SkSize s) const {
  SkPathBuilder b;
  b.addArc(SkRect::MakeWH(s.width(), s.height()), startDeg,
           std::min(sweepDeg, 359.9f));
  return b.detach();
}

SkPath Sector::path(SkSize s) const {
  const float cx = s.width() * 0.5f, cy = s.height() * 0.5f;
  // arcTo swallows a full turn, so an unclamped sector(start, 360,
  // inner) — a gauge's annular TRACK, the most obvious call there is —
  // draws nothing at all. Clamped here rather than at every call site.
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
  const SkRect innerBox = SkRect::MakeXYWH(
      cx - cx * inner, cy - cy * inner, s.width() * inner, s.height() * inner);
  b.arcTo(outerBox, startDeg, sweep, true);
  b.arcTo(innerBox, startDeg + sweep, -sweep, false);
  b.close();
  return b.detach();
}

SkPath Parallelogram::path(SkSize s) const {
  const float lean = std::tan(skewDeg * 0.017453293f) * s.height();
  const float l = std::max(0.0f, -lean), r = std::max(0.0f, lean);
  SkPathBuilder b;
  b.moveTo(l, 0);
  b.lineTo(s.width() - r + l, 0);  // top edge (shifted)
  b.lineTo(s.width() - l, s.height());
  b.lineTo(r - l >= 0 ? r : 0, s.height());
  b.close();
  return b.detach();
}

SkPath Lissajous::path(SkSize s) const {
  const float delta = deltaDeg * SK_FloatPI / 180.0f;
  return detail::samplePolyline(
      [fa = a, fb = b, delta](float t) {
        return SkPoint{std::sin(fa * t + delta), std::sin(fb * t)};
      },
      0.0f, turns * 2.0f * SK_FloatPI, samples, false, s);
}

SkPath Harmonograph::path(SkSize s) const {
  const float delta = deltaDeg * SK_FloatPI / 180.0f;
  return detail::samplePolyline(
      [fa = a, fb = b, delta, fdamping = damping,
       fprecession = precession](float t) {
        const float env = std::exp(-fdamping * t);
        const float x = env * std::sin(fa * t + delta);
        const float y = env * std::sin(fb * t);
        if (fprecession == 0.0f) return SkPoint{x, y};
        const float th = fprecession * t;
        const float c = std::cos(th), sn = std::sin(th);
        return SkPoint{x * c - y * sn, x * sn + y * c};
      },
      0.0f, turns * 2.0f * SK_FloatPI, samples, false, s);
}

SkPath Rose::path(SkSize s) const {
  return detail::samplePolyline(
      [fk = k](float th) {
        const float r = std::cos(fk * th);
        return SkPoint{r * std::cos(th), r * std::sin(th)};
      },
      0.0f, turns * 2.0f * SK_FloatPI, samples, false, s);
}

SkPath Spiral::path(SkSize s) const {
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

SkPath Trochoid::path(SkSize s) const {
  const float sign = inside ? -1.0f : 1.0f;
  const float sum = R + sign * r;
  const float extent = std::max(std::abs(sum) + std::abs(d), 1e-3f);
  return detail::samplePolyline(
      [fR = R, fr = r, fd = d, sign, sum, extent](float t) {
        const float k = sum / std::max(fr, 1e-3f);
        return SkPoint{
            (sum * std::cos(t) - sign * fd * std::cos(k * t)) / extent,
            (sum * std::sin(t) - fd * std::sin(k * t)) / extent};
      },
      0.0f, turns * 2.0f * SK_FloatPI, samples, false, s);
}

SkPath Rounded::path(SkSize s) const {
  SkPath src = inner(s);
  if (radius <= 0) return src;
  SkPathBuilder dst;
  SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
  if (sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(radius);
      fx && fx->filterPath(&dst, src, &rec))
    return dst.detach();
  return src;
}

SkPath Chamfered::path(SkSize s) const {
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
  if (has(mask, Corner::TopLeft)) b.lineTo(0, c);
  b.close();
  return b.detach();
}

SkPath Notched::path(SkSize s) const {
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

SkPath edges(const SkPath& outline, Edge mask, float step) {
  const SkRect bounds = outline.getBounds();
  const float cx = bounds.centerX(), cy = bounds.centerY();
  const float hw = std::max(bounds.width() / 2, 1.0f);
  const float hh = std::max(bounds.height() / 2, 1.0f);
  auto classify = [&](SkPoint p) {
    const float nx = (p.x() - cx) / hw, ny = (p.y() - cy) / hh;
    if (std::abs(nx) > std::abs(ny)) return nx > 0 ? Edge::Right : Edge::Left;
    return ny > 0 ? Edge::Bottom : Edge::Top;
  };

  SkPathBuilder out;
  SkContourMeasureIter iter(outline, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float length = contour->length();
    if (length <= 0) continue;
    const int samples = std::max(8, (int)std::ceil(length / step));
    float runStart = 0.0f;
    SkPoint pos;
    if (!contour->getPosTan(0, &pos, nullptr)) continue;
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
      if (!contour->getPosTan(std::min(d, length), &pos, nullptr)) continue;
      const Edge e = classify(pos);
      if (e != runEdge) {
        // The boundary lies between the previous sample and this one;
        // narrow the bracket rather than taking either sample, or every
        // run boundary sits up to one step away from the real corner.
        const float at = geometry::path::bisect(
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

void EdgeSlice::paint(SkCanvas& canvas, const PaintContext& ctx) const {
  PaintContext local = ctx;
  local.outline = edges(ctx.outline, mask, step);
  inner.paint(canvas, local);
}

void Inset::paint(SkCanvas& canvas, const PaintContext& ctx) const {
  PaintContext local = ctx;
  if (px != 0) {
    SkPaint offset;
    offset.setStyle(SkPaint::kStroke_Style);  // the RING, not the grown shape
    offset.setStrokeWidth(std::abs(px) * 2.0f);
    offset.setStrokeJoin(SkPaint::kMiter_Join);
    // The stroke-and-fill of the outline is the RING of width 2|px|
    // straddling it. Subtracting that ring shrinks the silhouette;
    // unioning it grows the silhouette by the same amount.
    const SkPath ring = skpathutils::FillPathWithPaint(ctx.outline, offset);
    SkPath result;
    if (Op(ctx.outline, ring,
           px > 0 ? SkPathOp::kDifference_SkPathOp : SkPathOp::kUnion_SkPathOp,
           &result))
      local.outline = std::move(result);
  }
  inner.paint(canvas, local);
}

SkPath Arrow::path(SkSize s) const {
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

}  // namespace sigil::compose::shapes
