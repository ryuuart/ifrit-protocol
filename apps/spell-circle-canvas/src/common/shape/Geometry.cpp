#include "sigilshape/Geometry.h"

#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathTypes.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace sigil::shape {

namespace {

float segmentLength(SkPoint a, SkPoint b) { return SkPoint::Distance(a, b); }

// Flatness test: max distance of the control points from the chord.
float controlDeviation(const SkPoint pts[], int count) {
  const SkPoint a = pts[0], b = pts[count - 1];
  const SkVector chord = b - a;
  const float chordLen2 = chord.dot(chord);
  float worst = 0;
  for (int i = 1; i < count - 1; ++i) {
    const SkVector v = pts[i] - a;
    float d;
    if (chordLen2 < 1e-12f) {
      d = std::sqrt(v.dot(v));
    } else {
      const float t = std::clamp(v.dot(chord) / chordLen2, 0.0f, 1.0f);
      const SkVector off = v - SkVector{chord.fX * t, chord.fY * t};
      d = std::sqrt(off.dot(off));
    }
    worst = std::max(worst, d);
  }
  return worst;
}

SkPoint evalQuad(const SkPoint p[3], float t) {
  const float u = 1 - t;
  return {u * u * p[0].fX + 2 * u * t * p[1].fX + t * t * p[2].fX,
          u * u * p[0].fY + 2 * u * t * p[1].fY + t * t * p[2].fY};
}

SkPoint evalCubic(const SkPoint p[4], float t) {
  const float u = 1 - t;
  return {u * u * u * p[0].fX + 3 * u * u * t * p[1].fX +
              3 * u * t * t * p[2].fX + t * t * t * p[3].fX,
          u * u * u * p[0].fY + 3 * u * u * t * p[1].fY +
              3 * u * t * t * p[2].fY + t * t * t * p[3].fY};
}

template <typename Eval>
void flattenCurve(std::vector<SkPoint>& out, const SkPoint pts[], int count,
                  float tolerance, Eval eval) {
  // Subdivision count from deviation: a curve whose control polygon
  // deviates d from the chord flattens to ~sqrt(d / tol) * 4 segments.
  const float dev = controlDeviation(pts, count);
  int segments =
      dev <= tolerance ? 1 : (int)std::ceil(std::sqrt(dev / tolerance) * 4.0f);
  segments = std::clamp(segments, 1, 256);
  for (int i = 1; i <= segments; ++i)
    out.push_back(eval(pts, (float)i / (float)segments));
}

}  // namespace

float Polyline::length() const {
  float total = 0;
  for (size_t i = 1; i < points.size(); ++i)
    total += segmentLength(points[i - 1], points[i]);
  if (closed && points.size() > 1)
    total += segmentLength(points.back(), points.front());
  return total;
}

SkPoint Polyline::centroid() const {
  if (points.empty()) return {0, 0};
  // Length-weighted centroid (vertex average biases toward dense spans).
  double x = 0, y = 0, w = 0;
  auto accumulate = [&](SkPoint a, SkPoint b) {
    const double len = segmentLength(a, b);
    x += (a.fX + b.fX) * 0.5 * len;
    y += (a.fY + b.fY) * 0.5 * len;
    w += len;
  };
  for (size_t i = 1; i < points.size(); ++i)
    accumulate(points[i - 1], points[i]);
  if (closed && points.size() > 1) accumulate(points.back(), points.front());
  if (w < 1e-9) {
    return points.front();
  }
  return {(float)(x / w), (float)(y / w)};
}

float Polyline::signedArea() const {
  double area = 0;
  const size_t n = points.size();
  for (size_t i = 0; i < n; ++i) {
    const SkPoint& a = points[i];
    const SkPoint& b = points[(i + 1) % n];
    area += (double)a.fX * b.fY - (double)b.fX * a.fY;
  }
  return (float)(area * 0.5);
}

void Polyline::reverse() { std::reverse(points.begin(), points.end()); }

std::vector<Polyline> flatten(const SkPath& path, float tolerance) {
  std::vector<Polyline> contours;
  Polyline current;
  SkPath::Iter iter(path, false);
  SkPoint pts[4];
  for (;;) {
    const SkPath::Verb verb = iter.next(pts);
    if (verb == SkPath::kDone_Verb) break;
    switch (verb) {
      case SkPath::kMove_Verb:
        if (current.points.size() > 1) contours.push_back(std::move(current));
        current = {};
        current.points.push_back(pts[0]);
        break;
      case SkPath::kLine_Verb:
        current.points.push_back(pts[1]);
        break;
      case SkPath::kQuad_Verb:
        flattenCurve(current.points, pts, 3, tolerance, evalQuad);
        break;
      case SkPath::kConic_Verb: {
        // Convert the conic to quads (2^1 = 2 of them is plenty at our
        // tolerances), then flatten those.
        SkPoint quads[1 + 2 * 2];
        const int count = SkPath::ConvertConicToQuads(
            pts[0], pts[1], pts[2], iter.conicWeight(), quads, 1);
        for (int q = 0; q < count; ++q)
          flattenCurve(current.points, quads + q * 2, 3, tolerance, evalQuad);
        break;
      }
      case SkPath::kCubic_Verb:
        flattenCurve(current.points, pts, 4, tolerance, evalCubic);
        break;
      case SkPath::kClose_Verb:
        current.closed = true;
        // Drop an explicit closing duplicate if the path emitted one.
        if (current.points.size() > 1 &&
            segmentLength(current.points.front(), current.points.back()) <
                1e-4f)
          current.points.pop_back();
        break;
      default:
        break;
    }
  }
  if (current.points.size() > 1) contours.push_back(std::move(current));
  return contours;
}

