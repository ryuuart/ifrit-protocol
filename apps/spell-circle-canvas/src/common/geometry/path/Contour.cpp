/** @file
 * A path's sub-paths addressed by arc length: position and tangent at
 * a distance, the piece between two distances, the corners, and the
 * parallel, displaced and corner-window constructions over every
 * contour of a path.
 */

#include "sigilgeometry/path/Contour.h"

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <utility>

#include "OffsetInternal.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::path {

namespace {

/** Position and tangent from the measure, or nullopt where Skia cannot
 *  evaluate. */
std::optional<Contour::Sample> sampleOf(const SkContourMeasure& m, float d) {
  SkPoint pos;
  SkVector tan;
  if (!m.getPosTan(d, &pos, &tan)) return std::nullopt;
  return Contour::Sample{fromSk(pos), fromSk(tan)};
}

/** Dot product spelled as one expression so the compiler contracts it as
 *  it does every other scalar expression here; a two-step vector dot
 *  rounds once more and lands a corner a bit away from where the same
 *  scan placed it before. */
float dot(glm::vec2 a, glm::vec2 b) { return a.x * b.x + a.y * b.y; }

/** The point `across` to the right of travel from a sample (y-down
 *  space: facing +x, right is +y). */
glm::vec2 beside(const Contour::Sample& s, float across) {
  return {s.position.x - s.tangent.y * across,
          s.position.y + s.tangent.x * across};
}

}  // namespace

Contour::Contour(sk_sp<SkContourMeasure> measure)
    : m_measure(std::move(measure)) {}

std::vector<Contour> Contour::of(const SkPath& path, bool forceClosed) {
  std::vector<Contour> out;
  SkContourMeasureIter iter(path, forceClosed);
  while (sk_sp<SkContourMeasure> m = iter.next())
    if (m->length() > 0) out.push_back(Contour(std::move(m)));
  return out;
}

float Contour::length() const { return m_measure ? m_measure->length() : 0; }

bool Contour::closed() const { return m_measure && m_measure->isClosed(); }

std::optional<Contour::Sample> Contour::at(float distance) const {
  if (!m_measure) return std::nullopt;
  return sampleOf(*m_measure, std::clamp(distance, 0.0f, length()));
}

Contour::Sample Contour::around(float distance) const {
  const float len = length();
  if (len <= 0) return {};
  const float d =
      closed() ? wrap(distance, len) : std::clamp(distance, 0.0f, len);
  return at(d).value_or(Sample{});
}

SkPath Contour::segment(float from, float to) const {
  SkPathBuilder b;
  appendSegment(b, from, to);
  return b.detach();
}

void Contour::appendSegment(SkPathBuilder& out, float from, float to) const {
  if (!m_measure) return;
  (void)m_measure->getSegment(from, to, &out, true);
}

std::vector<Contour::Corner> Contour::corners(float angleDeg, float minSpacing,
                                              float step,
                                              float* sharpestDeg) const {
  std::vector<Corner> corners;
  const float len = length();
  if (len <= 0) return corners;
  const SkContourMeasure& m = *m_measure;
  const float cosThresh = std::cos(angleDeg * kDegToRad);
  const float stride = std::max(step, 0.25f);
  float sharpestDot = 1.0f;
  glm::vec2 prev{0, 0}, atStart{0, 0};
  bool havePrev = false;
  // The samples sit at the accumulated stride, not at k * stride.
  // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
  for (float d = 0; d <= len; d += stride) {
    const auto s = sampleOf(m, std::min(d, len));
    if (!s) continue;
    const glm::vec2 tan = s->tangent;
    if (!havePrev) {
      atStart = tan;
    } else {
      const float cosine = dot(prev, tan);
      sharpestDot = std::min(sharpestDot, cosine);
      if (cosine < cosThresh) {
        glm::vec2 inTan = prev, outTan = tan;
        const float at = bisect(
            std::max(0.0f, d - stride), std::min(d, len), [&](float mid) {
              const auto ms = sampleOf(m, mid);
              if (!ms) return true;  // unevaluable: keep the near side
              if (dot(inTan, ms->tangent) < cosThresh) {
                outTan = ms->tangent;
                return false;
              }
              inTan = ms->tangent;
              return true;
            });
        if (corners.empty() || at - corners.back().distance > minSpacing)
          corners.push_back({at, inTan, outTan});
      }
    }
    prev = tan;
    havePrev = true;
  }
  if (closed() && havePrev) {
    const float cosine = dot(prev, atStart);
    sharpestDot = std::min(sharpestDot, cosine);
    if (cosine < cosThresh &&
        (corners.empty() || corners.front().distance > minSpacing))
      corners.insert(corners.begin(), {0.0f, prev, atStart});
  }
  if (sharpestDeg)
    *sharpestDeg = std::acos(std::clamp(sharpestDot, -1.0f, 1.0f)) * kRadToDeg;
  return corners;
}

