#include "sigilshape/Ops.h"

#include "sigilshape/Geometry.h"

#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/pathops/SkPathOps.h>

#include <algorithm>
#include <cmath>

namespace sigil::shape::ops {

namespace {

SkPath binary(const SkPath &a, const SkPath &b, SkPathOp op) {
  SkPath out;
  if (!Op(a, b, op, &out))
    return SkPath();
  return out;
}

float hashNoise(uint32_t seed, uint32_t i) {
  uint32_t x = seed * 0x9E3779B9u + i * 0x85EBCA6Bu;
  x ^= x >> 16;
  x *= 0x7FEB352Du;
  x ^= x >> 15;
  x *= 0x846CA68Bu;
  x ^= x >> 16;
  return (float)x / (float)0xFFFFFFFFu * 2.0f - 1.0f; // [-1, 1]
}

/** Outward normal at sample i of a closed/open polyline (central
 *  difference tangent, rotated). */
SkVector normalAt(const std::vector<SkPoint> &pts, size_t i, bool closed) {
  const size_t n = pts.size();
  const SkPoint prev = pts[closed ? (i + n - 1) % n
                                  : (i == 0 ? 0 : i - 1)];
  const SkPoint next = pts[closed ? (i + 1) % n
                                  : std::min(i + 1, n - 1)];
  SkVector t = next - prev;
  if (!t.normalize())
    t = {1, 0};
  return {t.fY, -t.fX};
}

template <typename Fn>
SkPath overSamples(const SkPath &path, float segmentPx, bool smooth,
                   Fn perContour) {
  SkPathBuilder out;
  for (const Polyline &contour : flatten(path, 0.25f)) {
    const float len = contour.length();
    const int count =
        std::max(8, (int)std::ceil(len / std::max(segmentPx, 0.5f)));
    Sampled samples = resample(contour, count);
    perContour(samples);
    out.addPath(toPath(samples, smooth));
  }
  return out.detach();
}

} // namespace

SkPath unite(const SkPath &a, const SkPath &b) {
  return binary(a, b, kUnion_SkPathOp);
}
SkPath subtract(const SkPath &a, const SkPath &b) {
  return binary(a, b, kDifference_SkPathOp);
}
SkPath intersect(const SkPath &a, const SkPath &b) {
  return binary(a, b, kIntersect_SkPathOp);
}
SkPath exclude(const SkPath &a, const SkPath &b) {
  return binary(a, b, kXOR_SkPathOp);
}

SkPath unite(const std::vector<SkPath> &paths) {
  SkOpBuilder builder;
  for (const SkPath &p : paths)
    builder.add(p, kUnion_SkPathOp);
  SkPath out;
  if (!builder.resolve(&out))
    return SkPath();
  return out;
}

SkPath simplify(const SkPath &path) {
  SkPath out;
  if (!Simplify(path, &out))
    return path;
  return out;
}

SkPath offset(const SkPath &path, float delta) {
  if (std::abs(delta) < 1e-3f)
    return path;
  SkPaint stroke;
  stroke.setStyle(SkPaint::kStroke_Style);
  stroke.setStrokeWidth(std::abs(delta) * 2.0f);
  stroke.setStrokeJoin(SkPaint::kRound_Join);
  stroke.setStrokeCap(SkPaint::kRound_Cap);
  const SkPath expanded = skpathutils::FillPathWithPaint(path, stroke);
  return delta > 0 ? unite(path, expanded)
                   : simplify(subtract(path, expanded));
}

SkPath Roughen::apply(const SkPath &path) const {
  uint32_t contourIndex = 0;
  return overSamples(path, segmentPx, smooth, [&](Sampled &samples) {
    const uint32_t base = seed + contourIndex++ * 7919u;
    for (size_t i = 0; i < samples.points.size(); ++i) {
      const SkVector n = normalAt(samples.points, i, samples.closed);
      const float d = hashNoise(base, (uint32_t)i) * amplitude;
      samples.points[i] += {n.fX * d, n.fY * d};
    }
  });
}

SkPath Zigzag::apply(const SkPath &path) const {
  // Sample at quarter wavelength so hard teeth land on their vertices.
  const float segment = std::max(wavelengthPx * 0.25f, 0.5f);
  return overSamples(path, segment, smooth, [&](Sampled &samples) {
    const size_t n = samples.points.size();
    const float cycles =
        std::max(1.0f, std::round(samples.sourceLength / wavelengthPx));
    std::vector<SkPoint> original = samples.points;
    for (size_t i = 0; i < n; ++i) {
      const float phase = (float)i / (float)n * cycles * 2.0f * (float)M_PI;
      const float wave = smooth ? std::sin(phase)
                                : (std::asin(std::sin(phase)) *
                                   (2.0f / (float)M_PI)); // triangle
      const SkVector nrm = normalAt(original, i, samples.closed);
      samples.points[i] += {nrm.fX * wave * amplitude,
                            nrm.fY * wave * amplitude};
    }
  });
}

SkPath PuckerBloat::apply(const SkPath &path) const {
  const float amount = std::clamp(this->amount, -1.0f, 1.0f);
  return overSamples(path, segmentPx, true, [&](Sampled &samples) {
    const SkPoint c = samples.centroid();
    float rMax = 1e-3f;
    for (const SkPoint &p : samples.points)
      rMax = std::max(rMax, SkPoint::Distance(p, c));
    // Radial power warp: exponent < 1 bloats (spherize), > 1 puckers.
    const float exponent = std::pow(2.0f, -amount * 1.6f);
    for (SkPoint &p : samples.points) {
      SkVector d = p - c;
      const float r = d.length();
      if (r < 1e-6f)
        continue;
      const float rNew = rMax * std::pow(r / rMax, exponent);
      p = {c.fX + d.fX / r * rNew, c.fY + d.fY / r * rNew};
    }
  });
}

SkPath Twirl::apply(const SkPath &path) const {
  return overSamples(path, segmentPx, true, [&](Sampled &samples) {
    const SkPoint c = samples.centroid();
    float rMax = 1e-3f;
    for (const SkPoint &p : samples.points)
      rMax = std::max(rMax, SkPoint::Distance(p, c));
    const float full = angleDeg * (float)M_PI / 180.0f;
    for (SkPoint &p : samples.points) {
      SkVector d = p - c;
      const float r = d.length();
      const float falloff = 1.0f - std::clamp(r / rMax, 0.0f, 1.0f);
      const float a = full * falloff * falloff;
      const float cs = std::cos(a), sn = std::sin(a);
      p = {c.fX + d.fX * cs - d.fY * sn, c.fY + d.fX * sn + d.fY * cs};
    }
  });
}

PathOp chain(std::vector<PathOp> steps) {
  return [steps = std::move(steps)](const SkPath &path) {
    SkPath current = path;
    for (const PathOp &step : steps)
      if (step)
        current = step(current);
    return current;
  };
}

} // namespace sigil::shape::ops
