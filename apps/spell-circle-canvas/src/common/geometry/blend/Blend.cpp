/** @file
 * Shape interpolation: keys resampled to matching point counts, aligned,
 * and expanded into steps spaced by count, distance or colour, along an
 * optional spine, with colours interpolated in OKLab.
 */

#include "sigilgeometry/blend/Blend.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Skia.h"
#include "sigilmaterial/core/Color.h"

namespace sigil::geometry::blend {

namespace {

/** `v` scaled to unit length, or `fallback` when it has none. The
 *  magnitude is taken in double so a nearly-degenerate vector keeps its
 *  direction instead of rounding to zero. */
glm::vec2 unit(glm::vec2 v, glm::vec2 fallback) {
  const double xx = v.x, yy = v.y;
  const double mag = std::sqrt(xx * xx + yy * yy);
  if (!(mag > 0) || !std::isfinite(mag)) return fallback;
  const double scale = 1.0 / mag;
  const glm::vec2 out{(float)(xx * scale), (float)(yy * scale)};
  if (!std::isfinite(out.x) || !std::isfinite(out.y)) return fallback;
  return out;
}

/** A key reduced to blendable form: aligned samples per contour plus a
 *  whole-shape centroid. */
struct Prepared {
  std::vector<Sampled> contours;
  glm::vec2 centroid = {0, 0};
};

Prepared prepare(const Key& key, int samples) {
  Prepared out;
  out.contours = resample(key.path, samples);
  double x = 0, y = 0;
  size_t n = 0;
  for (const Sampled& c : out.contours) {
    for (const glm::vec2& p : c.points) {
      x += p.x;
      y += p.y;
    }
    n += c.points.size();
  }
  if (n > 0) out.centroid = {(float)(x / (double)n), (float)(y / (double)n)};
  return out;
}

/** A degenerate contour every point of which is @p at — what a missing
 *  contour blends against (it grows from / collapses to a point). */
Sampled collapsed(const Sampled& like, glm::vec2 at) {
  Sampled out;
  out.closed = like.closed;
  out.points.assign(like.points.size(), at);
  return out;
}

struct Spine {
  Polyline line;
  std::vector<float> cumulative;  // arc length at each vertex
  float total = 0;

  void build(const Polyline& flat) {
    line = flat;
    cumulative.clear();
    cumulative.push_back(0);
    for (size_t i = 1; i < line.points.size(); ++i)
      cumulative.push_back(cumulative.back() +
                           distance(line.points[i - 1], line.points[i]));
    total = cumulative.empty() ? 0 : cumulative.back();
  }

