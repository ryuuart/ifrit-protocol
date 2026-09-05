/** @file
 * The resampling core: adaptive curve flattening that keeps corner
 * anchors exact, uniform arc-length sampling, alignment of two closed
 * contours, and the rebuild of samples into a path.
 *
 * Beside them the two resamplings keyed to a SPACING rather than to a
 * count — `subdivide` over the vertices, `catmullRom` through them — and
 * the questions a polyline answers about the area it bounds: its rect,
 * the even-odd ray test, and where a segment crosses its edges.
 */

#include "sigilgeometry/path/Polyline.h"

#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathTypes.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>

#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::path {

namespace {

float segmentLength(glm::vec2 a, glm::vec2 b) { return distance(a, b); }

/** The z of the 3D cross product of two planar vectors: positive when
 *  `b` turns clockwise from `a` in Skia's y-down space, and zero when
 *  the two are parallel. */
float cross(glm::vec2 a, glm::vec2 b) { return a.x * b.y - a.y * b.x; }

/** Farthest any interior control point sits from the chord: the flatness
 *  measure that decides how many segments a curve needs. */
float controlDeviation(const SkPoint pts[], int count) {
  const glm::vec2 a = fromSk(pts[0]), b = fromSk(pts[count - 1]);
  const glm::vec2 chord = b - a;
  const float chordLen2 = glm::dot(chord, chord);
  float worst = 0;
  for (int i = 1; i < count - 1; ++i) {
    const glm::vec2 v = fromSk(pts[i]) - a;
    float d;
    if (chordLen2 < 1e-12f) {
      d = length(v);
    } else {
      const float t = std::clamp(glm::dot(v, chord) / chordLen2, 0.0f, 1.0f);
      d = length(v - chord * t);
    }
    worst = std::max(worst, d);
  }
  return worst;
}

glm::vec2 evalQuad(const SkPoint p[3], float t) {
  const float u = 1 - t;
  return {u * u * p[0].fX + 2 * u * t * p[1].fX + t * t * p[2].fX,
          u * u * p[0].fY + 2 * u * t * p[1].fY + t * t * p[2].fY};
}

glm::vec2 evalCubic(const SkPoint p[4], float t) {
  const float u = 1 - t;
  return {u * u * u * p[0].fX + 3 * u * u * t * p[1].fX +
              3 * u * t * t * p[2].fX + t * t * t * p[3].fX,
          u * u * u * p[0].fY + 3 * u * u * t * p[1].fY +
              3 * u * t * t * p[2].fY + t * t * t * p[3].fY};
}

template <typename Eval>
void flattenCurve(std::vector<glm::vec2>& out, const SkPoint pts[], int count,
                  float tolerance, Eval eval) {
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

glm::vec2 Polyline::centroid() const {
  if (points.empty()) return {0, 0};
  double x = 0, y = 0, w = 0;
  auto accumulate = [&](glm::vec2 a, glm::vec2 b) {
    const double len = segmentLength(a, b);
    x += (a.x + b.x) * 0.5 * len;
    y += (a.y + b.y) * 0.5 * len;
    w += len;
  };
  for (size_t i = 1; i < points.size(); ++i)
    accumulate(points[i - 1], points[i]);
  if (closed && points.size() > 1) accumulate(points.back(), points.front());
  if (w < 1e-9) return points.front();
  return {(float)(x / w), (float)(y / w)};
}

float Polyline::signedArea() const {
  double area = 0;
  const size_t n = points.size();
  for (size_t i = 0; i < n; ++i) {
    const glm::vec2& a = points[i];
    const glm::vec2& b = points[(i + 1) % n];
    area += (double)a.x * b.y - (double)b.x * a.y;
  }
  return (float)(area * 0.5);
}

SkRect Polyline::bounds() const {
  const Polyline* one = this;
  return path::bounds(std::span<const Polyline>(one, 1));
}

bool Polyline::contains(glm::vec2 point) const {
  if (points.size() < 3) return false;
  // The ray test: count the edges a ray to +x crosses. `j` trails `i`, so
  // the seam edge is walked with the rest whether or not `closed` is set.
  bool inside = false;
  for (size_t i = 0, j = points.size() - 1; i < points.size(); j = i++) {
    const glm::vec2 a = points[i];
    const glm::vec2 b = points[j];
    if (((a.y > point.y) != (b.y > point.y)) &&
        point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
      inside = !inside;
  }
  return inside;
}

void Polyline::reverse() {
  std::reverse(points.begin(), points.end());
  std::reverse(lane.begin(), lane.end());
}

SkRect bounds(std::span<const Polyline> lines) {
  float left = std::numeric_limits<float>::infinity();
  float top = std::numeric_limits<float>::infinity();
  float right = -std::numeric_limits<float>::infinity();
  float bottom = -std::numeric_limits<float>::infinity();
  for (const Polyline& line : lines)
    for (const glm::vec2 point : line.points) {
      left = std::min(left, point.x);
      top = std::min(top, point.y);
      right = std::max(right, point.x);
      bottom = std::max(bottom, point.y);
    }
  if (!(left <= right) || !(top <= bottom)) return SkRect::MakeEmpty();
  return SkRect::MakeLTRB(left, top, right, bottom);
}

bool containsEvenOdd(std::span<const Polyline> rings, glm::vec2 point) {
  bool inside = false;
  for (const Polyline& ring : rings)
    if (ring.contains(point)) inside = !inside;
  return inside;
}

std::vector<glm::vec2> edgeCrossings(const Polyline& line, glm::vec2 from,
                                     glm::vec2 to) {
  std::vector<glm::vec2> hits;
  const glm::vec2 ray = to - from;
  const size_t n = line.points.size();
  const size_t edges = n < 2 ? 0 : (line.closed ? n : n - 1);
  for (size_t i = 0; i < edges; ++i) {
    const glm::vec2 start = line.points[i];
    const glm::vec2 edge = line.points[(i + 1) % n] - start;
    const float denominator = cross(ray, edge);
    // Parallel: no one crossing to answer.
    if (std::abs(denominator) < 1e-6f) continue;
    const glm::vec2 between = start - from;
    const float alongRay = cross(between, edge) / denominator;
    const float alongEdge = cross(between, ray) / denominator;
    if (alongRay >= 0.0f && alongRay <= 1.0f && alongEdge >= 0.0f &&
        alongEdge <= 1.0f)
      hits.push_back(from + ray * alongRay);
  }
  std::ranges::sort(hits, [&](glm::vec2 a, glm::vec2 b) {
    return distance(a, from) < distance(b, from);
  });
  return hits;
}

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
        current.points.push_back(fromSk(pts[0]));
        break;
      case SkPath::kLine_Verb:
        current.points.push_back(fromSk(pts[1]));
        break;
      case SkPath::kQuad_Verb:
        flattenCurve(current.points, pts, 3, tolerance, evalQuad);
        break;
      case SkPath::kConic_Verb: {
        SkPoint quads[1 + 2 * 2];
        const int count = SkPath::ConvertConicToQuads(
            pts[0], pts[1], pts[2], iter.conicWeight(), quads, 1);
        for (int q = 0; q < count; ++q)
          flattenCurve(current.points, quads + static_cast<ptrdiff_t>(q) * 2, 3,
                       tolerance, evalQuad);
        break;
      }
      case SkPath::kCubic_Verb:
        flattenCurve(current.points, pts, 4, tolerance, evalCubic);
        break;
      case SkPath::kClose_Verb:
        current.closed = true;
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

Polyline sample(const std::function<glm::vec2(float)>& f, float t0, float t1,
                int count, bool closed) {
  Polyline out;
  out.closed = closed;
  const int n = std::max(count, 2);
  out.points.reserve((size_t)n + 1);
  for (int i = 0; i <= n; ++i)
    out.points.push_back(f(t0 + (t1 - t0) * ((float)i / (float)n)));
  return out;
}

glm::vec2 Sampled::centroid() const {
  if (points.empty()) return {0, 0};
  double x = 0, y = 0;
  for (const glm::vec2& p : points) {
    x += p.x;
    y += p.y;
  }
  return {(float)(x / points.size()), (float)(y / points.size())};
}

Sampled resample(const Polyline& contour, int count) {
  Sampled out;
  out.closed = contour.closed;
  if (contour.points.empty() || count <= 0) return out;
  out.points.reserve((size_t)count);
  std::vector<float> cumulative;
  cumulative.reserve(contour.points.size() + 1);
  cumulative.push_back(0);
  const size_t n = contour.points.size();
  const size_t segments = contour.closed ? n : n - 1;
  for (size_t i = 0; i < segments; ++i)
    cumulative.push_back(
        cumulative.back() +
        segmentLength(contour.points[i], contour.points[(i + 1) % n]));
  const float total = cumulative.back();
  out.sourceLength = total;
  if (total < 1e-9f) {
    out.points.assign((size_t)count, contour.points.front());
    return out;
  }
  const float denom = contour.closed ? (float)count : (float)(count - 1);
  size_t seg = 0;
  for (int i = 0; i < count; ++i) {
    const float target = count == 1 ? 0 : total * (float)i / denom;
    while (seg + 1 < cumulative.size() - 1 && cumulative[seg + 1] < target)
      ++seg;
    const float span = cumulative[seg + 1] - cumulative[seg];
    const float t = span < 1e-9f ? 0 : (target - cumulative[seg]) / span;
    // n is at least one: cumulative.back() is finite only for a non-empty
    // contour, and the empty case returned above.
    const glm::vec2 a =
        contour.points[seg % n];  // NOLINT(clang-analyzer-core.DivideZero)
    const glm::vec2 b = contour.points[(seg + 1) % n];
    out.points.push_back(a + (b - a) * t);
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
  for (int reversed = 0; reversed < 2; ++reversed) {
    for (int offset = 0; offset < n; ++offset) {
      double cost = 0;
      for (int i = 0; i < n && cost < bestCost; ++i) {
        const int j =
            reversed ? (offset - i % n + 2 * n) % n : (offset + i) % n;
        const glm::vec2 d = a.points[(size_t)i] - b.points[(size_t)j];
        cost += (double)glm::dot(d, d);
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
    builder.moveTo(toSk(samples.points[0]));
    for (size_t i = 1; i < n; ++i) builder.lineTo(toSk(samples.points[i]));
    if (samples.closed) builder.close();
    return builder.detach();
  }
  auto at = [&](long i) -> glm::vec2 {
    if (samples.closed)
      return samples.points[(size_t)((i % (long)n + (long)n) % (long)n)];
    return samples.points[(size_t)std::clamp(i, 0L, (long)n - 1)];
  };
  builder.moveTo(toSk(at(0)));
  const long last = samples.closed ? (long)n : (long)n - 1;
  for (long i = 0; i < last; ++i) {
    const glm::vec2 p0 = at(i - 1), p1 = at(i), p2 = at(i + 1), p3 = at(i + 2);
    const glm::vec2 c1 = p1 + (p2 - p0) / 6.0f;
    const glm::vec2 c2 = p2 - (p3 - p1) / 6.0f;
    builder.cubicTo(toSk(c1), toSk(c2), toSk(p2));
  }
  if (samples.closed) builder.close();
  return builder.detach();
}

SkPath toPath(const Polyline& line) {
  SkPathBuilder builder;
  if (line.points.empty()) return builder.detach();
  builder.moveTo(toSk(line.points[0]));
  for (size_t i = 1; i < line.points.size(); ++i)
    builder.lineTo(toSk(line.points[i]));
  if (line.closed) builder.close();
  return builder.detach();
}

SkPath smoothThrough(std::span<const glm::vec2> points, bool closed) {
  SkPathBuilder builder;
  const size_t n = points.size();
  if (n < 2) return builder.detach();
  const auto midpoint = [&](size_t i, size_t j) {
    return toSk((points[i] + points[j]) * 0.5f);
  };
  if (!closed) {
    // The first point is the start, the last is the end, and every point
    // between steers one quad from the midpoint before it to the midpoint
    // after it; the one edge left over on each end is a straight run.
    builder.moveTo(toSk(points[0]));
    for (size_t i = 1; i + 1 < n; ++i)
      builder.quadTo(toSk(points[i]), midpoint(i, i + 1));
    builder.lineTo(toSk(points[n - 1]));
    return builder.detach();
  }
  if (n == 2) {
    builder.moveTo(toSk(points[0]));
    builder.lineTo(toSk(points[1]));
    builder.close();
    return builder.detach();
  }
  // Round a loop every point is interior, so every point steers a quad
  // and the curve starts where the seam edge is crossed: its midpoint.
  builder.moveTo(midpoint(n - 1, 0));
  for (size_t i = 0; i < n; ++i)
    builder.quadTo(toSk(points[i]), midpoint(i, (i + 1) % n));
  builder.close();
  return builder.detach();
}

SkPath smoothThrough(const Polyline& line) {
  return smoothThrough(line.points, line.closed);
}

Polyline subdivide(const Polyline& line, float spacing) {
  Polyline out;
  if (line.points.empty() || !(spacing > 0)) return line;
  const bool laned = line.lane.size() == line.points.size();
  out.lane.reserve(laned ? line.points.size() : 0);
  auto emit = [&](glm::vec2 point, float value) {
    out.points.push_back(point);
    if (laned) out.lane.push_back(value);
  };
  emit(line.points.front(), laned ? line.lane.front() : 0.0f);
  const size_t n = line.points.size();
  const size_t edges = n < 2 ? 0 : (line.closed ? n : n - 1);
  for (size_t i = 0; i < edges; ++i) {
    const size_t next = (i + 1) % n;
    const glm::vec2 a = line.points[i], b = line.points[next];
    const float len = distance(a, b);
    if (!(len > 0)) continue;
    const int steps = std::max(1, (int)std::ceil(len / spacing));
    for (int step = 1; step <= steps; ++step) {
      const float t = (float)step / (float)steps;
      emit(a + (b - a) * t,
           laned ? line.lane[i] + (line.lane[next] - line.lane[i]) * t : 0.0f);
    }
  }
  // A closed walk comes home to the point it left from; the seam is the
  // closure rather than a repeated vertex.
  if (line.closed && out.points.size() > 1) {
    out.points.pop_back();
    if (laned) out.lane.pop_back();
    out.closed = true;
  }
  return out;
}

Polyline catmullRom(const Polyline& controls, float spacing, float curvature) {
  if (controls.points.size() < 2 || !(spacing > 0)) return controls;
  curvature = std::clamp(curvature, 0.0f, 1.0f);
  const bool laned = controls.lane.size() == controls.points.size();
  // The uniform basis, one axis at a time.
  const auto basis = [](float p0, float p1, float p2, float p3, float t) {
    const float t2 = t * t;
    const float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * t +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
  };
  Polyline out;
  const size_t n = controls.points.size();
  for (size_t i = 0; i + 1 < n; ++i) {
    const glm::vec2 p0 = controls.points[i == 0 ? 0 : i - 1];
    const glm::vec2 p1 = controls.points[i];
    const glm::vec2 p2 = controls.points[i + 1];
    const glm::vec2 p3 = controls.points[std::min(i + 2, n - 1)];
    const float chord = distance(p1, p2);
    const int steps = std::max(1, (int)std::ceil(chord / spacing));
    // Every chord but the first starts where the one before it ended.
    for (int step = i == 0 ? 0 : 1; step <= steps; ++step) {
      const float t = (float)step / (float)steps;
      const glm::vec2 curve{basis(p0.x, p1.x, p2.x, p3.x, t),
                            basis(p0.y, p1.y, p2.y, p3.y, t)};
      const glm::vec2 chordAt = p1 + (p2 - p1) * t;
      out.points.push_back(chordAt + (curve - chordAt) * curvature);
      if (laned)
        out.lane.push_back(controls.lane[i] +
                           (controls.lane[i + 1] - controls.lane[i]) * t);
    }
  }
  return out;
}

Sampled lerp(const Sampled& a, const Sampled& b, float t) {
  Sampled out;
  out.closed = a.closed;
  const size_t n = std::min(a.points.size(), b.points.size());
  out.points.reserve(n);
  for (size_t i = 0; i < n; ++i)
    out.points.push_back(a.points[i] + (b.points[i] - a.points[i]) * t);
  out.sourceLength = a.sourceLength + (b.sourceLength - a.sourceLength) * t;
  return out;
}

}  // namespace sigil::geometry::path
