/** @file
 * Parallel marks clipped to a polygon.
 */

#include "HatchLines.h"

#include <sigildraw/Pen.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Hatch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace sigil::draw::brush {

namespace {

/** Every lane's spacing grows or shrinks by this share of the gradient. */
constexpr float kGradientStep = 0.1f;
constexpr int kLaneCap = 10000;

/** The pressure envelope of one hatch mark: thin at both ends. */
const Pressure kMarkPressure{0.16f, 1.0f, 0.16f};

}  // namespace

std::vector<HatchSegment> hatchLines(
    Pen& pen, std::span<const std::span<const SkPoint>> contours,
    const Hatch& style) {
  struct Edge {
    float x1;
    float y1;
    float x2;
    float y2;
  };

  // Rotate the contours so the scanlines are horizontal, scan, then
  // rotate the marks back.
  const float cosine = std::cos(style.angle);
  const float sine = std::sin(style.angle);
  float minimumY = std::numeric_limits<float>::infinity();
  float maximumY = -std::numeric_limits<float>::infinity();
  std::vector<Edge> edges;
  for (const std::span<const SkPoint> contour : contours) {
    if (contour.size() < 3) continue;
    std::vector<SkPoint> rotated;
    rotated.reserve(contour.size());
    for (const SkPoint point : contour) {
      const SkPoint transformed{point.fX * cosine - point.fY * sine,
                                point.fX * sine + point.fY * cosine};
      rotated.push_back(transformed);
      minimumY = std::min(minimumY, transformed.fY);
      maximumY = std::max(maximumY, transformed.fY);
    }
    for (size_t index = 0; index < rotated.size(); ++index) {
      const SkPoint from = rotated[index];
      const SkPoint to = rotated[(index + 1) % rotated.size()];
      if (from.fY != to.fY) edges.push_back({from.fX, from.fY, to.fX, to.fY});
    }
  }
  if (edges.empty() || !std::isfinite(minimumY) || !std::isfinite(maximumY))
    return {};

  std::vector<HatchSegment> segments;
  std::vector<float> crossings;
  float scanY = minimumY + style.spacing * 0.5f;
  float step = style.spacing;
  const float gradient = std::clamp(style.gradient, -1.0f, 1.0f);
  const float stepScale = gradient >= 0.0f
                              ? 1.0f + gradient * kGradientStep
                              : 1.0f / (1.0f - gradient * kGradientStep);
  int lanes = 0;
  while (scanY < maximumY && lanes++ < kLaneCap) {
    crossings.clear();
    for (const Edge& edge : edges) {
      if ((edge.y1 <= scanY) == (edge.y2 <= scanY)) continue;
      crossings.push_back(edge.x1 + (scanY - edge.y1) / (edge.y2 - edge.y1) *
                                        (edge.x2 - edge.x1));
    }
    std::ranges::sort(crossings);
    for (size_t index = 0; index + 1 < crossings.size(); index += 2) {
      const float fromX = crossings[index];
      const float toX = crossings[index + 1];
      segments.push_back({
          {fromX * cosine + scanY * sine, -fromX * sine + scanY * cosine},
          {toX * cosine + scanY * sine, -toX * sine + scanY * cosine},
      });
    }
    scanY += step;
    step = std::max(0.125f, step * stepScale);
  }

  const float jitter = std::max(0.0f, style.jitter) * style.spacing * 2.0f;
  if (jitter > 0.0f) {
    for (HatchSegment& segment : segments) {
      segment.from.fX += pen.random(-jitter, jitter);
      segment.from.fY += pen.random(-jitter, jitter);
      segment.to.fX += pen.random(-jitter, jitter);
      segment.to.fY += pen.random(-jitter, jitter);
    }
  }
  if (!style.continuous) return segments;

  std::vector<HatchSegment> continuous;
  continuous.reserve(segments.size() * 2);
  for (size_t index = 0; index < segments.size(); ++index) {
    HatchSegment segment = segments[index];
    if (index % 2 == 1) std::swap(segment.from, segment.to);
    if (!continuous.empty())
      continuous.push_back({continuous.back().to, segment.from, true});
    continuous.push_back(segment);
  }
  return continuous;
}

void hatch(Pen& pen, const Tool& tool, std::span<const SkPoint> polygon,
           const Hatch& style) {
  const std::array<std::span<const SkPoint>, 1> contours{polygon};
  hatch(pen, tool, contours, style);
}

void hatch(Pen& pen, const Tool& tool,
           std::span<const std::span<const SkPoint>> contours,
           const Hatch& style) {
  if (contours.empty() || !(style.spacing > 0.0f)) return;
  const std::vector<HatchSegment> segments = hatchLines(pen, contours, style);
  Tool mark = tool;
  mark.pressure = kMarkPressure;
  Stroke continuous;
  for (const HatchSegment& segment : segments) {
    if (style.continuous) {
      if (continuous.empty()) continuous.push_back({segment.from, 1.0f});
      continuous.push_back({segment.to, 1.0f});
    } else {
      line(pen, mark, segment.from, segment.to);
    }
  }
  if (style.continuous && continuous.size() >= 2) paint(pen, mark, continuous);
}

}  // namespace sigil::draw::brush