std::vector<OffsetJoin> offsetJoins(
    const Contour& contour,
    const std::function<float(float distance)>& acrossAt, float stride) {
  std::vector<OffsetJoin> joins;
  for (const Contour::Corner& hit :
       contour.corners(20.0f, std::max(stride, 1.0f), stride)) {
    const auto vertex = contour.at(hit.distance);
    if (!vertex) continue;
    // `beside` measures to the right of travel; the offset is asked for
    // on the left.
    const float side = -acrossAt(hit.distance);
    OffsetJoin join;
    join.distance = hit.distance;
    join.radius = std::abs(side);
    join.vertex = vertex->position;
    join.entering = beside({join.vertex, hit.in}, side);
    join.leaving = beside({join.vertex, hit.out}, side);
    const float turn = hit.in.x * hit.out.y - hit.in.y * hit.out.x;
    if (turn * side >= 0.0f && std::abs(turn) > 1e-4f) {
      // The two offset edges are struck along the SOURCE tangents: a
      // width that changes along the contour slants each offset edge
      // against the edge it came from, and the corner is read from the
      // width AT THE VERTEX rather than from either edge's slant.
      const glm::vec2 apart = join.leaving - join.entering;
      const float reach = (apart.x * hit.out.y - apart.y * hit.out.x) / turn;
      if (std::abs(reach) <= join.radius * 4.0f) {  // a near-reversal miters
        join.miter = true;                          // to infinity — bevel
        join.point = {join.entering.x + hit.in.x * reach,
                      join.entering.y + hit.in.y * reach};
      }
    } else if (turn * side < 0.0f) {
      join.arc = true;
      join.startRadians = std::atan2(join.entering.y - join.vertex.y,
                                     join.entering.x - join.vertex.x);
      const float leavingRadians = std::atan2(join.leaving.y - join.vertex.y,
                                              join.leaving.x - join.vertex.x);
      join.sweepRadians = leavingRadians - join.startRadians;
      while (join.sweepRadians > kPi) join.sweepRadians -= kTau;
      while (join.sweepRadians < -kPi) join.sweepRadians += kTau;
    }
    joins.push_back(join);
  }
  return joins;
}

void appendOffsetPoint(SkPathBuilder& out, glm::vec2 point, bool& started) {
  if (started) {
    out.lineTo(toSk(point));
  } else {
    out.moveTo(toSk(point));
    started = true;
  }
}

void appendOffsetJoin(SkPathBuilder& out, const OffsetJoin& join,
                      bool& started) {
  if (join.miter) {
    appendOffsetPoint(out, join.point, started);
    return;
  }
  appendOffsetPoint(out, join.entering, started);
  if (join.arc) {
    const SkRect oval = SkRect::MakeLTRB(
        join.vertex.x - join.radius, join.vertex.y - join.radius,
        join.vertex.x + join.radius, join.vertex.y + join.radius);
    out.arcTo(oval, join.startRadians * kRadToDeg,
              join.sweepRadians * kRadToDeg, false);
  }
  appendOffsetPoint(out, join.leaving, started);
}

bool swallowedByJoin(std::span<const OffsetJoin> joins, const Contour& contour,
                     float distance) {
  const float len = contour.length();
  for (const OffsetJoin& join : joins)
    if (join.miter && ((distance > join.distance - join.radius &&
                        distance < join.distance + join.radius) ||
                       (contour.closed() && join.distance < join.radius &&
                        distance > len - (join.radius - join.distance))))
      return true;
  return false;
}

