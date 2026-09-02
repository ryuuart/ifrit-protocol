/** @file
 * The path operators: booleans over Skia's pathops, the stroke-expansion
 * offset, and the four distortions applied to resampled points.
 */

#include "sigilgeometry/path/Ops.h"

#include <include/core/SkPaint.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkCornerPathEffect.h>
#include <include/pathops/SkPathOps.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

#include "sigilgeometry/path/Noise.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::path::ops {

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

SkPath roundCorners(const SkPath& path, float radius) {
  if (radius <= 0) return path;
  SkPathBuilder dst;
  SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
  if (sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(radius);
      fx && fx->filterPath(&dst, path, &rec))
    return dst.detach();
  return path;
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


// ---------------------------------------------------------------------------
// Corner and displacement treatments over a POLYLINE contour.

SkPath chamferCorners(const SkPath& path, float cut) {
  if (cut <= 0 || path.isEmpty()) return path;
  SkPathBuilder out;
  std::vector<SkPoint> run;  // current contour's polyline vertices
  SkPathBuilder verbatim;    // the same contour, copied exactly
  bool closed = false, anyCurve = false;

  // A vertex's cut points: entry on the incoming leg, exit on the
  // outgoing leg, each clamped to half its leg. False at a
  // straight-through or degenerate vertex (no corner to cut).
  const auto cutAt = [&](size_t i, SkPoint& entry, SkPoint& exit) {
    const size_t n = run.size();
    const SkPoint prev = run[(i + n - 1) % n], v = run[i],
                  next = run[(i + 1) % n];
    const SkVector in{v.x() - prev.x(), v.y() - prev.y()};
    const SkVector outV{next.x() - v.x(), next.y() - v.y()};
    const float lenIn = std::hypot(in.x(), in.y());
    const float lenOut = std::hypot(outV.x(), outV.y());
    if (lenIn < 1e-4f || lenOut < 1e-4f) return false;
    const float cross = in.x() * outV.y() - in.y() * outV.x();
    const float dot = in.x() * outV.x() + in.y() * outV.y();
    if (std::abs(cross) <= 1e-4f * lenIn * lenOut && dot > 0)
      return false;  // straight through — no corner
    const float cIn = std::min(cut, lenIn * 0.5f);
    const float cOut = std::min(cut, lenOut * 0.5f);
    entry = {v.x() - in.x() / lenIn * cIn, v.y() - in.y() / lenIn * cIn};
    exit = {v.x() + outV.x() / lenOut * cOut, v.y() + outV.y() / lenOut * cOut};
    return true;
  };

  const auto emitChamfered = [&] {
    if (run.empty()) return;
    if (closed && run.size() > 1 && run.front() == run.back())
      run.pop_back();  // the closing joint belongs to close()
    const size_t n = run.size();
    SkPoint entry, exit;
    if (n < 3) {  // nothing to cut — as collected
      out.moveTo(run.front());
      for (size_t i = 1; i < n; ++i) out.lineTo(run[i]);
      if (closed) out.close();
      return;
    }
    if (!closed) {
      out.moveTo(run.front());
      for (size_t i = 1; i + 1 < n; ++i) {
        if (cutAt(i, entry, exit)) {
          out.lineTo(entry);
          out.lineTo(exit);
        } else {
          out.lineTo(run[i]);
        }
      }
      out.lineTo(run.back());
    } else {  // every vertex is interior, the moveTo joint included
      bool started = false;
      // NOT named `emit`: this header reaches Qt TUs, where that is a macro.
      const auto put = [&](SkPoint p) {
        if (started)
          out.lineTo(p);
        else {
          out.moveTo(p);
          started = true;
        }
      };
      for (size_t i = 0; i < n; ++i) {
        if (cutAt(i, entry, exit)) {
          put(entry);
          out.lineTo(exit);
          started = true;
        } else {
          put(run[i]);
        }
      }
      out.close();
    }
  };

  const auto flushContour = [&] {
    if (anyCurve)  // chamfer is a polyline treatment: curves pass through
      out.addPath(verbatim.detach());
    else
      emitChamfered();
    verbatim = SkPathBuilder();
    run.clear();
    closed = false;
    anyCurve = false;
  };

  SkPath::Iter iter(path, false);
  SkPoint pts[4];
  SkPath::Verb verb;
  while ((verb = iter.next(pts)) != SkPath::kDone_Verb) {
    switch (verb) {
      case SkPath::kMove_Verb:
        flushContour();
        run.push_back(pts[0]);
        verbatim.moveTo(pts[0]);
        break;
      case SkPath::kLine_Verb:
        run.push_back(pts[1]);
        verbatim.lineTo(pts[1]);
        break;
      case SkPath::kQuad_Verb:
        anyCurve = true;
        verbatim.quadTo(pts[1], pts[2]);
        break;
      case SkPath::kConic_Verb:
        anyCurve = true;
        verbatim.conicTo(pts[1], pts[2], iter.conicWeight());
        break;
      case SkPath::kCubic_Verb:
        anyCurve = true;
        verbatim.cubicTo(pts[1], pts[2], pts[3]);
        break;
      case SkPath::kClose_Verb:
        closed = true;
        verbatim.close();
        break;
      default:
        break;
    }
  }
  flushContour();
  return out.detach();
}

SkPath displaceSquare(const SkPath& src, float amplitude, float wavelength) {
  SkPathBuilder out;
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

}  // namespace sigil::geometry::path::ops
