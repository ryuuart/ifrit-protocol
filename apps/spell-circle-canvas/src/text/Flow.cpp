#include "textflow/Flow.h"

#include <include/core/SkPathTypes.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace textflow {

namespace {

constexpr float kEps = 0.01f;

// Removes [lo, hi] from every interval in `avail` (sorted, disjoint).
void subtractSpan(std::vector<std::pair<float, float>> &avail, float lo,
                  float hi) {
  std::vector<std::pair<float, float>> next;
  next.reserve(avail.size() + 1);
  for (const auto &[a, b] : avail) {
    if (hi <= a || lo >= b) {
      next.emplace_back(a, b);
      continue;
    }
    if (lo > a)
      next.emplace_back(a, lo);
    if (hi < b)
      next.emplace_back(hi, b);
  }
  avail = std::move(next);
}

// Occupied x-intervals of a flattened polygon set within the band
// [top, bottom]: fill intervals sampled at three scanlines (respecting the
// fill rule, so holes and concave gaps stay open) unioned with every edge's
// x-travel through the band (conservative — catches features that fall
// between the samples, like a star tip). Appends to `occ` unmerged.
void bandOccupancy(const std::vector<std::vector<SkPoint>> &contours,
                   bool evenOdd, float top, float bottom,
                   std::vector<std::pair<float, float>> &occ) {
  static thread_local std::vector<std::pair<float, int>> crossings;
  const float ys[3] = {top + kEps, (top + bottom) * 0.5f, bottom - kEps};
  for (const float y : ys) {
    crossings.clear();
    for (const std::vector<SkPoint> &poly : contours) {
      for (size_t i = 0; i < poly.size(); ++i) {
        const SkPoint &p0 = poly[i];
        const SkPoint &p1 = poly[(i + 1) % poly.size()];
        const float y0 = p0.y(), y1 = p1.y();
        if (y0 == y1)
          continue;
        // Half-open [min, max) so shared vertices count exactly once.
        const bool up = y1 > y0;
        if (up ? (y < y0 || y >= y1) : (y < y1 || y >= y0))
          continue;
        const float t = (y - y0) / (y1 - y0);
        crossings.emplace_back(p0.x() + t * (p1.x() - p0.x()), up ? 1 : -1);
      }
    }
    std::sort(crossings.begin(), crossings.end());
    int winding = 0, parity = 0;
    bool inside = false;
    float openX = 0;
    for (const auto &[x, dir] : crossings) {
      winding += dir;
      parity ^= 1;
      const bool nowInside = evenOdd ? parity != 0 : winding != 0;
      if (nowInside && !inside) {
        openX = x;
        inside = true;
      } else if (!nowInside && inside) {
        occ.emplace_back(openX, x);
        inside = false;
      }
    }
  }

  for (const std::vector<SkPoint> &poly : contours) {
    for (size_t i = 0; i < poly.size(); ++i) {
      const SkPoint &p0 = poly[i];
      const SkPoint &p1 = poly[(i + 1) % poly.size()];
      const float lo = std::min(p0.y(), p1.y());
      const float hi = std::max(p0.y(), p1.y());
      if (hi <= top || lo >= bottom)
        continue;
      float t0 = 0, t1 = 1;
      if (p0.y() != p1.y()) {
        const float ta = (top - p0.y()) / (p1.y() - p0.y());
        const float tb = (bottom - p0.y()) / (p1.y() - p0.y());
        t0 = std::clamp(std::min(ta, tb), 0.0f, 1.0f);
        t1 = std::clamp(std::max(ta, tb), 0.0f, 1.0f);
      }
      const float xA = p0.x() + t0 * (p1.x() - p0.x());
      const float xB = p0.x() + t1 * (p1.x() - p0.x());
      occ.emplace_back(std::min(xA, xB), std::max(xA, xB));
    }
  }
}

void mergeSpans(std::vector<std::pair<float, float>> &spans) {
  std::sort(spans.begin(), spans.end());
  size_t w = 0;
  for (const auto &span : spans) {
    if (w > 0 && span.first <= spans[w - 1].second)
      spans[w - 1].second = std::max(spans[w - 1].second, span.second);
    else
      spans[w++] = span;
  }
  spans.resize(w);
}

} // namespace

