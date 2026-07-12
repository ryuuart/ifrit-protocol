#include "textflow/Flow.h"

#include <algorithm>
#include <cmath>

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

} // namespace

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

  for (const Shape &shape : m_shapes) {
    if (shape.kind == Shape::kCircle) {
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
