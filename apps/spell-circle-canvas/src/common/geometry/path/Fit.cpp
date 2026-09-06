/** @file
 * Schneider's cubic fit: one least-squares cubic over a run, its
 * parameters improved against the curve it produced, and a split at the
 * worst point when the fit is not close enough.
 */

#include "sigilgeometry/path/Fit.h"

#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <glm/geometric.hpp>
#include <vector>

#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::path {

namespace {

using Cubic = std::array<glm::vec2, 4>;

glm::vec2 unitOr(glm::vec2 v, glm::vec2 fallback) {
  const float length = glm::length(v);
  return length > 1e-9f ? v / length : fallback;
}

/** The Bernstein basis, written out: the fit's normal equations and its
 *  Newton step are both expressions in these four. */
float b0(float t) { return (1 - t) * (1 - t) * (1 - t); }
float b1(float t) { return 3 * t * (1 - t) * (1 - t); }
float b2(float t) { return 3 * t * t * (1 - t); }
float b3(float t) { return t * t * t; }

glm::vec2 evalCubic(const Cubic& curve, float t) {
  return curve[0] * b0(t) + curve[1] * b1(t) + curve[2] * b2(t) +
         curve[3] * b3(t);
}

/** The first and second derivatives, as the lower-order curves their
 *  control differences describe. */
glm::vec2 evalFirst(const Cubic& curve, float t) {
  const glm::vec2 d0 = (curve[1] - curve[0]) * 3.0f;
  const glm::vec2 d1 = (curve[2] - curve[1]) * 3.0f;
  const glm::vec2 d2 = (curve[3] - curve[2]) * 3.0f;
  return d0 * ((1 - t) * (1 - t)) + d1 * (2 * t * (1 - t)) + d2 * (t * t);
}
glm::vec2 evalSecond(const Cubic& curve, float t) {
  const glm::vec2 e0 = (curve[2] - curve[1] * 2.0f + curve[0]) * 6.0f;
  const glm::vec2 e1 = (curve[3] - curve[2] * 2.0f + curve[1]) * 6.0f;
  return e0 * (1 - t) + e1 * t;
}

/** Each point's first guess at its own parameter: how far along the
 *  chain of chords it stands, which is the parametrisation a uniform one
 *  gets wrong wherever the points bunch. */
std::vector<float> chordParameters(std::span<const glm::vec2> points) {
  std::vector<float> t(points.size(), 0.0f);
  for (size_t i = 1; i < points.size(); ++i)
    t[i] = t[i - 1] + glm::length(points[i] - points[i - 1]);
  const float total = t.back();
  if (!(total > 0)) return t;
  for (float& value : t) value /= total;
  return t;
}

/** THE LEAST-SQUARES CUBIC through the run's two ends, leaving along
 *  `leaving` and arriving along `arriving`, with each point weighted at
 *  its own parameter. The two unknowns are how far each handle reaches;
 *  the 2x2 normal equations answer both at once. */
Cubic fitOne(std::span<const glm::vec2> points, std::span<const float> t,
             glm::vec2 leaving, glm::vec2 arriving) {
  const glm::vec2 first = points.front();
  const glm::vec2 last = points.back();
  float c00 = 0, c01 = 0, c11 = 0, x0 = 0, x1 = 0;
  for (size_t i = 0; i < points.size(); ++i) {
    const glm::vec2 a0 = leaving * b1(t[i]);
    const glm::vec2 a1 = arriving * b2(t[i]);
    c00 += glm::dot(a0, a0);
    c01 += glm::dot(a0, a1);
    c11 += glm::dot(a1, a1);
    const glm::vec2 residual =
        points[i] - (first * (b0(t[i]) + b1(t[i])) + last * (b2(t[i]) + b3(t[i])));
    x0 += glm::dot(a0, residual);
    x1 += glm::dot(a1, residual);
  }
  const float determinant = c00 * c11 - c01 * c01;
  float reachOut = 0, reachIn = 0;
  if (std::abs(determinant) > 1e-12f) {
    reachOut = (x0 * c11 - x1 * c01) / determinant;
    reachIn = (c00 * x1 - c01 * x0) / determinant;
  }
  // A negative or absurd reach is the least-squares answer to a run the
  // end tangents do not suit; a third of the chord is the fallback every
  // implementation of this uses, and it is what makes the split below
  // converge rather than oscillate.
  const float chord = glm::length(last - first);
  if (!(reachOut > chord * 1e-4f) || !(reachIn > chord * 1e-4f))
    reachOut = reachIn = chord / 3.0f;
  return Cubic{first, first + leaving * reachOut, last + arriving * reachIn,
               last};
}

/** One Newton-Raphson step on a point's parameter: where the curve comes
 *  nearest the point, to first order. */
float improve(const Cubic& curve, glm::vec2 point, float t) {
  const glm::vec2 offset = evalCubic(curve, t) - point;
  const glm::vec2 first = evalFirst(curve, t);
  const glm::vec2 second = evalSecond(curve, t);
  const float denominator =
      glm::dot(first, first) + glm::dot(offset, second);
  if (!(std::abs(denominator) > 1e-12f)) return t;
  return std::clamp(t - glm::dot(offset, first) / denominator, 0.0f, 1.0f);
}

/** The point that stands furthest from the curve, and how far.
 *
 *  Measured against the CURVE rather than against each point's own
 *  parameter: the reparametrisation moves those parameters toward the
 *  nearest point but is not guaranteed to reach it, and a split decided
 *  on a parameter that has drifted splits in the wrong place. A coarse
 *  sweep for the nearest sample and a few Newton steps off it is the
 *  distance itself. */
float distanceTo(const Cubic& curve, glm::vec2 point) {
  constexpr int kSamples = 24;
  float best = 0;
  float nearest = 1e30f;
  for (int i = 0; i <= kSamples; ++i) {
    const float t = (float)i / (float)kSamples;
    const float d = glm::length(evalCubic(curve, t) - point);
    if (d < nearest) {
      nearest = d;
      best = t;
    }
  }
  for (int step = 0; step < 3; ++step) {
    best = improve(curve, point, best);
    nearest = std::min(nearest, glm::length(evalCubic(curve, best) - point));
  }
  return nearest;
}

std::pair<size_t, float> worst(const Cubic& curve,
                               std::span<const glm::vec2> points) {
  size_t at = points.size() / 2;
  float furthest = 0;
  for (size_t i = 1; i + 1 < points.size(); ++i) {
    const float distance = distanceTo(curve, points[i]);
    if (distance > furthest) {
      furthest = distance;
      at = i;
    }
  }
  return {at, furthest};
}

void fitRun(std::span<const glm::vec2> points, glm::vec2 leaving,
            glm::vec2 arriving, float tolerance, int depth,
            std::vector<Cubic>& out) {
  if (points.size() < 2) return;
  if (points.size() == 2) {
    const float third = glm::length(points[1] - points[0]) / 3.0f;
    out.push_back(Cubic{points[0], points[0] + leaving * third,
                        points[1] + arriving * third, points[1]});
    return;
  }

  std::vector<float> t = chordParameters(points);
  Cubic curve = fitOne(points, t, leaving, arriving);
  auto [at, error] = worst(curve, points);
  // Four reparametrisations is where the improvement stops paying: past
  // that a run that has not converged is a run that wants splitting.
  for (int pass = 0; pass < 4 && error > tolerance; ++pass) {
    for (size_t i = 0; i < points.size(); ++i)
      t[i] = improve(curve, points[i], t[i]);
    curve = fitOne(points, t, leaving, arriving);
    std::tie(at, error) = worst(curve, points);
  }
  if (error <= tolerance || depth <= 0 || at == 0 || at + 1 >= points.size()) {
    out.push_back(curve);
    return;
  }

  // Split at the worst point, with the run's own direction there as the
  // tangent both halves share, so the two cubics meet smoothly.
  const glm::vec2 across =
      unitOr(points[at - 1] - points[at + 1], glm::vec2{1, 0});
  fitRun(points.subspan(0, at + 1), leaving, across, tolerance, depth - 1, out);
  fitRun(points.subspan(at), -across, arriving, tolerance, depth - 1, out);
}

std::vector<glm::vec2> withoutRepeats(std::span<const glm::vec2> points) {
  std::vector<glm::vec2> out;
  out.reserve(points.size());
  for (const glm::vec2 point : points)
    if (out.empty() || glm::length(point - out.back()) > 1e-6f)
      out.push_back(point);
  return out;
}

}  // namespace