// Flattened-polygon form of an exclusion SkPath, cached by generation ID.
struct ExclusionFlow::FlatPath {
  std::vector<std::vector<SkPoint>> contours; // closed polylines
  SkRect bounds = SkRect::MakeEmpty();
  bool evenOdd = false;
};

ExclusionFlow::ExclusionFlow(const SkRect &rect) : m_rect(rect) {}
ExclusionFlow::~ExclusionFlow() = default;

const ExclusionFlow::FlatPath &ExclusionFlow::flattened(const SkPath &path) {
  const uint32_t key = path.getGenerationID();
  auto it = m_flatCache.find(key);
  if (it != m_flatCache.end())
    return *it->second;
  if (m_flatCache.size() > 64)
    m_flatCache.clear(); // paths churned every frame: don't grow forever

  auto flat = std::make_unique<FlatPath>();
  const SkPathFillType fill = path.getFillType();
  flat->evenOdd = fill == SkPathFillType::kEvenOdd ||
                  fill == SkPathFillType::kInverseEvenOdd;
  flat->bounds = path.computeTightBounds();

  // Fixed-count curve subdivision: layout avoidance needs a couple of pixels
  // of fidelity, not rendering accuracy.
  constexpr int kCurveSegs = 12;
  std::vector<SkPoint> poly;
  auto flush = [&] {
    if (poly.size() >= 3)
      flat->contours.push_back(std::move(poly));
    poly.clear();
  };

  SkPath::Iter iter(path, /*forceClose=*/true);
  SkPoint pts[4];
  for (;;) {
    const SkPath::Verb verb = iter.next(pts);
    if (verb == SkPath::kDone_Verb)
      break;
    switch (verb) {
    case SkPath::kMove_Verb:
      flush();
      poly.push_back(pts[0]);
      break;
    case SkPath::kLine_Verb:
      poly.push_back(pts[1]);
      break;
    case SkPath::kQuad_Verb:
      for (int i = 1; i <= kCurveSegs; ++i) {
        const float t = static_cast<float>(i) / kCurveSegs, u = 1 - t;
        poly.push_back(
            {u * u * pts[0].x() + 2 * u * t * pts[1].x() + t * t * pts[2].x(),
             u * u * pts[0].y() + 2 * u * t * pts[1].y() + t * t * pts[2].y()});
      }
      break;
    case SkPath::kConic_Verb: {
      const float w = iter.conicWeight();
      for (int i = 1; i <= kCurveSegs; ++i) {
        const float t = static_cast<float>(i) / kCurveSegs, u = 1 - t;
        const float denom = u * u + 2 * w * u * t + t * t;
        poly.push_back({(u * u * pts[0].x() + 2 * w * u * t * pts[1].x() +
                         t * t * pts[2].x()) /
                            denom,
                        (u * u * pts[0].y() + 2 * w * u * t * pts[1].y() +
                         t * t * pts[2].y()) /
                            denom});
      }
      break;
    }
    case SkPath::kCubic_Verb:
      for (int i = 1; i <= kCurveSegs; ++i) {
        const float t = static_cast<float>(i) / kCurveSegs, u = 1 - t;
        poly.push_back({u * u * u * pts[0].x() + 3 * u * u * t * pts[1].x() +
                            3 * u * t * t * pts[2].x() + t * t * t * pts[3].x(),
                        u * u * u * pts[0].y() + 3 * u * u * t * pts[1].y() +
                            3 * u * t * t * pts[2].y() +
                            t * t * t * pts[3].y()});
      }
      break;
    case SkPath::kClose_Verb:
      flush();
      break;
    default:
      break;
    }
  }
  flush();

  auto [pos, inserted] = m_flatCache.emplace(key, std::move(flat));
  return *pos->second;
}

bool BlockFlow::lineIntervals(int index, float lineHeight, float ascent,
                              std::vector<LineInterval> &out) {
  out.clear();
  const float top = m_rect.top() + static_cast<float>(index) * lineHeight;
  if (top + lineHeight > m_rect.bottom() + kEps)
    return false;
  LineInterval interval;
  interval.origin = {m_rect.left(), top + ascent};
  interval.dir = {1, 0};
  interval.length = m_rect.width();
  out.push_back(interval);
  return true;
}

