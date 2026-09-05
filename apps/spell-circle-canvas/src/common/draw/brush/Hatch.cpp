/** @file
 * Parallel marks clipped to a polygon.
 */

#include "HatchLines.h"
#include "PolygonMath.h"

#include <sigilgeometry/path/Lattice.h>
#include <sigilgeometry/path/Skia.h>
#include <sigildraw/Pen.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Hatch.h>

#include <algorithm>
#include <array>
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
    Pen& pen, std::span<const geometry::path::Polyline> rings,
    const Hatch& style) {
  const float gradient = std::clamp(style.gradient, -1.0f, 1.0f);
  const geometry::path::LatticeOptions options{
      .spacing = style.spacing,
      .angle = style.angle,
      // The dial is a share of one step per lane either way, so its two
      // signs are each other's inverse rather than one added and one
      // subtracted.
      .taper = gradient >= 0.0f ? 1.0f + gradient * kGradientStep
                                : 1.0f / (1.0f - gradient * kGradientStep),
      .maxLines = kLaneCap};

  std::vector<HatchSegment> segments;
  for (const geometry::path::LatticeMark& mark :
       geometry::path::lattice(rings, options))
    segments.push_back(
        {geometry::path::toSk(mark.from), geometry::path::toSk(mark.to)});

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