SkPoint Sampled::centroid() const {
  if (points.empty()) return {0, 0};
  double x = 0, y = 0;
  for (const SkPoint& p : points) {
    x += p.fX;
    y += p.fY;
  }
  return {(float)(x / points.size()), (float)(y / points.size())};
}

Sampled resample(const Polyline& contour, int count) {
  Sampled out;
  out.closed = contour.closed;
  if (contour.points.empty() || count <= 0) return out;
  out.points.reserve((size_t)count);

  // Cumulative arc length over the (possibly wrapped) segment list.
  std::vector<float> cumulative;
  cumulative.reserve(contour.points.size() + 1);
  cumulative.push_back(0);
  const size_t n = contour.points.size();
  const size_t segments = contour.closed ? n : n - 1;
  for (size_t i = 0; i < segments; ++i) {
    const SkPoint a = contour.points[i];
    const SkPoint b = contour.points[(i + 1) % n];
    cumulative.push_back(cumulative.back() + segmentLength(a, b));
  }
  const float total = cumulative.back();
  out.sourceLength = total;
  if (total < 1e-9f) {
    out.points.assign((size_t)count, contour.points.front());
    return out;
  }

  // Closed: count samples over [0, total); open: over [0, total].
  const float denom = contour.closed ? (float)count : (float)(count - 1);
  size_t seg = 0;
  for (int i = 0; i < count; ++i) {
    const float target = count == 1 ? 0 : total * (float)i / denom;
    while (seg + 1 < cumulative.size() - 1 && cumulative[seg + 1] < target)
      ++seg;
    const float span = cumulative[seg + 1] - cumulative[seg];
    const float t = span < 1e-9f ? 0 : (target - cumulative[seg]) / span;
    const SkPoint a = contour.points[seg % n];
    const SkPoint b = contour.points[(seg + 1) % n];
    out.points.push_back({a.fX + (b.fX - a.fX) * t, a.fY + (b.fY - a.fY) * t});
  }
  return out;
}

std::vector<Sampled> resample(const SkPath& path, int count, float tolerance) {
  std::vector<Sampled> out;
  for (const Polyline& contour : flatten(path, tolerance))
    out.push_back(resample(contour, count));
  return out;
}

Alignment bestAlignment(const Sampled& a, const Sampled& b) {
  Alignment best;
  if (a.points.size() != b.points.size() || a.points.empty()) return best;
  const int n = (int)a.points.size();
  double bestCost = std::numeric_limits<double>::max();
  // O(n^2) over offsets x2 directions; n is a blend's sample count
  // (typically 64-256), evaluated once per key pair, never per step.
  for (int reversed = 0; reversed < 2; ++reversed) {
    for (int offset = 0; offset < n; ++offset) {
      double cost = 0;
      for (int i = 0; i < n && cost < bestCost; ++i) {
        const int j =
            reversed ? (offset - i % n + 2 * n) % n : (offset + i) % n;
        const SkVector d = a.points[(size_t)i] - b.points[(size_t)j];
        cost += (double)d.dot(d);
      }
      if (cost < bestCost) {
        bestCost = cost;
        best.offset = offset;
        best.reversed = reversed != 0;
      }
    }
  }
  return best;
}

Sampled applyAlignment(const Sampled& b, const Alignment& alignment) {
  Sampled out = b;
  const int n = (int)b.points.size();
  if (n == 0) return out;
  for (int i = 0; i < n; ++i) {
    const int j = alignment.reversed ? (alignment.offset - i % n + 2 * n) % n
                                     : (alignment.offset + i) % n;
    out.points[(size_t)i] = b.points[(size_t)j];
  }
  return out;
}

SkPath toPath(const Sampled& samples, bool smooth) {
  SkPathBuilder builder;
  const size_t n = samples.points.size();
  if (n < 2) return builder.detach();
  if (!smooth) {
    builder.moveTo(samples.points[0]);
    for (size_t i = 1; i < n; ++i) builder.lineTo(samples.points[i]);
    if (samples.closed) builder.close();
    return builder.detach();
  }
  // Catmull-Rom through the samples, expressed as cubic Beziers.
  auto at = [&](long i) -> SkPoint {
    if (samples.closed)
      return samples.points[(size_t)((i % (long)n + (long)n) % (long)n)];
    return samples.points[(size_t)std::clamp(i, 0L, (long)n - 1)];
  };
  builder.moveTo(at(0));
  const long last = samples.closed ? (long)n : (long)n - 1;
  for (long i = 0; i < last; ++i) {
    const SkPoint p0 = at(i - 1), p1 = at(i), p2 = at(i + 1), p3 = at(i + 2);
    const SkPoint c1 = {p1.fX + (p2.fX - p0.fX) / 6.0f,
                        p1.fY + (p2.fY - p0.fY) / 6.0f};
    const SkPoint c2 = {p2.fX - (p3.fX - p1.fX) / 6.0f,
                        p2.fY - (p3.fY - p1.fY) / 6.0f};
    builder.cubicTo(c1, c2, p2);
  }
  if (samples.closed) builder.close();
  return builder.detach();
}

Sampled lerp(const Sampled& a, const Sampled& b, float t) {
  Sampled out;
  out.closed = a.closed;
  const size_t n = std::min(a.points.size(), b.points.size());
  out.points.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    const SkPoint& pa = a.points[i];
    const SkPoint& pb = b.points[i];
    out.points.push_back(
        {pa.fX + (pb.fX - pa.fX) * t, pa.fY + (pb.fY - pa.fY) * t});
  }
  out.sourceLength = a.sourceLength + (b.sourceLength - a.sourceLength) * t;
  return out;
}

}  // namespace sigil::shape
