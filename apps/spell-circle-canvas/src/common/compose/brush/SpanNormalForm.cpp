/** @file
 * Spans — the boundary's arc length in claimed runs, in one normal form:
 * the contour measure, the corner and fit windows, normalize, complement,
 * intersect, the span path, and the `spans::` vocabulary that resolves to
 * them.
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypes.h>  // SkDebugf — the slot-rename diagnostic
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/pathops/SkPathOps.h>

#include <algorithm>
#include <cmath>   // std::isfinite — the profileOffset non-finite guard
#include <cstdio>  // std::snprintf — variationDrive's effect key
#include <set>

#include "ComposeInternal.h"
#include "SpanArithmetic.h"
#include "SpanContours.h"
#include "sigilgeometry/path/Contour.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Spans: the boundary's arc length, in claimed runs
//
// Every answer is in ONE normal form (clamped, sorted, merged, non-empty)
// so overlap tests and complements are plain interval arithmetic. Any rule
// added here must normalize too: the alternative is per-rule special cases
// in every consumer, which is exactly how one rule ends up behaving subtly
// differently from its neighbours.

namespace detail {

std::vector<ContourRun> measureContours(const SkPath& path, float* total) {
  std::vector<ContourRun> runs;
  float at = 0;
  SkContourMeasureIter iter(path, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    runs.push_back({at, len, contour->isClosed()});
    at += len;
  }
  if (total) *total = at;
  return runs;
}

void pushWindow(std::vector<Span>& out, float lo, float hi, float total) {
  if (total <= 0 || hi <= lo) return;
  out.push_back({lo / total, hi / total});
}

/** A window around `d` on one contour, wrapping on a closed contour and
 *  clamping on an open one — an open contour has no seam to wrap over. */
void pushCornerWindow(std::vector<Span>& out, const ContourRun& run, float d,
                      float arm, float total) {
  const float lo = d - arm, hi = d + arm;
  if (!run.closed) {
    pushWindow(out, run.start + std::max(lo, 0.0f),
               run.start + std::min(hi, run.length), total);
    return;
  }
  if (lo < 0) {
    pushWindow(out, run.start + run.length + lo, run.start + run.length, total);
    pushWindow(out, run.start, run.start + std::min(hi, run.length), total);
  } else if (hi > run.length) {
    pushWindow(out, run.start + lo, run.start + run.length, total);
    pushWindow(out, run.start, run.start + (hi - run.length), total);
  } else {
    pushWindow(out, run.start + lo, run.start + hi, total);
  }
}

std::vector<Span> cornerSpans(const SkPath& outline, float arm,
                              float angleDeg) {
  std::vector<Span> out;
  float total = 0;
  const std::vector<ContourRun> runs = measureContours(outline, &total);
  if (total <= 0) return out;
  size_t i = 0;
  for (const geometry::path::Contour& contour :
       geometry::path::Contour::of(outline)) {
    if (i >= runs.size()) break;
    for (const geometry::path::Contour::Corner& hit :
         detail::cornersOrWarn(contour, angleDeg))
      pushCornerWindow(out, runs[i], hit.distance, arm, total);
    ++i;
  }
  return detail::normalizeSpans(out);
}

/** The runs of the outline that lie inside a rect — spans::fit(). Walked
 *  rather than solved: the boundary is any shape, including one the rect
 *  enters and leaves several times. */
std::vector<Span> fitSpans(const SkPath& outline, const SkRect& box,
                           float margin) {
  std::vector<Span> out;
  SkRect grown = box;
  grown.outset(margin, margin);
  float total = 0;
  const std::vector<ContourRun> runs = measureContours(outline, &total);
  if (total <= 0) return out;
  size_t i = 0;
  SkContourMeasureIter iter(outline, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    if (i >= runs.size()) break;
    const ContourRun& run = runs[i++];
    const float step = std::max(1.0f, run.length / 512.0f);
    bool inside = false;
    float enter = 0;
    // the loop walks a distance; the accumulated float is the position
    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
    for (float d = 0; d <= run.length + step * 0.5f; d += step) {
      SkPoint pos;
      const float at = std::min(d, run.length);
      if (!contour->getPosTan(at, &pos, nullptr)) continue;
      const bool now = grown.contains(pos.fX, pos.fY);
      if (now && !inside) {
        enter = at;
        inside = true;
      } else if (!now && inside) {
        pushWindow(out, run.start + enter, run.start + at, total);
        inside = false;
      }
    }
    if (inside)
      pushWindow(out, run.start + enter, run.start + run.length, total);
  }
  return detail::normalizeSpans(out);
}

}  // namespace detail