bool ExclusionFlow::lineIntervals(int index, float lineHeight, float ascent,
                                  std::vector<LineInterval> &out) {
  out.clear();
  const float top = m_rect.top() + static_cast<float>(index) * lineHeight;
  const float bottom = top + lineHeight;
  if (bottom > m_rect.bottom() + kEps)
    return false;

  std::vector<std::pair<float, float>> avail = {
      {m_rect.left(), m_rect.right()}};

  static thread_local std::vector<std::pair<float, float>> occ;
  for (const Shape &shape : m_shapes) {
    if (shape.kind == Shape::kPath) {
      const FlatPath &flat = flattened(shape.path);
      if (flat.contours.empty())
        continue;
      const float ox = shape.pathOffset.x(), oy = shape.pathOffset.y();
      const float pad = shape.padding;
      if (flat.bounds.bottom() + oy + pad <= top ||
          flat.bounds.top() + oy - pad >= bottom)
        continue;
      // Band and results are in path-local space (shifted by the offset);
      // padding expands the band vertically and each span horizontally.
      occ.clear();
      bandOccupancy(flat.contours, flat.evenOdd, top - oy - pad,
                    bottom - oy + pad, occ);
      mergeSpans(occ);
      for (const auto &[a, b] : occ)
        subtractSpan(avail, a + ox - pad, b + ox + pad);
    } else if (shape.kind == Shape::kCircle) {
      const float r =
          std::min(shape.bounds.width(), shape.bounds.height()) * 0.5f +
          shape.padding;
      const float cx = shape.bounds.centerX();
      const float cy = shape.bounds.centerY();
      if (cy + r <= top || cy - r >= bottom)
        continue;
      // Widest chord of the circle within the band: at cy if the band
      // contains it, else at the nearest band edge.
      const float dy = (cy < top) ? top - cy : (cy > bottom ? cy - bottom : 0);
      if (dy >= r)
        continue;
      const float halfChord = std::sqrt(r * r - dy * dy);
      subtractSpan(avail, cx - halfChord, cx + halfChord);
    } else {
      SkRect b = shape.bounds.makeOutset(shape.padding, shape.padding);
      if (b.bottom() <= top || b.top() >= bottom)
        continue;
      subtractSpan(avail, b.left(), b.right());
    }
    if (avail.empty())
      break;
  }

  for (const auto &[a, b] : avail) {
    if (b - a < m_minIntervalWidth)
      continue;
    LineInterval interval;
    interval.origin = {a, top + ascent};
    interval.dir = {1, 0};
    interval.length = b - a;
    out.push_back(interval);
  }
  return true;
}

bool VerticalBlockFlow::lineIntervals(int index, float lineHeight,
                                      float /*ascent*/,
                                      std::vector<LineInterval> &out) {
  out.clear();
  const float right =
      m_rect.right() - static_cast<float>(index) * lineHeight;
  if (right - lineHeight < m_rect.left() - kEps)
    return false;
  LineInterval interval;
  interval.origin = {right - lineHeight * 0.5f, m_rect.top()};
  interval.dir = {0, 1};
  interval.length = m_rect.height();
  out.push_back(interval);
  return true;
}

bool LineSetFlow::lineIntervals(int index, float /*lineHeight*/,
                                float /*ascent*/,
                                std::vector<LineInterval> &out) {
  out.clear();
  if (index < 0 || static_cast<size_t>(index) >= m_lines.size())
    return false;
  out = m_lines[static_cast<size_t>(index)];
  return true;
}

PathFlow::PathFlow(const SkPath &path) { addPath(path); }

void PathFlow::addPath(const SkPath &path) {
  SkContourMeasureIter iter(path, /*forceClosed=*/false);
  while (sk_sp<SkContourMeasure> contour = iter.next())
    m_contours.push_back(std::move(contour));
}

bool PathFlow::lineIntervals(int index, float /*lineHeight*/,
                             float /*ascent*/,
                             std::vector<LineInterval> &out) {
  out.clear();
  if (index < 0 || static_cast<size_t>(index) >= m_contours.size())
    return false;
  LineInterval interval;
  interval.contour = m_contours[static_cast<size_t>(index)];
  interval.contourStart = 0;
  interval.length = interval.contour->length();
  out.push_back(interval);
  return true;
}

} // namespace textflow
