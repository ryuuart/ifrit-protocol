/** @file
 * The silhouette catalog's bodies: every generator's `outline(size)` and
 * the corner shapes, each built on a Skia path builder and handed back
 * as an outline.
 */

#include "sigilgeometry/kit/Silhouettes.h"

#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcore/compute/Noise.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "sigilgeometry/path/Operations.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/advanced/Skia.h"

namespace sigil::geometry::shapes {

namespace detail {

path::Outline samplePolyline(const std::function<glm::vec2(float)>& f,
                             float t0, float t1, int samples, bool close,
                             glm::vec2 size) {
  const float cx = size.x * 0.5f, cy = size.y * 0.5f;
  const path::Polyline unit = path::sample(f, t0, t1, samples, close);
  SkPathBuilder b;
  bool first = true;
  for (const glm::vec2& u : unit.points) {
    const SkPoint p{cx + cx * u.x, cy + cy * u.y};
    if (first)
      b.moveTo(p);
    else
      b.lineTo(p);
    first = false;
  }
  if (close) b.close();
  return path::fromSk(b.detach());
}

path::Outline cornerOutline(const path::Outline& outline, float radius,
                            const CornerOptions& options) {
  const SkPath source = path::toSk(outline);
  if (options.shape == CornerShape::Bevel)
    return path::fromSk(path::operations::chamferCorners(source, radius));
  return path::fromSk(path::operations::roundCorners(source, radius));
}

path::Outline translated(const path::Outline& outline, glm::vec2 offset) {
  return path::fromSk(path::toSk(outline).makeOffset(offset.x, offset.y));
}

path::Outline shape(const path::Outline& outline, const path::Shaper& shaper) {
  return shaper.shape(outline);
}

}  // namespace detail

path::Outline Blob::outline(glm::vec2 size) const {
  const SkSize s = path::toSkSize(size);
  const int n = std::max(lobes, 3);
  const float cx = s.width() / 2, cy = s.height() / 2;
  std::vector<SkPoint> pts((size_t)n);
  for (int i = 0; i < n; ++i) {
    const float a = -SK_FloatPI / 2 + i * (2 * SK_FloatPI / n);
    const float r =
        1.0f - amplitude * (0.5f + 0.5f * core::noise::hash(seed, (uint32_t)i));
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
  return path::fromSk(b.detach());
}

path::Outline Parallelogram::outline(glm::vec2 size) const {
  const SkSize s = path::toSkSize(size);
  // One signed lean drives both ends: the top edge slides right by it and
  // the bottom left by it, or the reverse when the skew is negative. So
  // either sign inscribes a parallelogram of width w - |lean| in the box
  // rather than running one edge past it.
  const float lean = std::tan(skewDeg * 0.017453293f) * s.height();
  const float top = std::max(0.0f, lean), bottom = std::max(0.0f, -lean);
  SkPathBuilder b;
  b.moveTo(top, 0);
  b.lineTo(s.width() - bottom, 0);
  b.lineTo(s.width() - top, s.height());
  b.lineTo(bottom, s.height());
  b.close();
  return path::fromSk(b.detach());
}

path::Outline Lissajous::outline(glm::vec2 size) const {
  const float delta = deltaDeg * SK_FloatPI / 180.0f;
  return detail::samplePolyline(
      [fa = a, fb = b, delta](float t) {
        return glm::vec2{std::sin(fa * t + delta), std::sin(fb * t)};
      },
      0.0f, turns * 2.0f * SK_FloatPI, samples, false, size);
}

path::Outline Harmonograph::outline(glm::vec2 size) const {
  const float delta = deltaDeg * SK_FloatPI / 180.0f;
  return detail::samplePolyline(
      [fa = a, fb = b, delta, fdamping = damping,
       fprecession = precession](float t) {
        const float envelope = std::exp(-fdamping * t);
        const float x = envelope * std::sin(fa * t + delta);
        const float y = envelope * std::sin(fb * t);
        if (fprecession == 0.0f) return glm::vec2{x, y};
        const float th = fprecession * t;
        const float c = std::cos(th), sn = std::sin(th);
        return glm::vec2{x * c - y * sn, x * sn + y * c};
      },
      0.0f, turns * 2.0f * SK_FloatPI, samples, false, size);
}

path::Outline Rose::outline(glm::vec2 size) const {
  return detail::samplePolyline(
      [fk = k](float th) {
        const float r = std::cos(fk * th);
        return glm::vec2{r * std::cos(th), r * std::sin(th)};
      },
      0.0f, turns * 2.0f * SK_FloatPI, samples, false, size);
}

path::Outline Spiral::outline(glm::vec2 size) const {
  const float total = turns * 2.0f * SK_FloatPI;
  return detail::samplePolyline(
      [flog = logarithmic, fgrowth = growth, total](float th) {
        const float r = flog
                            ? std::exp(fgrowth * th) / std::exp(fgrowth * total)
                            : th / total;
        return glm::vec2{r * std::cos(th), r * std::sin(th)};
      },
      0.0f, total, samples, false, size);
}

path::Outline Trochoid::outline(glm::vec2 size) const {
  const float sign = inside ? -1.0f : 1.0f;
  const float sum = R + sign * r;
  const float extent = std::max(std::abs(sum) + std::abs(d), 1e-3f);
  return detail::samplePolyline(
      [fR = R, fr = r, fd = d, sign, sum, extent](float t) {
        const float k = sum / std::max(fr, 1e-3f);
        return glm::vec2{
            (sum * std::cos(t) - sign * fd * std::cos(k * t)) / extent,
            (sum * std::sin(t) - fd * std::sin(k * t)) / extent};
      },
      0.0f, turns * 2.0f * SK_FloatPI, samples, false, size);
}

path::Outline Chamfered::outline(glm::vec2 size) const {
  const SkSize s = path::toSkSize(size);
  const float w = s.width(), h = s.height();
  // A 45 degree cut clamps to the SHORT side so it stays at 45 degrees;
  // an anisotropic one was never at 45 and clamps each leg to its own
  // half-side. A cut of no rise is a square corner and needs no spelling
  // of its own, which is what lets zero mean "the rise is the run".
  const bool square = cutRise <= 0.0f;
  const float run = square ? std::clamp(cut, 0.0f, std::min(w, h) * 0.5f)
                           : std::clamp(cut, 0.0f, w * 0.5f);
  const float rise = square ? run : std::clamp(cutRise, 0.0f, h * 0.5f);
  const float r = std::clamp(radius, 0.0f, std::min(w, h) * 0.5f);
  const float d = r * 2.0f;
  // A CUT OF ZERO IS A SQUARE CORNER, not a cut of no length. Emitting the
  // two vertices anyway puts a duplicate point at each corner, and every
  // treatment that reads the vertices afterwards — rounding among them —
  // sees a degenerate segment there and rounds nothing.
  const auto cutting = [&](Corner corner) {
    return run > 0.0f && rise > 0.0f && has(mask, corner);
  };
  SkPathBuilder b;
  if (cutting(Corner::TopLeft)) {
    b.moveTo(run, 0);
  } else if (r > 0.0f) {
    b.moveTo(0, r);
    b.arcTo(SkRect::MakeXYWH(0, 0, d, d), 180, 90, false);
  } else {
    b.moveTo(0, 0);
  }
  if (cutting(Corner::TopRight)) {
    b.lineTo(w - run, 0);
    b.lineTo(w, rise);
  } else if (r > 0.0f) {
    b.lineTo(w - r, 0);
    b.arcTo(SkRect::MakeXYWH(w - d, 0, d, d), 270, 90, false);
  } else {
    b.lineTo(w, 0);
  }
  if (cutting(Corner::BottomRight)) {
    b.lineTo(w, h - rise);
    b.lineTo(w - run, h);
  } else if (r > 0.0f) {
    b.lineTo(w, h - r);
    b.arcTo(SkRect::MakeXYWH(w - d, h - d, d, d), 0, 90, false);
  } else {
    b.lineTo(w, h);
  }
  if (cutting(Corner::BottomLeft)) {
    b.lineTo(run, h);
    b.lineTo(0, h - rise);
  } else if (r > 0.0f) {
    b.lineTo(r, h);
    b.arcTo(SkRect::MakeXYWH(0, h - d, d, d), 90, 90, false);
  } else {
    b.lineTo(0, h);
  }
  if (cutting(Corner::TopLeft)) b.lineTo(0, rise);
  b.close();
  return path::fromSk(b.detach());
}

path::Outline Notched::outline(glm::vec2 size) const {
  const SkSize s = path::toSkSize(size);
  const float w = s.width(), h = s.height();
  const float n = std::clamp(notchWidth, 0.0f, std::min(w, h) * 0.45f);
  const float d = std::clamp(depth, 0.0f, std::min(w, h) * 0.45f);
  // A bite with no width or no depth is a SQUARE CORNER, for the same
  // reason a chamfer of zero is: the vertices it would emit stand on top
  // of each other and every later treatment reads them as a segment.
  const auto biting = [&](Corner corner) {
    return n > 0.0f && d > 0.0f && has(mask, corner);
  };
  SkPathBuilder b;
  if (biting(Corner::TopLeft))
    b.moveTo(n, 0);
  else
    b.moveTo(0, 0);
  if (biting(Corner::TopRight)) {
    b.lineTo(w - n, 0);
    b.lineTo(w - n, d);
    b.lineTo(w, d);
  } else {
    b.lineTo(w, 0);
  }
  if (biting(Corner::BottomRight)) {
    b.lineTo(w, h - d);
    b.lineTo(w - n, h - d);
    b.lineTo(w - n, h);
  } else {
    b.lineTo(w, h);
  }
  if (biting(Corner::BottomLeft)) {
    b.lineTo(n, h);
    b.lineTo(n, h - d);
    b.lineTo(0, h - d);
  } else {
    b.lineTo(0, h);
  }
  if (biting(Corner::TopLeft)) {
    b.lineTo(0, d);
    b.lineTo(n, d);
  }
  b.close();
  return path::fromSk(b.detach());
}

path::Outline Arrow::outline(glm::vec2 size) const {
  const SkSize s = path::toSkSize(size);
  const float w = s.width(), h = s.height();
  const float half = std::clamp(shaftFrac, 0.02f, 1.0f) * h * 0.5f;
  const float head = std::clamp(headFrac, 0.05f, 1.0f) * w;
  const float span = std::clamp(headSpan, 0.0f, 1.0f) * h * 0.5f;
  const float cy = h * 0.5f;
  SkPathBuilder b;
  b.moveTo(0, cy - half);
  b.lineTo(w - head, cy - half);
  b.lineTo(w - head, cy - span);
  b.lineTo(w, cy);
  b.lineTo(w - head, cy + span);
  b.lineTo(w - head, cy + half);
  b.lineTo(0, cy + half);
  b.close();
  return path::fromSk(b.detach());
}

path::Outline Chevron::outline(glm::vec2 size) const {
  const SkSize s = path::toSkSize(size);
  const float w = s.width(), h = s.height();
  const float cx = w * 0.5f, cy = h * 0.5f;
  const float out = w * spread, fall = h * drop, weight = h * thickness;
  // The shoulders stand a third of the drop above centre, which is what
  // keeps the two arms shallow enough to read as a level rather than as
  // an arrowhead.
  const float shoulder = cy - fall * 0.35f;
  SkPathBuilder b;
  b.moveTo(cx - out, shoulder);
  b.lineTo(cx, cy + fall);
  b.lineTo(cx + out, shoulder);
  b.lineTo(cx + out - weight * 0.4f, shoulder - weight);
  b.lineTo(cx, cy + fall - weight * 1.5f);
  b.lineTo(cx - out + weight * 0.4f, shoulder - weight);
  b.close();
  if (bars > 0) {
    const float run = w * bars;
    b.addRect({0, cy - weight * 0.5f, run, cy + weight * 0.5f});
    b.addRect({w - run, cy - weight * 0.5f, w, cy + weight * 0.5f});
  }
  return path::fromSk(b.detach());
}

}  // namespace sigil::geometry::shapes
