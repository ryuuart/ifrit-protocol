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

#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry {

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

SkPath parallel(const SkPath& path, float across, float step) {
  if (across == 0) return path;
  // `beside` measures to the right of travel; the parallel is asked for
  // on the left.
  const float side = -across;
  const float stride = std::isfinite(step) ? std::max(step, 0.5f) : 0.5f;
  const float radius = std::abs(side);
  SkPathBuilder out;
  for (const Contour& contour : Contour::of(path)) {
    const float len = contour.length();
    struct Join {
      float d = 0;
      bool miter = false;
      glm::vec2 pt{0, 0};  // miter: the single replacement point
      glm::vec2 pIn{0, 0}, pOut{0, 0};
      glm::vec2 vertex{0, 0};
      float a0 = 0, sweep = 0;
      bool arc = false;
    };
    std::vector<Join> joins;
    for (const Contour::Corner& hit :
         contour.corners(20.0f, std::max(stride, 1.0f), stride)) {
      const auto v = contour.at(hit.distance);
      if (!v) continue;
      Join j;
      j.d = hit.distance;
      j.vertex = v->position;
      j.pIn = beside({j.vertex, hit.in}, side);
      j.pOut = beside({j.vertex, hit.out}, side);
      const float turn = hit.in.x * hit.out.y - hit.in.y * hit.out.x;
      if (turn * side >= 0.0f && std::abs(turn) > 1e-4f) {
        const glm::vec2 d = j.pOut - j.pIn;
        const float t = (d.x * hit.out.y - d.y * hit.out.x) / turn;
        if (std::abs(t) <= radius * 4.0f) {  // a near-reversal miters to
          j.miter = true;                    // infinity — bevel instead
          j.pt = {j.pIn.x + hit.in.x * t, j.pIn.y + hit.in.y * t};
        }
      } else if (turn * side < 0.0f) {
        j.arc = true;
        j.a0 = std::atan2(j.pIn.y - j.vertex.y, j.pIn.x - j.vertex.x);
        const float a1 =
            std::atan2(j.pOut.y - j.vertex.y, j.pOut.x - j.vertex.x);
        j.sweep = a1 - j.a0;
        while (j.sweep > kPi) j.sweep -= kTau;
        while (j.sweep < -kPi) j.sweep += kTau;
      }
      joins.push_back(j);
    }
    size_t next = 0;
    bool first = true;
    const auto push = [&](glm::vec2 p) {
      if (first) {
        out.moveTo(toSk(p));
        first = false;
      } else {
        out.lineTo(toSk(p));
      }
    };
    for (float d = 0;; d += stride) {
      const float at = std::min(d, len);
      while (next < joins.size() && joins[next].d <= at) {
        const Join& j = joins[next++];
        if (j.miter) {
          push(j.pt);
        } else {
          push(j.pIn);
          if (j.arc) {
            const SkRect oval =
                SkRect::MakeLTRB(j.vertex.x - radius, j.vertex.y - radius,
                                 j.vertex.x + radius, j.vertex.y + radius);
            out.arcTo(oval, j.a0 * kRadToDeg, j.sweep * kRadToDeg, false);
          }
          push(j.pOut);
        }
      }
      const auto s = contour.at(at);
      if (!s) break;
      bool swallowed = false;
      for (const Join& j : joins)
        if (j.miter &&
            ((at > j.d - radius && at < j.d + radius) ||
             (contour.closed() && j.d < radius && at > len - (radius - j.d)))) {
          swallowed = true;
          break;
        }
      if (!swallowed) push(beside(*s, side));
      if (at >= len) break;
    }
    if (contour.closed()) out.close();
  }
  return out.detach();
}

SkPath displace(const SkPath& path, float amplitude, float wavelength,
                bool zigzag) {
  SkPathBuilder out;
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

}  // namespace sigil::geometry
