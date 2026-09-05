/** @file
 * The mass of curved gestures: chords across the shape bent into arcs
 * around a pivot outside it.
 */

#include "HatchLines.h"
#include "PolygonMath.h"

#include <sigildraw/Math.h>
#include <sigildraw/Pen.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Mass.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>

namespace sigil::draw::brush {

namespace {

/** The turn from @p start to @p stop going clockwise, in [0, 2π). */
float positiveSweep(float start, float stop) {
  float sweep = std::fmod(stop - start, TWO_PI);
  if (sweep < 0.0f) sweep += TWO_PI;
  return sweep;
}

/** Whether the arc's interior samples all lie inside the contours. */
bool arcFits(std::span<const std::span<const SkPoint>> contours, SkPoint center,
             float radius, float start, float stop) {
  const float sweep = positiveSweep(start, stop);
  for (int sample = 1; sample < 8; ++sample) {
    const float angle = start + sweep * (float)sample / 8.0f;
    if (!pointInContours(contours, {center.fX + radius * std::cos(angle),
                                    center.fY + radius * std::sin(angle)}))
      return false;
  }
  return true;
}

/** The shorter arc between two chord ends, or the other way round,
 *  whichever stays inside; neither answers nothing. */
std::optional<std::pair<float, float>> massArc(
    std::span<const std::span<const SkPoint>> contours, SkPoint center,
    float radius, SkPoint from, SkPoint to) {
  float start = std::atan2(from.fY - center.fY, from.fX - center.fX);
  float stop = std::atan2(to.fY - center.fY, to.fX - center.fX);
  if (positiveSweep(start, stop) > PI) std::swap(start, stop);
  if (arcFits(contours, center, radius, start, stop))
    return std::pair{start, stop};
  if (arcFits(contours, center, radius, stop, start))
    return std::pair{stop, start};
  return std::nullopt;
}

Stroke arcStroke(Pen& pen, SkPoint center, float radius, float start,
                 float stop, float wiggle) {
  const float sweep = positiveSweep(start, stop);
  const int steps = std::clamp((int)std::ceil(sweep / TWO_PI * 96.0f), 8, 96);
  const float phase = pen.random(0.0f, 2048.0f);
  Stroke result;
  result.reserve((size_t)steps + 1);
  for (int step = 0; step <= steps; ++step) {
    const float progress = (float)step / (float)steps;
    const float angle = start + sweep * progress;
    const float displacement =
        (pen.noise(progress * 4.0f, phase, 0.37f) - 0.5f) * 2.0f * wiggle;
    const float displacedRadius = std::max(0.0f, radius + displacement);
    result.push_back({{center.fX + displacedRadius * std::cos(angle),
                       center.fY + displacedRadius * std::sin(angle)},
                      1.0f});
  }
  return result;
}

/** A chord broken into two around a gap near its middle. */
std::array<HatchSegment, 2> splitSegment(Pen& pen,
                                         const HatchSegment& segment) {
  const float at = pen.random(0.35f, 0.65f);
  const SkPoint middle{lerp(segment.from.fX, segment.to.fX, at),
                       lerp(segment.from.fY, segment.to.fY, at)};
  const float dx = segment.to.fX - segment.from.fX;
  const float dy = segment.to.fY - segment.from.fY;
  const float length = std::hypot(dx, dy);
  const float gap = pen.random(0.04f, 0.10f) * length * 0.5f;
  const float gx = length > 0.0f ? dx / length * gap : 0.0f;
  const float gy = length > 0.0f ? dy / length * gap : 0.0f;
  return {{{segment.from, {middle.fX - gx, middle.fY - gy}},
           {{middle.fX + gx, middle.fY + gy}, segment.to}}};
}

}  // namespace

void mass(Pen& pen, const Tool& tool, std::span<const SkPoint> polygon,
          const Mass& style) {
  const std::array<std::span<const SkPoint>, 1> contours{polygon};
  mass(pen, tool, contours, style);
}

void mass(Pen& pen, const Tool& tool,
          std::span<const std::span<const SkPoint>> contours,
          const Mass& style) {
  if (contours.empty() || !(tool.width > 0.0f)) return;
  for (const std::span<const SkPoint> contour : contours)
    if (contour.size() < 3) return;

  const float precision = std::clamp(style.precision, 0.0f, 1.0f);
  const float strength = std::clamp(style.strength, 0.0f, 1.0f);
  const float gradient = std::clamp(style.gradient, -1.0f, 1.0f);
  const int passes =
      1 + (strength > 0.33f ? 1 : 0) + (strength > 0.66f ? 1 : 0);
  const float hatchSpacing = std::max(
      0.125f, 1.6f * pen.random(tool.scatter * 0.65f, tool.scatter * 0.85f) -
                  0.4f * gradient);
  const float baseAngle = pen.random(-HALF_PI, HALF_PI);
  const bool positive = baseAngle >= 0.0f;
  const bool firstBias = pen.random() < 0.5f;
  const SkPoint pivotBias = positive
                                ? (firstBias ? SkPoint{1, 1} : SkPoint{-1, -1})
                                : (firstBias ? SkPoint{-1, 1} : SkPoint{1, -1});

  if (style.outline) {
    for (const std::span<const SkPoint> contour : contours)
      for (size_t index = 0; index < contour.size(); ++index)
        line(pen, tool, contour[index], contour[(index + 1) % contour.size()]);
  }

  for (int pass = 0; pass < passes; ++pass) {
    const float maximumJitter = std::min(tool.scatter * 2.0f, 5.0f);
    const SkPoint translation =
        pass == 0 ? SkPoint{0, 0}
                  : SkPoint{pen.random(-maximumJitter, maximumJitter),
                            pen.random(-maximumJitter, maximumJitter)};
    std::vector<std::vector<SkPoint>> storage;
    std::vector<std::span<const SkPoint>> layer;
    storage.reserve(contours.size());
    layer.reserve(contours.size());
    for (const std::span<const SkPoint> contour : contours) {
      std::vector<SkPoint>& moved = storage.emplace_back();
      moved.reserve(contour.size());
      for (const SkPoint point : contour)
        moved.push_back({point.fX + translation.fX, point.fY + translation.fY});
    }
    for (const std::vector<SkPoint>& contour : storage)
      layer.push_back(contour);

    const float angleJitter = pass == 1 ? 20.0f : 15.0f;
    const float angle =
        baseAngle +
        (pass == 0 ? 0.0f : radians(pen.random(-angleJitter, angleJitter)));
    const float spacingScale = pass == 0 ? 0.9f : (pass == 1 ? 1.0f : 0.8f);
    const float jitter =
        pass == 0 ? 2.0f - 2.0f * precision : 0.6f - 0.6f * precision;
    const Hatch hatchStyle{.spacing = hatchSpacing * spacingScale,
                           .angle = angle,
                           .jitter = jitter,
                           .gradient = gradient,
                           .continuous = layer.size() == 1};
    const std::vector<HatchSegment> segments =
        hatchLines(pen, layer, hatchStyle);
    const SkRect bounds = contourBounds(layer);
    const float size = std::hypot(bounds.width(), bounds.height());
    const float anchorDistance = size * pen.random(0.6f, 1.4f);
    const SkPoint anchor{bounds.centerX() + pivotBias.fX * anchorDistance,
                         bounds.centerY() + pivotBias.fY * anchorDistance};

    for (const HatchSegment& segment : segments) {
      const bool shouldSplit = !segment.connector && pen.random() < 0.35f;
      const std::array<HatchSegment, 2> split =
          shouldSplit ? splitSegment(pen, segment)
                      : std::array<HatchSegment, 2>{segment, segment};
      const int partCount = shouldSplit ? 2 : 1;
      for (int partIndex = 0; partIndex < partCount; ++partIndex) {
        const HatchSegment& part = partCount == 1 ? segment : split[partIndex];
        const float dx = part.to.fX - part.from.fX;
        const float dy = part.to.fY - part.from.fY;
        const float length = std::hypot(dx, dy);
        if (!(length > 0.0f)) continue;
        const SkPoint middle{(part.from.fX + part.to.fX) * 0.5f,
                             (part.from.fY + part.to.fY) * 0.5f};
        const SkPoint normal{-dy / length, dx / length};
        const float projected = (anchor.fX - middle.fX) * normal.fX +
                                (anchor.fY - middle.fY) * normal.fY;
        SkPoint center{middle.fX + normal.fX * projected,
                       middle.fY + normal.fY * projected};
        if (layer.size() > 1) {
          const float shortness =
              1.0f - std::min(1.0f, length / std::max(size * 0.42f, 1.0f));
          const float bias = 0.08f + shortness * 0.18f;
          center = {center.fX + (middle.fX - center.fX) * bias,
                    center.fY + (middle.fY - center.fY) * bias};
        }
        const float radius =
            std::hypot(center.fX - part.from.fX, center.fY - part.from.fY);
        if (!(radius > 0.0f)) continue;
        const std::optional<std::pair<float, float>> angles =
            massArc(layer, center, radius, part.from, part.to);
        if (!angles) continue;
        paint(pen, tool,
              arcStroke(pen, center, radius, angles->first, angles->second,
                        2.0f - precision));
      }
    }
  }
}

}  // namespace sigil::draw::brush