namespace detail {

std::vector<Span> normalizeSpans(const std::vector<Span>& spans) {
  std::vector<Span> out;
  out.reserve(spans.size());
  for (Span s : spans) {
    if (s.end < s.begin) std::swap(s.begin, s.end);
    s.begin = std::clamp(s.begin, 0.0f, 1.0f);
    s.end = std::clamp(s.end, 0.0f, 1.0f);
    if (s.end - s.begin > 1e-6f) out.push_back(s);
  }
  std::sort(out.begin(), out.end(),
            [](const Span& a, const Span& b) { return a.begin < b.begin; });
  std::vector<Span> merged;
  for (const Span& s : out) {
    if (!merged.empty() && s.begin <= merged.back().end + 1e-6f)
      merged.back().end = std::max(merged.back().end, s.end);
    else
      merged.push_back(s);
  }
  return merged;
}

std::vector<Span> complementSpans(const std::vector<Span>& spans) {
  std::vector<Span> out;
  float at = 0;
  for (const Span& s : spans) {
    if (s.begin - at > 1e-6f) out.push_back({at, s.begin});
    at = std::max(at, s.end);
  }
  if (1.0f - at > 1e-6f) out.push_back({at, 1.0f});
  return out;
}

std::vector<Span> intersectSpans(const std::vector<Span>& a,
                                 const std::vector<Span>& b) {
  // Both inputs are normalized (sorted, disjoint, non-degenerate), so one
  // sweep suffices. Touching endpoints are not an intersection, by the
  // same 1e-6 rule normalizeSpans drops empties with — two runs meeting
  // at a corner share no arc length.
  std::vector<Span> out;
  size_t i = 0, j = 0;
  while (i < a.size() && j < b.size()) {
    const float lo = std::max(a[i].begin, b[j].begin);
    const float hi = std::min(a[i].end, b[j].end);
    if (hi - lo > 1e-6f) out.push_back({lo, hi});
    if (a[i].end < b[j].end)
      ++i;
    else
      ++j;
  }
  return out;
}

std::optional<Span> spansOverlap(const std::vector<Span>& a,
                                 const std::vector<Span>& b) {
  for (const Span& x : a)
    for (const Span& y : b) {
      const float lo = std::max(x.begin, y.begin);
      const float hi = std::min(x.end, y.end);
      // A shared END POINT is two runs meeting, not two runs overlapping —
      // exactly what corners() next to edges() produces, and it must not
      // be an error.
      if (hi - lo > 1e-4f) return Span{lo, hi};
    }
  return std::nullopt;
}

SkPath spanPath(const SkPath& src, const std::vector<Span>& spans) {
  SkPathBuilder out;
  float total = 0;
  const std::vector<ContourRun> runs = measureContours(src, &total);
  if (total <= 0 || spans.empty()) return out.detach();
  size_t i = 0;
  SkContourMeasureIter iter(src, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    if (i >= runs.size()) break;
    const ContourRun& run = runs[i++];
    // Emit one span against this contour. `stitch` appends WITHOUT a
    // moveTo, continuing the run already in flight.
    const auto emit = [&](const Span& s, bool stitch) {
      const float lo = std::max(s.begin * total, run.start) - run.start;
      const float hi =
          std::min(s.end * total, run.start + run.length) - run.start;
      if (hi - lo <= 1e-4f) return false;
      // A whole contour claimed whole stays whole — closed stays closed,
      // so joins and additive brushes behave as they do untrimmed.
      //
      // The close() is load-bearing. getSegment hands back an OPEN run
      // whose ends merely coincide, so without it the vertex at the seam
      // gets two butt caps instead of a miter join: a notch at one corner,
      // small under a hairline and obvious under any wide or additive
      // brush.
      if (lo <= 1e-4f && hi >= run.length - 1e-4f) {
        (void)contour->getSegment(0, run.length, &out, !stitch);
        if (run.closed && !stitch) out.close();
      } else
        (void)contour->getSegment(lo, hi, &out, !stitch);
      return true;
    };

    // THE SEAM. Spans arrive sorted, so a claim that straddles fraction 0
    // — a corner sitting on the seam, a wrapped window — arrives as its
    // two halves at opposite ends of the list. On a CLOSED contour those
    // halves are geometrically adjacent, and emitting them as two
    // subpaths makes round caps and additive halo brushes double-hit
    // there (the same defect the Wrap-mode trim path stitches away). So
    // emit the tail first and append the head to it. An OPEN contour has
    // no seam: joining its ends would invent a straight chord.
    const bool seamStraddled =
        run.closed && spans.size() >= 2 &&
        spans.front().begin * total <= run.start + 1e-4f &&
        spans.back().end * total >= run.start + run.length - 1e-4f;
    if (seamStraddled) {
      const bool inFlight = emit(spans.back(), false);
      (void)emit(spans.front(), inFlight);
      for (size_t k = 1; k + 1 < spans.size(); ++k) (void)emit(spans[k], false);
      continue;
    }
    for (const Span& s : spans) (void)emit(s, false);
  }
  return out.detach();
}

}  // namespace detail

Spans& Spans::offset(Animatable<float> by) {
  for (size_t i = 0; i + 1 < terms.size(); ++i) terms[i].offset = by;
  if (!terms.empty()) terms.back().offset = std::move(by);
  return *this;
}

