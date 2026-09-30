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
#include <optional>
#include <span>
#include <utility>

#include "OffsetInternal.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/advanced/Skia.h"

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

/** The one door into a contour's measure, which the header holds
 *  opaque. */
struct ContourAccess {
  static const SkContourMeasure* measure(const Contour& contour) {
    return static_cast<const SkContourMeasure*>(contour.m_measure.get());
  }
  static Contour make(std::shared_ptr<const SkContourMeasure> measure) {
    return Contour(std::move(measure));
  }
};

Contour::Contour(std::shared_ptr<const void> measure)
    : m_measure(std::move(measure)) {}

std::vector<Contour> contoursOf(const SkPath& path, bool forceClosed) {
  std::vector<Contour> out;
  SkContourMeasureIter iter(path, forceClosed);
  while (sk_sp<SkContourMeasure> m = iter.next())
    if (m->length() > 0)
      out.push_back(ContourAccess::make(std::shared_ptr<const SkContourMeasure>(
          m.release(), [](const SkContourMeasure* measure) {
            SkSafeUnref(measure);
          })));
  return out;
}

std::vector<Contour> Contour::of(const Outline& outline, bool forceClosed) {
  return contoursOf(toSk(outline), forceClosed);
}

float lengthOf(const SkPath& path) {
  float total = 0;
  SkContourMeasureIter iter(path, false);
  while (sk_sp<SkContourMeasure> m = iter.next()) total += m->length();
  return total;
}

float Contour::lengthOf(const Outline& outline) {
  return path::lengthOf(toSk(outline));
}

float Contour::length() const { return m_measure ? ContourAccess::measure(*this)->length() : 0; }

bool Contour::closed() const { return m_measure && ContourAccess::measure(*this)->isClosed(); }

std::optional<Contour::Sample> Contour::at(float distance) const {
  if (!m_measure) return std::nullopt;
  return sampleOf(*ContourAccess::measure(*this), std::clamp(distance, 0.0f, length()));
}

Contour::Sample Contour::around(float distance) const {
  const float len = length();
  if (len <= 0) return {};
  const float d =
      closed() ? wrap(distance, len) : std::clamp(distance, 0.0f, len);
  return at(d).value_or(Sample{});
}

SkPath segmentOf(const Contour& contour, float from, float to) {
  SkPathBuilder b;
  appendSegment(b, contour, from, to);
  return b.detach();
}

void appendSegment(SkPathBuilder& out, const Contour& contour, float from,
                   float to, bool startWithMoveTo) {
  const SkContourMeasure* measure = ContourAccess::measure(contour);
  if (!measure) return;
  (void)measure->getSegment(from, to, &out, startWithMoveTo);
}

std::pair<SkPath, SkPath> splitOf(const Contour& contour, float distance) {
  const float len = contour.length();
  const float at = std::clamp(distance, 0.0f, len);
  return {segmentOf(contour, 0, at), segmentOf(contour, at, len)};
}

Outline Contour::segment(float from, float to) const {
  return fromSk(segmentOf(*this, from, to));
}

std::pair<Outline, Outline> Contour::split(float distance) const {
  auto [before, after] = splitOf(*this, distance);
  return {fromSk(std::move(before)), fromSk(std::move(after))};
}

Contour::Nearest Contour::nearest(glm::vec2 point, float step) const {
  const float len = length();
  if (len <= 0) return {};
  const float stride = std::max(step, 0.25f);
  const auto gapAt = [&](float d) {
    const Sample s = at(d).value_or(Sample{});
    return glm::distance(s.position, point);
  };
  float best = 0, bestGap = gapAt(0);
  const int strides = (int)std::ceil(len / stride);
  for (int i = 1; i <= strides; ++i) {
    const float d = std::min((float)i * stride, len);
    const float gap = gapAt(d);
    if (gap < bestGap) {
      best = d;
      bestGap = gap;
    }
  }
  // Golden-section refinement over the strides either side of the best
  // sample: the gap is unimodal there unless the contour folds back on
  // itself inside one stride.
  constexpr float kInverseGolden = 0.6180339887f;
  float lo = std::max(0.0f, best - stride), hi = std::min(len, best + stride);
  for (int iteration = 0; iteration < 32 && hi - lo > 1e-4f; ++iteration) {
    const float left = hi - (hi - lo) * kInverseGolden;
    const float right = lo + (hi - lo) * kInverseGolden;
    if (gapAt(left) < gapAt(right))
      hi = right;
    else
      lo = left;
  }
  const float refined = 0.5f * (lo + hi);
  if (gapAt(refined) < bestGap) best = refined;
  const Sample s = at(best).value_or(Sample{});
  return {best, s.position, glm::distance(s.position, point)};
}

