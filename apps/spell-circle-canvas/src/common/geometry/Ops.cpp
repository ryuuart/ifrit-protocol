#include "sigilgeometry/Ops.h"

#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/pathops/SkPathOps.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

#include "sigilgeometry/Noise.h"
#include "sigilgeometry/Numeric.h"
#include "sigilgeometry/Polyline.h"

namespace sigil::geometry::ops {

namespace {

SkPath binary(const SkPath& a, const SkPath& b, SkPathOp op) {
  SkPath out;
  if (!Op(a, b, op, &out)) return SkPath();
  return out;
}

glm::vec2 normalAt(const std::vector<glm::vec2>& pts, size_t i, bool closed) {
  const size_t n = pts.size();
  const glm::vec2 prev = pts[closed ? (i + n - 1) % n : (i == 0 ? 0 : i - 1)];
  const glm::vec2 next = pts[closed ? (i + 1) % n : std::min(i + 1, n - 1)];
  glm::vec2 t = next - prev;
  const float len = length(t);
  t = len > 0 ? t / len : glm::vec2{1, 0};
  return {t.y, -t.x};
}

template <typename Fn>
SkPath overSamples(const SkPath& path, float segmentPx, bool smooth,
                   Fn perContour) {
  SkPathBuilder out;
  for (const Polyline& contour : flatten(path, 0.25f)) {
    const float len = contour.length();
    const int count =
        std::max(8, (int)std::ceil(len / std::max(segmentPx, 0.5f)));
    Sampled samples = resample(contour, count);
    perContour(samples);
    out.addPath(toPath(samples, smooth));
  }
  return out.detach();
}

}  // namespace

SkPath unite(const SkPath& a, const SkPath& b) {
  return binary(a, b, kUnion_SkPathOp);
}
SkPath subtract(const SkPath& a, const SkPath& b) {
  return binary(a, b, kDifference_SkPathOp);
}
SkPath intersect(const SkPath& a, const SkPath& b) {
  return binary(a, b, kIntersect_SkPathOp);
}
SkPath exclude(const SkPath& a, const SkPath& b) {
  return binary(a, b, kXOR_SkPathOp);
}

SkPath unite(const std::vector<SkPath>& paths) {
  SkOpBuilder builder;
  for (const SkPath& p : paths) builder.add(p, kUnion_SkPathOp);
  SkPath out;
  if (!builder.resolve(&out)) return SkPath();
  return out;
}

SkPath simplify(const SkPath& path) {
  SkPath out;
  if (!Simplify(path, &out)) return path;
  return out;
}

SkPath offset(const SkPath& path, float delta) {
  if (std::abs(delta) < 1e-3f) return path;
  SkPaint stroke;
  stroke.setStyle(SkPaint::kStroke_Style);
  stroke.setStrokeWidth(std::abs(delta) * 2.0f);
  stroke.setStrokeJoin(SkPaint::kRound_Join);
  stroke.setStrokeCap(SkPaint::kRound_Cap);
  const SkPath expanded = skpathutils::FillPathWithPaint(path, stroke);
  return delta > 0 ? unite(path, expanded) : simplify(subtract(path, expanded));
}

SkPath Roughen::apply(const SkPath& path) const {
  uint32_t contourIndex = 0;
  return overSamples(path, segmentPx, smooth, [&](Sampled& samples) {
    const uint32_t base = seed + contourIndex++ * 7919u;
    for (size_t i = 0; i < samples.points.size(); ++i) {
      const glm::vec2 n = normalAt(samples.points, i, samples.closed);
      samples.points[i] += n * (noise::hash(base, (uint32_t)i) * amplitude);
    }
  });
}

SkPath Zigzag::apply(const SkPath& path) const {
  // Sample at quarter wavelength so hard teeth land on their vertices.
  const float segment = std::max(wavelengthPx * 0.25f, 0.5f);
  return overSamples(path, segment, smooth, [&](Sampled& samples) {
    const size_t n = samples.points.size();
    const float cycles =
        std::max(1.0f, std::round(samples.sourceLength / wavelengthPx));
    std::vector<glm::vec2> original = samples.points;
    for (size_t i = 0; i < n; ++i) {
      const float phase = (float)i / (float)n * cycles * kTau;
      const float wave =
          smooth ? std::sin(phase)
                 : (std::asin(std::sin(phase)) * (2.0f / kPi));  // triangle
      samples.points[i] +=
          normalAt(original, i, samples.closed) * (wave * amplitude);
    }
  });
}

SkPath PuckerBloat::apply(const SkPath& path) const {
  const float amount = std::clamp(this->amount, -1.0f, 1.0f);
  return overSamples(path, segmentPx, true, [&](Sampled& samples) {
    const glm::vec2 c = samples.centroid();
    float rMax = 1e-3f;
    for (const glm::vec2& p : samples.points)
      rMax = std::max(rMax, distance(p, c));
    // Radial power warp: exponent < 1 bloats (spherize), > 1 puckers.
    const float exponent = std::pow(2.0f, -amount * 1.6f);
    for (glm::vec2& p : samples.points) {
      const glm::vec2 d = p - c;
      const float r = length(d);
      if (r < 1e-6f) continue;
      const float rNew = rMax * std::pow(r / rMax, exponent);
      p = c + d / r * rNew;
    }
  });
}

SkPath Twirl::apply(const SkPath& path) const {
  return overSamples(path, segmentPx, true, [&](Sampled& samples) {
    const glm::vec2 c = samples.centroid();
    float rMax = 1e-3f;
    for (const glm::vec2& p : samples.points)
      rMax = std::max(rMax, distance(p, c));
    const float full = angleDeg * kDegToRad;
    for (glm::vec2& p : samples.points) {
      const glm::vec2 d = p - c;
      const float r = length(d);
      const float falloff = 1.0f - std::clamp(r / rMax, 0.0f, 1.0f);
      const float a = full * falloff * falloff;
      const float cs = std::cos(a), sn = std::sin(a);
      p = {c.x + d.x * cs - d.y * sn, c.y + d.x * sn + d.y * cs};
    }
  });
}

PathOp chain(std::vector<PathOp> steps) {
  return [steps = std::move(steps)](const SkPath& path) {
    SkPath current = path;
    for (const PathOp& step : steps)
      if (step) current = step(current);
    return current;
  };
}

}  // namespace sigil::geometry::ops