SkPath fitCurve(std::span<const glm::vec2> points, float tolerance) {
  const std::vector<glm::vec2> run = withoutRepeats(points);
  SkPathBuilder out;
  if (run.size() < 2) return out.detach();
  if (run.size() == 2) {
    out.moveTo(toSk(run[0]));
    out.lineTo(toSk(run[1]));
    return out.detach();
  }

  std::vector<Cubic> curves;
  fitRun(run, unitOr(run[1] - run[0], {1, 0}),
         unitOr(run[run.size() - 2] - run.back(), {-1, 0}),
         std::max(tolerance, 1e-4f), 12, curves);
  if (curves.empty()) return out.detach();
  out.moveTo(toSk(curves.front()[0]));
  for (const Cubic& curve : curves)
    out.cubicTo(toSk(curve[1]), toSk(curve[2]), toSk(curve[3]));
  return out.detach();
}

SkPath fitCurve(const Polyline& line, float tolerance) {
  if (!line.closed) return fitCurve(std::span<const glm::vec2>(line.points),
                                    tolerance);
  std::vector<glm::vec2> loop = line.points;
  if (!loop.empty()) loop.push_back(loop.front());
  SkPath open = fitCurve(std::span<const glm::vec2>(loop), tolerance);
  SkPathBuilder closed(open);
  closed.close();
  return closed.detach();
}

}  // namespace sigil::geometry::path