  void at(float distance, glm::vec2* pos, glm::vec2* tangent) const {
    if (line.points.size() < 2) {
      *pos = line.points.empty() ? glm::vec2{0, 0} : line.points.front();
      *tangent = {1, 0};
      return;
    }
    distance = std::clamp(distance, 0.0f, total);
    size_t seg = 0;
    while (seg + 2 < cumulative.size() && cumulative[seg + 1] < distance) ++seg;
    const float span = cumulative[seg + 1] - cumulative[seg];
    const float t = span < 1e-9f ? 0 : (distance - cumulative[seg]) / span;
    const glm::vec2 a = line.points[seg];
    const glm::vec2 b = line.points[seg + 1];
    *pos = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
    *tangent = unit(b - a, {1, 0});
  }
};

int stepsForPair(const Options& options, const Key& a, const Key& b,
                 float spanLength) {
  switch (options.spacing) {
    case Spacing::Steps:
      return std::max(options.steps, 0);
    case Spacing::Distance:
      return options.distance <= 0
                 ? 0
                 : std::max(0,
                            (int)std::floor(spanLength / options.distance) - 1);
    case Spacing::SmoothColor: {
      // Enough steps that adjacent colors differ by under a display
      // quantum: Illustrator's 254-step black-to-white, scaled by the
      // actual color distance.
      auto channelDelta = [](const SkColor4f& x, const SkColor4f& y) {
        return std::max({std::abs(x.fR - y.fR), std::abs(x.fG - y.fG),
                         std::abs(x.fB - y.fB), std::abs(x.fA - y.fA)});
      };
      float delta = channelDelta(a.fill, b.fill);
      if (a.stroke && b.stroke)
        delta = std::max(delta, channelDelta(*a.stroke, *b.stroke));
      return std::clamp((int)std::ceil(delta * 254.0f), 1, 254);
    }
  }
  return 0;
}

Step makeStep(const Prepared& a, const Prepared& b,
              const std::vector<Alignment>& alignments, const Key& keyA,
              const Key& keyB, float u, const Options& options,
              const Spine& spine, float spanStart, float spanLength,
              glm::vec2 baseline) {
  Step step;
  step.fill = detail::lerpOklab(keyA.fill, keyB.fill, u);
  if (keyA.stroke || keyB.stroke) {
    const SkColor4f sa = keyA.stroke.value_or(keyA.fill);
    const SkColor4f sb = keyB.stroke.value_or(keyB.fill);
    step.stroke = detail::lerpOklab(sa, sb, u);
  }
  step.strokeWidth =
      keyA.strokeWidth + (keyB.strokeWidth - keyA.strokeWidth) * u;
  step.opacity = keyA.opacity + (keyB.opacity - keyA.opacity) * u;

  // Interpolate every contour pair; unmatched contours collapse to the
  // other key's centroid.
  const size_t contourCount = std::max(a.contours.size(), b.contours.size());
  SkPathBuilder builder;
  for (size_t c = 0; c < contourCount; ++c) {
    const bool hasA = c < a.contours.size();
    const bool hasB = c < b.contours.size();
    Sampled sa = hasA ? a.contours[c] : collapsed(b.contours[c], b.centroid);
    Sampled sb = hasB ? (hasA ? applyAlignment(b.contours[c], alignments[c])
                              : b.contours[c])
                      : collapsed(a.contours[c], a.centroid);
    const Sampled blended = lerp(sa, sb, u);
    builder.addPath(toPath(blended, options.smoothOutlines));
  }
  SkPath path = builder.detach();

  // Ride the spine: default (empty) spine IS the straight centroid
  // line, which plain interpolation already follows — only a custom
  // spine needs a placement transform.
  if (spine.line.points.size() >= 2) {
    glm::vec2 pos;
    glm::vec2 tangent;
    spine.at(spanStart + u * spanLength, &pos, &tangent);
    const glm::vec2 naturalCenter = {
        a.centroid.x + (b.centroid.x - a.centroid.x) * u,
        a.centroid.y + (b.centroid.y - a.centroid.y) * u};
    SkMatrix placement =
        SkMatrix::Translate(pos.x - naturalCenter.x, pos.y - naturalCenter.y);
    if (options.orientation == Orientation::AlignToPath) {
      const float tangentDeg =
          SkRadiansToDegrees(std::atan2(tangent.y, tangent.x));
      const float baselineDeg =
          SkRadiansToDegrees(std::atan2(baseline.y, baseline.x));
      placement.preConcat(
          SkMatrix::RotateDeg(tangentDeg - baselineDeg, toSk(naturalCenter)));
    }
    path = path.makeTransform(placement);
  }

  step.path = std::move(path);
  return step;
}

}  // namespace

namespace detail {

SkColor4f lerpOklab(const SkColor4f& a, const SkColor4f& b, float t) {
  const material::Color c = material::lerpOklab({a.fR, a.fG, a.fB, a.fA},
                                                {b.fR, b.fG, b.fB, b.fA}, t);
  return {c.r, c.g, c.b, c.a};
}

}  // namespace detail

std::vector<Step> make(std::span<const Key> keys, const Options& options) {
  std::vector<Step> steps;
  if (keys.empty()) return steps;
  if (keys.size() == 1) {
    if (options.includeKeys)
      steps.push_back({keys[0].path, keys[0].fill, keys[0].stroke,
                       keys[0].strokeWidth, keys[0].opacity, 0.0f});
    return steps;
  }

  const int samples = std::max(options.samples, 8);
  std::vector<Prepared> prepared;
  prepared.reserve(keys.size());
  for (const Key& key : keys) prepared.push_back(prepare(key, samples));

  // Custom spine: flatten once, split into one equal arc-length span
  // per key pair.
  Spine spine;
  if (!options.spine.isEmpty()) {
    std::vector<Polyline> flat = flatten(options.spine, 0.25f);
    if (!flat.empty()) {
      if (options.reverseSpine) flat.front().reverse();
      spine.build(flat.front());
    }
  }
  const float pairSpan =
      spine.total > 0 ? spine.total / (float)(keys.size() - 1) : 0;

  const float pairs = (float)(keys.size() - 1);
  for (size_t p = 0; p + 1 < keys.size(); ++p) {
    const Prepared& a = prepared[p];
    const Prepared& b = prepared[p + 1];

    std::vector<Alignment> alignments(
        std::min(a.contours.size(), b.contours.size()));
    for (size_t c = 0; c < alignments.size(); ++c)
      if (a.contours[c].closed && b.contours[c].closed)
        alignments[c] = bestAlignment(a.contours[c], b.contours[c]);

    const float spanStart = pairSpan * (float)p;
    const float spanLength =
        spine.total > 0 ? pairSpan : distance(a.centroid, b.centroid);
    const glm::vec2 baseline = unit(b.centroid - a.centroid, {1, 0});

    const int between = stepsForPair(options, keys[p], keys[p + 1], spanLength);
    const bool emitFirstKey = options.includeKeys && p == 0;
    const bool emitLastKey = options.includeKeys;
    const int total = between + 2;  // including both keys
    for (int i = 0; i < total; ++i) {
      const bool isFirst = i == 0;
      const bool isLast = i == total - 1;
      if (isFirst && !emitFirstKey) continue;
      if (isLast && !emitLastKey) continue;
      if (!isFirst && !isLast && between == 0) continue;
      const float u = total <= 1 ? 0 : (float)i / (float)(total - 1);
      Step step = makeStep(a, b, alignments, keys[p], keys[p + 1], u, options,
                           spine, spanStart,
                           spine.total > 0 ? pairSpan : spanLength, baseline);
      step.t = ((float)p + u) / pairs;
      steps.push_back(std::move(step));
    }
  }
  return steps;
}

std::vector<Step> make(const Key& from, const Key& to, const Options& options) {
  const Key keys[2] = {from, to};
  return make(std::span<const Key>(keys, 2), options);
}

void draw(SkCanvas& canvas, std::span<const Step> steps) {
  for (const Step& step : steps) {
    SkPaint fill;
    fill.setAntiAlias(true);
    SkColor4f fc = step.fill;
    fc.fA *= step.opacity;
    fill.setColor4f(fc);
    canvas.drawPath(step.path, fill);
    if (step.stroke && step.strokeWidth > 0) {
      SkPaint stroke;
      stroke.setAntiAlias(true);
      stroke.setStyle(SkPaint::kStroke_Style);
      stroke.setStrokeWidth(step.strokeWidth);
      SkColor4f sc = *step.stroke;
      sc.fA *= step.opacity;
      stroke.setColor4f(sc);
      canvas.drawPath(step.path, stroke);
    }
  }
}

}  // namespace sigil::geometry::blend