SkPath parallel(const SkPath& path, float across, float step) {
  if (across == 0) return path;
  // `beside` measures to the right of travel; the parallel is asked for
  // on the left.
  const float side = -across;
  const float stride = std::isfinite(step) ? std::max(step, 0.5f) : 0.5f;
  SkPathBuilder out(path.getFillType());
  for (const Contour& contour : Contour::of(path)) {
    const float len = contour.length();
    const std::vector<OffsetJoin> joins =
        offsetJoins(contour, [across](float) { return across; }, stride);
    size_t next = 0;
    bool started = false;
    for (float d = 0;; d += stride) {
      const float at = std::min(d, len);
      while (next < joins.size() && joins[next].distance <= at)
        appendOffsetJoin(out, joins[next++], started);
      const auto sample = contour.at(at);
      if (!sample) break;
      if (!swallowedByJoin(joins, contour, at))
        appendOffsetPoint(out, beside(*sample, side), started);
      if (at >= len) break;
    }
    if (contour.closed()) out.close();
  }
  return out.detach();
}

SkPath displace(const SkPath& path, float amplitude, float wavelength,
                bool zigzag) {
  SkPathBuilder out(path.getFillType());
  for (const Contour& contour : Contour::of(path)) {
    const float len = contour.length();
    const float lambdaMax = std::max(wavelength, 2.0f);
    const float lambda = len / std::max(1.0f, std::round(len / lambdaMax));
    const float step = zigzag ? lambda * 0.25f : lambda * 0.0625f;
    bool first = true;
    int k = 0;
    for (float d = 0;; d += step, ++k) {
      const float at = std::min(d, len);
      const auto s = contour.at(at);
      if (!s) break;
      float disp;
      if (zigzag) {
        static constexpr float kQuarters[4] = {0, 1, 0, -1};
        disp = amplitude * kQuarters[k % 4];
      } else {
        disp = amplitude * std::sin(at * kTau / lambda);
      }
      // Both kinds are zero-phase at the endpoints, so an open contour's
      // ends stay on the source curve regardless of float drift.
      if (at >= len) disp = 0;
      const glm::vec2 p = beside(*s, disp);
      if (first) {
        out.moveTo(toSk(p));
        first = false;
      } else {
        out.lineTo(toSk(p));
      }
      if (at >= len) break;
    }
    if (contour.closed()) out.close();
  }
  return out.detach();
}

SkPath cornerWindows(const SkPath& path, float radius, bool keepNearCorners,
                     float angleDeg) {
  SkPathBuilder out;
  for (const Contour& contour : Contour::of(path)) {
    const float len = contour.length();
    const bool closed = contour.closed();
    std::vector<float> corners;
    for (const Contour::Corner& c : contour.corners(angleDeg))
      corners.push_back(c.distance);
    if (!closed) {
      corners.insert(corners.begin(), 0.0f);
      corners.push_back(len);
    }
    if (corners.empty()) {
      if (!keepNearCorners) contour.appendSegment(out, 0, len);
      continue;
    }
    std::vector<std::pair<float, float>> near;
    for (float c : corners) {
      float a = c - radius, b = c + radius;
      if (!closed) {
        a = std::max(a, 0.0f);
        b = std::min(b, len);
        if (b > a) near.emplace_back(a, b);
        continue;
      }
      a = wrap(a, len);
      b = wrap(b, len);
      if (a <= b)
        near.emplace_back(a, b);
      else {  // the window straddles the seam
        near.emplace_back(a, len);
        near.emplace_back(0, b);
      }
    }
    std::sort(near.begin(), near.end());
    std::vector<std::pair<float, float>> merged;
    for (const auto& window : near) {
      if (!merged.empty() && window.first <= merged.back().second + 0.01f)
        merged.back().second = std::max(merged.back().second, window.second);
      else
        merged.push_back(window);
    }
    if (keepNearCorners) {
      for (const auto& window : merged)
        contour.appendSegment(out, window.first, window.second);
    } else {
      float cursor = 0;
      for (const auto& window : merged) {
        if (window.first > cursor)
          contour.appendSegment(out, cursor, window.first);
        cursor = std::max(cursor, window.second);
      }
      if (cursor < len) contour.appendSegment(out, cursor, len);
    }
  }
  return out.detach();
}

}  // namespace sigil::geometry::path
