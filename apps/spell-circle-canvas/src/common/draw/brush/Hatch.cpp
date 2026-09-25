/** @file
 * Parallel marks clipped to a polygon.
 */

#include <sigildraw/Pen.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Hatch.h>
#include <sigilgeometry/kit/Hatches.h>
#include <sigilgeometry/path/Skia.h>

#include <algorithm>
#include <array>
#include <utility>

#include "HatchLines.h"
#include "PolygonMath.h"

namespace sigil::draw::brush {

namespace {

/** Every lane's spacing grows or shrinks by this share of the gradient. */
constexpr float kGradientStep = 0.1f;

/** The pressure envelope of one hatch mark: thin at both ends. */
const Pressure kMarkPressure{0.16f, 1.0f, 0.16f};

}  // namespace

float gradientTaper(float gradient) {
  const float dial = std::clamp(gradient, -1.0f, 1.0f);
  // The dial is a share of one step per lane either way, so its two signs
  // are each other's inverse rather than one added and one subtracted.
  return dial >= 0.0f ? 1.0f + dial * kGradientStep
                      : 1.0f / (1.0f - dial * kGradientStep);
}

std::vector<HatchSegment> hatchLines(
    Pen& pen, std::span<const geometry::path::Polyline> rings,
    const Hatch& style) {
  std::vector<HatchSegment> segments;
  for (const geometry::path::LatticeMark& mark :
       geometry::shapes::hatchMarks(rings, style.pattern))
    segments.push_back(
        {geometry::path::toSk(mark.from), geometry::path::toSk(mark.to)});

  const float jitter =
      std::max(0.0f, style.jitter) * style.pattern.spacing * 2.0f;
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
  if (contours.empty() || !(style.pattern.spacing > 0.0f)) return;
  const std::vector<HatchSegment> segments =
      hatchLines(pen, rings(contours), style);
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
