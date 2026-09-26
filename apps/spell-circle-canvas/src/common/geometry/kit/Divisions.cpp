/** @file
 * The division ladders' bodies: ticks, arc segments and chords, each
 * emitted as one outline of N contours on a polar frame.
 */

#include "sigilgeometry/kit/Divisions.h"

#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <vector>

#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::shapes {

path::Outline ticks(const path::PolarFrame& frame, const Ticks& t) {
  SkPathBuilder b;
  const int n = std::max(0, t.divisions);
  if (n == 0) return path::fromSk(b.detach());
  const int count = t.closed ? n + 1 : n;
  const float step = t.sweep / (float)n;
  for (int i = 0; i < count; ++i) {
    Span s = (t.longEvery > 0 && i % t.longEvery == 0) ? t.longMark : t.mark;
    if (t.classify) s = t.classify(i, s);
    if (s.inner == s.outer) continue;
    const float deg = t.from + step * (float)i;
    const SkPoint inner = frame.at(deg, s.inner);
    const SkPoint outer = frame.at(deg, s.outer);
    if (t.markPx <= 0.0f) {
      b.moveTo(inner);
      b.lineTo(outer);
      continue;
    }
    // A closed mark: the same radial run, given a width across it. The
    // offset is perpendicular to the frame's own outward direction, so a
    // mark stands square to its radius whatever the frame's conventions
    // are.
    const SkVector out = frame.dir(deg);
    const SkVector across{-out.fY * t.markPx * 0.5f, out.fX * t.markPx * 0.5f};
    b.moveTo(inner.fX + across.fX, inner.fY + across.fY);
    b.lineTo(outer.fX + across.fX, outer.fY + across.fY);
    b.lineTo(outer.fX - across.fX, outer.fY - across.fY);
    b.lineTo(inner.fX - across.fX, inner.fY - across.fY);
    b.close();
  }
  return path::fromSk(b.detach());
}

path::Outline arcs(const path::PolarFrame& frame, const Arcs& a) {
  SkPathBuilder b;
  const int n = std::max(0, a.divisions);
  if (n == 0 || a.spanDeg == 0.0f || a.mark.inner == a.mark.outer)
    return path::fromSk(b.detach());
  const int count = a.closed ? n + 1 : n;
  const float step = a.sweep / (float)n;
  const SkRect outer = frame.box(a.mark.outer);
  const SkRect inner = frame.box(a.mark.inner);
  for (int i = 0; i < count; ++i) {
    const float centre = a.from + step * (float)i;
    const float start = centre - a.spanDeg * 0.5f;
    const float end = centre + a.spanDeg * 0.5f;
    // Out along the far edge, in across the end, back along the near one:
    // one contour whose two curved sides are the ring's own arcs rather
    // than a polyline that would show its facets under a stroke.
    b.arcTo(outer, frame.skiaDeg(start), frame.skiaSweep(a.spanDeg), true);
    b.lineTo(frame.at(end, a.mark.inner));
    b.arcTo(inner, frame.skiaDeg(end), frame.skiaSweep(-a.spanDeg), false);
    b.close();
  }
  return path::fromSk(b.detach());
}

path::Outline chords(const path::PolarFrame& frame, const Chords& c) {
  SkPathBuilder b;
  const int n = std::max(2, c.sides);
  const int step = std::max(1, c.step);
  const float pitch = 360.0f / (float)n;
  auto vertex = [&](int k) {
    return frame.at(c.from + pitch * (float)((k % n + n) % n), c.radius);
  };
  if (c.closed) {
    // Walk k, k+step, k+2·step … until it returns to k; repeat for every
    // ring the step generates. gcd(n, step) rings, n/gcd vertices each.
    std::vector<bool> seen((size_t)n, false);
    for (int start = 0; start < n; ++start) {
      if (seen[(size_t)start]) continue;
      int k = start;
      bool first = true;
      do {
        seen[(size_t)k] = true;
        const SkPoint p = vertex(k);
        first ? b.moveTo(p) : b.lineTo(p);
        first = false;
        k = (k + step) % n;
      } while (k != start);
      b.close();
    }
    return path::fromSk(b.detach());
  }
  for (int k = 0; k < n; ++k) {
    SkPoint a = vertex(k), z = vertex(k + step);
    if (c.inset > 0) {
      const SkVector d{z.fX - a.fX, z.fY - a.fY};
      const float len = std::hypot(d.fX, d.fY);
      if (len <= 2 * c.inset) continue;
      const SkVector u{d.fX / len, d.fY / len};
      a = {a.fX + u.fX * c.inset, a.fY + u.fY * c.inset};
      z = {z.fX - u.fX * c.inset, z.fY - u.fY * c.inset};
    }
    b.moveTo(a);
    b.lineTo(z);
  }
  return path::fromSk(b.detach());
}

}  // namespace sigil::geometry::shapes
