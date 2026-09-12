/** @file
 * THE DISTORTS THAT RESAMPLE: a contour walked at a fixed spacing, its
 * samples displaced, and the displaced run drawn back as the answer —
 * a roughening along the normal, a wave with a whole number of cycles
 * around the contour, a radial power warp, a twirl that falls off with
 * radius, and the square wave a contour measure plots directly.
 *
 * Resampling is what they have in common and what separates them from
 * every other operator here: the answer's nodes are the samples, so it
 * no longer interpolates against the source, and the source's fill type
 * is what carries over instead.
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Operations.h"
#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::path::operations {

namespace {

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
  // The answer is the source shape moved, so it fills the way the source
  // fills: a default builder would return an even-odd donut as winding
  // and fill its hole solid.
  SkPathBuilder out(path.getFillType());
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

SkPath Roughen::apply(const SkPath& path) const {
  uint64_t contourIndex = 0;
  return overSamples(path, segmentPx, smooth, [&](Sampled& samples) {
    core::chance::Stream stream = core::chance::Stream::of(
        source, seed + contourIndex++ * 7919u, parameter);
    for (size_t i = 0; i < samples.points.size(); ++i) {
      const glm::vec2 n = normalAt(samples.points, i, samples.closed);
      samples.points[i] += n * (stream.signedUnit() * amplitude);
    }
  });
}

SkPath Zigzag::apply(const SkPath& path) const {
  // Sample at quarter wavelength so hard teeth land on their vertices.
  const float segment = std::max(wavelengthPx * 0.25f, 0.5f);
  return overSamples(path, segment, smooth, [&](Sampled& samples) {
    const size_t n = samples.points.size();
    if (n < 2) return;
    const float cycles =
        std::max(1.0f, std::round(samples.sourceLength / wavelengthPx));
    // A closed contour's last sample is one step short of its first, so
    // the phase runs over n steps; an open one's last sample IS its end,
    // and a whole number of cycles over n − 1 steps puts the wave back at
    // zero there — which is what keeps an open mark's endpoints on the
    // curve they were displaced from.
    const float denominator = samples.closed ? (float)n : (float)(n - 1);
    std::vector<glm::vec2> original = samples.points;
    for (size_t i = 0; i < n; ++i) {
      const float phase = (float)i / denominator * cycles * kTau;
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

SkPath displaceSquare(const SkPath& src, float amplitude, float wavelength) {
  SkPathBuilder out(src.getFillType());
  SkContourMeasureIter iter(src, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    const float lambdaMax = std::max(wavelength, 2.0f);
    const float lambda = len / std::max(1.0f, std::round(len / lambdaMax));
    auto plot = [&](float d, float disp, bool first) {
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(std::min(d, len), &pos, &tan)) return;
      const SkPoint p{pos.x() - tan.y() * disp, pos.y() + tan.x() * disp};
      if (first)
        out.moveTo(p);
      else
        out.lineTo(p);
    };
    plot(0, 0, true);
    float cur = amplitude;
    plot(0, cur, false);
    // the loop walks a distance; the accumulated float is the position
    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
    for (float d = lambda * 0.5f; d < len - 0.25f; d += lambda * 0.5f) {
      plot(d, cur, false);
      cur = -cur;
      plot(d, cur, false);
    }
    plot(len, cur, false);
    plot(len, 0, false);
    if (contour->isClosed()) out.close();
  }
  return out.detach();
}

}  // namespace sigil::geometry::path::operations