std::vector<Contour::Corner> Contour::corners(float angleDeg, float minSpacing,
                                              float step,
                                              float* sharpestDeg) const {
  std::vector<Corner> corners;
  const float len = length();
  if (len <= 0) return corners;
  const SkContourMeasure& m = *ContourAccess::measure(*this);
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
        // THE CORNER IS THE LAST DISTANCE THE INCOMING TANGENT STILL
        // HOLDS. A measure answers a vertex with the tangent of the
        // piece that ENDS there, so at a real vertex that distance is
        // the vertex itself — exactly, when a sample lands on it.
        // `bisect` answers the other side of the same bracket, the first
        // distance past the turn, which places a corner a fraction of a
        // stride down the outgoing edge and carries an offset walk's
        // join there with it.
        float at = std::max(0.0f, d - stride);
        (void)bisect(at, std::min(d, len), [&](float mid) {
          const auto ms = sampleOf(m, mid);
          if (!ms) return true;  // unevaluable: keep the near side
          if (dot(inTan, ms->tangent) < cosThresh) {
            outTan = ms->tangent;
            return false;
          }
          inTan = ms->tangent;
          at = mid;
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
    const std::function<float(float distance)>& acrossAt, float stride,
    Join outsideJoin, float miterLimit) {
  const std::vector<Contour::Corner> corners =
      contour.corners(20.0f, std::max(stride, 1.0f), stride);
  const float len = contour.length();
  const size_t count = corners.size();
  // HOW MUCH CONTOUR A CORNER HAS TO ITSELF, each way, before the next
  // corner — around the seam where the contour is closed and up to its
  // own ends where it is not. Nothing a corner does reaches past its
  // neighbour: the fold a turn makes is between the two edges this
  // vertex sits between, and past the next vertex the contour has
  // turned away.
  const auto behind = [&](size_t k) {
    if (k > 0) return corners[k].distance - corners[k - 1].distance;
    if (!contour.closed()) return corners[k].distance;
    return count > 1 ? len - corners.back().distance + corners[k].distance
                     : len;
  };
  const auto ahead = [&](size_t k) {
    if (k + 1 < count) return corners[k + 1].distance - corners[k].distance;
    if (!contour.closed()) return len - corners[k].distance;
    return count > 1 ? len - corners[k].distance + corners.front().distance
                     : len;
  };
  // `beside` measures to the right of travel; the offset is asked for on
  // the left.
  const auto sideAt = [&](float distance) { return -acrossAt(distance); };
  // THE HEADING OF AN OFFSET EDGE: its source tangent, slanted by how
  // fast the offset grows along it. Zero growth is the tangent itself,
  // to the bit.
  const auto slanted = [](glm::vec2 tangent, float rate) {
    return glm::vec2{tangent.x - tangent.y * rate, tangent.y + tangent.x * rate};
  };
  const auto cross = [](glm::vec2 a, glm::vec2 b) { return a.x * b.y - a.y * b.x; };
  std::vector<OffsetJoin> joins;
  for (size_t k = 0; k < count; ++k) {
    const Contour::Corner& hit = corners[k];
    const auto vertex = contour.at(hit.distance);
    if (!vertex) continue;
    const float side = sideAt(hit.distance);
    OffsetJoin join;
    join.distance = hit.distance;
    join.radius = std::abs(side);
    join.vertex = vertex->position;
    join.entering = beside({join.vertex, hit.in}, side);
    join.leaving = beside({join.vertex, hit.out}, side);
    const float turn = hit.in.x * hit.out.y - hit.in.y * hit.out.x;
    const float room = behind(k), roomOn = ahead(k);
    // How fast the offset grows along the edge arriving and the edge
    // leaving, read as the rise across `before` back and `after` on —
    // never past the neighbouring corner, where the edge has ended.
    const auto rates = [&](float before, float after) {
      const float back = std::clamp(before, 1e-3f, std::max(room, 1e-3f));
      const float on = std::clamp(after, 1e-3f, std::max(roomOn, 1e-3f));
      return std::pair{(side - sideAt(hit.distance - back)) / back,
                       (sideAt(hit.distance + on) - side) / on};
    };
    const glm::vec2 apart = join.leaving - join.entering;
    if (turn * side >= 0.0f && std::abs(turn) > 1e-4f) {
      const float reach = (apart.x * hit.out.y - apart.y * hit.out.x) / turn;
      // A TURN INTO THE OFFSET FOLDS THE TWO OFFSET EDGES ACROSS EACH
      // OTHER, and they meet `reach` back from where they end — the
      // offset over the tangent of half the interior angle, which at a
      // right angle is the offset, above one less and below one more.
      // There is no limit to cap that by: the meeting retreats ALONG
      // the edges rather than spiking away from them, and what bounds
      // it is the edge it retreats down. Cut each edge no further back
      // than its neighbouring corner and the two cuts are one point
      // wherever the corner has the room for it, and the chord across
      // the corner where it does not.
      const float fold = std::abs(reach);
      const float towards = reach < 0 ? -1.0f : 1.0f;
      // The fold a constant offset makes, which is the same distance back
      // along the edge arriving as on along the edge leaving.
      float foldBefore = fold, foldAfter = fold;
      glm::vec2 headingIn = hit.in, headingOut = hit.out;
      // A LAW THAT VARIES SLANTS BOTH EDGES, and the fold is where the
      // slanted edges meet. Each edge's heading is read as the rise from
      // the vertex to the place the fold stands at, and the fold is then
      // placed again from those headings; where it settles, the place it
      // names on each edge is the rail's own point at that distance, so
      // the rail before the corner, the cut and the rail after it are one
      // curve. A constant law rises by nothing and is left as it stands.
      if (const auto [rateIn, rateOut] = rates(fold, fold);
          rateIn != 0.0f || rateOut != 0.0f) {
        float before = fold, after = fold;
        bool settled = false;
        for (int pass = 0; pass < 16; ++pass) {
          const auto [slantIn, slantOut] = rates(before, after);
          const glm::vec2 alongIn = slanted(hit.in, slantIn);
          const glm::vec2 alongOut = slanted(hit.out, slantOut);
          const float meeting = cross(alongIn, alongOut);
          if (std::abs(meeting) <= 1e-6f) break;
          // Distances along each slanted edge from its end at the vertex:
          // back along the one arriving, on along the one leaving.
          const float nextBefore = -cross(apart, alongOut) / meeting;
          const float nextAfter = cross(apart, alongIn) / meeting;
          if (!std::isfinite(nextBefore) || !std::isfinite(nextAfter) ||
              nextBefore <= 0.0f || nextAfter <= 0.0f)
            break;
          headingIn = alongIn;
          headingOut = alongOut;
          settled = std::abs(nextBefore - before) < 1e-3f &&
                    std::abs(nextAfter - after) < 1e-3f;
          before = nextBefore;
          after = nextAfter;
          if (settled) break;
        }
        if (settled) {
          foldBefore = before;
          foldAfter = after;
        } else {
          headingIn = hit.in;
          headingOut = hit.out;
        }
      }
      const float back = towards * std::min(foldBefore, room);
      const float on = towards * std::min(foldAfter, roomOn);
      join.miter = true;
      join.cutEntering = {join.entering.x + headingIn.x * back,
                          join.entering.y + headingIn.y * back};
      join.cutLeaving = room >= foldBefore && roomOn >= foldAfter
                            ? join.cutEntering
                            : glm::vec2{join.leaving.x - headingOut.x * on,
                                        join.leaving.y - headingOut.y * on};
      // Every sample inside the fold stands past the meeting, so the
      // join answers for it. At a corner blunter than a right angle the
      // offset is the wider of the two and it is still the corner's own
      // place, because that is how far the rail stands from the spine
      // there.
      join.answersBefore = std::min(std::max(join.radius, foldBefore), room);
      join.answersAfter = std::min(std::max(join.radius, foldAfter), roomOn);
    } else if (turn * side < 0.0f && outsideJoin == Join::Miter) {
      // THE OUTSIDE OF A TURN, MITRED: the two offset edges carried on
      // past their ends to where they meet, along the headings they
      // arrive and leave with. Past `miterLimit` widths from the vertex
      // the point is refused and the corner is the chord, as a stroke's
      // own mitre is.
      const auto [rateIn, rateOut] = rates(stride, stride);
      const glm::vec2 alongIn = slanted(hit.in, rateIn);
      const glm::vec2 alongOut = slanted(hit.out, rateOut);
      const float meeting = cross(alongIn, alongOut);
      const float reach = cross(apart, alongOut) / meeting;
      const glm::vec2 point{join.entering.x + alongIn.x * reach,
                            join.entering.y + alongIn.y * reach};
      if (std::abs(meeting) > 1e-6f && std::isfinite(reach) && reach > 0.0f &&
          glm::distance(point, join.vertex) <= miterLimit * join.radius) {
        join.miter = true;
        join.cutEntering = point;
        join.cutLeaving = point;
      }
    } else if (turn * side < 0.0f && outsideJoin == Join::Round) {
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
  if (!started) {
    out.moveTo(toSk(point));
    started = true;
    return;
  }
  // ONE PLACE IS WRITTEN ONCE. A join's own end and the arc that already
  // landed there are the same point twice, and the segment between them
  // is nothing at all — but a reader asking whether the rail doubles
  // back sees two edges meeting at a point with an edge between them,
  // and cannot tell that from a fold.
  const std::optional<SkPoint> last = out.getLastPt();
  if (last && *last == toSk(point)) return;
  out.lineTo(toSk(point));
}

void appendOffsetJoin(SkPathBuilder& out, const OffsetJoin& join,
                      bool& started) {
  if (join.miter) {
    appendOffsetPoint(out, join.cutEntering, started);
    appendOffsetPoint(out, join.cutLeaving, started);
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
  const bool wraps = contour.closed() && len > 0;
  for (const OffsetJoin& join : joins) {
    float delta = distance - join.distance;
    // A closed contour has no first place and no last, so two of its
    // places are as far apart as the nearer way round the seam.
    if (wraps) delta = wrap(delta + len * 0.5f, len) - len * 0.5f;
    if (delta < 0 ? -delta < join.answersBefore : delta < join.answersAfter)
      return true;
  }
  return false;
}

bool joinAlreadyWrote(std::span<const OffsetJoin> written, float distance) {
  return !written.empty() && distance <= written.back().distance;
}

SkPath parallel(const SkPath& path, float across, float step, Join join,
                float miterLimit) {
  if (across == 0) return path;
  // `beside` measures to the right of travel; the parallel is asked for
  // on the left.
  const float side = -across;
  const float stride = std::isfinite(step) ? std::max(step, 0.5f) : 0.5f;
  SkPathBuilder out(path.getFillType());
  for (const Contour& contour : contoursOf(path)) {
    const float len = contour.length();
    const std::vector<OffsetJoin> joins =
        offsetJoins(contour, [across](float) { return across; }, stride, join,
                    miterLimit);
    size_t next = 0;
    bool started = false;
    for (float d = 0;; d += stride) {
      const float at = std::min(d, len);
      while (next < joins.size() && joins[next].distance <= at)
        appendOffsetJoin(out, joins[next++], started);
      const auto sample = contour.at(at);
      if (!sample) break;
      if (!joinAlreadyWrote(std::span(joins).first(next), at) &&
          !swallowedByJoin(joins, contour, at))
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
  for (const Contour& contour : contoursOf(path)) {
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
  for (const Contour& contour : contoursOf(path)) {
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
      if (!keepNearCorners) appendSegment(out, contour, 0, len);
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
        appendSegment(out, contour, window.first, window.second);
    } else {
      float cursor = 0;
      for (const auto& window : merged) {
        if (window.first > cursor)
          appendSegment(out, contour, cursor, window.first);
        cursor = std::max(cursor, window.second);
      }
      if (cursor < len) appendSegment(out, contour, cursor, len);
    }
  }
  return out.detach();
}

Outline parallel(const Outline& outline, float across, float step, Join join,
                 float miterLimit) {
  return fromSk(parallel(toSk(outline), across, step, join, miterLimit));
}

Outline displace(const Outline& outline, float amplitude, float wavelength,
                 bool zigzag) {
  return fromSk(displace(toSk(outline), amplitude, wavelength, zigzag));
}

Outline cornerWindows(const Outline& outline, float radius,
                      bool keepNearCorners, float angleDeg) {
  return fromSk(
      cornerWindows(toSk(outline), radius, keepNearCorners, angleDeg));
}

}  // namespace sigil::geometry::path