std::vector<Span> Spans::resolve(const SpanInput& in) const {
  std::vector<Span> out;
  if (!in.outline) return out;
  // begin, end, offset per term — the order Instance::spanAnims and
  // spanEndpoints() both walk. The offset is ADDED to both ends before the
  // interval is read, which is exactly what trim() does with its third
  // argument (Bounds.cpp's trim block: s0 = start + off, e0 = end + off).
  auto at = [&](size_t i, float fallback) {
    return in.values && in.values->size() > i ? (*in.values)[i] : fallback;
  };
  for (size_t t = 0; t < terms.size(); ++t) {
    const Term& term = terms[t];
    switch (term.rule) {
      case Rule::Range: {
        const float off = at(t * 3 + 2, 0.0f);
        const float a = at(t * 3, 0.0f) + off;
        const float b = at(t * 3 + 1, 1.0f) + off;
        out.push_back({a, b});
        break;
      }
      case Rule::Wrap: {
        const float off = at(t * 3 + 2, 0.0f);
        const float a = at(t * 3, 0.0f) + off;
        const float b = at(t * 3 + 1, 1.0f) + off;
        // The same three cases TrimMode::Wrap resolves, and deliberately in
        // the same order: the RAW difference decides emptiness and fullness
        // (a window driven to [1.1, 1.35] is still a quarter of the cycle),
        // and only then do the endpoints wrap into [0,1).
        const float length = b - a;
        if (length <= 0.0f) break;  // claims nothing
        if (length >= 1.0f) {
          out.push_back({0.0f, 1.0f});  // the whole cycle
          break;
        }
        const float s = a - std::floor(a);
        const float e = b - std::floor(b);
        if (s < e) {
          out.push_back({s, e});
        } else {
          // Straddles the seam: two runs, which normalizeSpans sorts to the
          // ends of the list and spanPath stitches back into one contour.
          out.push_back({s, 1.0f});
          out.push_back({0.0f, e});
        }
        break;
      }
      case Rule::Corners: {
        const std::vector<Span> hits =
            cornerSpans(*in.outline, term.arm, term.angleDeg);
        out.insert(out.end(), hits.begin(), hits.end());
        break;
      }
      case Rule::Edges: {
        const std::vector<Span> gaps = detail::complementSpans(
            cornerSpans(*in.outline, term.arm, term.angleDeg));
        out.insert(out.end(), gaps.begin(), gaps.end());
        break;
      }
      case Rule::Every: {
        const int n = std::max(1, term.count);
        const float duty = std::clamp(term.duty, 0.0f, 1.0f);
        for (int k = 0; k < n; ++k)
          out.push_back({(float)k / (float)n, ((float)k + duty) / (float)n});
        break;
      }
      case Rule::At: {
        const int n = std::max(1, term.count);
        const int k = std::clamp(term.index, 0, n - 1);
        out.push_back({(float)k / (float)n, (float)(k + 1) / (float)n});
        break;
      }
      case Rule::Fit: {
        if (!in.fitRects) break;
        for (const auto& [key, box] : *in.fitRects)
          if (key == term.key) {
            const std::vector<Span> hits =
                fitSpans(*in.outline, box, term.margin);
            out.insert(out.end(), hits.begin(), hits.end());
          }
        break;
      }
      case Rule::Rest:
        break;  // the complement needs the element's other passes
    }
  }
  return detail::normalizeSpans(out);
}

namespace spans {

Spans range(Animatable<float> begin, Animatable<float> end) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Range;
  t.begin = std::move(begin);
  t.end = std::move(end);
  s.terms.push_back(std::move(t));
  return s;
}
Spans wrap(Animatable<float> begin, Animatable<float> end) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Wrap;
  t.begin = std::move(begin);
  t.end = std::move(end);
  s.terms.push_back(std::move(t));
  return s;
}
Spans upTo(Animatable<float> end) { return range(0.0f, std::move(end)); }
Spans corners(float arm, float angleDeg) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Corners;
  t.arm = arm;
  t.angleDeg = angleDeg;
  s.terms.push_back(std::move(t));
  return s;
}
Spans edges(float arm, float angleDeg) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Edges;
  t.arm = arm;
  t.angleDeg = angleDeg;
  s.terms.push_back(std::move(t));
  return s;
}
Spans every(int count, float duty) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Every;
  t.count = count;
  t.duty = duty;
  s.terms.push_back(std::move(t));
  return s;
}
Spans at(int index, int count) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::At;
  t.index = index;
  t.count = count;
  s.terms.push_back(std::move(t));
  return s;
}
Spans fit(std::string_view key, float margin) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Fit;
  t.key = std::string(key);
  t.margin = margin;
  s.terms.push_back(std::move(t));
  return s;
}
Spans rest() {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Rest;
  s.terms.push_back(std::move(t));
  return s;
}
Spans rest(std::string_view passName) {
  Spans s;
  Spans::Term t;
  t.rule = Spans::Rule::Rest;
  t.key = std::string(passName);
  s.terms.push_back(std::move(t));
  return s;
}

}  // namespace spans

}  // namespace sigil::compose
